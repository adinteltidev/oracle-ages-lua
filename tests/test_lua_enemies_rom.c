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
#include "script/enemies.h"
#include "game/features.h"

#define OBJECT 0xd240
static GB *g,*seed,*prepared,*expected,*engine_seed;
static GBSample *samples;
static RenderLineRegs render_seed;
static unsigned cases;
static uint64_t covered[GAMEPLAY_ENEMY_COUNT];
static void gather(void){for(unsigned i=0;i<GAMEPLAY_ENEMY_COUNT;++i)covered[i]+=gameplay_enemy_calls(i);}

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
static void enemy(uint8_t id,uint8_t subid){
 base(0,0);mem_wr(g,OBJECT,0);for(unsigned page=0xd0;page<=0xdf;++page)for(unsigned offset=0x80;offset<0xc0;++offset)g->wram[1][((page&15)<<8)+offset]=0;
 mem_wr(g,0xd280,1);mem_wr(g,0xd281,id);mem_wr(g,0xd282,subid);mem_wr(g,0xd280+OBJ_YH,0x58);mem_wr(g,0xd280+OBJ_XH,0x58);mem_wr(g,0xd2a9,0x40);
 wr16(g,0xd280+OBJ_RELATED1,0xd180);wr16(g,0xd280+OBJ_RELATED2,0xd180);mem_wr(g,0xd180+OBJ_ID,7);mem_wr(g,0xd180+OBJ_STATE,8);mem_wr(g,0xd180+OBJ_XH,0x48);mem_wr(g,0xd180+OBJ_YH,0x48);
 mem_wr(g,wNumEnemies,1);mem_wr(g,hActiveObjectType,ENEMY_BASE);mem_wr(g,hActiveObject,0xd2);g->d=0xd2;g->e=0x84;
}
static void octorok_frames(void){
 context="Octorok patrol/attack, four RNG seeds, state-by-state save/load and recreated VM";
 const unsigned frames=240;GB *trajectory=malloc(frames*sizeof *trajectory);uint64_t *hash=malloc(frames*sizeof *hash);ASSERT(trajectory && hash);
 char path[4096];ASSERT(oracles_temp_path(path,sizeof path));unsigned saves=0,reloaded_states=0;
 for(unsigned subid=0;subid<2;++subid)for(unsigned seed_id=0;seed_id<4;++seed_id)for(unsigned reload=0;reload<2;++reload){
  static const uint16_t seeds[]={0,1,0x7fff,0xffff};enemy(9,subid);*prepared=*g;
  for(int mode=0;mode<2;++mode){struct Fibers *fib=g->fib;fibers_reset(g);*g=*prepared;g->fib=fib;*render_line_regs(g)=render_seed;engine_context();gameplay_enable(mode!=0);mem_wr(g,hRng1,seeds[seed_id]&255);mem_wr(g,hRng2,seeds[seed_id]>>8);unsigned checkpoint=0;
   for(unsigned i=0;i<frames;++i){
    mem_wr(g,0xd000+OBJ_INVINCIBILITY_COUNTER,0x7f);
    if(reload && checkpoint<4 && i>=48+checkpoint*40 && threads_parked(g)){
     mem_wr(g,0xd284,8+checkpoint);mem_wr(g,0xd286,2);mem_wr(g,0xd2b0,2);
     ASSERT(oracles_save_boot_state(g,path));ASSERT(oracles_load_boot_state(g,path));gather();gameplay_close();ASSERT(gameplay_init());gameplay_enable(mode!=0);reloaded_states|=1u<<checkpoint;++checkpoint;++saves;
    }
    gb_run_frame(g);if(g->hung)fprintf(stderr,"octorok sub=%u seed=%u frame=%u reload=%u: %s\n",subid,seed_id,i,reload,gb_stop_reason);ASSERT(!g->hung);g->apu.ring_r=g->apu.ring_w;uint64_t h=fnv1a64_update(FNV1A64_INIT,(const unsigned char*)g->sample,sizeof *g->sample);if(!mode){trajectory[i]=*g;hash[i]=h;}else{ASSERT(!memcmp(&trajectory[i],g,sizeof *g));ASSERT_EQ(hash[i],h);}
   }if(reload)ASSERT_EQ(checkpoint,4);
  }++cases;
 }ASSERT_EQ(saves,64);ASSERT_EQ(reloaded_states,15);ASSERT_EQ(remove(path),0);printf("16 combat scenarios x 240 frames: four seeds, 64 saves/VM recreations, states 8-11\n");free(trajectory);free(hash);
}

