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
#include "script/doors.h"
void doorController_updateLinkRespawn_hook(GB *gb);
void doorController_decideActionBasedOnTriggers_hook(GB *gb);
void doorController_checkMinecartCollidedWithDoor_hook(GB *gb);
void doorController_checkTileIsMinecartTrack_hook(GB *gb);
void doorController_checkEnoughTorchesLit_hook(GB *gb);
void checkAndDecKeyCount_hook(GB *gb);
void nextToKeyDoor_hook(GB *gb);
void nextToOverworldKeyhole_hook(GB *gb);
void scriptFunc_setState_hook(GB *gb);

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
static void base(uint8_t state,uint8_t subid) {
  struct Fibers *fib=g->fib;fibers_reset(g);*g=*seed;g->fib=fib;
  memset(g->samples,0,16*sizeof *g->samples);g->sample_head=g->sample_count=0;g->sample_overflow=0;g->sample=&g->samples[0];
  *render_line_regs(g)=render_seed;mem_wr(g,IO_SVBK,1);mem_wr(g,hActiveObjectType,INTERACTION_BASE);mem_wr(g,hActiveObject,OBJECT>>8);
  for(unsigned i=0;i<0xc0;++i) {mem_wr(g,wRoomLayout+i,0xa0);mem_wr(g,wRoomCollisions+i,0);}
  for(unsigned i=0;i<0x40;++i) mem_wr(g,OBJECT+i,0);
  mem_wr(g,OBJECT,1);mem_wr(g,OBJECT+OBJ_ID,0x1e);mem_wr(g,OBJECT+OBJ_STATE,state);mem_wr(g,OBJECT+OBJ_SUBID,subid);
  mem_wr(g,OBJECT+OBJ_XH,0x48);mem_wr(g,OBJECT+OBJ_YH,0x48);mem_wr(g,OBJECT+OBJ_VAR3E,0x44);mem_wr(g,OBJECT+OBJ_ANGLE,0x10);
  wr16(g,OBJECT+OBJ_SCRIPT_PTR,SYM(stubScript));mem_wr(g,wActiveMusic,3);mem_wr(g,wScrollMode,1);
  mem_wr(g,wPaletteThread_mode,0);mem_wr(g,wLinkDeathTrigger,0);mem_wr(g,wTextIsActive,0);mem_wr(g,wSwitchHookState,0);
  mem_wr(g,w1Link_xh,0x28);mem_wr(g,w1Link_yh,0x28);
  g->d=OBJECT>>8;g->sp=wThread0StackTop-2;mem_wr(g,g->sp,0);mem_wr(g,g->sp+1,0);
}
static void compare(bool lua) {
  if(!lua) { *expected=*g; memcpy(samples,g->samples,16*sizeof *samples); }
  else {
    if(memcmp(expected,g,sizeof *g)) fprintf(stderr,"mismatch: %s case %u\n",context,cases);
    ASSERT(!memcmp(expected,g,sizeof *g)); ASSERT(!memcmp(samples,g->samples,16*sizeof *samples));
  }
}

