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
#include "script/levers_lava.h"
void interactionCoded8_toggleLavaSource_hook(GB *gb);

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
static void base(uint8_t state,uint8_t subid,uint8_t counter,uint8_t distance) {
  struct Fibers *fib=g->fib;fibers_reset(g);*g=*seed;g->fib=fib;
  memset(g->samples,0,16*sizeof *g->samples);g->sample_head=g->sample_count=0;g->sample_overflow=0;g->sample=&g->samples[0];
  *render_line_regs(g)=render_seed;mem_wr(g,IO_SVBK,1);mem_wr(g,hActiveObjectType,INTERACTION_BASE);
  for(unsigned i=0;i<0xc0;++i) {mem_wr(g,wRoomLayout+i,0xa0);mem_wr(g,wRoomCollisions+i,0);}
  for(unsigned i=0;i<0x40;++i) mem_wr(g,OBJECT+i,0);
  mem_wr(g,OBJECT,1);mem_wr(g,OBJECT+OBJ_ID,0xd8);mem_wr(g,OBJECT+OBJ_STATE,state);mem_wr(g,OBJECT+OBJ_SUBID,subid);
  mem_wr(g,OBJECT+OBJ_COUNTER1,counter);mem_wr(g,OBJECT+OBJ_COUNTER2,subid?6:4);
  mem_wr(g,OBJECT+OBJ_XH,0x48);mem_wr(g,OBJECT+OBJ_YH,0x48);
  mem_wr(g,wLever1PullDistance,distance);mem_wr(g,wActiveMusic,3);
  for(unsigned i=0;i<3;++i) mem_wr(g,wRoomLayout+0x13+i,0xc3+i);
  g->d=OBJECT>>8;g->sp=wThread0StackTop-2;g->rom_bank=SYMBANK(interactionCoded8);mem_wr(g,hRomBank,g->rom_bank);
  mem_wr(g,g->sp,0);mem_wr(g,g->sp+1,0);
}
static void compare(bool lua) {
  if(!lua) { *expected=*g; memcpy(samples,g->samples,16*sizeof *samples); }
  else {
    if(memcmp(expected,g,sizeof *g)) fprintf(stderr,"mismatch: %s case %u\n",context,cases);
    ASSERT(!memcmp(expected,g,sizeof *g)); ASSERT(!memcmp(samples,g->samples,16*sizeof *samples));
  }
}
static uint16_t scripts[6];
static unsigned groups[6];
static void script_pointer(uint16_t pointer) {wr16(g,OBJECT+OBJ_SCRIPT_PTR,pointer);}
static uint8_t resource(uint16_t address) {ASSERT(address>=0x4000 && address<0x8000);return g->rom[0x0b*0x4000+address-0x4000];}
static void enter(bool dispatched) {
  g->sp=wThread0StackTop-2;mem_wr(g,g->sp,0);mem_wr(g,g->sp+1,0);
  g->pc=SYM(interactionCoded8);
  if(dispatched) ASSERT(hook_dispatch(g));else hook_run(g,interactionCoded8_hook,SYM(interactionCoded8));
  if(g->hung) fprintf(stderr,"stopped: %s: %s\n",context,gb_stop_reason);
  ASSERT(!g->hung);ASSERT(!g->sample_overflow);
}
static void initialization_and_signals(void) {
  context="subtypes and partial/full lever";
  for(unsigned subid=0;subid<8;++subid) for(int mode=0;mode<2;++mode) {
    base(0,subid,1,0);gameplay_enable(mode!=0);enter(true);ASSERT_EQ(mem_rd(g,OBJECT+OBJ_STATE),1);ASSERT_EQ(mem_rd(g,OBJECT+OBJ_COUNTER2),subid?6:4);compare(mode!=0);if(mode)++cases;
  }
  for(unsigned subid=0;subid<6;++subid) for(unsigned distance=0;distance<256;++distance) for(int state=1;state<=3;state+=2) {
    for(int mode=0;mode<2;++mode) {
      base(state,subid,7,distance);gameplay_enable(mode!=0);enter((distance&1)!=0);compare(mode!=0);
      ASSERT_EQ(mem_rd(g,OBJECT+OBJ_STATE),state==1?((distance&128)?2:1):(distance?3:4));
    }
    ++cases;
  }
}
static void timer_wrap_and_groups(void) {
  context="timer wrap and script groups";
  for(unsigned subid=0;subid<6;++subid) for(int state=2;state<=4;state+=2) for(unsigned counter=0;counter<256;++counter) {
    for(int mode=0;mode<2;++mode) {
      base(state,subid,counter,0);script_pointer(scripts[subid]);gameplay_enable(mode!=0);enter(true);compare(mode!=0);
      ASSERT_EQ(mem_rd(g,OBJECT+OBJ_COUNTER1),counter==1?(subid?6:4):(uint8_t)(counter-1));
    }
    ++cases;
  }
  for(unsigned subid=0;subid<6;++subid) {
    uint16_t pointer=scripts[subid];unsigned count=0;
    for(;;) {
      uint16_t next=pointer;uint8_t positions[192];unsigned length=0;
      while(resource(next)) {ASSERT(length<192);positions[length++]=resource(next++);ASSERT(positions[length-1]<0xc0);}
      ++next;
      for(int state=2;state<=4;state+=2) {
        for(int mode=0;mode<2;++mode) {
          base(state,subid,1,0);script_pointer(pointer);gameplay_enable(mode!=0);enter(false);compare(mode!=0);
          if(!length) ASSERT_EQ(mem_rd(g,OBJECT+OBJ_STATE),state==2?3:1);
          else for(unsigned i=0;i<length;++i) {uint8_t tile=mem_rd(g,wRoomLayout+positions[i]);ASSERT(state==2?tile==1:(tile>=0x61 && tile<=0x64));}
          if(length) ASSERT_EQ(rd16(g,OBJECT+OBJ_SCRIPT_PTR),next);
        }
        ++cases;
      }
      if(!length) break;pointer=next;++count;
    }
    groups[subid]=count;ASSERT(count>0);printf("subid %u: %u original ROM groups\n",subid,count);
  }
}
static void source_orientations(void) {
  context="source orientation and segment limits";
  const uint8_t kinds[]={0xc3,0xc6,0xc9,0xcc};
  for(unsigned kind=0;kind<4;++kind) for(unsigned length=1;length<=6;++length) for(unsigned slot=0;slot<3;++slot) {
    uint8_t start=(const uint8_t[]){0,0x13,0xb8}[slot];
    for(int mode=0;mode<2;++mode) {
      base(1,0,1,128);for(unsigned i=0;i<0xc0;++i) mem_wr(g,wRoomLayout+i,0xa0);
      for(unsigned i=0;i<length;++i) mem_wr(g,wRoomLayout+start+i,kinds[kind]+i);
      gameplay_enable(mode!=0);g->pc=SYM(interactionCoded8__toggleLavaSource);hook_run(g,interactionCoded8_toggleLavaSource_hook,SYM(interactionCoded8__toggleLavaSource));ASSERT(!g->hung);compare(mode!=0);
    }
    ++cases;
  }
}
static void lifecycle(bool serialize) {
  context=serialize?"cycle with recreated VM":"two full lever/lava cycles";
  const unsigned limit=1500;GB *trajectory=malloc(limit*sizeof *trajectory);uint64_t *hashes=malloc(limit*sizeof *hashes);ASSERT(trajectory && hashes);
  char path[4096];ASSERT(oracles_temp_path(path,sizeof path));
  for(unsigned subid=0;subid<6;++subid) {
    base(0,subid,1,0);*prepared=*g;unsigned expected_frames=0;
    for(int mode=0;mode<2;++mode) {
      struct Fibers *fib=g->fib;fibers_reset(g);*g=*prepared;g->fib=fib;*render_line_regs(g)=render_seed;
      memcpy(g,engine_seed,offsetof(GB,rom));g->rom_bank=engine_seed->rom_bank;g->ram_bank=engine_seed->ram_bank;g->ie=engine_seed->ie;memcpy(g->io,engine_seed->io,sizeof g->io);memcpy(g->hram,engine_seed->hram,sizeof g->hram);
      g->input_at=NULL;g->input_ctx=NULL;g->joy=0;memset(g->samples,0,16*sizeof *g->samples);gameplay_enable(mode!=0);
      unsigned cycles=0,hold=0,previous=0;bool saved_drain=false,saved_refill=false;
      for(unsigned frame=0;frame<limit;++frame) {
        unsigned state=g->wram[1][(OBJECT+OBJ_STATE)&0xfff];
        if(previous==4 && state==1) {++cycles;hold=0;}
        if(cycles==2) {if(!mode) expected_frames=frame;else ASSERT_EQ(frame,expected_frames);break;}
        uint8_t distance=state==1?(hold++<3?0x7f:0x80):state==3?(hold++<5?0x40:0):state==2?0x80:0;
        if(previous!=state && state==3) hold=0;
        mem_wr(g,wLever1PullDistance,distance);previous=state;
        if(serialize && ((state==2 && !saved_drain) || (state==4 && !saved_refill))) {
          ASSERT(oracles_save_boot_state(g,path));ASSERT(oracles_load_boot_state(g,path));gameplay_close();ASSERT(gameplay_init());gameplay_enable(mode!=0);
          if(state==2)saved_drain=true;else saved_refill=true;
        }
        gb_run_frame(g);
        if(g->hung) fprintf(stderr,"lifecycle stopped subid=%u frame=%u state=%u: %s\n",subid,frame,state,gb_stop_reason);
        ASSERT(!g->hung);ASSERT(!g->sample_overflow);g->apu.ring_r=g->apu.ring_w;
        uint64_t hash=fnv1a64_update(FNV1A64_INIT,(const unsigned char *)g->sample,sizeof *g->sample);
        if(!mode) {trajectory[frame]=*g;hashes[frame]=hash;}
        else {if(memcmp(&trajectory[frame],g,sizeof *g))fprintf(stderr,"frame mismatch subid=%u frame=%u state=%u\n",subid,frame,state);ASSERT(!memcmp(&trajectory[frame],g,sizeof *g));ASSERT_EQ(hashes[frame],hash);}
      }
      ASSERT_EQ(cycles,2);if(serialize)ASSERT(saved_drain && saved_refill);
    }
    printf("lifecycle subid=%u reload=%d: %u frames matched\n",subid,serialize,expected_frames);++cases;
  }
  if(serialize)ASSERT_EQ(remove(path),0);free(hashes);free(trajectory);
}
static void room_reentry(void) {
  context="abandon active update and recreate room objects";
  for(unsigned subid=0;subid<6;++subid)for(int mode=0;mode<2;++mode) {
    gameplay_close();ASSERT(gameplay_init());gameplay_enable(mode!=0);
    base(0,subid,1,128);enter(true);enter(true);ASSERT_EQ(mem_rd(g,OBJECT+OBJ_STATE),2);
    for(unsigned tick=0;tick<40;++tick) {g->sp=wThread0StackTop-2;mem_wr(g,g->sp,0);mem_wr(g,g->sp+1,0);enter(false);}
    ASSERT_EQ(mem_rd(g,OBJECT+OBJ_STATE),2);
    // Same VM, newly loaded room: no progress may be retained in Lua.
    base(0,subid,1,0);enter(true);ASSERT_EQ(mem_rd(g,OBJECT+OBJ_STATE),1);ASSERT_EQ(mem_rd(g,wRoomLayout+0x13),0xc3);
    g->sp=wThread0StackTop-2;mem_wr(g,g->sp,0);mem_wr(g,g->sp+1,0);mem_wr(g,wLever1PullDistance,128);enter(true);
    ASSERT_EQ(mem_rd(g,OBJECT+OBJ_STATE),2);ASSERT_EQ(rd16(g,OBJECT+OBJ_SCRIPT_PTR),scripts[subid]);ASSERT_EQ(mem_rd(g,OBJECT+OBJ_COUNTER1),30);
    compare(mode!=0);if(mode)++cases;
  }
}
static void failure_stops(void) {
  extern const unsigned char ages_levers_lava_lua[];extern const unsigned int ages_levers_lava_lua_size;
  size_t capacity=ages_levers_lava_lua_size+1024;char *source=malloc(capacity);ASSERT(source);
  const char *names[]={"fully_pulled","released","timer_elapsed","has_tile","source_found","source_segment","source_delta","next_state","floor_tile","floor_tile"};
  const int states[]={1,3,2,2,1,1,1,0,2,4};
  for(unsigned i=0;i<sizeof states/sizeof *states;++i) {
    int n=snprintf(source,capacity,"local rules=(function() %.*s end)();rules.%s=function()return {} end;return rules",(int)ages_levers_lava_lua_size,ages_levers_lava_lua,names[i]);
    GameplaySources override={.levers_lava=source,.levers_lava_size=(size_t)n};ASSERT(gameplay_load_sources(&override));gameplay_enable(true);
    base(states[i],0,1,states[i]==3?0:128);script_pointer(scripts[0]);g->pc=SYM(interactionCoded8);hook_run(g,interactionCoded8_hook,SYM(interactionCoded8));ASSERT(g->hung);ASSERT(strstr(gameplay_error(),names[i]));
    if(!strcmp(names[i],"source_segment")) {ASSERT_EQ(mem_rd(g,wRoomLayout+0x13),0xc9);ASSERT_EQ(mem_rd(g,wRoomLayout+0x14),0xc4);}
    if(!strcmp(names[i],"source_found") || !strcmp(names[i],"source_delta")) ASSERT_EQ(mem_rd(g,wRoomLayout+0x13),0xc3);
    if(!strcmp(names[i],"next_state")) ASSERT_EQ(mem_rd(g,OBJECT+OBJ_STATE),0);
    if(!strcmp(names[i],"floor_tile")) ASSERT_EQ(mem_rd(g,wRoomLayout+0x2a),0xa0);
  }
  const char *fields[]={"fully_pulled","released","timer_elapsed","has_tile","source_segment","source_delta","floor_tile"};
  const char *values[]={"false","false","false","false","false","6","0x62"};
  const int initial[]={1,3,2,2,1,1,2};
  for(unsigned i=0;i<sizeof initial/sizeof *initial;++i) {
    int n=snprintf(source,capacity,"local rules=(function() %.*s end)();rules.%s=function()return %s end;return rules",(int)ages_levers_lava_lua_size,ages_levers_lava_lua,fields[i],values[i]);
    GameplaySources override={.levers_lava=source,.levers_lava_size=(size_t)n};ASSERT(gameplay_load_sources(&override));gameplay_enable(true);
    base(initial[i],0,1,initial[i]==3?0:128);script_pointer(scripts[0]);
    if(i==5)for(unsigned j=0;j<3;++j)mem_wr(g,wRoomLayout+0x13+j,0xc9+j);
    enter(false);
    if(i==0)ASSERT_EQ(mem_rd(g,OBJECT+OBJ_STATE),1);
    if(i==1)ASSERT_EQ(mem_rd(g,OBJECT+OBJ_STATE),3);
    if(i==2)ASSERT_EQ(mem_rd(g,OBJECT+OBJ_COUNTER1),0);
    if(i==3)ASSERT_EQ(mem_rd(g,OBJECT+OBJ_STATE),3);
    if(i==4) {ASSERT_EQ(mem_rd(g,wRoomLayout+0x13),0xc9);ASSERT_EQ(mem_rd(g,wRoomLayout+0x14),0xc4);}
    if(i==5)ASSERT_EQ(mem_rd(g,wRoomLayout+0x13),0xcf);
    if(i==6)ASSERT_EQ(mem_rd(g,wRoomLayout+0x2a),0x62);
  }
  free(source);gameplay_close();ASSERT(gameplay_init());
}
int main(int argc,char **argv) {
  const char *rom_path=GAME_ROM_DIR "/Legend of Zelda, The - Oracle of Ages (USA, Australia).gbc";
  for(int i=1;i<argc;++i) {if(i+1>=argc)return 2;if(!strcmp(argv[i],"--rom"))rom_path=argv[++i];else if(!strcmp(argv[i],"--tas-dir"))tas_dir=argv[++i];else return 2;}
  size_t size;uint8_t *rom=oracles_read_file(rom_path,&size);if(!rom)SKIP("Ages ROM missing");char hash[41];sha1_hex(rom,size,hash);ASSERT(size==1048576 && !strcmp(hash,"880374fb978b18af4aa529e2e32f7ffb4d7dd2f4"));
  g=calloc(1,sizeof *g);seed=malloc(sizeof *seed);prepared=malloc(sizeof *prepared);expected=malloc(sizeof *expected);engine_seed=malloc(sizeof *engine_seed);samples=malloc(16*sizeof *samples);ASSERT(g && seed && prepared && expected && engine_seed && samples);
  gb_init(g);ASSERT(gb_load_rom(g,rom,size));g->cyctab=cyctab_alloc(rom,size);g->code_bits=assets_code_bits(rom,size);assets_zero_code(rom,size);gb_reset(g);hook_mode=HOOK_MODE_REPLACE;
  uint16_t table=SYM(interactionCoded8__scriptTable);
  for(unsigned i=0;i<6;++i)scripts[i]=resource(table+i*2)|(uint16_t)resource(table+i*2+1)<<8;
  prepare_scene();ASSERT(gameplay_init());RUN(initialization_and_signals);RUN(timer_wrap_and_groups);RUN(source_orientations);
  for(int i=0;i<GAMEPLAY_LAVA_RULE_COUNT;++i) {ASSERT(gameplay_lava_calls(i)>0);printf("%s=%llu\n",gameplay_lava_rule_name(i),(unsigned long long)gameplay_lava_calls(i));}
  lifecycle(false);lifecycle(true);RUN(room_reentry);RUN(failure_stops);
  printf("ok lua_levers_lava_rom: %u cases; all six ROM scripts and frame comparisons; SHA256 %s\n",cases,gameplay_revision());gameplay_close();return 0;
}