static const struct {uint8_t id;const char *family;} catalog[]={
#include "lua_enemy_catalog.h"
};
static int only_family=-1;
static void families(void){
 context="Ages enemy families: same seed and 80 native frames, C and Lua";
 const unsigned frames=80;GB *trajectory=malloc(frames*sizeof *trajectory);uint64_t *hash=malloc(frames*sizeof *hash);ASSERT(trajectory && hash);
 for(unsigned index=0;index<sizeof catalog/sizeof *catalog;++index){
  uint8_t id=catalog[index].id;if(only_family>=0 && only_family!=id)continue;
  enemy(id,id==0x7c?5:0);*prepared=*g;uint64_t before=gameplay_enemy_owner_calls(id);
  for(int mode=0;mode<2;++mode){struct Fibers *fib=g->fib;fibers_reset(g);*g=*prepared;g->fib=fib;*render_line_regs(g)=render_seed;engine_context();gameplay_enable(mode!=0);
   mem_wr(g,hRng1,0x37);mem_wr(g,hRng2,0xb9);
   for(unsigned frame=0;frame<frames;++frame){gb_run_frame(g);if(g->hung)fprintf(stderr,"family %02x %s lua=%d frame=%u: %s\n",id,catalog[index].family,mode,frame,gb_stop_reason);ASSERT(!g->hung);ASSERT(!g->sample_overflow);g->apu.ring_r=g->apu.ring_w;uint64_t h=fnv1a64_update(FNV1A64_INIT,(const unsigned char*)g->sample,sizeof *g->sample);
    if(!mode){trajectory[frame]=*g;hash[frame]=h;}else{if(memcmp(&trajectory[frame],g,sizeof *g))fprintf(stderr,"family mismatch %02x %s frame=%u\n",id,catalog[index].family,frame);ASSERT(!memcmp(&trajectory[frame],g,sizeof *g));ASSERT_EQ(hash[frame],h);}
   }
  }
  uint64_t calls=gameplay_enemy_owner_calls(id)-before;printf("family %02x %s matched %u frames, calls=%llu final_state=%u\n",id,catalog[index].family,frames,(unsigned long long)calls,mem_rd(g,0xd284));ASSERT(calls>0);++cases;
 }free(trajectory);free(hash);
}
static void state_bytes(void){
 context="native state/substate boundaries in all byte values";
 for(unsigned state=0;state<256;++state)for(unsigned sub=0;sub<2;++sub)for(int mode=0;mode<2;++mode){enemy(9,0);gameplay_enable(mode!=0);mem_wr(g,0xd280+(sub?OBJ_SUBSTATE:OBJ_STATE),state);if(sub)ENTER(ecom_incSubstate_b0d);else ENTER(ecom_incState_b0d);ASSERT_EQ(mem_rd(g,0xd280+(sub?OBJ_SUBSTATE:OBJ_STATE)),(state+1)&255);compare(mode!=0);if(mode)++cases;}
}
static void override(const char *field,const char *body){
 extern const unsigned char ages_enemies_ai_lua[];extern const unsigned int ages_enemies_ai_lua_size;
 char *source=malloc(ages_enemies_ai_lua_size+1024);ASSERT(source);int n=snprintf(source,ages_enemies_ai_lua_size+1024,"local r=(function() %.*s end)();r.%s=function(...) %s end;return r",ages_enemies_ai_lua_size,ages_enemies_ai_lua,field,body);GameplaySources sources={.enemies=source,.enemies_size=n};gather();ASSERT(gameplay_load_sources(&sources));free(source);gameplay_enable(true);
}
static void authority(void){
 context="Lua decisions control native state, speed and actual Octorok attacks";
 enemy(9,0);mem_wr(g,0xd284,8);override("transition","local current=...;return current");ENTER(ecom_incState_b0d);ASSERT_EQ(mem_rd(g,0xd284),8);++cases;
 enemy(9,0);g->a=0x14;override("speed","return 0");ENTER(ecom_setSpeedAndState8AndVisible_b0d);ASSERT_EQ(mem_rd(g,0xd290),0);++cases;
 enemy(9,0);mem_wr(g,0xd284,8);mem_wr(g,0xd2b2,0);override("octorok_attack","return false");ENTER(octorok_state_08);ASSERT_EQ(mem_rd(g,0xd284),9);++cases;
 enemy(9,0);mem_wr(g,0xd284,8);mem_wr(g,0xd2b2,0);override("octorok_attack","return true");ENTER(octorok_state_08);ASSERT_EQ(mem_rd(g,0xd284),11);++cases;
 gather();ASSERT(gameplay_load_sources(NULL));
}
static void faults(void){
 context="invalid responses stop ten rules in native hooks";
 for(unsigned rule=0;rule<GAMEPLAY_ENEMY_COUNT;++rule){
  enemy(9,rule==GAMEPLAY_ENEMY_bit_zero?4:0);mem_wr(g,0xd286,1);mem_wr(g,0xd289,16);g->a=0x14;
  const char *field=strrchr(gameplay_enemy_rule_name(rule),'.')+1;override(field,"return nil");
  void(*hook)(GB*)=NULL;uint16_t pc=0;uint8_t bank=0;
#define CHOOSE(name) do {hook=name##_hook;pc=SYM(name);bank=SYMBANK(name);}while(0)
  switch(rule){
   case GAMEPLAY_ENEMY_condition:CHOOSE(octorok_state_09);break;
   case GAMEPLAY_ENEMY_transition:CHOOSE(ecom_incState_b0d);break;
   case GAMEPLAY_ENEMY_angle:CHOOSE(ecom_setRandomAngle_b0d);break;
   case GAMEPLAY_ENEMY_direction:CHOOSE(ecom_updateAnimationFromAngle_b0d);break;
   case GAMEPLAY_ENEMY_speed:CHOOSE(ecom_setSpeedAndState8AndVisible_b0d);break;
   case GAMEPLAY_ENEMY_octorok_attack:case GAMEPLAY_ENEMY_octorok_aim:CHOOSE(octorok_state_08);break;
   case GAMEPLAY_ENEMY_compare:case GAMEPLAY_ENEMY_bit_zero:CHOOSE(octorok_state_uninitialized);break;
   case GAMEPLAY_ENEMY_zero:CHOOSE(octorok_state_followingScentSeed);break;
   default:ASSERT(false);
  }
  g->sp=wThread0StackTop-2;wr16(g,g->sp,0);g->pc=pc;g->rom_bank=bank;mem_wr(g,hRomBank,bank);hook_run(g,hook,pc);
  if(!g->hung)fprintf(stderr,"enemy rule did not stop: %u %s\n",rule,field);ASSERT(g->hung);ASSERT(gameplay_error()[0]);++cases;
#undef CHOOSE
 }gather();ASSERT(gameplay_load_sources(NULL));
}
int main(int argc,char **argv){
 setvbuf(stdout,NULL,_IONBF,0);const char *rom_path=GAME_ROM_DIR "/Legend of Zelda, The - Oracle of Ages (USA, Australia).gbc";
 for(int i=1;i<argc;++i){if(i+1>=argc)return 2;if(!strcmp(argv[i],"--rom"))rom_path=argv[++i];else if(!strcmp(argv[i],"--tas-dir"))tas_dir=argv[++i];else if(!strcmp(argv[i],"--family"))only_family=(int)strtoul(argv[++i],NULL,0);else return 2;}
 size_t size;uint8_t *rom=oracles_read_file(rom_path,&size);if(!rom)SKIP("Ages ROM missing");char hash[41];sha1_hex(rom,size,hash);ASSERT(size==1048576 && !strcmp(hash,"880374fb978b18af4aa529e2e32f7ffb4d7dd2f4"));
 g=calloc(1,sizeof *g);seed=malloc(sizeof *seed);prepared=malloc(sizeof *prepared);expected=malloc(sizeof *expected);engine_seed=malloc(sizeof *engine_seed);samples=malloc(16*sizeof *samples);ASSERT(g && seed && prepared && expected && engine_seed && samples);
 gb_init(g);ASSERT(gb_load_rom(g,rom,size));g->cyctab=cyctab_alloc(rom,size);g->code_bits=assets_code_bits(rom,size);assets_zero_code(rom,size);gb_reset(g);hook_mode=HOOK_MODE_REPLACE;
 prepare_scene();gameplay_close();ASSERT(gameplay_init());if(only_family<0){RUN(octorok_frames);RUN(state_bytes);RUN(authority);RUN(faults);}RUN(families);gather();for(unsigned i=0;i<GAMEPLAY_ENEMY_COUNT;++i)printf("enemy rule %u calls=%llu\n",i,(unsigned long long)covered[i]);printf("ok lua_enemies_rom: %u cases; SHA256 %s\n",cases,gameplay_revision());gameplay_close();return 0;
}
