#include "unit.h"
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include "core/gb.h"
#include "core/bus.h"
#include "core/sha1.h"
#include "assets/assets.h"
#include "platform/setup.h"
#include "platform/tempfile.h"
#include "platform/tas.h"
#include "rt/fibers.h"
#include "core/hash.h"
#include "hw/render.h"
#include "game/game.h"
#include "game/gen.h"
#include "script/block_rules.h"

// Expose the real private cube/settlement routines only in this test unit.
#define interactionCode14_hook unused_test_pushblock_hook
#include "../src/game/common/interactions/pushblock.c"
#undef interactionCode14_hook

#define OBJECT 0xd240
static GB *g, *seed, *engine_seed, *prepared, *expected;
static GBSample *samples;
static unsigned cases;
static const char *context;
static char scenario[128];
static double *cpu_ms[2];
static size_t cpu_count[2],cpu_capacity[2];
static bool profiling;
static uint64_t cpu_time(void) {
#ifdef _WIN32
  FILETIME created,exited,kernel,user;
  ASSERT(GetThreadTimes(GetCurrentThread(),&created,&exited,&kernel,&user));
  ULARGE_INTEGER k={.LowPart=kernel.dwLowDateTime,.HighPart=kernel.dwHighDateTime};
  ULARGE_INTEGER u={.LowPart=user.dwLowDateTime,.HighPart=user.dwHighDateTime};
  return (k.QuadPart+u.QuadPart)*100;
#else
  struct timespec t; ASSERT(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t)==0);
  return (uint64_t)t.tv_sec*1000000000u+t.tv_nsec;
#endif
}
static void record_cpu(int mode,double ms) {
  if(cpu_count[mode]==cpu_capacity[mode]) {
    cpu_capacity[mode]=cpu_capacity[mode]?cpu_capacity[mode]*2:2048;
    double *p=realloc(cpu_ms[mode],cpu_capacity[mode]*sizeof *p); ASSERT(p); cpu_ms[mode]=p;
  }
  cpu_ms[mode][cpu_count[mode]++]=ms;
}
static int compare_ms(const void *a,const void *b) {
  double x=*(const double *)a,y=*(const double *)b; return (x>y)-(x<y);
}
static void write_profile(void) {
  if(!profiling) return;
  FILE *file=fopen(getenv("PHASE1_RULES_PROFILE"),"w"); ASSERT(file);
  fprintf(file,"{\"kind\":\"CPU per gb_run_frame; artificial water/panel scenarios; frame comparator disabled\",\"modes\":{");
  for(int mode=0;mode<2;++mode) {
    size_t n=cpu_count[mode]; ASSERT(n);
    double total=0; for(size_t i=0;i<n;++i) total+=cpu_ms[mode][i];
    qsort(cpu_ms[mode],n,sizeof(double),compare_ms);
    fprintf(file,"%s\"%s\":{\"frames\":%zu,\"mean_ms\":%.9f,\"median_ms\":%.9f,\"p95_ms\":%.9f,\"p99_ms\":%.9f,\"max_ms\":%.9f}",
      mode?",":"",mode?"lua":"c",n,total/n,(cpu_ms[mode][(n-1)/2]+cpu_ms[mode][n/2])/2,
      cpu_ms[mode][(n*95-1)/100],cpu_ms[mode][(n*99-1)/100],cpu_ms[mode][n-1]);
  }
  GameplayMemoryStats stats=gameplay_memory_stats();
  fprintf(file,"},\"lua_heap\":%zu,\"lua_peak\":%zu,\"scripts_sha256\":\"%s\"}\n",stats.current,stats.peak,gameplay_revision());
  ASSERT(fclose(file)==0);
}
static RenderLineRegs render_seed;
static const char *fixture_dir=GAME_ROM_DIR "/../tas";
static char fixture_path[4096];
static const char *fixture(const char *name) {
  int n=snprintf(fixture_path,sizeof fixture_path,"%s/%s",fixture_dir,name);
  ASSERT(n>0 && (size_t)n<sizeof fixture_path); return fixture_path;
}
static uint8_t scene_input(void *ctx,uint64_t frame) { return tas_input_at((Tas *)ctx,frame); }
static bool scene_ready(GB *gb) {
  unsigned x=gb->wram[1][0x0c]|gb->wram[1][0x0d]<<8;
  unsigned y=gb->wram[1][0x0a]|gb->wram[1][0x0b]<<8;
  return gb->wram[0][0xc2d]==0 && gb->wram[0][0xc30]==0x8a && gb->wram[0][0x2ee]==2 &&
    gb->wram[0][0x2ef]<=1 && gb->wram[1][4]==1 && !gb->wram[0][0xba0] &&
    !(gb->wram[0][0xc8a]&0x81) && !gb->wram[0][0x4ab] &&
    !(gb->wram[0][0xd00]&0x0e) && !gb->wram[0][0xc4f] && y==56*256 && x>=70*256 && x<=76*256;
}
static void prepare_engine_scene(void) {
  const char *scene=getenv("ORACLES_PHASE1_STATE");
  if(scene) ASSERT(oracles_load_boot_state(g,scene));
  else {
    Tas t={0}; ASSERT(tas_load(&t,fixture("ages-consoleverified.inputs")));
    ASSERT(oracles_load_boot_state(g,fixture("ages-boot.state")));
    g->input_at=scene_input; g->input_ctx=&t;
    for(unsigned i=0;i<20000 && !scene_ready(g);++i) {
      gb_run_frame(g); ASSERT(!g->hung); g->apu.ring_r=g->apu.ring_w;
    }
    ASSERT(scene_ready(g)); g->input_at=NULL; g->input_ctx=NULL; g->joy=0;
    for(unsigned i=0;i<10;++i) { gb_run_frame(g); ASSERT(!g->hung); g->apu.ring_r=g->apu.ring_w; }
    for(unsigned i=0;i<120 && !threads_parked(g);++i) { gb_run_frame(g); ASSERT(!g->hung); g->apu.ring_r=g->apu.ring_w; }
    ASSERT(threads_parked(g)); tas_free(&t);
  }
  *engine_seed=*g;
  render_seed=*render_line_regs(g);
  g->ime=false; g->ie=0;
  *seed=*g;
}