static void call(void (*hook)(GB *),uint16_t pc,uint8_t bank) {
  g->sp=wThread0StackTop-2;mem_wr(g,g->sp,0);mem_wr(g,g->sp+1,0);g->pc=pc;g->rom_bank=bank;mem_wr(g,hRomBank,bank);
  hook_run(g,hook,pc);
  if(g->hung)fprintf(stderr,"stopped %s case=%u: %s\n",context,cases,gb_stop_reason);
  ASSERT(!g->hung);ASSERT(!g->sample_overflow);
}
#define ENTER(name) call(name##_hook,SYM(name),SYMBANK(name))
static void initialization(void) {
  context="all 24 door subtypes and original scripts";
  for(unsigned subid=0;subid<24;++subid)for(unsigned requirement=0;requirement<2;++requirement) {
    for(int mode=0;mode<2;++mode) {
      base(0,subid);gameplay_enable(mode!=0);mem_wr(g,OBJECT+OBJ_XH,0);mem_wr(g,OBJECT+OBJ_YH,0x44);
      mem_wr(g,wActiveTriggers,requirement);mem_wr(g,wNumEnemies,requirement);mem_wr(g,wNumTorchesLit,requirement?2:0);
      for(unsigned step=0;step<12 && mem_rd(g,OBJECT);++step)ENTER(interactionCode1e);
      compare(mode!=0);
    }++cases;
  }
}
static void timers_and_tiles(void) {
  context="opening and closing timers, palette pause and collision";
  for(unsigned state=2;state<=3;++state)for(unsigned counter=0;counter<256;++counter)for(int mode=0;mode<2;++mode) {
    base(state,4);gameplay_enable(mode!=0);mem_wr(g,OBJECT+OBJ_SUBSTATE,1);mem_wr(g,OBJECT+OBJ_COUNTER1,counter);
    ENTER(interactionCode1e);compare(mode!=0);if(mode)++cases;
  }
  for(unsigned busy=0;busy<256;++busy)for(int mode=0;mode<2;++mode) {
    base(2,0);gameplay_enable(mode!=0);mem_wr(g,wPaletteThread_mode,busy);ENTER(interactionCode1e);compare(mode!=0);if(mode)++cases;
  }
  for(unsigned busy=0;busy<256;++busy)for(int mode=0;mode<2;++mode) {
    base(1,1);gameplay_enable(mode!=0);mem_wr(g,wSwitchHookState,busy);ENTER(interactionCode1e);compare(mode!=0);if(mode)++cases;
  }
  for(unsigned angle=0;angle<32;angle+=2)for(unsigned state=2;state<=3;++state)for(unsigned collision=0;collision<2;++collision)for(unsigned somaria=0;somaria<2;++somaria)for(int mode=0;mode<2;++mode) {
    base(state,0);gameplay_enable(mode!=0);mem_wr(g,OBJECT+OBJ_ANGLE,angle);mem_wr(g,wRoomCollisions+0x44,collision?15:0);mem_wr(g,wRoomLayout+0x44,somaria?0xda:0xa0);
    ENTER(interactionCode1e);compare(mode!=0);if(mode)++cases;
  }
  context="Link on the closing tile and local respawn";
  for(unsigned same=0;same<2;++same)for(unsigned angle=0x10;angle<0x18;angle+=2)for(int mode=0;mode<2;++mode) {
    base(3,4);gameplay_enable(mode!=0);mem_wr(g,OBJECT+OBJ_ANGLE,angle);mem_wr(g,OBJECT+OBJ_SUBSTATE,1);mem_wr(g,OBJECT+OBJ_COUNTER1,1);
    if(same){mem_wr(g,w1Link_xh,0x48);mem_wr(g,w1Link_yh,0x48);}ENTER(interactionCode1e);compare(mode!=0);if(mode)++cases;
  }
}
static void trigger_torch_minecart_helpers(void) {
  context="triggers, exact torch counts and minecart scripts";
  for(unsigned orientation=0;orientation<4;++orientation)for(unsigned bit=0;bit<8;++bit)for(unsigned flags=0;flags<4;++flags)for(unsigned solid=0;solid<2;++solid)for(int mode=0;mode<2;++mode) {
    base(1,4+orientation);gameplay_enable(mode!=0);uint8_t mask=1u<<bit;mem_wr(g,OBJECT+OBJ_VAR3D,mask);mem_wr(g,OBJECT+OBJ_ANGLE,0x10+2*orientation);
    mem_wr(g,wActiveTriggers,(const uint8_t[]){0,mask,255,(uint8_t)~mask}[flags]);mem_wr(g,wRoomCollisions+0x44,solid?15:0);
    uint16_t a=SYM(interactionCode1e__shutterTiles)+0x10+2*orientation+1;mem_wr(g,wRoomLayout+0x44,solid?g->rom[8*0x4000+a-0x4000]:0xa0);
    ENTER(doorController_decideActionBasedOnTriggers);compare(mode!=0);if(mode)++cases;
  }
  for(unsigned required=1;required<=2;++required)for(unsigned lit=0;lit<256;++lit)for(int mode=0;mode<2;++mode) {
    base(1,0x14);gameplay_enable(mode!=0);mem_wr(g,OBJECT+OBJ_SPEED,required);mem_wr(g,wNumTorchesLit,lit);ENTER(doorController_checkEnoughTorchesLit);ASSERT_EQ(mem_rd(g,wEnemyPlacement),lit==required);compare(mode!=0);if(mode)++cases;
  }
  for(unsigned tile=0;tile<256;++tile)for(int mode=0;mode<2;++mode) {
    base(1,12);gameplay_enable(mode!=0);mem_wr(g,wRoomLayout+0x44,tile);ENTER(doorController_checkTileIsMinecartTrack);ASSERT_EQ(mem_rd(g,wTmpcfc0_normal_doorControllerState),tile==0x5d || tile==0x5e);compare(mode!=0);if(mode)++cases;
  }
  for(unsigned riding=0;riding<2;++riding)for(unsigned touching=0;touching<2;++touching)for(int mode=0;mode<2;++mode) {
    base(1,12);gameplay_enable(mode!=0);mem_wr(g,wLinkObjectIndex,riding);mem_wr(g,OBJECT+OBJ_COLLISION_RADIUS_Y,0x10);mem_wr(g,OBJECT+OBJ_COLLISION_RADIUS_X,0x10);
    mem_wr(g,w1Link_xh,touching?0x48:0x28);mem_wr(g,w1Link_yh,touching?0x48:0x28);
    for(unsigned i=0;i<0x40;++i)mem_wr(g,0xd100+i,mem_rd(g,0xd000+i));
    ENTER(doorController_checkMinecartCollidedWithDoor);compare(mode!=0);if(mode)++cases;
  }
  for(unsigned same=0;same<2;++same)for(unsigned orientation=0;orientation<4;++orientation)for(int mode=0;mode<2;++mode) {
    base(1,4);gameplay_enable(mode!=0);mem_wr(g,OBJECT+OBJ_ANGLE,0x10+2*orientation);mem_wr(g,wLinkLocalRespawnY,same?0x48:0x28);mem_wr(g,wLinkLocalRespawnX,same?0x48:0x28);
    ENTER(doorController_updateLinkRespawn);compare(mode!=0);if(mode)++cases;
  }
}
static void key_requirements(void) {
  context="small and boss key ownership, debit and no-dungeon sentinel";
  for(unsigned index=0;index<5;++index)for(unsigned kind=0;kind<2;++kind)for(unsigned count=0;count<256;++count)for(int mode=0;mode<2;++mode) {
    base(1,0);gameplay_enable(mode!=0);uint8_t dungeon=(const uint8_t[]){0,7,8,15,255}[index];mem_wr(g,wDungeonIndex,dungeon);g->b=kind?0x42:0x32;
    mem_wr(g,0xc672+(dungeon&15),count);mem_wr(g,0xc682,kind && (count&1)?255:0);mem_wr(g,0xc683,kind && (count&1)?255:0);
    ENTER(checkAndDecKeyCount);compare(mode!=0);
    if(dungeon!=255)ASSERT_EQ(mem_rd(g,0xc672+dungeon),kind?count:(count?count-1:0));
    if(mode)++cases;
  }
}
static void gate_fixture(uint8_t direction,uint8_t flags) {
  base(0, direction<<4);mem_wr(g,OBJECT+OBJ_ID,0x1b);mem_wr(g,wSwitchState,flags);
  memcpy(g,engine_seed,offsetof(GB,rom));g->rom_bank=engine_seed->rom_bank;g->ram_bank=engine_seed->ram_bank;g->ie=engine_seed->ie;
  memcpy(g->io,engine_seed->io,sizeof g->io);memcpy(g->hram,engine_seed->hram,sizeof g->hram);
  for(unsigned frame=0;frame<4;++frame){gb_run_frame(g);ASSERT(!g->hung);g->apu.ring_r=g->apu.ring_w;}
  mem_wr(g,IO_SVBK,1);g->ime=false;g->ie=0;g->d=OBJECT>>8;mem_wr(g,hActiveObjectType,INTERACTION_BASE);mem_wr(g,hActiveObject,OBJECT>>8);
}
static void gates(void) {
  context="both minecart gate directions, switches and animation completion";
  for(unsigned direction=0;direction<=2;direction+=2)for(unsigned flags=0;flags<256;++flags)for(int mode=0;mode<2;++mode) {
    gameplay_enable(mode!=0);gate_fixture(direction,flags);compare(mode!=0);if(mode)++cases;
  }
  for(unsigned direction=0;direction<=2;direction+=2)for(unsigned initial=0;initial<2;++initial)for(unsigned after=0;after<2;++after)for(int mode=0;mode<2;++mode) {
    gameplay_enable(mode!=0);gate_fixture(direction,initial);mem_wr(g,wSwitchState,after);ENTER(interactionCode1b);compare(mode!=0);if(mode)++cases;
  }
  for(unsigned direction=0;direction<=2;direction+=2)for(unsigned token=0;token<256;++token)for(int mode=0;mode<2;++mode) {
    gameplay_enable(mode!=0);gate_fixture(direction,0);mem_wr(g,OBJECT+OBJ_STATE,3);mem_wr(g,OBJECT+OBJ_ANIM_COUNTER,200);mem_wr(g,OBJECT+OBJ_ANIM_PARAMETER,token);
    ENTER(interactionCode1b);compare(mode!=0);if(mode)++cases;
  }
}
static void key_door_fixture(uint8_t keys,uint8_t busy) {
  base(1,0);g->d=0xd0;mem_wr(g,hActiveObjectType,0);mem_wr(g,hActiveObject,0xd0);
  mem_wr(g,wLinkPushingDirection,0);mem_wr(g,wLinkAngle,0);mem_wr(g,wPushingAgainstTileCounter,1);mem_wr(g,hFF8D,0x44);
  mem_wr(g,wDungeonIndex,0);mem_wr(g,0xc672,keys);mem_wr(g,w1ReservedInteraction0_enabled,busy);g->b=0x32;
}
static void key_doors_and_keyholes(void) {
  context="key door spawn, single debit per actuation and occupied reserved slot";
  for(unsigned keys=0;keys<=3;++keys)for(unsigned busy=0;busy<2;++busy)for(int mode=0;mode<2;++mode) {
    gameplay_enable(mode!=0);key_door_fixture(keys,busy);ENTER(nextToKeyDoor);compare(mode!=0);if(mode)++cases;
    ASSERT_EQ(mem_rd(g,0xc672),keys?keys-1:0);
  }
  for(int mode=0;mode<2;++mode) {
    gameplay_enable(mode!=0);key_door_fixture(3,0);ENTER(nextToKeyDoor);uint8_t count=mem_rd(g,0xc672);g->d=0xd0;g->b=0x32;ENTER(nextToKeyDoor);ASSERT_EQ(mem_rd(g,0xc672),count);compare(mode!=0);if(mode)++cases;
  }
  context="overworld keyhole locked/unlocked and entrance key absent/present";
  for(unsigned unlocked=0;unlocked<2;++unlocked)for(unsigned owned=0;owned<2;++owned)for(int mode=0;mode<2;++mode) {
    gameplay_enable(mode!=0);key_door_fixture(0,0);mem_wr(g,wActiveGroup,0);mem_wr(g,wActiveRoom,0x5c);mem_wr(g,w1Link_direction,0);mem_wr(g,w1Link_adjacentWallsBitset,0xff);
    for(unsigned i=0;i<16;++i)mem_wr(g,wObtainedTreasureFlags+i,owned?255:0);
    ENTER(getThisRoomFlags);mem_wr(g,((uint16_t)g->h<<8)|g->l,unlocked?128:0);g->d=0xd0;
    ENTER(nextToOverworldKeyhole);compare(mode!=0);if(mode)++cases;
  }
}
static void script_owner_boundary(void) {
  context="generic script VM remains C for other actors and substate commands";
  for(unsigned owner=0;owner<2;++owner)for(unsigned substate=0;substate<2;++substate)for(unsigned command=0;command<2;++command)for(int mode=0;mode<2;++mode) {
    base(1,4);gameplay_enable(mode!=0);mem_wr(g,OBJECT+OBJ_ID,owner?0x1e:0x13);g->e=INTERACTION_BASE+(substate?OBJ_SUBSTATE:OBJ_STATE);
    g->h=0xcf;g->l=0xc0;mem_wr(g,0xcfc0,command?255:2);uint64_t before=gameplay_door_calls(GAMEPLAY_DOOR_script_state);
    ENTER(scriptFunc_setState);ASSERT_EQ(gameplay_door_calls(GAMEPLAY_DOOR_script_state)-before,mode && owner && !substate?1:0);compare(mode!=0);if(mode)++cases;
  }
}
static void engine_context(void) {
  memcpy(g,engine_seed,offsetof(GB,rom));g->rom_bank=engine_seed->rom_bank;g->ram_bank=engine_seed->ram_bank;g->ie=engine_seed->ie;
  memcpy(g->io,engine_seed->io,sizeof g->io);memcpy(g->hram,engine_seed->hram,sizeof g->hram);
  g->input_at=NULL;g->input_ctx=NULL;g->joy=0;
}
static void lifecycle(bool serialize) {
  context=serialize?"frame comparisons with recreated VM":"complete door and gate frame sequences";
  const unsigned limit=120;GB *trajectory=malloc(limit*sizeof *trajectory);uint64_t *hashes=malloc(limit*sizeof *hashes);ASSERT(trajectory && hashes);
  char path[4096];ASSERT(oracles_temp_path(path,sizeof path));unsigned save_mask=0,save_count=0;
  for(unsigned family=0;family<22;++family) {
    uint8_t subid=family==0?0:family<21?family+3:0;
    base(0,subid);mem_wr(g,OBJECT+OBJ_XH,0);mem_wr(g,OBJECT+OBJ_YH,0x44);
    if(family==21){mem_wr(g,OBJECT+OBJ_ID,0x1b);mem_wr(g,OBJECT+OBJ_XH,0x48);mem_wr(g,OBJECT+OBJ_YH,0x48);}
    *prepared=*g;
    for(int mode=0;mode<2;++mode) {
      struct Fibers *fib=g->fib;fibers_reset(g);*g=*prepared;g->fib=fib;*render_line_regs(g)=render_seed;engine_context();gameplay_enable(mode!=0);
      unsigned saved_states=0;
      for(unsigned frame=0;frame<limit;++frame) {
        uint8_t state=g->wram[1][(OBJECT+OBJ_STATE)&0xfff],substate=g->wram[1][(OBJECT+OBJ_SUBSTATE)&0xfff];
        mem_wr(g,wActiveTriggers,frame>=40 && frame<80?1:0);mem_wr(g,wSwitchState,frame>=40 && frame<80?1:0);
        mem_wr(g,wNumTorchesLit,frame>=30?2:0);mem_wr(g,wNumEnemies,frame<20?1:0);
        if(serialize && g->wram[1][OBJECT&0xfff] && state>=2 && state<=3 && !(saved_states&(1u<<state)) && (family==21 || substate==1)) {
          ASSERT(oracles_save_boot_state(g,path));ASSERT(oracles_load_boot_state(g,path));gameplay_close();ASSERT(gameplay_init());gameplay_enable(mode!=0);
          saved_states|=1u<<state;save_mask|=1u<<state;++save_count;
        }
        gb_run_frame(g);if(g->hung)fprintf(stderr,"lifecycle family=%u mode=%d frame=%u state=%u: %s\n",family,mode,frame,state,gb_stop_reason);
        ASSERT(!g->hung);ASSERT(!g->sample_overflow);g->apu.ring_r=g->apu.ring_w;
        uint64_t hash=fnv1a64_update(FNV1A64_INIT,(const unsigned char *)g->sample,sizeof *g->sample);
        if(!mode){trajectory[frame]=*g;hashes[frame]=hash;}
        else {if(memcmp(&trajectory[frame],g,sizeof *g))fprintf(stderr,"frame mismatch family=%u frame=%u\n",family,frame);ASSERT(!memcmp(&trajectory[frame],g,sizeof *g));ASSERT_EQ(hash,hashes[frame]);}
      }
    }++cases;
  }
  if(serialize){ASSERT_EQ(save_mask,12);ASSERT(save_count>0);ASSERT_EQ(remove(path),0);}
  printf("22 families x %u frames matched, reload=%d saves=%u\n",limit,serialize,save_count);free(trajectory);free(hashes);
}
static void room_reentry(void) {
  context="leave an animated door and recreate its room in the same VM";
  for(unsigned subid=4;subid<24;++subid)for(int mode=0;mode<2;++mode) {
    base(3,subid);gameplay_enable(mode!=0);ENTER(interactionCode1e);ASSERT_EQ(mem_rd(g,OBJECT+OBJ_SUBSTATE),1);
    base(0,subid);mem_wr(g,OBJECT+OBJ_XH,0);mem_wr(g,OBJECT+OBJ_YH,0x44);ENTER(interactionCode1e);compare(mode!=0);if(mode)++cases;
  }
}
static void override_rule(const char *field,const char *body) {
  extern const unsigned char ages_doors_lua[];extern const unsigned int ages_doors_lua_size;
  size_t cap=ages_doors_lua_size+1024;char *source=malloc(cap);ASSERT(source);
  int n=snprintf(source,cap,"local rules=(function() %.*s end)();rules.%s=function(...) %s end;return rules",(int)ages_doors_lua_size,ages_doors_lua,field,body);
  ASSERT(n>0 && (size_t)n<cap);GameplaySources overrides={.doors=source,.doors_size=(size_t)n};ASSERT(gameplay_load_sources(&overrides));free(source);gameplay_enable(true);
}
static void raw_call(void (*hook)(GB *),uint16_t pc,uint8_t bank) {
  g->sp=wThread0StackTop-2;mem_wr(g,g->sp,0);mem_wr(g,g->sp+1,0);g->pc=pc;g->rom_bank=bank;mem_wr(g,hRomBank,bank);hook_run(g,hook,pc);
}
#define RAW(name) raw_call(name##_hook,SYM(name),SYMBANK(name))
static void failure_stops(void) {
  context="bad Lua results stop at the real door branch";
  for(int rule=0;rule<GAMEPLAY_DOOR_RULE_COUNT;++rule) {
    const char *field=strrchr(gameplay_door_rule_name(rule),'.')+1;ASSERT(gameplay_load_sources(NULL));gameplay_enable(true);base(1,4);
    if(!strncmp(field,"gate_",5))gate_fixture(0,0);
    if(!strcmp(field,"overworld_locked") || !strcmp(field,"overworld_key")) {
      key_door_fixture(0,0);mem_wr(g,wActiveGroup,0);mem_wr(g,wActiveRoom,0x5c);mem_wr(g,w1Link_direction,0);mem_wr(g,w1Link_adjacentWallsBitset,255);
      ENTER(getThisRoomFlags);mem_wr(g,((uint16_t)g->h<<8)|g->l,0);g->d=0xd0;
    }
    override_rule(field,"return {} ");
    switch(rule) {
      case GAMEPLAY_DOOR_switch_hook_busy:RAW(interactionCode1e);break;
      case GAMEPLAY_DOOR_palette_busy:case GAMEPLAY_DOOR_open_collision:
        mem_wr(g,OBJECT+OBJ_STATE,2);RAW(interactionCode1e);ASSERT_EQ(mem_rd(g,wRoomLayout+0x44),0xa0);break;
      case GAMEPLAY_DOOR_close_collision:case GAMEPLAY_DOOR_somaria_tile:
        mem_wr(g,OBJECT+OBJ_STATE,3);RAW(interactionCode1e);ASSERT_EQ(mem_rd(g,wRoomLayout+0x44),0xa0);break;
      case GAMEPLAY_DOOR_same_position:RAW(doorController_updateLinkRespawn);break;
      case GAMEPLAY_DOOR_timer_elapsed:
        mem_wr(g,OBJECT+OBJ_STATE,2);mem_wr(g,OBJECT+OBJ_SUBSTATE,1);mem_wr(g,OBJECT+OBJ_COUNTER1,1);RAW(interactionCode1e);ASSERT_EQ(mem_rd(g,wRoomLayout+0x44),0xa0);break;
      case GAMEPLAY_DOOR_trigger_active:case GAMEPLAY_DOOR_collision_clear:
        mem_wr(g,wActiveTriggers,0);RAW(doorController_decideActionBasedOnTriggers);break;
      case GAMEPLAY_DOOR_tile_matches:RAW(doorController_checkTileIsMinecartTrack);break;
      case GAMEPLAY_DOOR_minecart_riding:case GAMEPLAY_DOOR_minecart_collided:
        mem_wr(g,wLinkObjectIndex,1);RAW(doorController_checkMinecartCollidedWithDoor);break;
      case GAMEPLAY_DOOR_torches_ready:RAW(doorController_checkEnoughTorchesLit);break;
      case GAMEPLAY_DOOR_gate_open:case GAMEPLAY_DOOR_gate_is_open:mem_wr(g,wSwitchState,1);RAW(interactionCode1b);break;
      case GAMEPLAY_DOOR_gate_switch:RAW(interactionCode1b);break;
      case GAMEPLAY_DOOR_gate_animation_done:case GAMEPLAY_DOOR_gate_state:
        mem_wr(g,OBJECT+OBJ_STATE,3);mem_wr(g,OBJECT+OBJ_ANIM_COUNTER,200);mem_wr(g,OBJECT+OBJ_ANIM_PARAMETER,255);RAW(interactionCode1b);break;
      case GAMEPLAY_DOOR_script_state:mem_wr(g,OBJECT+OBJ_STATE,0);RAW(interactionCode1e);ASSERT_EQ(mem_rd(g,OBJECT+OBJ_STATE),0);break;
      case GAMEPLAY_DOOR_no_enemies:case GAMEPLAY_DOOR_link_clear:
        mem_wr(g,OBJECT+OBJ_STATE,0);mem_wr(g,OBJECT+OBJ_SUBID,rule==GAMEPLAY_DOOR_no_enemies?8:4);mem_wr(g,OBJECT+OBJ_XH,0);mem_wr(g,OBJECT+OBJ_YH,0x44);mem_wr(g,wNumEnemies,0);
        for(unsigned step=0;step<12 && !g->hung;++step)RAW(interactionCode1e);break;
      case GAMEPLAY_DOOR_valid_dungeon:case GAMEPLAY_DOOR_boss_door:case GAMEPLAY_DOOR_small_key:
        key_door_fixture(3,0);RAW(nextToKeyDoor);ASSERT_EQ(mem_rd(g,0xc672),3);break;
      case GAMEPLAY_DOOR_boss_key:
        key_door_fixture(3,0);g->b=0x42;mem_wr(g,0xc682,1);RAW(nextToKeyDoor);ASSERT_EQ(mem_rd(g,w1ReservedInteraction0_enabled),0);break;
      case GAMEPLAY_DOOR_reserved_free:key_door_fixture(3,0);RAW(nextToKeyDoor);ASSERT_EQ(mem_rd(g,w1ReservedInteraction0_enabled),0);break;
      case GAMEPLAY_DOOR_overworld_locked:case GAMEPLAY_DOOR_overworld_key:RAW(nextToOverworldKeyhole);break;
      default:ASSERT(false);
    }
    ASSERT(g->hung);ASSERT(strstr(gameplay_error(),field));
  }
  ASSERT(gameplay_load_sources(NULL));
}
static void lua_authority(void) {
  context="valid Lua decisions govern native effects";
  base(1,4);mem_wr(g,wDungeonIndex,0);mem_wr(g,0xc672,3);g->b=0x32;override_rule("small_key","return false");ENTER(checkAndDecKeyCount);ASSERT_EQ(mem_rd(g,0xc672),3);ASSERT(g->f&FZ);
  ASSERT(gameplay_load_sources(NULL));base(1,4);g->e=INTERACTION_BASE+OBJ_STATE;g->h=0xcf;g->l=0xc0;mem_wr(g,0xcfc0,2);override_rule("script_state","return 3");ENTER(scriptFunc_setState);ASSERT_EQ(mem_rd(g,OBJECT+OBJ_STATE),3);
  for(unsigned family=0;family<2;++family) {
    ASSERT(gameplay_load_sources(NULL));base(0,family?4:8);mem_wr(g,OBJECT+OBJ_XH,0);mem_wr(g,OBJECT+OBJ_YH,0x44);mem_wr(g,wNumEnemies,0);
    override_rule(family?"link_clear":"no_enemies","return false");for(unsigned step=0;step<32;++step)ENTER(interactionCode1e);ASSERT_EQ(mem_rd(g,OBJECT),1);ASSERT_EQ(mem_rd(g,OBJECT+OBJ_STATE),1);
  }
  ASSERT(gameplay_load_sources(NULL));gameplay_enable(true);gate_fixture(0,0);mem_wr(g,OBJECT+OBJ_STATE,3);mem_wr(g,OBJECT+OBJ_ANIM_COUNTER,200);mem_wr(g,OBJECT+OBJ_ANIM_PARAMETER,255);
  override_rule("gate_state","return 1");ENTER(interactionCode1b);ASSERT_EQ(mem_rd(g,OBJECT+OBJ_STATE),1);
  ASSERT(gameplay_load_sources(NULL));gameplay_enable(true);bool seasons=game_seasons;game_seasons=true;ASSERT(!DOOR_ACTIVE);game_seasons=seasons;
}
int main(int argc,char **argv) {
  const char *rom_path=GAME_ROM_DIR "/Legend of Zelda, The - Oracle of Ages (USA, Australia).gbc";
  for(int i=1;i<argc;++i){if(i+1>=argc)return 2;if(!strcmp(argv[i],"--rom"))rom_path=argv[++i];else if(!strcmp(argv[i],"--tas-dir"))tas_dir=argv[++i];else return 2;}
  size_t size;uint8_t *rom=oracles_read_file(rom_path,&size);if(!rom)SKIP("Ages ROM missing");char hash[41];sha1_hex(rom,size,hash);ASSERT(size==1048576 && !strcmp(hash,"880374fb978b18af4aa529e2e32f7ffb4d7dd2f4"));
  g=calloc(1,sizeof *g);seed=malloc(sizeof *seed);prepared=malloc(sizeof *prepared);expected=malloc(sizeof *expected);engine_seed=malloc(sizeof *engine_seed);samples=malloc(16*sizeof *samples);ASSERT(g && seed && prepared && expected && engine_seed && samples);
  gb_init(g);ASSERT(gb_load_rom(g,rom,size));g->cyctab=cyctab_alloc(rom,size);g->code_bits=assets_code_bits(rom,size);assets_zero_code(rom,size);gb_reset(g);hook_mode=HOOK_MODE_REPLACE;
  prepare_scene();ASSERT(gameplay_init());RUN(initialization);RUN(timers_and_tiles);RUN(trigger_torch_minecart_helpers);RUN(key_requirements);RUN(gates);RUN(key_doors_and_keyholes);RUN(script_owner_boundary);
  for(int i=0;i<GAMEPLAY_DOOR_RULE_COUNT;++i){ASSERT(gameplay_door_calls(i)>0);printf("%s=%llu\n",gameplay_door_rule_name(i),(unsigned long long)gameplay_door_calls(i));}
  lifecycle(false);lifecycle(true);RUN(room_reentry);RUN(failure_stops);RUN(lua_authority);
  printf("ok lua_doors_rom: %u directed cases; SHA256 %s\n",cases,gameplay_revision());gameplay_close();return 0;
}
