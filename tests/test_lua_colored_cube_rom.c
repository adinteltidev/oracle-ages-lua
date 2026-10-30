#include "unit.h"
#include "core/gb.h"
#include "core/bus.h"
#include "core/sha1.h"
#include "core/hash.h"
#include "assets/assets.h"
#include "platform/setup.h"
#include "platform/tas.h"
#include "platform/tempfile.h"
#include "rt/fibers.h"
#include "hw/render.h"
#include "game/game.h"
#include "game/gen.h"
#include "script/colored_cube.h"

// Exercise the production private palette routine, without copying its logic.
#define interactionCode1a_hook unused_phase2_flame_hook
#include "../src/game/ages/interactions/coloredCubeFlame.c"
#undef interactionCode1a_hook

#define OBJECT 0xd240
static GB *g,*seed,*prepared,*expected,*engine_seed;
static GBSample *samples;
static RenderLineRegs render_seed;
static unsigned cases;
static const char *tas_dir=GAME_ROM_DIR "/../tas";
static const char *context;
static const char *fixture(const char *name) {
  static char path[4096]; int n=snprintf(path,sizeof path,"%s/%s",tas_dir,name);
  ASSERT(n>0 && (size_t)n<sizeof path); return path;
}
static uint8_t input(void *ctx,uint64_t frame) { return tas_input_at((Tas *)ctx,frame); }
static bool ready(void) {
  unsigned x=g->wram[1][0x0c]|g->wram[1][0x0d]<<8,y=g->wram[1][0x0a]|g->wram[1][0x0b]<<8;
  return g->wram[0][0xc2d]==0 && g->wram[0][0xc30]==0x8a && g->wram[0][0x2ee]==2 &&
    g->wram[0][0x2ef]<=1 && g->wram[1][4]==1 && !g->wram[0][0xba0] &&
    !(g->wram[0][0xc8a]&0x81) && !g->wram[0][0x4ab] &&
    !(g->wram[0][0xd00]&0x0e) && !g->wram[0][0xc4f] && y==56*256 && x>=70*256 && x<=76*256;
}
static void prepare_scene(void) {
  Tas t={0}; ASSERT(tas_load(&t,fixture("ages-consoleverified.inputs")));
  ASSERT(oracles_load_boot_state(g,fixture("ages-boot.state"))); g->input_at=input; g->input_ctx=&t;
  for(unsigned i=0;i<20000 && !ready();++i) { gb_run_frame(g); ASSERT(!g->hung); g->apu.ring_r=g->apu.ring_w; }
  ASSERT(ready()); g->input_at=NULL; g->input_ctx=NULL; g->joy=0;
  for(unsigned i=0;i<10;++i) { gb_run_frame(g); ASSERT(!g->hung); g->apu.ring_r=g->apu.ring_w; }
  for(unsigned i=0;i<120 && !threads_parked(g);++i) { gb_run_frame(g); ASSERT(!g->hung); g->apu.ring_r=g->apu.ring_w; }
  ASSERT(threads_parked(g)); tas_free(&t); *engine_seed=*g; render_seed=*render_line_regs(g);
  g->ime=false; g->ie=0; *seed=*g;
}
static void base(uint8_t state,uint8_t position,uint8_t color,uint8_t flags) {
  struct Fibers *fib=g->fib; fibers_reset(g); *g=*seed; g->fib=fib;
  memset(g->samples,0,16*sizeof *g->samples); g->sample_head=g->sample_count=0; g->sample_overflow=0; g->sample=&g->samples[0];
  *render_line_regs(g)=render_seed; mem_wr(g,IO_SVBK,1); mem_wr(g,hActiveObjectType,INTERACTION_BASE);
  for(unsigned i=0;i<0x40;++i) mem_wr(g,OBJECT+i,0);
  mem_wr(g,OBJECT,1); mem_wr(g,OBJECT+OBJ_ID,0x1a); mem_wr(g,OBJECT+OBJ_STATE,state);
  mem_wr(g,OBJECT+OBJ_XH,0x48); mem_wr(g,OBJECT+OBJ_YH,0x48); mem_wr(g,OBJECT+OBJ_OAM_FLAGS,flags);
  mem_wr(g,wRotatingCubePos,position); mem_wr(g,wRotatingCubeColor,color); mem_wr(g,wActiveMusic,3);
  g->d=OBJECT>>8; g->sp=wThread0StackTop-2; g->rom_bank=SYMBANK(interactionCode1a); mem_wr(g,hRomBank,g->rom_bank);
  mem_wr(g,g->sp,0); mem_wr(g,g->sp+1,0);
}
static void compare(bool lua) {
  if(!lua) { *expected=*g; memcpy(samples,g->samples,16*sizeof *samples); }
  else {
    if(memcmp(expected,g,sizeof *g)) fprintf(stderr,"mismatch: %s case %u\n",context,cases);
    ASSERT(!memcmp(expected,g,sizeof *g)); ASSERT(!memcmp(samples,g->samples,16*sizeof *samples));
  }
}
static void palette_cases(void) {
  context="ROM palette and OAM";
  for(unsigned color=0;color<256;++color) {
    if((color&0x7f)>=3) continue;
    unsigned count=256;
    for(unsigned f=0;f<count;++f) {
      uint8_t flags=count==256?f:(const uint8_t[]){0,7,0xa8,0xff}[f];
      for(int mode=0;mode<2;++mode) {
        base(1,0x44,color,flags); gameplay_enable(mode!=0); g->pc=SYM(interactionCode1a)+38;
        coloredCubeFlame_updateColor(g);
        compare(mode!=0);
        if(!g->hung && (color&0x7f)<3) {
          uint8_t palette=(const uint8_t[]){2,3,1}[color&0x7f];
          ASSERT_EQ(mem_rd(g,OBJECT+OBJ_OAM_FLAGS),(flags&0xf8)|palette);
        }
      }
      ++cases;
    }
  }
}
static void absent_and_hidden(void) {
  context="absent cube / hidden flame";
  for(unsigned color=0;color<256;++color) for(int dispatch=0;dispatch<2;++dispatch) {
    for(int mode=0;mode<2;++mode) {
      base(0,0,color,0xd5); gameplay_enable(mode!=0); g->pc=SYM(interactionCode1a);
      if(dispatch) ASSERT(hook_dispatch(g)); else hook_run(g,interactionCode1a_hook,SYM(interactionCode1a));
      ASSERT(!g->hung); ASSERT_EQ(mem_rd(g,OBJECT+OBJ_STATE),0); ASSERT_EQ(mem_rd(g,OBJECT+OBJ_OAM_FLAGS),0xd5); compare(mode!=0);
    }
    ++cases;
  }
  for(unsigned color=0;color<128;++color) for(int mode=0;mode<2;++mode) {
    base(1,0x44,color,0xb5); gameplay_enable(mode!=0); g->pc=SYM(interactionCode1a);
    hook_run(g,interactionCode1a_hook,SYM(interactionCode1a)); ASSERT(!g->hung); compare(mode!=0);
    if(mode) ++cases;
  }
}
static void frame_cases(bool serialize) {
  context=serialize?"fresh VM save/load":"initialization, color changes and room re-entry";
  char path[4096]; ASSERT(oracles_temp_path(path,sizeof path));
  static const uint8_t colors[]={0x80,0x81,0x82,0,1,2,0x82,0x80};
  GB *trajectory=malloc(40*sizeof *trajectory); uint64_t hashes[40]; ASSERT(trajectory);
  for(int state=0;state<2;++state) {
    base(0,0x44,0x80,0xa8); *prepared=*g;
    for(int mode=0;mode<2;++mode) {
      struct Fibers *fib=g->fib; fibers_reset(g); *g=*prepared; g->fib=fib;
      *render_line_regs(g)=render_seed;
      memcpy(g,engine_seed,offsetof(GB,rom)); g->rom_bank=engine_seed->rom_bank; g->ram_bank=engine_seed->ram_bank;
      g->ie=engine_seed->ie; memcpy(g->io,engine_seed->io,sizeof g->io); memcpy(g->hram,engine_seed->hram,sizeof g->hram);
      g->input_at=NULL; g->input_ctx=NULL; g->joy=0; memset(g->samples,0,16*sizeof *g->samples);
      gameplay_enable(mode!=0);
      for(unsigned frame=0;frame<40;++frame) {
        if((frame==0 || frame==8) && serialize) {
          ASSERT(oracles_save_boot_state(g,path)); ASSERT(oracles_load_boot_state(g,path));
          gameplay_close(); ASSERT(gameplay_init()); gameplay_enable(mode!=0);
        }
        if(frame==24 && state) {
          // A room unload/reload replaces the object, while C owns its state.
          for(unsigned i=0;i<0x40;++i) mem_wr(g,OBJECT+i,0);
          mem_wr(g,OBJECT,1); mem_wr(g,OBJECT+OBJ_ID,0x1a);
          mem_wr(g,OBJECT+OBJ_XH,0x58); mem_wr(g,OBJECT+OBJ_YH,0x48); mem_wr(g,wRotatingCubePos,0x45);
        }
        mem_wr(g,wRotatingCubeColor,colors[frame/5]);
        gb_run_frame(g);
        if(g->hung) fprintf(stderr,"engine stopped: %s frame=%u reason=%s\n",context,frame,gb_stop_reason);
        ASSERT(!g->hung); ASSERT(!g->sample_overflow); g->apu.ring_r=g->apu.ring_w;
        uint64_t hash=fnv1a64_update(FNV1A64_INIT,(const unsigned char *)g->sample,sizeof *g->sample);
        if(!mode) { trajectory[frame]=*g; hashes[frame]=hash; }
        else { ASSERT(!memcmp(&trajectory[frame],g,sizeof *g)); ASSERT_EQ(hashes[frame],hash); }
      }
    }
    ++cases;
  }
  if(serialize) ASSERT_EQ(remove(path),0); free(trajectory);
}
static void failures_and_authority(void) {
  for(int rule=0;rule<3;++rule) {
    char source[512]; int n=snprintf(source,sizeof source,"return {present=function() %s end,visible=function() %s end,palette_offset=function() %s end}",rule==0?"return 1":"return true",rule==1?"return 1":"return true",rule==2?"return 128":"return 0");
    GameplaySources override={.colored_cube=source,.colored_cube_size=(size_t)n}; ASSERT(gameplay_load_sources(&override)); gameplay_enable(true);
    base(rule==1?1:0,0x44,0x80,0xd5); g->pc=SYM(interactionCode1a); hook_run(g,interactionCode1a_hook,SYM(interactionCode1a));
    ASSERT(g->hung); ASSERT_EQ(mem_rd(g,OBJECT+OBJ_OAM_FLAGS),0xd5); ASSERT_EQ(mem_rd(g,OBJECT+OBJ_STATE),rule==1?1:0); ASSERT(strstr(gameplay_error(),"colored_cube"));
  }
  const char source[]="return {present=function() return false end,visible=function() return false end,palette_offset=function() return 2 end}";
  GameplaySources override={.colored_cube=source,.colored_cube_size=sizeof source-1}; ASSERT(gameplay_load_sources(&override)); gameplay_enable(true);
  base(0,0x44,0x80,0xd5); g->pc=SYM(interactionCode1a); hook_run(g,interactionCode1a_hook,SYM(interactionCode1a)); ASSERT(!g->hung); ASSERT_EQ(mem_rd(g,OBJECT+OBJ_STATE),0);
  base(1,0x44,0x80,0xa8); mem_wr(g,OBJECT+OBJ_VISIBLE,0x82); g->pc=SYM(interactionCode1a); hook_run(g,interactionCode1a_hook,SYM(interactionCode1a)); ASSERT(!g->hung); ASSERT_EQ(mem_rd(g,OBJECT+OBJ_VISIBLE)&0x80,0);
  base(1,0x44,0,0xa8); coloredCubeFlame_updateColor(g); ASSERT(!g->hung); ASSERT_EQ(mem_rd(g,OBJECT+OBJ_OAM_FLAGS),0xa9);
  // No phase-2 consultation when Seasons is selected.
  uint64_t calls=gameplay_colored_cube_calls(GAMEPLAY_COLORED_CUBE_PALETTE); syms_select(1,0);
  base(1,0x44,0,0xa8); coloredCubeFlame_updateColor(g); ASSERT_EQ(gameplay_colored_cube_calls(GAMEPLAY_COLORED_CUBE_PALETTE),calls); syms_select(0,0);
  gameplay_close(); ASSERT(gameplay_init());
}
int main(int argc,char **argv) {
  int invalid=-1; bool reference=false;
  const char *rom_path=GAME_ROM_DIR "/Legend of Zelda, The - Oracle of Ages (USA, Australia).gbc";
  for(int i=1;i<argc;++i) {
    if(i+1>=argc) return 2;
    if(!strcmp(argv[i],"--rom")) rom_path=argv[++i]; else if(!strcmp(argv[i],"--tas-dir")) tas_dir=argv[++i];
    else if(!strcmp(argv[i],"--invalid-color")) { char *end; unsigned long value=strtoul(argv[++i],&end,10); if(*end || value>255) return 2; invalid=(int)value; }
    else if(!strcmp(argv[i],"--mode")) { const char *mode=argv[++i]; if(strcmp(mode,"c") && strcmp(mode,"lua")) return 2; reference=!strcmp(mode,"c"); }
    else return 2;
  }
  size_t size; uint8_t *rom=oracles_read_file(rom_path,&size); if(!rom) SKIP("Ages ROM missing");
  char hash[41]; sha1_hex(rom,size,hash); ASSERT(size==1048576 && !strcmp(hash,"880374fb978b18af4aa529e2e32f7ffb4d7dd2f4"));
  g=calloc(1,sizeof *g); seed=malloc(sizeof *seed); prepared=malloc(sizeof *prepared);expected=malloc(sizeof *expected); engine_seed=malloc(sizeof *engine_seed); samples=malloc(16*sizeof *samples); ASSERT(g && seed && prepared && expected && engine_seed && samples);
  gb_init(g); ASSERT(gb_load_rom(g,rom,size)); g->cyctab=cyctab_alloc(rom,size); g->code_bits=assets_code_bits(rom,size); assets_zero_code(rom,size); gb_reset(g); hook_mode=HOOK_MODE_REPLACE;
  if(invalid>=0) {
    *seed=*g; render_seed=*render_line_regs(g); base(1,0x44,(uint8_t)invalid,0xa8);
    gameplay_enable(!reference); g->pc=SYM(interactionCode1a)+38; coloredCubeFlame_updateColor(g);
    printf("palette_flags=%u\n",mem_rd(g,OBJECT+OBJ_OAM_FLAGS)); return 0;
  }
  prepare_scene(); ASSERT(gameplay_init()); RUN(palette_cases); RUN(absent_and_hidden);
  for(int i=0;i<GAMEPLAY_COLORED_CUBE_RULE_COUNT;++i) { ASSERT(gameplay_colored_cube_calls(i)>0); printf("%s=%llu\n",gameplay_colored_cube_rule_name(i),(unsigned long long)gameplay_colored_cube_calls(i)); }
  frame_cases(false); frame_cases(true); RUN(failures_and_authority);
  printf("ok lua_colored_cube_rom: %u cases; frame/save comparisons passed; SHA256 %s\n",cases,gameplay_revision());
  gameplay_close(); return 0;
}