static void base(uint8_t id, uint8_t state, uint8_t subid) {
  struct Fibers *fib=g->fib; fibers_reset(g); *g=*seed; g->fib=fib;
  memset(g->samples,0,16*sizeof *g->samples);
  *render_line_regs(g)=render_seed;
  g->sample_head=g->sample_count=0; g->sample_overflow=0; g->sample=&g->samples[0];
  mem_wr(g,IO_SVBK,1); mem_wr(g,hActiveObjectType,INTERACTION_BASE);
  for (unsigned i=0;i<0xc0;++i) {
    mem_wr(g,wRoomLayout+i,0xa0); mem_wr(g,wRoomCollisions+i,0);
  }
  for(unsigned i=0;i<0x40;++i) mem_wr(g,OBJECT+i,0);
  mem_wr(g,OBJECT,1); mem_wr(g,OBJECT+OBJ_ID,id);
  mem_wr(g,OBJECT+OBJ_STATE,state); mem_wr(g,OBJECT+OBJ_SUBID,subid);
  mem_wr(g,OBJECT+OBJ_XH,0x48); mem_wr(g,OBJECT+OBJ_YH,0x48);
  mem_wr(g,w1Link_xh,0x18); mem_wr(g,w1Link_yh,0x18);
  mem_wr(g,wActiveMusic,3);
  g->d=OBJECT>>8; g->sp=wThread0StackTop-2;
}

