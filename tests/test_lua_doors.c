#include "unit.h"
#include "script/doors.h"
extern const unsigned char ages_doors_lua[];
extern const unsigned int ages_doors_lua_size;

static void exhaustive_contract(void) {
  ASSERT(gameplay_init()); ASSERT(gameplay_doors_enabled());
  for (unsigned a=0; a<256; ++a) {
    bool out; uint8_t state;
#define CHECK(name, expected) ASSERT(gameplay_door_##name(a,&out)); ASSERT_EQ(out,(expected))
    CHECK(switch_hook_busy,a==2); CHECK(palette_busy,a!=0);
    CHECK(somaria_tile,a==0xda); CHECK(trigger_active,a!=0);
    CHECK(collision_clear,a==0); CHECK(gate_open,a==0); CHECK(gate_switch,a!=0);
    CHECK(gate_animation_done,a==0); CHECK(no_enemies,a==0);
    CHECK(valid_dungeon,a!=255); CHECK(boss_door,a>=64); CHECK(small_key,a!=0);
    CHECK(reserved_free,a==0); CHECK(overworld_locked,a==0);
#undef CHECK
    ASSERT(gameplay_door_gate_state(a,&state)); ASSERT_EQ(state,(a&1)+1);
    for (unsigned b=0; b<256; ++b) {
      ASSERT(gameplay_door_same_position(a,b,&out)); ASSERT_EQ(out,a==b);
      ASSERT(gameplay_door_tile_matches(a,b,&out)); ASSERT_EQ(out,a==b);
      ASSERT(gameplay_door_torches_ready(a,b,&out)); ASSERT_EQ(out,a==b);
    }
  }
  for (int a=0; a<2; ++a) {
    bool out;
#define CHECK(name, expected) ASSERT(gameplay_door_##name(a,&out)); ASSERT_EQ(out,(expected))
    CHECK(open_collision,a); CHECK(close_collision,!a); CHECK(timer_elapsed,a);
    CHECK(minecart_riding,a); CHECK(minecart_collided,a); CHECK(gate_is_open,a);
    CHECK(link_clear,!a); CHECK(boss_key,a); CHECK(overworld_key,a);
#undef CHECK
  }
  for (unsigned a=0; a<4; ++a) {uint8_t state; ASSERT(gameplay_door_script_state(a,&state)); ASSERT_EQ(state,a);}
  for(int i=0;i<GAMEPLAY_DOOR_RULE_COUNT;++i) ASSERT(gameplay_door_calls(i)>0);
  GameplayMemoryStats before=gameplay_memory_stats();
  for(unsigned a=0;a<1000;++a) {bool out; ASSERT(gameplay_door_small_key(a,&out));}
  GameplayMemoryStats after=gameplay_memory_stats(); ASSERT_EQ(before.allocations,after.allocations);
  ASSERT(after.peak<256*1024);
  printf("28 typed rules; all byte pairs; heap=%zu peak=%zu; SHA256=%s\n",after.current,after.peak,gameplay_revision());
  gameplay_enable(false);ASSERT(!gameplay_doors_enabled());gameplay_enable(true);
}
static bool invoke(int rule,bool *boolean,uint8_t *byte) {
  switch(rule) {
#define DOOR_RULE1(name, a) case GAMEPLAY_DOOR_##name:return gameplay_door_##name(1,boolean);
#define DOOR_RULE2(name, a, b) case GAMEPLAY_DOOR_##name:return gameplay_door_##name(1,1,boolean);
#define DOOR_RULE_FLAG(name, a) case GAMEPLAY_DOOR_##name:return gameplay_door_##name(true,boolean);
#define DOOR_BYTE(name, a, low, high) case GAMEPLAY_DOOR_##name:return gameplay_door_##name(1,byte);
#include "script/doors.def"
#undef DOOR_RULE1
#undef DOOR_RULE2
#undef DOOR_RULE_FLAG
#undef DOOR_BYTE
    default:return false;
  }
}
static void invalid_results(void) {
  const char *bodies[]={"return -1","return 256","return 1.0","return nil","return {}","return","return true,false","while true do end","return '1'"};
  size_t cap=ages_doors_lua_size+1024;char *source=malloc(cap);ASSERT(source);
  for(int rule=0;rule<GAMEPLAY_DOOR_RULE_COUNT;++rule) for(unsigned i=0;i<sizeof bodies/sizeof *bodies;++i) {
    const char *field=strrchr(gameplay_door_rule_name(rule),'.')+1;
    int n=snprintf(source,cap,"local rules=(function() %.*s end)();rules.%s=function(...) %s end;return rules",(int)ages_doors_lua_size,ages_doors_lua,field,bodies[i]);
    ASSERT(n>0 && (size_t)n<cap);GameplaySources overrides={.doors=source,.doors_size=(size_t)n};ASSERT(gameplay_load_sources(&overrides));
    bool boolean=false;uint8_t byte=77;ASSERT(!invoke(rule,&boolean,&byte));ASSERT(strstr(gameplay_error(),field));ASSERT_EQ(byte,77);
    ASSERT(!gameplay_door_small_key(1,&boolean));
  }
  const char *fields[]={"gate_state","script_state"};const char *values[]={"0","4"};
  for(unsigned i=0;i<2;++i) {
    int n=snprintf(source,cap,"local rules=(function() %.*s end)();rules.%s=function()return %s end;return rules",(int)ages_doors_lua_size,ages_doors_lua,fields[i],values[i]);
    GameplaySources overrides={.doors=source,.doors_size=(size_t)n};ASSERT(gameplay_load_sources(&overrides));uint8_t out=77;
    ASSERT(!(i?gameplay_door_script_state(1,&out):gameplay_door_gate_state(1,&out)));ASSERT_EQ(out,77);
  }
  ASSERT(gameplay_load_sources(NULL));uint8_t out=77;ASSERT(!gameplay_door_script_state(4,&out));ASSERT_EQ(out,77);
  ASSERT(gameplay_load_sources(NULL));ASSERT(!gameplay_door_small_key(1,NULL));
  ASSERT_EQ(gameplay_door_calls(-1),0);ASSERT_EQ(gameplay_door_calls(GAMEPLAY_DOOR_RULE_COUNT),0);
  free(source);gameplay_close();ASSERT(gameplay_init());
}
static void sandbox(void) {
  const char *bad[]={"return io.open('x')","return os.execute('x')","local t={} while true do t[#t+1]={} end"};
  for(unsigned i=0;i<sizeof bad/sizeof *bad;++i) {GameplaySources s={.doors=bad[i],.doors_size=strlen(bad[i])};ASSERT(!gameplay_load_sources(&s));ASSERT(gameplay_error()[0]);}
  size_t cap=ages_doors_lua_size+64;char *source=malloc(cap);ASSERT(source);
  int n=snprintf(source,cap,"sentinel=88; %.*s",(int)ages_doors_lua_size,ages_doors_lua);
  const char *switches="return {changed=function()return sentinel==nil end,alternate=function()return false end}";
  GameplaySources s={.doors=source,.doors_size=(size_t)n,.switch_tiles=switches,.switch_tiles_size=strlen(switches)};ASSERT(gameplay_load_sources(&s));bool out=false;ASSERT(gameplay_decide(0,0,0,&out));ASSERT(out);
  free(source);gameplay_close();ASSERT(gameplay_init());
}
int main(void) { RUN(exhaustive_contract);RUN(invalid_results);RUN(sandbox);gameplay_close();return 0; }
