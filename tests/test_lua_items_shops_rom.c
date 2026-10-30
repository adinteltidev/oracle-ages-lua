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
#include "script/items_shops.h"
#include "game/features.h"
#include "lua_items_catalog.h"
void shopkeeperGetItemPrice_hook(GB *gb);
void shopkeeperCheckLinkHasItemAlready_hook(GB *gb);
void shopItemState3_hook(GB *gb);
void giveTreasure_body__applyParameter_hook(GB *gb);
void giveTreasure_body__mode8_hook(GB *gb);
void giveTreasure_body__mode4_hook(GB *gb);
void interactionLoadTreasureData_hook(GB *gb);
#define OBJECT 0xd240
static GB *g,*seed,*prepared,*expected,*engine_seed;
static GBSample *samples;
static RenderLineRegs render_seed;
static unsigned cases;
static uint64_t covered[15];
static void gather(void){for(unsigned i=0;i<GAMEPLAY_ITEMS_COUNT;++i)covered[i]+=gameplay_item_calls(i);for(unsigned i=0;i<GAMEPLAY_SHOPS_COUNT;++i)covered[6+i]+=gameplay_shop_calls(i);}
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

static uint8_t bcd(unsigned n){return (n/10)*16+n%10;}
static void actor(uint8_t id){base(1,0);mem_wr(g,OBJECT+OBJ_ID,id);}
static void currency(void){
  context="all balances 0..999 against all 21 rupee value selectors";
  for(unsigned price=0;price<=20;++price)for(unsigned balance=0;balance<=999;++balance)for(int mode=0;mode<2;++mode){
    actor(0x46);gameplay_enable(mode!=0);mem_wr(g,wNumRupees,bcd(balance%100));mem_wr(g,wNumRupees+1,bcd(balance/100));g->a=price;
    ENTER(cpRupeeValue);ASSERT_EQ(g->a,((bcd(balance/100)*256+bcd(balance%100))<(((unsigned)g->b<<8)|g->c))?1:0);compare(mode!=0);if(mode)++cases;
  }
}
static void capacity(void){
  context="inventory capacity, shields, seed satchel and already owned offers";
  const uint8_t products[]={0,1,2,3,4,0x0d,0x11,0x12,0x13};
  for(unsigned i=0;i<sizeof products;++i)for(unsigned amount=0;amount<256;++amount)for(int mode=0;mode<2;++mode){
    actor(0x46);gameplay_enable(mode!=0);mem_wr(g,wNumBombs,amount);mem_wr(g,wMaxBombs,0x30);mem_wr(g,wNumEmberSeeds,amount);mem_wr(g,0xc6aa,amount);mem_wr(g,0xc6ab,0x50);
    g->a=products[i];ENTER(shopkeeperCheckLinkHasItemAlready);compare(mode!=0);if(mode)++cases;
  }
}
static void upgrade_and_bcd(void){
  context="level upgrades and decimal saturation";
  for(unsigned kind=0;kind<2;++kind)for(unsigned value=0;value<256;++value)for(unsigned parameter=0;parameter<256;parameter+=17)for(int mode=0;mode<2;++mode){
    actor(0x60);gameplay_enable(mode!=0);g->d=0xc6;g->e=0xf0;g->c=parameter;mem_wr(g,0xc6f0,value);
    if(kind)ENTER(giveTreasure_body__mode4);else ENTER(giveTreasure_body__mode8);
    compare(mode!=0);if(mode)++cases;
  }
}
static void catalog(void){
  context="all valid Ages treasure object IDs and subids";
  for(unsigned id=0;id<99;++id)for(unsigned subid=0;subid<catalog_counts[id];++subid)for(int mode=0;mode<2;++mode){
    actor(0x60);gameplay_enable(mode!=0);mem_wr(g,OBJECT+OBJ_SUBID,id);mem_wr(g,OBJECT+OBJ_VAR03,subid);ENTER(interactionLoadTreasureData);
    ASSERT_EQ(mem_rd(g,OBJECT+0x30),id);uint8_t data[4];features_treasure_object(g,id,subid,data);ASSERT_EQ(mem_rd(g,OBJECT+OBJ_SUBID),data[3]);compare(mode!=0);if(mode)++cases;
  }
}
static unsigned mod_calls;static uint8_t mod_original,mod_subid;
static bool swap(int group,int room,uint8_t id,uint8_t subid,uint8_t *next,uint8_t *parameter){
  (void)group;(void)room;++mod_calls;mod_original=id;mod_subid=subid;*next=0x0d;*parameter=0;return true;
}
static void override(const char *module,const char *field,const char *body){
  extern const unsigned char ages_items_lua[],ages_shops_lua[];extern const unsigned int ages_items_lua_size,ages_shops_lua_size;
  const unsigned char *bytes=!strcmp(module,"items")?ages_items_lua:ages_shops_lua;unsigned size=!strcmp(module,"items")?ages_items_lua_size:ages_shops_lua_size;
  size_t cap=size+1024;char *s=malloc(cap);ASSERT(s);int n=snprintf(s,cap,"local r=(function() %.*s end)();r.%s=function(...) %s end;return r",size,bytes,field,body);ASSERT(n>0 && (size_t)n<cap);
  GameplaySources sources={0};if(!strcmp(module,"items")){sources.items=s;sources.items_size=n;}else{sources.shops=s;sources.shops_size=n;}gather();ASSERT(gameplay_load_sources(&sources));free(s);gameplay_enable(true);
}
static void mods_order(void){
  context="Lua base selection, original-location mod matching, mod priority";
  actor(0x60);gameplay_enable(false);mem_wr(g,OBJECT+OBJ_SUBID,5);mem_wr(g,OBJECT+OBJ_VAR03,2);features.treasure_swap=swap;mod_calls=0;
  ENTER(interactionLoadTreasureData);ASSERT_EQ(mod_calls,1);ASSERT_EQ(mod_original,5);ASSERT_EQ(mod_subid,2);compare(false);
  actor(0x60);mem_wr(g,OBJECT+OBJ_SUBID,5);mem_wr(g,OBJECT+OBJ_VAR03,2);override("items","reward_id","return 0x15");mod_calls=0;
  ENTER(interactionLoadTreasureData);ASSERT_EQ(mod_calls,1);ASSERT_EQ(mod_original,5);ASSERT_EQ(mod_subid,2);ASSERT_EQ(mem_rd(g,OBJECT+0x30),0x0d);compare(true);++cases;features.treasure_swap=NULL;
  actor(0x60);mem_wr(g,OBJECT+OBJ_SUBID,5);mem_wr(g,OBJECT+OBJ_VAR03,2);ENTER(interactionLoadTreasureData);ASSERT_EQ(mem_rd(g,OBJECT+0x30),0x15);ASSERT_EQ(mem_rd(g,OBJECT+OBJ_VAR03),0);++cases;
  gather();ASSERT(gameplay_load_sources(NULL));
}
static void all_parameter_modes(void){
  context="all sixteen resource application modes";
  for(unsigned kind=0;kind<16;++kind)for(unsigned parameter=0;parameter<4;++parameter)for(int mode=0;mode<2;++mode){
    actor(0x60);gameplay_enable(mode!=0);g->d=0xc6;g->e=0xf0;g->c=parameter;g->a=kind;mem_wr(g,0xc6f0,0x10);mem_wr(g,0xc6f1,0x30);mem_wr(g,wDungeonIndex,0);mem_wr(g,wSeedSatchelLevel,1);
    ENTER(giveTreasure_body__applyParameter);compare(mode!=0);if(mode)++cases;
  }
}
static void stock_and_delivery(void){
  context="shop pricing, exact balance and one delivery per state transition";
  for(unsigned product=0;product<=21;++product)for(unsigned owned=0;owned<2;++owned)for(int mode=0;mode<2;++mode){
    actor(0x46);gameplay_enable(mode!=0);mem_wr(g,wNumRupees,owned?0x99:0);mem_wr(g,wNumRupees+1,owned?9:0);g->a=product;ENTER(shopkeeperGetItemPrice);compare(mode!=0);if(mode)++cases;
  }
}
static void delivery_once(void){
  context="one debit/delivery, repeated updates while text is open";
  const uint8_t products[]={0,1,2,3,4,0x0d,0x11,0x12,0x13,0x14};
  for(unsigned i=0;i<sizeof products;++i)for(int mode=0;mode<2;++mode){
    actor(0x47);gameplay_enable(mode!=0);mem_wr(g,OBJECT+OBJ_STATE,3);mem_wr(g,OBJECT+OBJ_SUBID,products[i]);mem_wr(g,wNumRupees,0x99);mem_wr(g,wNumRupees+1,9);
    ENTER(interactionCode47);ASSERT_EQ(mem_rd(g,OBJECT+OBJ_STATE),5);uint16_t balance=rd16(g,wNumRupees);
    for(unsigned tick=0;tick<10;++tick){mem_wr(g,wTextIsActive,1);g->d=OBJECT>>8;ENTER(interactionCode47);ASSERT_EQ(rd16(g,wNumRupees),balance);ASSERT_EQ(mem_rd(g,OBJECT+OBJ_STATE),5);}
    compare(mode!=0);if(mode)++cases;
  }
}
static void engine_context(void){
  memcpy(g,engine_seed,offsetof(GB,rom));g->rom_bank=engine_seed->rom_bank;g->ram_bank=engine_seed->ram_bank;g->ie=engine_seed->ie;
  memcpy(g->io,engine_seed->io,sizeof g->io);memcpy(g->hram,engine_seed->hram,sizeof g->hram);g->input_at=NULL;g->input_ctx=NULL;g->joy=0;
}
static void stock_initialization(void){
  context="shop and Tokay stock, progression and linked-state flags";
  GB *trajectory=malloc(8*sizeof *trajectory);ASSERT(trajectory);
  for(unsigned family=0;family<2;++family)for(unsigned subid=0;subid<(family?7:22);++subid)for(unsigned flag=0;flag<2;++flag){
    // Tokay display variants 2/3 are produced from 0/1; their original var03 is 0/1.
    if(family && (subid==2 || subid==3))continue;
    actor(family?0x81:0x47);mem_wr(g,OBJECT+OBJ_STATE,0);mem_wr(g,OBJECT+OBJ_SUBID,subid);mem_wr(g,wInShop,2);
    mem_wr(g,wShieldLevel,flag?2:0);mem_wr(g,wRingBoxLevel,flag?2:1);mem_wr(g,wSeedSatchelLevel,flag?1:0);
    for(unsigned i=0;i<13;++i)mem_wr(g,wObtainedTreasureFlags+i,flag?255:0);mem_wr(g,wBoughtShopItems1,flag?255:0);mem_wr(g,wBoughtShopItems2,flag?255:0);*prepared=*g;
    for(int mode=0;mode<2;++mode){struct Fibers *fib=g->fib;fibers_reset(g);*g=*prepared;g->fib=fib;*render_line_regs(g)=render_seed;engine_context();gameplay_enable(mode!=0);
      for(unsigned frame=0;frame<8;++frame){gb_run_frame(g);if(g->hung)fprintf(stderr,"stock family=%u subid=%u frame=%u %s\n",family,subid,frame,gb_stop_reason);ASSERT(!g->hung);g->apu.ring_r=g->apu.ring_w;if(!mode)trajectory[frame]=*g;else ASSERT(!memcmp(&trajectory[frame],g,sizeof *g));}
    }++cases;
  }free(trajectory);
}
static void script_decisions(void){
  context="purchase outcome and text-option dispatch confined to shop owners";
  for(unsigned owner=0;owner<2;++owner)for(unsigned choice=0;choice<2;++choice)for(int mode=0;mode<2;++mode){
    actor(owner?0x46:0x13);gameplay_enable(mode!=0);g->h=0xcf;g->l=0xc0;
    mem_wr(g,0xcfc1,0x7a);mem_wr(g,0xcfc2,choice?255:1);g->sp=wThread0StackTop-4;wr16(g,g->sp,0xcfc0);wr16(g,g->sp+2,0);g->rom_bank=SYMBANK(scriptCmd_writeInteractionByte);mem_wr(g,hRomBank,g->rom_bank);g->pc=SYM(scriptCmd_writeInteractionByte);
    uint64_t before=gameplay_shop_calls(GAMEPLAY_SHOPS_purchase_outcome);hook_run(g,scriptCmd_writeInteractionByte_hook,SYM(scriptCmd_writeInteractionByte));ASSERT(!g->hung);ASSERT_EQ(gameplay_shop_calls(GAMEPLAY_SHOPS_purchase_outcome)-before,owner && mode?1:0);ASSERT_EQ(mem_rd(g,OBJECT+0x3a),choice?255:1);compare(mode!=0);if(mode)++cases;
  }
}
static void purchase_scripts(void){
  context="original shop purchase script: yes/no, funds and already-owned stock";
  GB *trajectory=malloc(100*sizeof *trajectory);ASSERT(trajectory);
  for(unsigned choice=0;choice<2;++choice)for(unsigned funds=0;funds<2;++funds)for(unsigned full=0;full<2;++full){
    unsigned reference_steps=0;
    for(int mode=0;mode<2;++mode){
      actor(0x46);gameplay_enable(mode!=0);mem_wr(g,OBJECT+OBJ_STATE,2);mem_wr(g,wLinkGrabState,1);wr16(g,w1Link_relatedObj2,0xd340);
      for(unsigned i=0;i<64;++i)mem_wr(g,0xd340+i,0);mem_wr(g,0xd340,1);mem_wr(g,0xd341,0x47);mem_wr(g,0xd342,4);mem_wr(g,0xd344,2);mem_wr(g,0xd345,1);
      mem_wr(g,wNumBombs,full?0x30:0);mem_wr(g,wMaxBombs,0x30);mem_wr(g,wNumRupees,funds?0x99:0);mem_wr(g,wNumRupees+1,funds?9:0);mem_wr(g,OBJECT+0x33,0x0e);mem_wr(g,OBJECT+0x30,0x80);ENTER(shopkeeperState2);
      unsigned step=0;for(;step<100;++step){mem_wr(g,wTextIsActive,0);mem_wr(g,wSelectedTextOption,choice);g->d=OBJECT>>8;ENTER(shopkeeperState3);
        if(!mode)trajectory[step]=*g;else ASSERT(!memcmp(&trajectory[step],g,sizeof *g));
        if(mem_rd(g,OBJECT+OBJ_STATE)==1)break;
      }
      if(step==100)fprintf(stderr,"purchase did not finish choice=%u funds=%u full=%u state=%u ptr=%04x\n",choice,funds,full,mem_rd(g,OBJECT+OBJ_STATE),rd16(g,OBJECT+OBJ_SCRIPT_PTR));ASSERT(step<100);
      if(!mode)reference_steps=step;else ASSERT_EQ(step,reference_steps);
      ASSERT_EQ(rd16(g,wNumRupees),funds?0x0999:0); // confirmation never debits; stock state 3 does.
      if(mode)++cases;
    }
  }free(trajectory);
}
static void d6_reward(void){
  context="D6 boss key puzzle: lever, previous reward, first failure and RNG outcomes";
  for(unsigned lever=0;lever<2;++lever)for(unsigned opened=0;opened<2;++opened)for(unsigned attempts=0;attempts<2;++attempts)for(unsigned rng=0;rng<4;++rng)for(int mode=0;mode<2;++mode){
    actor(0x72);gameplay_enable(mode!=0);mem_wr(g,OBJECT+OBJ_STATE,1);mem_wr(g,OBJECT+OBJ_COUNTER2,attempts);mem_wr(g,wLever1PullDistance,lever?0x80:0);mem_wr(g,wLever2PullDistance,0);mem_wr(g,hRng1,rng);mem_wr(g,hRng2,0);
    ENTER(getThisRoomFlags);mem_wr(g,((uint16_t)g->h<<8)|g->l,opened?0x20:0);g->d=OBJECT>>8;ENTER(miscPuzzles_subid00);
    if(lever && opened){ASSERT_EQ(mem_rd(g,OBJECT),0);ASSERT_EQ(mem_rd(g,wActiveTriggers),1);}
    if(lever && !opened && attempts && rng==0)ASSERT_EQ(mem_rd(g,OBJECT+OBJ_STATE),3);
    compare(mode!=0);if(mode)++cases;
  }
}
static void inventory_and_unique(void){
  context="18-slot inventory, repeated possession and ring capacity";
  for(unsigned occupied=0;occupied<=18;++occupied)for(unsigned repeat=0;repeat<2;++repeat)for(int mode=0;mode<2;++mode){
    actor(0x60);gameplay_enable(mode!=0);
    for(unsigned i=0;i<18;++i)mem_wr(g,wInventoryB+i,i<occupied?2:0);
    mem_wr(g,wInventoryB+18,0);mem_wr(g,hFF8B,0x15);ENTER(addTreasureToInventory);
    if(repeat){g->a=0x15;g->d=OBJECT>>8;ENTER(addTreasureToInventory);}
    unsigned count=0;for(unsigned i=0;i<19;++i)count+=mem_rd(g,wInventoryB+i)==0x15;ASSERT_EQ(count,1);
    compare(mode!=0);if(mode)++cases;
  }
}
static void lifecycle(bool serialize){
  context=serialize?"shop/item frame sequences with save/load and VM restart":"shop/item complete native frame sequences";
  const unsigned limit=64;GB *trajectory=malloc(limit*sizeof *trajectory);uint64_t *hashes=malloc(limit*sizeof *hashes);ASSERT(trajectory && hashes);
  char path[4096];ASSERT(oracles_temp_path(path,sizeof path));unsigned saves=0;
  // Three purchases, repeatable rupees, a unique shovel, Tokay feather and bracelet.
  for(unsigned family=0;family<7;++family){
    actor(family<3?0x47:family<5?0x60:0x81);mem_wr(g,OBJECT+OBJ_STATE,0);
    mem_wr(g,OBJECT+OBJ_SUBID,(const uint8_t[]){1,4,0x0d,0x28,0x15,0,1}[family]);
    mem_wr(g,wNumRupees,0x99);mem_wr(g,wNumRupees+1,9);mem_wr(g,wNumBombs,0);mem_wr(g,wMaxBombs,0x30);mem_wr(g,wInShop,2);
    if(family>=5){mem_wr(g,wSeedSatchelLevel,1);mem_wr(g,wNumMysterySeeds,0x20);mem_wr(g,wNumScentSeeds,0x20);mem_wr(g,wObtainedTreasureFlags+3,mem_rd(g,wObtainedTreasureFlags+3)|2);mem_wr(g,wObtainedTreasureFlags+4,mem_rd(g,wObtainedTreasureFlags+4)|0x12);}
    *prepared=*g;
    for(int mode=0;mode<2;++mode){struct Fibers *fib=g->fib;fibers_reset(g);*g=*prepared;g->fib=fib;*render_line_regs(g)=render_seed;engine_context();gameplay_enable(mode!=0);
      unsigned checkpoints=0;uint16_t after_purchase=0;bool paid=false;
      for(unsigned frame=0;frame<limit;++frame){
        if(family<3 && frame==12)g->wram[1][(OBJECT+OBJ_STATE)&0xfff]=3;
        if(family==3 || family==4){g->wram[1][(OBJECT+OBJ_XH)&0xfff]=g->wram[1][w1Link_xh&0xfff];g->wram[1][(OBJECT+OBJ_YH)&0xfff]=g->wram[1][w1Link_yh&0xfff];}
        if(family>=5 && frame>=8 && frame<10)g->wram[1][(OBJECT+0x31)&0xfff]=1;
        if(family>=5 || frame>=36)mem_wr(g,wTextIsActive,0);
        mem_wr(g,wSelectedTextOption,0);
        if(serialize && threads_parked(g) && ((frame>=11 && !(checkpoints&1)) || (frame>=14 && !(checkpoints&2)) || (frame>=40 && !(checkpoints&4)))){
          ASSERT(oracles_save_boot_state(g,path));ASSERT(oracles_load_boot_state(g,path));gather();gameplay_close();ASSERT(gameplay_init());gameplay_enable(mode!=0);
          checkpoints|=frame>=40?4:frame>=14?2:1;++saves;
        }
        gb_run_frame(g);if(g->hung)fprintf(stderr,"transaction frame failure family=%u frame=%u lua=%d %s\n",family,frame,mode,gb_stop_reason);ASSERT(!g->hung);ASSERT(!g->sample_overflow);g->apu.ring_r=g->apu.ring_w;
        if(family<3 && frame==12){mem_wr(g,IO_SVBK,1);after_purchase=rd16(g,wNumRupees);paid=true;}
        if(paid){mem_wr(g,IO_SVBK,1);ASSERT_EQ(rd16(g,wNumRupees),after_purchase);}
        uint64_t hash=fnv1a64_update(FNV1A64_INIT,(const unsigned char*)g->sample,sizeof *g->sample);
        if(!mode){trajectory[frame]=*g;hashes[frame]=hash;}else{if(memcmp(&trajectory[frame],g,sizeof *g))fprintf(stderr,"transaction mismatch family=%u frame=%u reload=%d\n",family,frame,serialize);ASSERT(!memcmp(&trajectory[frame],g,sizeof *g));ASSERT_EQ(hash,hashes[frame]);}
      }
      if(family>=5){mem_wr(g,IO_SVBK,1);uint16_t seeds=family==5?wNumMysterySeeds:wNumScentSeeds;fprintf(stderr,"Tokay family=%u seeds=%02x state=%u script=%04x\n",family,mem_rd(g,seeds),g->wram[1][(OBJECT+OBJ_STATE)&0xfff],rd16(g,OBJECT+OBJ_SCRIPT_PTR));ASSERT_EQ(mem_rd(g,seeds),0x10);}
      if(serialize)ASSERT_EQ(checkpoints,7);
    }++cases;
  }
  if(serialize){ASSERT(saves>0);ASSERT_EQ(remove(path),0);}printf("7 families x 64 frames, reload=%d saves=%u\n",serialize,saves);free(trajectory);free(hashes);
}
static void frames_normal(void){lifecycle(false);}
static void frames_reload(void){lifecycle(true);}
static void authority(void){
  context="valid Lua alternatives control inventory mode, comparison, funds and outcome";
  actor(0x60);override("items","compare","return 2");g->d=0xc6;g->e=0xf0;g->c=2;mem_wr(g,0xc6f0,1);ENTER(giveTreasure_body__mode8);ASSERT_EQ(mem_rd(g,0xc6f0),1);++cases;
  actor(0x60);override("items","parameter_mode","return 0");g->d=0xc6;g->e=0xf0;g->c=2;g->a=8;mem_wr(g,0xc6f0,1);ENTER(giveTreasure_body__applyParameter);ASSERT_EQ(mem_rd(g,0xc6f0),1);++cases;
  actor(0x46);override("shops","affordable","return false");mem_wr(g,wNumRupees,0x99);mem_wr(g,wNumRupees+1,9);g->a=1;ENTER(cpRupeeValue);ASSERT_EQ(g->a,1);ASSERT_EQ(rd16(g,wNumRupees),0x0999);++cases;
  gather();ASSERT(gameplay_load_sources(NULL));
}
static void failures(void){
  context="bad Lua responses stop real native decision points";
  const char *names[]={"compare","zero","bit_zero","carry","reward_id","parameter_mode","compare","zero","bit_zero","carry","reward_id","price_index","table_index","purchase_outcome","affordable"};
  for(unsigned rule=0;rule<15;++rule){
    actor(rule<6?0x60:0x46);override(rule<6?"items":"shops",names[rule],"return nil");
    if(rule==8){mem_wr(g,OBJECT+OBJ_ID,0x47);mem_wr(g,OBJECT+OBJ_STATE,0);mem_wr(g,OBJECT+OBJ_SUBID,2);mem_wr(g,wInShop,2);mem_wr(g,wBoughtShopItems1,255);mem_wr(g,wBoughtShopItems2,255);engine_context();
      for(unsigned frame=0;frame<8 && !g->hung;++frame)gb_run_frame(g);ASSERT(g->hung);ASSERT(gameplay_error()[0]);++cases;continue;}
    void(*hook)(GB*)=NULL;uint16_t pc=0;uint8_t bank=0;
#define CHOOSE(name) do {hook=name##_hook;pc=SYM(name);bank=SYMBANK(name);}while(0)
    if(rule==0){g->d=0xc6;g->e=0xf0;g->c=2;mem_wr(g,0xc6f0,1);CHOOSE(giveTreasure_body__mode8);}
    if(rule==1){g->a=0x60;CHOOSE(checkTreasureObtained);}
    if(rule==2){g->a=0x28;g->c=1;CHOOSE(giveTreasure);}
    if(rule==3){g->d=0xc6;g->e=0xf0;g->c=0x99;mem_wr(g,0xc6f0,0x99);CHOOSE(giveTreasure_body__mode4);}
    if(rule==4){mem_wr(g,OBJECT+OBJ_SUBID,5);CHOOSE(interactionLoadTreasureData);}
    if(rule==5){g->d=0xc6;g->e=0xf0;g->a=8;g->c=2;CHOOSE(giveTreasure_body__applyParameter);}
    if(rule==6 || rule==7 || rule==9){g->a=rule==9?3:0;CHOOSE(shopkeeperCheckLinkHasItemAlready);}
    if(rule==8){mem_wr(g,OBJECT+OBJ_ID,0x47);mem_wr(g,OBJECT+OBJ_SUBID,0);mem_wr(g,wInShop,2);mem_wr(g,wRingBoxLevel,2);CHOOSE(shopItemState0);}
    if(rule==10){mem_wr(g,OBJECT+OBJ_ID,0x47);mem_wr(g,OBJECT+OBJ_SUBID,4);mem_wr(g,OBJECT+OBJ_STATE,3);mem_wr(g,wNumRupees,0x99);mem_wr(g,wNumRupees+1,9);CHOOSE(shopItemState3);}
    if(rule==11 || rule==14){g->a=1;CHOOSE(cpRupeeValue);}
    if(rule==12 || rule==13){g->h=0xcf;g->l=0xc0;mem_wr(g,0xcfc1,rule==12?0x77:0x7a);mem_wr(g,0xcfc2,1);mem_wr(g,OBJECT+0x37,0);if(rule==12)CHOOSE(scriptCmd_jumpTable);else CHOOSE(scriptCmd_writeInteractionByte);}
    g->sp=wThread0StackTop-(rule==12 || rule==13?4:2);if(rule==12 || rule==13){wr16(g,g->sp,0xcfc0);wr16(g,g->sp+2,0);}else wr16(g,g->sp,0);
    g->pc=pc;g->rom_bank=bank;mem_wr(g,hRomBank,bank);hook_run(g,hook,pc);
    if(!g->hung)fprintf(stderr,"rule did not stop %u %s\n",rule,names[rule]);ASSERT(g->hung);ASSERT(gameplay_error()[0]);++cases;
#undef CHOOSE
  }
  gather();ASSERT(gameplay_load_sources(NULL));
}
#define SELECT(name) do {if(!suite || !strcmp(suite,#name))RUN(name);}while(0)
int main(int argc,char **argv){
  setvbuf(stdout,NULL,_IONBF,0);const char *suite=NULL;
  const char *rom_path=GAME_ROM_DIR "/Legend of Zelda, The - Oracle of Ages (USA, Australia).gbc";
  for(int i=1;i<argc;++i){if(i+1>=argc)return 2;if(!strcmp(argv[i],"--rom"))rom_path=argv[++i];else if(!strcmp(argv[i],"--tas-dir"))tas_dir=argv[++i];else if(!strcmp(argv[i],"--suite"))suite=argv[++i];else return 2;}
  size_t size;uint8_t *rom=oracles_read_file(rom_path,&size);if(!rom)SKIP("Ages ROM missing");char hash[41];sha1_hex(rom,size,hash);ASSERT(size==1048576 && !strcmp(hash,"880374fb978b18af4aa529e2e32f7ffb4d7dd2f4"));
  g=calloc(1,sizeof *g);seed=malloc(sizeof *seed);prepared=malloc(sizeof *prepared);expected=malloc(sizeof *expected);engine_seed=malloc(sizeof *engine_seed);samples=malloc(16*sizeof *samples);ASSERT(g && seed && prepared && expected && engine_seed && samples);
  gb_init(g);ASSERT(gb_load_rom(g,rom,size));g->cyctab=cyctab_alloc(rom,size);g->code_bits=assets_code_bits(rom,size);assets_zero_code(rom,size);gb_reset(g);hook_mode=HOOK_MODE_REPLACE;
  prepare_scene();ASSERT(gameplay_init());SELECT(currency);SELECT(capacity);SELECT(upgrade_and_bcd);SELECT(catalog);SELECT(all_parameter_modes);SELECT(stock_and_delivery);SELECT(mods_order);SELECT(delivery_once);SELECT(stock_initialization);SELECT(script_decisions);SELECT(purchase_scripts);SELECT(d6_reward);SELECT(inventory_and_unique);SELECT(frames_normal);SELECT(frames_reload);SELECT(authority);gather();if(!suite){for(unsigned i=0;i<15;++i){printf("rule %u coverage=%llu\n",i,(unsigned long long)covered[i]);ASSERT(covered[i]>0);}}SELECT(failures);
  printf("ok lua_items_shops_rom: %u cases; SHA256 %s\n",cases,gameplay_revision());gameplay_close();return 0;
}