static void pair(HookFn fn, uint16_t address, uint8_t bank) {
  *prepared=*g;
  RenderLineRegs render_before=*render_line_regs(g);
  for (int mode=0;mode<2;++mode) {
    *g=*prepared; memset(g->samples,0,16*sizeof *g->samples);
    *render_line_regs(g)=render_before;
    g->sp=wThread0StackTop-2; g->pc=address; g->rom_bank=bank;
    mem_wr(g,g->sp,0); mem_wr(g,g->sp+1,0);
    mem_wr(g,hRomBank,bank); // bank-switching services restore this HRAM shadow
    gameplay_enable(mode!=0); hook_run(g,fn,address);
    if (g->hung) fprintf(stderr,"stopped: %s: %s\n",context,gb_stop_reason);
    ASSERT(!g->hung);
    if(g->sample_overflow) fprintf(stderr,"sample overflow: %s count=%llu\n",context,(unsigned long long)g->sample_overflow);
    ASSERT(!g->sample_overflow);
    if (!mode) { *expected=*g; memcpy(samples,g->samples,16*sizeof *samples); }
    else {
      if (memcmp(expected,g,sizeof *g)) {
        fprintf(stderr,"mismatch case %u: %s\n",cases,context);
        for(size_t i=0,n=0;i<sizeof *g && n<8;++i)
          if(((unsigned char*)expected)[i]!=((unsigned char*)g)[i]) {
            fprintf(stderr,"byte %zu C=%02x Lua=%02x\n",i,((unsigned char*)expected)[i],((unsigned char*)g)[i]); ++n;
          }
      }
      ASSERT(!memcmp(expected,g,sizeof *g));
      ASSERT(!memcmp(samples,g->samples,16*sizeof *samples));
    }
  }
  ++cases;
}

// Graphics loaders can yield to the kernel. Run those scenarios on the real
// scheduler, comparing every consumed frame and complete GB state at its end.
static void engine_pair(unsigned frames) {
  if(getenv("PHASE1_TRACE")) fprintf(stderr,"engine case: %s\n",context);
  *prepared=*g;
  RenderLineRegs render_before=*render_line_regs(g);
  GB *trajectory=profiling?NULL:malloc(frames*sizeof *trajectory);
  uint64_t *sample_hashes=profiling?NULL:malloc(frames*sizeof *sample_hashes);
  ASSERT(profiling || (trajectory && sample_hashes));
  for(int pass=0;pass<2;++pass) {
    int mode=getenv("PHASE1_LUA_FIRST") ? !pass : pass;
    struct Fibers *fib=g->fib; fibers_reset(g); *g=*prepared; g->fib=fib;
    *render_line_regs(g)=render_before;
    memcpy(g,engine_seed,offsetof(GB,rom));
    g->rom_bank=engine_seed->rom_bank; g->ram_bank=engine_seed->ram_bank;
    g->ie=engine_seed->ie; memcpy(g->io,engine_seed->io,sizeof g->io);
    memcpy(g->hram,engine_seed->hram,sizeof g->hram);
    g->input_at=NULL; g->input_ctx=NULL; g->joy=0;
    memset(g->samples,0,16*sizeof *g->samples);
    gameplay_enable(mode!=0);
    for(unsigned frame=0;frame<frames;++frame) {
      uint64_t started=profiling?cpu_time():0;
      gb_run_frame(g);
      uint64_t ended=profiling?cpu_time():0;
      if(profiling) record_cpu(mode,(ended-started)/1000000.0);
      if(g->hung) fprintf(stderr,"engine stopped: %s frame=%u: %s\n",context,frame,gb_stop_reason);
      ASSERT(!g->hung); ASSERT(!g->sample_overflow);
      g->apu.ring_r=g->apu.ring_w;
      if(profiling) continue;
      uint64_t hash=fnv1a64_update(FNV1A64_INIT,(const unsigned char *)g->sample,sizeof *g->sample);
      if(!pass) { trajectory[frame]=*g; sample_hashes[frame]=hash; }
      else {
        if(memcmp(&trajectory[frame],g,sizeof *g)) fprintf(stderr,"engine mismatch: %s frame=%u\n",context,frame);
        ASSERT(!memcmp(&trajectory[frame],g,sizeof *g)); ASSERT_EQ(sample_hashes[frame],hash);
      }
    }
  }
  free(sample_hashes); free(trajectory); ++cases;
}

static void room_flags(uint8_t flags) {
  g->rom_bank=0x0b; g->sp=wThread0StackTop-2;
  hook_run(g,getThisRoomFlags_hook,SYM(getThisRoomFlags));
  mem_wr(g,(uint16_t)((g->h<<8)|g->l),flags);
}

