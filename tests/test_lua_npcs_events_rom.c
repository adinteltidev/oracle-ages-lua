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
#include "script/npcs_events.h"
#include "game/features.h"

#define OBJECT 0xd240
static GB *g,*seed,*prepared,*expected,*engine_seed;
static GBSample *samples;
static RenderLineRegs render_seed;
static unsigned cases;
static uint64_t covered[GAMEPLAY_NPCS_COUNT+GAMEPLAY_EVENTS_COUNT];
static void gather(void){for(unsigned i=0;i<GAMEPLAY_NPCS_COUNT;++i)covered[i]+=gameplay_npc_calls(i);for(unsigned i=0;i<GAMEPLAY_EVENTS_COUNT;++i)covered[GAMEPLAY_NPCS_COUNT+i]+=gameplay_event_calls(i);}
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

static void actor(uint8_t id){base(1,0);mem_wr(g,OBJECT+OBJ_ID,id);}
static void engine_context(void){
  memcpy(g,engine_seed,offsetof(GB,rom));g->rom_bank=engine_seed->rom_bank;g->ram_bank=engine_seed->ram_bank;g->ie=engine_seed->ie;
  memcpy(g->io,engine_seed->io,sizeof g->io);memcpy(g->hram,engine_seed->hram,sizeof g->hram);g->input_at=NULL;g->input_ctx=NULL;g->joy=0;
}
static void flag(unsigned index,bool value){uint16_t addr=wGlobalFlags+(index>>3);uint8_t bit=1u<<(index&7);mem_wr(g,addr,(mem_rd(g,addr)&~bit)|(value?bit:0));}
static void progress(unsigned level){
  flag(0x11,level==3);flag(0x13,level==5);flag(0x14,level==7);mem_wr(g,wIsLinkedGame,level==6);mem_wr(g,wGroup4RoomFlags+252,level==6?0x80:0);
  mem_wr(g,wEssencesObtained,level==1 || level==3?2:level==2?8:level==4?0x40:0);uint8_t mask=1;mem_wr(g,wObtainedTreasureFlags+8,(mem_rd(g,wObtainedTreasureFlags+8)&~mask)|(level>=1 && level<=4?mask:0));
}
static void opcode(void(*hook)(GB*),uint16_t pc,uint8_t bank){
  g->h=0xcf;g->l=0xc0;g->sp=wThread0StackTop-4;wr16(g,g->sp,0xcfc0);wr16(g,g->sp+2,0);g->pc=pc;g->rom_bank=bank;mem_wr(g,hRomBank,bank);hook_run(g,hook,pc);ASSERT(!g->hung);
}
#define OPCODE(name) opcode(name##_hook,SYM(name),SYMBANK(name))
static const uint8_t owners[]={0x10,0x11,0x16,0x1c,0x20,0x21,0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x30,0x31,0x34,0x35,0x36,0x37,0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f,0x40,0x41,0x42,0x43,0x44,0x45,0x48,0x49,0x4a,0x4b,0x4c,0x4d,0x4e,0x4f,0x52,0x53,0x54,0x55,0x57,0x58,0x59,0x5a,0x5b,0x5c,0x5d,0x5f,0x64,0x65,0x66,0x67,0x68,0x69,0x6a,0x6b,0x6c,0x6d,0x6e,0x70,0x71,0x73,0x74,0x75,0x76,0x7f,0x83,0x87,0x88,0x89,0x8a,0x8b,0x8d,0x93,0x94,0x96,0x97,0x9a,0x9b,0x9c,0x9d,0xa6,0xa7,0xa8,0xaa,0xab,0xac,0xad,0xb0,0xb1,0xb2,0xb3,0xb5,0xb8,0xba,0xbb,0xbc,0xbf,0xc1,0xc2,0xc3,0xc4,0xc6,0xc8,0xc9,0xca,0xcb,0xcc,0xcd,0xce,0xcf,0xd0,0xd1,0xd3,0xd5,0xd6,0xd7,0xd9,0xda,0xdb,0xdc,0xdf,0xe0,0xe3,0xe5};
static void owner_scope(void){
 context="all inventoried script owners; memory compare and previous-phase isolation";
 for(unsigned index=0;index<sizeof owners+8;++index)for(unsigned equal=0;equal<2;++equal)for(int mode=0;mode<2;++mode){
  static const uint8_t previous[]={0,1,0x13,0x1a,0x1e,0x46,0x60,0x81};uint8_t owner=index<sizeof owners?owners[index]:previous[index-sizeof owners];
  actor(owner);gameplay_enable(mode!=0);mem_wr(g,0xcfc1,0xf0);mem_wr(g,0xcfc2,0xcf);mem_wr(g,0xcfc3,equal?7:8);mem_wr(g,0xcff0,7);
  uint64_t before=gameplay_event_calls(GAMEPLAY_EVENTS_compare);OPCODE(scriptCmd_checkMemoryEq);ASSERT_EQ(gameplay_event_calls(GAMEPLAY_EVENTS_compare)-before,mode && index<sizeof owners?1:0);compare(mode!=0);if(mode)++cases;
 }
}
static void state_transitions(void){
 context="native state/substate transitions across all byte values";
 for(unsigned kind=0;kind<2;++kind)for(unsigned state=0;state<256;++state)for(unsigned sub=0;sub<2;++sub)for(int mode=0;mode<2;++mode){
  actor(0x45);gameplay_enable(mode!=0);uint16_t addr=OBJECT+(sub?OBJ_SUBSTATE:OBJ_STATE);mem_wr(g,addr,state);
  if(!kind){if(sub)ENTER(interactionIncSubstate);else ENTER(interactionIncState);ASSERT_EQ(mem_rd(g,addr),(state+1)&255);}
  else{g->h=0xcf;g->l=0xc0;mem_wr(g,0xcfc0,state);g->e=addr&255;ENTER(scriptFunc_setState);ASSERT_EQ(mem_rd(g,addr),state==255?0:state);}
  compare(mode!=0);if(mode)++cases;
 }

}
static void global_cutscenes(void){
 context="global scene state without an active NPC owner";
 for(unsigned kind=0;kind<3;++kind)for(unsigned state=0;state<256;++state)for(int mode=0;mode<2;++mode){
  base(1,0);mem_wr(g,hActiveObject,0);mem_wr(g,hActiveObjectType,0);gameplay_enable(mode!=0);
  uint16_t addr=kind==0?wCutsceneState:kind==1?wTmpcbb3:wTmpcbb4;mem_wr(g,addr,state);
  uint64_t before=gameplay_event_calls(GAMEPLAY_EVENTS_transition);
  if(kind==0)ENTER(incCutsceneState);else if(kind==1)ENTER(linkedCutscene_incSubstate);else ENTER(func_03_7b90);
  ASSERT_EQ(mem_rd(g,addr),(state+1)&255);ASSERT_EQ(gameplay_event_calls(GAMEPLAY_EVENTS_transition)-before,mode?1:0);
  compare(mode!=0);if(mode)++cases;
 }
}
static void bytecode_rules(void){
 context="original bytecode predicates, table dispatch, dialogue, linked state and event completion";
 for(unsigned kind=0;kind<2;++kind)for(unsigned value=0;value<256;++value)for(int mode=0;mode<2;++mode){
  actor(0x45);gameplay_enable(mode!=0);
  if(!kind){mem_wr(g,wIsLinkedGame,value);ENTER(checkIsLinkedGame);}
  else{mem_wr(g,wTextIsActive,value);ENTER(interactionRunScript);}
  compare(mode!=0);if(mode)++cases;
 }
 for(unsigned kind=0;kind<7;++kind)for(unsigned variant=0;variant<2;++variant)for(int mode=0;mode<2;++mode){
  actor(0x45);gameplay_enable(mode!=0);
  if(kind==0){mem_wr(g,0xcfc1,0x37);mem_wr(g,OBJECT+0x37,variant);wr16(g,0xcfc2,0xcfd0);wr16(g,0xcfc4,0xcfe0);OPCODE(scriptCmd_jumpTable);}
  if(kind==1){mem_wr(g,0xcfc1,variant?0x80:0);OPCODE(scriptCmd_setOrUnsetGlobalFlag);}
  if(kind==2){mem_wr(g,0xcfc1,variant?0x15:5);wr16(g,0xcfc2,0xcfd0);OPCODE(scriptCmd_df);}
  if(kind==3){mem_wr(g,0xcfc1,0x29);wr16(g,0xcfc2,0xcfd0);flag(0x29,variant);OPCODE(scriptCmd_jumpIfGlobalFlagSet);}
  if(kind==4){g->b=0x18;g->c=variant?0x0a:0;ENTER(showText);}
  if(kind==5){mem_wr(g,OBJECT+0x30,variant?0x80:0);mem_wr(g,OBJECT+0x33,0x18);g->h=0xcf;g->l=0xc0;mem_wr(g,0xcfc0,0x18);mem_wr(g,0xcfc1,0x0a);ENTER(scriptFunc_getTextIndex);}
  if(kind==6){wr16(g,OBJECT+OBJ_SCRIPT_PTR,SYM(stubScript));ENTER(interactionRunScript);}
  compare(mode!=0);if(mode)++cases;
 }
}
static void lifecycle(bool reload){
 context=reload?"NPC event frames, save/load and recreated VM":"NPC event frames, conversations and progress branches";
 const unsigned frames=80;GB *trajectory=malloc(frames*sizeof *trajectory);uint64_t *hashes=malloc(frames*sizeof *hashes);ASSERT(trajectory && hashes);
 char path[4096];ASSERT(oracles_temp_path(path,sizeof path));unsigned saves=0;
 // Lady: both subids x eight progress stages. Symmetry: all thirteen x repaired/unrepaired.
 for(unsigned family=0;family<44;++family){
  bool lady=family<16;unsigned subid=lady?family/8:(family-16)/2;unsigned parameter=lady?family%8:(family-16)%2;
  actor(lady?0x45:family<42?0xbf:family==42?0x38:0x43);if(family>=42)subid=0;
  mem_wr(g,OBJECT+OBJ_STATE,0);mem_wr(g,OBJECT+OBJ_SUBID,subid);progress(lady?parameter:0);flag(0x29,parameter!=0);*prepared=*g;
  for(int mode=0;mode<2;++mode){struct Fibers *fib=g->fib;fibers_reset(g);*g=*prepared;g->fib=fib;*render_line_regs(g)=render_seed;engine_context();gameplay_enable(mode!=0);unsigned checkpoints=0;
   for(unsigned frame=0;frame<frames;++frame){
    mem_wr(g,wTextIsActive,0);mem_wr(g,wSelectedTextOption,(frame/20)&1);if(frame%20==8)g->wram[1][(OBJECT+0x31)&0xfff]=1;
    if(reload && threads_parked(g) && ((frame>=2 && !(checkpoints&1)) || (frame>=16 && !(checkpoints&2)) || (frame>=48 && !(checkpoints&4)))){
      ASSERT(oracles_save_boot_state(g,path));ASSERT(oracles_load_boot_state(g,path));gather();gameplay_close();ASSERT(gameplay_init());gameplay_enable(mode!=0);checkpoints|=frame>=48?4:frame>=16?2:1;++saves;
    }
    if(!lady && family<42 && (subid==8 || subid==9) && frame==10){g->wram[1][(OBJECT+OBJ_STATE)&0xfff]=2;mem_wr(g,wTmpcfc0_genericCutscene_state,parameter);}
    gb_run_frame(g);if(g->hung)fprintf(stderr,"NPC scene family=%u sub=%u progress=%u frame=%u lua=%d: %s\n",family,subid,parameter,frame,mode,gb_stop_reason);ASSERT(!g->hung);ASSERT(!g->sample_overflow);g->apu.ring_r=g->apu.ring_w;
    uint64_t hash=fnv1a64_update(FNV1A64_INIT,(const unsigned char*)g->sample,sizeof *g->sample);
    if(!mode){trajectory[frame]=*g;hashes[frame]=hash;}else{if(memcmp(&trajectory[frame],g,sizeof *g))fprintf(stderr,"NPC mismatch family=%u frame=%u reload=%d\n",family,frame,reload);ASSERT(!memcmp(&trajectory[frame],g,sizeof *g));ASSERT_EQ(hash,hashes[frame]);}
   }
   if(reload)ASSERT_EQ(checkpoints,7);
  }++cases;
 }
 if(reload){ASSERT(saves>0);ASSERT_EQ(remove(path),0);}printf("44 NPC families x 80 frames matched, reload=%d saves=%u\n",reload,saves);free(trajectory);free(hashes);
}
static void frames_normal(void){lifecycle(false);}
static void frames_reload(void){lifecycle(true);}
static void override(const char *module,const char *field,const char *body){
 extern const unsigned char ages_npcs_rules_lua[],ages_events_flow_lua[];extern const unsigned int ages_npcs_rules_lua_size,ages_events_flow_lua_size;
 bool npc=!strcmp(module,"npcs");const unsigned char *base=npc?ages_npcs_rules_lua:ages_events_flow_lua;unsigned size=npc?ages_npcs_rules_lua_size:ages_events_flow_lua_size;
 size_t cap=size+1024;char *source=malloc(cap);ASSERT(source);int n=snprintf(source,cap,"local r=(function() %.*s end)();r.%s=function(...) %s end;return r",size,base,field,body);ASSERT(n>0 && (size_t)n<cap);
 GameplaySources sources={0};if(npc){sources.npcs=source;sources.npcs_size=n;}else{sources.events=source;sources.events_size=n;}gather();ASSERT(gameplay_load_sources(&sources));free(source);gameplay_enable(true);
}
static void authority(void){
 context="valid alternatives affect NPC availability, state, text and actual script branches";
 actor(0x45);mem_wr(g,OBJECT+OBJ_STATE,0);flag(0x14,false);override("npcs","old_lady_available","return false");ENTER(interactionCode45);ASSERT_EQ(mem_rd(g,OBJECT),0);++cases;
 actor(0x45);override("events","next_state","local current=...;return current");ENTER(interactionIncState);ASSERT_EQ(mem_rd(g,OBJECT+OBJ_STATE),1);++cases;
 actor(0x45);override("events","transition","local current=...;return current");g->h=0xcf;g->l=0xc0;g->e=0x44;mem_wr(g,0xcfc0,2);ENTER(scriptFunc_setState);ASSERT_EQ(mem_rd(g,OBJECT+OBJ_STATE),1);++cases;
 actor(0x45);override("events","dialogue","return 0");g->b=0x18;g->c=0x0a;ENTER(showText);ASSERT_EQ(mem_rd(g,wTextDisplayMode+1),0);++cases;
 actor(0x45);override("events","condition","return false");flag(0x29,true);mem_wr(g,0xcfc1,0x29);wr16(g,0xcfc2,0xcfd0);OPCODE(scriptCmd_jumpIfGlobalFlagSet);ASSERT_EQ(((unsigned)g->h<<8)|g->l,0xcfc4);++cases;
 actor(0x45);override("events","script_ready","return false");uint16_t before=rd16(g,OBJECT+OBJ_SCRIPT_PTR);ENTER(interactionRunScript);ASSERT_EQ(rd16(g,OBJECT+OBJ_SCRIPT_PTR),before);++cases;
 actor(0x45);override("events","script_ended","return true");wr16(g,OBJECT+OBJ_SCRIPT_PTR,0xcfc0);mem_wr(g,0xcfc0,0x80);ENTER(interactionRunScript);ASSERT(g->f&FC);++cases;
 base(1,0);mem_wr(g,hActiveObject,0);mem_wr(g,wCutsceneState,5);override("events","transition","local current=...;return current");ENTER(incCutsceneState);ASSERT_EQ(mem_rd(g,wCutsceneState),5);++cases;
 base(1,0);mem_wr(g,hActiveObject,0);mem_wr(g,wCutsceneState,5);override("events","transition","return nil");g->pc=SYM(incCutsceneState);g->rom_bank=SYMBANK(incCutsceneState);mem_wr(g,hRomBank,g->rom_bank);hook_run(g,incCutsceneState_hook,g->pc);ASSERT(g->hung);ASSERT_EQ(mem_rd(g,wCutsceneState),5);++cases;
 gather();ASSERT(gameplay_load_sources(NULL));
}
static void faults(void){
 context="invalid decisions stop all twenty rules at real native hook points";
 for(unsigned rule=0;rule<20;++rule){
  actor(rule==2 || rule==6 || rule==7?0xbf:0x45);
  const char *name=rule<8?gameplay_npc_rule_name(rule):gameplay_event_rule_name(rule-8);const char *field=strrchr(name,'.')+1;
  override(rule<8?"npcs":"events",field,"return nil");
  void(*hook)(GB*)=NULL;uint16_t pc=0;uint8_t bank=0;bool op=false;
#define CHOOSE(n) do {hook=n##_hook;pc=SYM(n);bank=SYMBANK(n);}while(0)
  if(rule==0 || rule==1 || rule==3){progress(4);CHOOSE(getGameProgress_2);}
  if(rule==2 || rule==7){mem_wr(g,OBJECT+OBJ_STATE,2);mem_wr(g,wTmpcfc0_genericCutscene_state,0);CHOOSE(interactionCodebf);}
  if(rule==4 || rule==5){mem_wr(g,OBJECT+OBJ_STATE,0);mem_wr(g,OBJECT+OBJ_SUBID,rule==4?0:1);progress(0);CHOOSE(interactionCode45);}
  if(rule==6){mem_wr(g,OBJECT+OBJ_STATE,0);mem_wr(g,OBJECT+OBJ_SUBID,12);engine_context();for(unsigned i=0;i<8 && !g->hung;++i)gb_run_frame(g);ASSERT(g->hung);ASSERT(gameplay_error()[0]);++cases;continue;}
  if(rule==8){mem_wr(g,0xcfc1,0xf0);mem_wr(g,0xcfc2,0xcf);mem_wr(g,0xcfc3,1);mem_wr(g,0xcff0,1);op=true;CHOOSE(scriptCmd_checkMemoryEq);}
  if(rule==9){g->h=0xcf;g->l=0xc0;CHOOSE(scriptFunc_getTextIndex);}
  if(rule==10){mem_wr(g,0xcfc1,0x80);op=true;CHOOSE(scriptCmd_setOrUnsetGlobalFlag);}
  if(rule==11){mem_wr(g,0xcfc1,5);op=true;CHOOSE(scriptCmd_df);}
  if(rule==12){mem_wr(g,0xcfc1,0x77);mem_wr(g,OBJECT+0x37,0);op=true;CHOOSE(scriptCmd_jumpTable);}
  if(rule==13){g->h=0xcf;g->l=0xc0;g->e=0x44;mem_wr(g,0xcfc0,2);CHOOSE(scriptFunc_setState);}
  if(rule==14)CHOOSE(interactionIncState);
  if(rule==15){g->b=0x18;g->c=0x0a;CHOOSE(showText);}
  if(rule==16)CHOOSE(checkIsLinkedGame);
  if(rule==17 || rule==18){mem_wr(g,wTextIsActive,rule==17?1:0);CHOOSE(interactionRunScript);}
  if(rule==19){mem_wr(g,0xcfc1,0x29);wr16(g,0xcfc2,0xcfd0);op=true;CHOOSE(scriptCmd_jumpIfGlobalFlagSet);}
  if(op){g->h=0xcf;g->l=0xc0;}g->sp=wThread0StackTop-(op?4:2);wr16(g,g->sp,op?0xcfc0:0);if(op)wr16(g,g->sp+2,0);g->pc=pc;g->rom_bank=bank;mem_wr(g,hRomBank,bank);hook_run(g,hook,pc);
  if(!g->hung)fprintf(stderr,"rule did not stop: %u %s\n",rule,name);ASSERT(g->hung);ASSERT(gameplay_error()[0]);++cases;
#undef CHOOSE
 }
 gather();ASSERT(gameplay_load_sources(NULL));
}
static void story_gates(void){
 context="story gates: Maku opening and black tower conditions";
 for(unsigned kind=0;kind<2;++kind)for(unsigned variation=0;variation<(kind?4:2);++variation){
  actor(kind?0xc6:0x76);mem_wr(g,OBJECT+OBJ_STATE,0);mem_wr(g,OBJECT+OBJ_SUBID,kind?0:variation);if(kind){ENTER(getThisRoomFlags);mem_wr(g,((unsigned)g->h<<8)|g->l,(variation&1)?0x40:0);mem_wr(g,wObtainedTreasureFlags+6,(mem_rd(g,wObtainedTreasureFlags+6)&~0x40)|((variation&2)?0x40:0));}
  *prepared=*g;GB *trajectory=malloc(100*sizeof *trajectory);ASSERT(trajectory);
  for(int mode=0;mode<2;++mode){struct Fibers *fib=g->fib;fibers_reset(g);*g=*prepared;g->fib=fib;*render_line_regs(g)=render_seed;engine_context();gameplay_enable(mode!=0);
   for(unsigned frame=0;frame<100;++frame){gb_run_frame(g);if(g->hung)fprintf(stderr,"gate kind=%u variant=%u frame=%u: %s\n",kind,variation,frame,gb_stop_reason);ASSERT(!g->hung);g->apu.ring_r=g->apu.ring_w;if(!mode)trajectory[frame]=*g;else ASSERT(!memcmp(&trajectory[frame],g,sizeof *g));}
  }free(trajectory);++cases;
 }
}
int main(int argc,char **argv){
  setvbuf(stdout,NULL,_IONBF,0);const char *suite=NULL;
  const char *rom_path=GAME_ROM_DIR "/Legend of Zelda, The - Oracle of Ages (USA, Australia).gbc";
  for(int i=1;i<argc;++i){if(i+1>=argc)return 2;if(!strcmp(argv[i],"--rom"))rom_path=argv[++i];else if(!strcmp(argv[i],"--tas-dir"))tas_dir=argv[++i];else if(!strcmp(argv[i],"--suite"))suite=argv[++i];else return 2;}
  size_t size;uint8_t *rom=oracles_read_file(rom_path,&size);if(!rom)SKIP("Ages ROM missing");char hash[41];sha1_hex(rom,size,hash);ASSERT(size==1048576 && !strcmp(hash,"880374fb978b18af4aa529e2e32f7ffb4d7dd2f4"));
  g=calloc(1,sizeof *g);seed=malloc(sizeof *seed);prepared=malloc(sizeof *prepared);expected=malloc(sizeof *expected);engine_seed=malloc(sizeof *engine_seed);samples=malloc(16*sizeof *samples);ASSERT(g && seed && prepared && expected && engine_seed && samples);
  gb_init(g);ASSERT(gb_load_rom(g,rom,size));g->cyctab=cyctab_alloc(rom,size);g->code_bits=assets_code_bits(rom,size);assets_zero_code(rom,size);gb_reset(g);hook_mode=HOOK_MODE_REPLACE;
  prepare_scene();gameplay_close();ASSERT(gameplay_init());
#define SELECT(name) do {if(!suite || !strcmp(suite,#name))RUN(name);}while(0)
  SELECT(owner_scope);SELECT(state_transitions);SELECT(bytecode_rules);SELECT(global_cutscenes);SELECT(frames_normal);SELECT(frames_reload);SELECT(story_gates);SELECT(authority);SELECT(faults);
  gather();for(unsigned i=0;i<20;++i){printf("NPC/event rule %u coverage=%llu\n",i,(unsigned long long)covered[i]);if(!suite)ASSERT(covered[i]>0);}
  printf("ok lua_npcs_events_rom: %u cases; SHA256 %s\n",cases,gameplay_revision());gameplay_close();return 0;
}
