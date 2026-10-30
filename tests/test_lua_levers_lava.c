#include "unit.h"
#include "script/levers_lava.h"
extern const unsigned char ages_levers_lava_lua[];
extern const unsigned int ages_levers_lava_lua_size;

static void exhaustive_contract(void) {
  ASSERT(gameplay_init()); ASSERT(gameplay_levers_lava_enabled());
  unsigned decisions=0;
  for(unsigned value=0;value<256;++value) {
    bool out;uint8_t tile;
    ASSERT(gameplay_lava_fully_pulled(value,&out));ASSERT_EQ(out,(value&128)!=0);
    ASSERT(gameplay_lava_released(value,&out));ASSERT_EQ(out,value==0);
    ASSERT(gameplay_lava_has_tile(value,&out));ASSERT_EQ(out,value!=0);
    ASSERT(gameplay_lava_source_segment(value,&out));ASSERT_EQ(out,value<12);
    ASSERT(gameplay_lava_floor_tile(2,value,&tile));ASSERT_EQ(tile,1);
    ASSERT(gameplay_lava_floor_tile(4,value,&tile));ASSERT_EQ(tile,0x61+(value&3));decisions+=6;
  }
  for(int flag=0;flag<2;++flag) {
    bool out;uint8_t delta;
    ASSERT(gameplay_lava_timer_elapsed(flag,&out));ASSERT_EQ(out,flag);
    ASSERT(gameplay_lava_source_found(flag,&out));ASSERT_EQ(out,flag);
    ASSERT(gameplay_lava_source_delta(flag,&delta));ASSERT_EQ(delta,flag?250:6);decisions+=3;
  }
  for(int state=0;state<5;++state) {uint8_t next;ASSERT(gameplay_lava_next_state(state,&next));ASSERT_EQ(next,state==4?1:state+1);++decisions;}
  ASSERT_EQ(decisions,1547);
  for(int i=0;i<GAMEPLAY_LAVA_RULE_COUNT;++i) ASSERT(gameplay_lava_calls(i)>0);
  GameplayMemoryStats stats=gameplay_memory_stats();ASSERT(stats.peak<256*1024);
  printf("%u typed decisions; heap=%zu peak=%zu SHA256=%s\n",decisions,stats.current,stats.peak,gameplay_revision());
  gameplay_enable(false);ASSERT(!gameplay_levers_lava_enabled());gameplay_enable(true);
}
static bool invoke(int rule,bool *boolean,uint8_t *byte) {
  switch(rule) {
    case 0:return gameplay_lava_fully_pulled(128,boolean);
    case 1:return gameplay_lava_released(0,boolean);
    case 2:return gameplay_lava_timer_elapsed(true,boolean);
    case 3:return gameplay_lava_has_tile(1,boolean);
    case 4:return gameplay_lava_source_found(true,boolean);
    case 5:return gameplay_lava_source_segment(1,boolean);
    case 6:return gameplay_lava_source_delta(false,byte);
    case 7:return gameplay_lava_next_state(0,byte);
    case 8:return gameplay_lava_floor_tile(2,0,byte);
    default:return false;
  }
}
static void invalid_results(void) {
  const char *bodies[]={"return 0","return -1","return 256","return 1.0","return nil","return {}","return","return true,false","while true do end"};
  size_t capacity=ages_levers_lava_lua_size+1024;char *source=malloc(capacity);ASSERT(source);
  for(int rule=0;rule<GAMEPLAY_LAVA_RULE_COUNT;++rule) for(unsigned i=0;i<sizeof bodies/sizeof *bodies;++i) {
    const char *name=strrchr(gameplay_lava_rule_name(rule),'.')+1;
    int n=snprintf(source,capacity,"local rules=(function() %.*s end)();rules.%s=function() %s end;return rules",(int)ages_levers_lava_lua_size,ages_levers_lava_lua,name,bodies[i]);ASSERT(n>0 && (size_t)n<capacity);
    GameplaySources override={.levers_lava=source,.levers_lava_size=(size_t)n};ASSERT(gameplay_load_sources(&override));
    bool boolean=true;uint8_t byte=0xa5;ASSERT(!invoke(rule,&boolean,&byte));ASSERT(boolean);ASSERT_EQ(byte,0xa5);ASSERT(strstr(gameplay_error(),"ages/levers_lava."));ASSERT_EQ(gameplay_lava_calls(rule),0);
  }
  free(source);
  for(int rule=0;rule<GAMEPLAY_LAVA_RULE_COUNT;++rule) {gameplay_close();ASSERT(gameplay_init());ASSERT(!invoke(rule,NULL,NULL));}
  gameplay_close();ASSERT(gameplay_init());uint8_t next=0xa5;ASSERT(!gameplay_lava_next_state(5,&next));ASSERT_EQ(next,0xa5);
  gameplay_close();ASSERT(gameplay_init());ASSERT(!gameplay_lava_floor_tile(3,0,&next));ASSERT_EQ(next,0xa5);
  ASSERT_EQ(gameplay_lava_calls(-1),0);ASSERT_EQ(gameplay_lava_calls(GAMEPLAY_LAVA_RULE_COUNT),0);
}
static void limits_and_isolation(void) {
  const char loop[]="while true do end";GameplaySources override={.levers_lava=loop,.levers_lava_size=sizeof loop-1};ASSERT(!gameplay_load_sources(&override));ASSERT(strstr(gameplay_error(),"instruction limit"));
  size_t size=300000;char *source=malloc(size+64);ASSERT(source);memcpy(source,"local s='",9);memset(source+9,'x',size);memcpy(source+9+size,"';return {}",12);
  override=(GameplaySources){.levers_lava=source,.levers_lava_size=9+size+11};ASSERT(!gameplay_load_sources(&override));ASSERT(gameplay_memory_stats().peak<=256*1024);free(source);
  bool out=true;ASSERT(!gameplay_lava_released(0,&out));ASSERT(out);
  const char switches[]="leak=123;return {changed=function()return true end,alternate=function()return false end}";
  size_t capacity=ages_levers_lava_lua_size+512;source=malloc(capacity);ASSERT(source);
  int n=snprintf(source,capacity,"local rules=(function() %.*s end)();rules.released=function()return leak==nil and _G==nil and os==nil and io==nil and math==nil and debug==nil end;return rules",(int)ages_levers_lava_lua_size,ages_levers_lava_lua);
  override=(GameplaySources){.switch_tiles=switches,.switch_tiles_size=sizeof switches-1,.levers_lava=source,.levers_lava_size=(size_t)n};ASSERT(gameplay_load_sources(&override));ASSERT(gameplay_lava_released(0,&out));ASSERT(out);free(source);
  gameplay_close();ASSERT(gameplay_init());
}
int main(void) {RUN(exhaustive_contract);RUN(invalid_results);RUN(limits_and_isolation);gameplay_close();puts("ok lua_levers_lava");return 0;}