static void settle_entry(GB *gb) { pushblock_func_449d(gb,gb->sp); }

static void cubes_and_settlement(void) {
  context="cube permission";
  const uint8_t positions[]={0,1,255}, tiles[]={0,0x2b,0x2c,0x2d,0xab,0xac,0xff};
  for(size_t p=0;p<sizeof positions;++p) for(int color=0;color<256;++color)
    for(size_t t=0;t<sizeof tiles;++t) {
      base(0x14,0,0); mem_wr(g,wRotatingCubePos,positions[p]); mem_wr(g,wRotatingCubeColor,color);
      mem_wr(g,OBJECT+OBJ_VAR31,tiles[t]);
      pair(pushblock_checkRotatingCubePermitsPushing,SYM(interactionCode14)+194,SYMBANK(interactionCode14));
    }
  context="block settlement";
  const uint8_t replacements[]={0,0x2c,0xff};
  for(size_t r=0;r<sizeof replacements;++r) for(int properties=0;properties<256;++properties) {
    base(0x14,1,0); mem_wr(g,OBJECT+OBJ_VAR33,replacements[r]); mem_wr(g,OBJECT+OBJ_VAR34,properties);
    pair(settle_entry,SYM(interactionCode14)+109,SYMBANK(interactionCode14));
  }
}

static void synchronizer(void) {
  context="synchronizer"; base(0xbd,1,0);
  mem_wr(g,w1ReservedInteraction1_enabled,1); mem_wr(g,w1ReservedInteraction1_var31,0xda);
  mem_wr(g,wRoomLayout+0x33,0xda);
  pair(interactionCodebd_hook,SYM(interactionCodebd),SYMBANK(interactionCodebd));
}

static void water(void) {
  const uint8_t counters[]={0,1,2,255};
  for(int subid=0;subid<2;++subid) {
    context=scenario;
    for(int flag=0;flag<2;++flag) {
      snprintf(scenario,sizeof scenario,"water init subid=%d flag=%d",subid,flag);
      base(0x9e,0,subid); room_flags(flag);
      mem_wr(g,wActiveMusic,3); // valid resumed music when entering later substates
      engine_pair(8);
    }
    context=scenario;
    for(int substate=0;substate<12;++substate) for(size_t c=0;c<sizeof counters;++c) {
      snprintf(scenario,sizeof scenario,"water animation subid=%d substate=%d counter=%u",subid,substate,counters[c]);
      base(0x9e,3,subid); mem_wr(g,OBJECT+OBJ_SUBSTATE,substate); mem_wr(g,OBJECT+OBJ_COUNTER1,counters[c]);
      mem_wr(g,wActiveMusic,3);
      engine_pair(16);
    }
    context="water moving";
    for(size_t c=0;c<sizeof counters;++c) {
      base(0x9e,2,subid); mem_wr(g,OBJECT+OBJ_COUNTER1,counters[c]);
      pair(interactionCode9e_hook,SYM(interactionCode9e),SYMBANK(interactionCode9e));
    }
    context="water pushing side";
    const int offsets[][2]={{0,-6},{0,6},{-6,0},{6,0}};
    for(unsigned side=0;side<4;++side) for(int timer=1;timer<3;++timer) {
      base(0x9e,1,subid);
      mem_wr(g,OBJECT+OBJ_COLLIDE_RADIUS_Y,6); mem_wr(g,OBJECT+OBJ_COLLIDE_RADIUS_Y+1,6);
      mem_wr(g,0xd000+OBJ_COLLIDE_RADIUS_Y,6); mem_wr(g,0xd000+OBJ_COLLIDE_RADIUS_Y+1,6);
      mem_wr(g,w1Link_yh,0x48+offsets[side][0]); mem_wr(g,w1Link_xh,0x48+offsets[side][1]);
      mem_wr(g,wLinkAngle,0); mem_wr(g,wLinkCanPassNpcs,0); mem_wr(g,wLinkObjectIndex,0xd0);
      mem_wr(g,wGameKeysPressed,0); mem_wr(g,OBJECT+OBJ_COUNTER1,timer);
      pair(interactionCode9e_hook,SYM(interactionCode9e),SYMBANK(interactionCode9e));
    }
  }
}

static void panels(void) {
  const uint8_t counters[]={0,1,2,255};
  for(int subid=0;subid<2;++subid) {
    context="panel initialization";
    for(int opened=0;opened<2;++opened) {
      base(0x7b,0,subid); room_flags(opened?0x40:0);
      engine_pair(8);
    }
    context="panel trigger";
    for(int trigger=0;trigger<256;++trigger) {
      base(0x7b,1,subid); mem_wr(g,wActiveTriggers,trigger);
      pair(interactionCode7b_hook,SYM(interactionCode7b),SYMBANK(interactionCode7b));
    }
    context="panel animation";
    for(int substate=0;substate<3;++substate) for(size_t c=0;c<sizeof counters;++c)
      for(int parity=0;parity<2;++parity) {
        base(0x7b,2,subid); mem_wr(g,OBJECT+OBJ_SUBSTATE,substate); mem_wr(g,OBJECT+OBJ_COUNTER1,counters[c]);
        mem_wr(g,wFrameCounter,parity);
        pair(interactionCode7b_hook,SYM(interactionCode7b),SYMBANK(interactionCode7b));
      }
  }
}

static void tile_parser(void) {
  context="streaming tile parser";
  const uint8_t table[]={0x2c,0x33,0x44,0xff,0x2d,0x55,0};
  for(int changed=0;changed<4;++changed) {
    base(0x90,1,0);
    for(size_t i=0;i<sizeof table;++i) mem_wr(g,0xc100+i,table[i]);
    mem_wr(g,wRoomLayout+0x33,0x2c); mem_wr(g,wRoomLayout+0x44,0x2c); mem_wr(g,wRoomLayout+0x55,0x2d);
    if(changed) mem_wr(g,wRoomLayout+(changed==1?0x33:changed==2?0x44:0x55),0);
    g->h=0xc1; g->l=0;
    pair(miscPuzzles_verifyTilesAtPositions_hook,SYM(miscPuzzles_verifyTilesAtPositions),SYMBANK(miscPuzzles_verifyTilesAtPositions));
  }
  // A zero at the first byte is a distinct parser exit.
  base(0x90,1,0); mem_wr(g,0xc100,0); g->h=0xc1; g->l=0;
  pair(miscPuzzles_verifyTilesAtPositions_hook,SYM(miscPuzzles_verifyTilesAtPositions),SYMBANK(miscPuzzles_verifyTilesAtPositions));
}

static void miscellaneous(void) {
  context="rolling cube floor";
  HookFn cubes[]={miscPuzzles_subid02_hook,miscPuzzles_subid14_hook};
  uint16_t cube_addresses[]={SYM(miscPuzzles_subid02),SYM(miscPuzzles_subid14)};
  for(int fn=0;fn<2;++fn) for(int color=0;color<4;++color) for(int pos=0;pos<2;++pos) {
    base(0x90,1,fn?0x14:2); mem_wr(g,wRoomLayout+0x44,0xad+color);
    mem_wr(g,wRotatingCubePos,pos?0x44:0x45); mem_wr(g,wRotatingCubeColor,color);
    pair(cubes[fn],cube_addresses[fn],SYMBANK(miscPuzzles_subid02));
  }
  context="panel staircase";
  for(int state=0;state<3;++state) for(int trigger=0;trigger<256;++trigger) {
    base(0x90,state,0x0d); mem_wr(g,wActiveTriggers,trigger); mem_wr(g,wNumTorchesLit,1);
    mem_wr(g,OBJECT+OBJ_COUNTER1,1);
    pair(miscPuzzles_subid0d_hook,SYM(miscPuzzles_subid0d),SYMBANK(miscPuzzles_subid0d));
  }
  context="cube staircase";
  for(int trigger=0;trigger<8;++trigger) {
    base(0x90,1,0x15); mem_wr(g,wActiveTriggers,trigger);
    pair(miscPuzzles_subid15_hook,SYM(miscPuzzles_subid15),SYMBANK(miscPuzzles_subid15));
  }
  context="statue key";
  for(int a=0;a<2;++a) for(int b=0;b<2;++b) {
    base(0x90,1,0x18); mem_wr(g,wRoomLayout+0x95,a?0x2a:0); mem_wr(g,wRoomLayout+0x5d,b?0x2a:0);
    pair(miscPuzzles_subid18_hook,SYM(miscPuzzles_subid18),SYMBANK(miscPuzzles_subid18));
  }
  context="trap reset";
  for(int state=1;state<3;++state) for(int solid=0;solid<3;++solid) for(int counter=1;counter<3;++counter) {
    base(0x90,state,0x1f); mem_wr(g,OBJECT+OBJ_COUNTER1,counter);
    for(int i=0;i<256;++i) { mem_wr(g,wRoomCollisions+i,solid?0x0f:0); mem_wr(g,wRoomLayout+i,solid==2?0:0x2c); }
    pair(miscPuzzles_subid1f_hook,SYM(miscPuzzles_subid1f),SYMBANK(miscPuzzles_subid1f));
  }
}

static void seasons_guard(GB *gb,bool *out) {
  bool condition=true; BLOCK_DECIDE(condition,cube_absent,1); *out=condition;
}
static void guard_and_failures(void) {
  base(0xbd,1,0);
  uint64_t before=gameplay_block_rule_calls(GAMEPLAY_RULE_cube_absent);
  syms_select(1,0); bool result=false;
  gameplay_enable(true); seasons_guard(g,&result);
  ASSERT(result); ASSERT_EQ(gameplay_block_rule_calls(GAMEPLAY_RULE_cube_absent),before);
  syms_select(0,0);
  extern const unsigned char ages_block_rules_lua[];
  extern const unsigned int ages_block_rules_lua_size;
  size_t capacity=ages_block_rules_lua_size+256; char *source=malloc(capacity); ASSERT(source);
  const char *names[]={"sync_awake","sync_eligible","sync_next_state"};
  for(unsigned i=0;i<3;++i) {
    int n=snprintf(source,capacity,"local rules=(function()\n%.*s\nend)(); rules.%s=function() return 99 end; return rules",
      (int)ages_block_rules_lua_size,(const char *)ages_block_rules_lua,names[i]);
    ASSERT(n>0 && (size_t)n<capacity);
    GameplaySources override={.block_rules=source,.block_rules_size=(size_t)n};
    ASSERT(gameplay_load_sources(&override)); gameplay_enable(true);
    base(0xbd,i==2?0:1,0);
    mem_wr(g,w1ReservedInteraction1_enabled,1); mem_wr(g,w1ReservedInteraction1_var31,0xda);
    mem_wr(g,wRoomLayout+0x33,0xda); mem_wr(g,hFF8D,0xcc);
    g->pc=SYM(interactionCodebd); g->rom_bank=SYMBANK(interactionCodebd);
    hook_run(g,interactionCodebd_hook,SYM(interactionCodebd));
    ASSERT(g->hung); ASSERT_EQ(mem_rd(g,OBJECT+OBJ_STATE),i==2?0:1);
    ASSERT_EQ(mem_rd(g,hFF8D),0xcc); ASSERT(strstr(gameplay_error(),names[i]));
  }
  gameplay_close(); ASSERT(gameplay_init()); free(source);
}

static void reload_scenarios(void) {
  char path[4096]; ASSERT(oracles_temp_path(path,sizeof path));
  context="serialized water continuation";
  for(int subid=0;subid<2;++subid) for(int substate=0;substate<12;++substate) {
    base(0x9e,3,subid); mem_wr(g,OBJECT+OBJ_SUBSTATE,substate); mem_wr(g,OBJECT+OBJ_COUNTER1,1);
    ASSERT(oracles_save_boot_state(g,path)); mem_wr(g,OBJECT+OBJ_SUBSTATE,0xff);
    ASSERT(oracles_load_boot_state(g,path)); ASSERT_EQ(mem_rd(g,OBJECT+OBJ_SUBSTATE),substate);
    gameplay_close(); ASSERT(gameplay_init()); engine_pair(8);
  }
  context="serialized panel continuation";
  for(int substate=0;substate<3;++substate) {
    base(0x7b,2,0); mem_wr(g,OBJECT+OBJ_SUBSTATE,substate); mem_wr(g,OBJECT+OBJ_COUNTER1,1);
    ASSERT(oracles_save_boot_state(g,path)); mem_wr(g,OBJECT+OBJ_SUBSTATE,0xff);
    ASSERT(oracles_load_boot_state(g,path));
    gameplay_close(); ASSERT(gameplay_init()); engine_pair(8);
  }
  ASSERT_EQ(remove(path),0);
}

int main(int argc,char **argv) {
  const char *rom_path=GAME_ROM_DIR "/Legend of Zelda, The - Oracle of Ages (USA, Australia).gbc";
  for(int i=1;i<argc;++i) {
    if(i+1>=argc) { fputs("usage: test_lua_block_rules_rom [--rom FILE] [--tas-dir DIR]\n",stderr); return 2; }
    if(!strcmp(argv[i],"--rom")) rom_path=argv[++i];
    else if(!strcmp(argv[i],"--tas-dir")) fixture_dir=argv[++i];
    else return 2;
  }
  size_t size; uint8_t *rom=oracles_read_file(rom_path,&size);
  if(!rom) SKIP("Ages ROM missing");
  char hash[41]; sha1_hex(rom,size,hash); ASSERT(size==1048576 && !strcmp(hash,"880374fb978b18af4aa529e2e32f7ffb4d7dd2f4"));
  g=calloc(1,sizeof *g); seed=malloc(sizeof *seed); engine_seed=malloc(sizeof *engine_seed); prepared=malloc(sizeof *prepared); expected=malloc(sizeof *expected);
  samples=malloc(16*sizeof *samples); ASSERT(g && seed && engine_seed && prepared && expected && samples);
  gb_init(g); ASSERT(gb_load_rom(g,rom,size)); g->cyctab=cyctab_alloc(rom,size); g->code_bits=assets_code_bits(rom,size);
  assets_zero_code(rom,size); gb_reset(g); g->ime=false; g->ie=0; mem_wr(g,hActiveObjectType,INTERACTION_BASE);
  hook_mode=HOOK_MODE_REPLACE; prepare_engine_scene(); ASSERT(gameplay_init());
  const char *only=getenv("PHASE1_RULES_CASES");
  profiling=getenv("PHASE1_RULES_PROFILE")!=NULL;
  ASSERT(!only || !strcmp(only,"cube") || !strcmp(only,"sync") || !strcmp(only,"water") || !strcmp(only,"panels") || !strcmp(only,"parser") || !strcmp(only,"misc"));
  if(!only || !strcmp(only,"cube")) RUN(cubes_and_settlement);
  if(!only || !strcmp(only,"sync")) RUN(synchronizer);
  if(!only || !strcmp(only,"water")) RUN(water);
  if(!only || !strcmp(only,"panels")) RUN(panels);
  if(!only || !strcmp(only,"parser")) RUN(tile_parser);
  if(!only || !strcmp(only,"misc")) RUN(miscellaneous);
  for(int i=0;i<GAMEPLAY_BLOCK_RULE_COUNT;++i) {
    printf("coverage %s=%llu\n",gameplay_block_rule_name(i),(unsigned long long)gameplay_block_rule_calls(i));
    if(!only) ASSERT(gameplay_block_rule_calls(i)>0);
  }
  write_profile();
  if(!only && !profiling) { RUN(reload_scenarios); RUN(guard_and_failures); }
  printf("ok lua_block_rules_rom: %u full GB/sample comparisons\n",cases);
  gameplay_close(); fibers_reset(g); free(cpu_ms[0]); free(cpu_ms[1]); free(samples); free(expected); free(prepared); free(engine_seed); free(seed); free(g); free(rom); return 0;
}
