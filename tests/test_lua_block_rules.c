#include "unit.h"
#include "script/block_rules.h"

extern const unsigned char ages_block_rules_lua[];
extern const unsigned int ages_block_rules_lua_size;
static uint64_t checked;
static void exhaustive_contracts(void) {
  ASSERT(gameplay_init()); bool result;
  for (int a=0; a<256; ++a) {
    ASSERT(gameplay_cube_absent(a, &result));
    ASSERT_EQ(result, a == 0); ++checked;
  }
  for (int a=0; a<256; ++a) {
    ASSERT(gameplay_cube_active(a, &result));
    ASSERT_EQ(result, (a & 0x80) != 0); ++checked;
  }
  for (int a=0; a<256; ++a) for (int b=0; b<256; ++b) {
    ASSERT(gameplay_cube_match(a, b, &result));
    ASSERT_EQ(result, a == b); ++checked;
  }
  for (int a=0; a<256; ++a) {
    ASSERT(gameplay_block_has_replacement(a, &result));
    ASSERT_EQ(result, a != 0); ++checked;
  }
  for (int a=0; a<256; ++a) {
    ASSERT(gameplay_block_finishes_puzzle(a, &result));
    ASSERT_EQ(result, (a & 1) != 0); ++checked;
  }
  for (int a=0; a<256; ++a) {
    ASSERT(gameplay_sync_awake(a, &result));
    ASSERT_EQ(result, a != 0); ++checked;
  }
  for (int a=0; a<256; ++a) {
    ASSERT(gameplay_sync_eligible(a, &result));
    ASSERT_EQ(result, a != 0xda); ++checked;
  }
  for (int a=0; a<256; ++a) for (int b=0; b<256; ++b) {
    ASSERT(gameplay_water_exists(a, b, &result));
    ASSERT_EQ(result, a == 0 ? b == 0 : b != 0); ++checked;
  }
  for (int a=0; a<256; ++a) {
    ASSERT(gameplay_water_initial_side(a, &result));
    ASSERT_EQ(result, a == 0); ++checked;
  }
  for (int a=0; a<256; ++a) for (int b=0; b<256; ++b) {
    ASSERT(gameplay_water_direction_match(a, b, &result));
    ASSERT_EQ(result, a == b); ++checked;
  }
  for (int a=0; a<2; ++a) {
    ASSERT(gameplay_water_advance(a, &result));
    ASSERT_EQ(result, a != 0); ++checked;
  }
  for (int a=0; a<256; ++a) {
    ASSERT(gameplay_panel_opened(a, &result));
    ASSERT_EQ(result, a != 0); ++checked;
  }
  for (int a=0; a<256; ++a) {
    ASSERT(gameplay_panel_trigger(a, &result));
    ASSERT_EQ(result, (a & 0x80) != 0); ++checked;
  }
  for (int a=0; a<256; ++a) {
    ASSERT(gameplay_panel_right(a, &result));
    ASSERT_EQ(result, a != 0); ++checked;
  }
  for (int a=0; a<256; ++a) {
    ASSERT(gameplay_panel_move_tick(a, &result));
    ASSERT_EQ(result, (a & 0x80) != 0); ++checked;
  }
  for (int a=0; a<2; ++a) {
    ASSERT(gameplay_panel_advance(a, &result));
    ASSERT_EQ(result, a != 0); ++checked;
  }
  for (int a=0; a<256; ++a) {
    ASSERT(gameplay_tiles_end(a, &result));
    ASSERT_EQ(result, a == 0); ++checked;
  }
  for (int a=0; a<256; ++a) {
    ASSERT(gameplay_tiles_new_group(a, &result));
    ASSERT_EQ(result, a == 0xff); ++checked;
  }
  for (int a=0; a<256; ++a) for (int b=0; b<256; ++b) {
    ASSERT(gameplay_tiles_match(a, b, &result));
    ASSERT_EQ(result, a == b); ++checked;
  }
  for (int a=0; a<256; ++a) for (int b=0; b<256; ++b) {
    ASSERT(gameplay_puzzle_cube_position(a, b, &result));
    ASSERT_EQ(result, a == b); ++checked;
  }
  for (int a=0; a<256; ++a) for (int b=0; b<256; ++b) {
    ASSERT(gameplay_puzzle_cube_color(a, b, &result));
    ASSERT_EQ(result, a == b); ++checked;
  }
  for (int a=0; a<256; ++a) {
    ASSERT(gameplay_puzzle_cube_valid_color(a, &result));
    ASSERT_EQ(result, a < 3); ++checked;
  }
  for (int a=0; a<256; ++a) {
    ASSERT(gameplay_panels_torch_count(a, &result));
    ASSERT_EQ(result, a == 1); ++checked;
  }
  for (int a=0; a<256; ++a) {
    ASSERT(gameplay_panels_all_triggers(a, &result));
    ASSERT_EQ(result, a == 7); ++checked;
  }
  for (int a=0; a<256; ++a) {
    ASSERT(gameplay_panels_wait_opened(a, &result));
    ASSERT_EQ(result, (a & 0x80) == 0); ++checked;
  }
  for (int a=0; a<256; ++a) {
    ASSERT(gameplay_statue_placed(a, &result));
    ASSERT_EQ(result, a == 0x2a); ++checked;
  }
  for (int a=0; a<256; ++a) {
    ASSERT(gameplay_trap_route_open(a, &result));
    ASSERT_EQ(result, a == 0); ++checked;
  }
  for (int a=0; a<256; ++a) {
    ASSERT(gameplay_trap_skip_corner(a, &result));
    ASSERT_EQ(result, (a & 1) != 0); ++checked;
  }
  for (int a=0; a<256; ++a) {
    ASSERT(gameplay_trap_solid_tile(a, &result));
    ASSERT_EQ(result, a != 0); ++checked;
  }
  for (int a=0; a<256; ++a) {
    ASSERT(gameplay_trap_all_checked(a, &result));
    ASSERT_EQ(result, a == 0); ++checked;
  }
  for (int a=0; a<2; ++a) {
    ASSERT(gameplay_trap_advance(a, &result));
    ASSERT_EQ(result, a != 0); ++checked;
  }
  for (uint8_t state=0;state<3;++state) {
    uint8_t next=0; ASSERT(gameplay_sync_next_state(state,&next));
    ASSERT_EQ(next,state==1?2:1); ++checked;
  }
  ASSERT_EQ(checked, 398857);
  for (int r=0;r<GAMEPLAY_BLOCK_RULE_COUNT;++r) ASSERT(gameplay_block_rule_calls(r)>0);
  ASSERT(gameplay_memory() < 256*1024);
  gameplay_close();
}

static void failures_and_isolation(void) {
  const char *bodies[] = {"return 1", "return 0.0", "return nil", "return {}", "return", "return true,false",
    "while true do end", "local s='x'; for i=1,30 do s=s..s end; return true"};
  size_t capacity=ages_block_rules_lua_size+512; char *source=malloc(capacity); ASSERT(source);
  for (size_t i=0;i<sizeof bodies/sizeof *bodies;++i) {
    int n=snprintf(source,capacity,"local rules=(function()\n%.*s\nend)(); rules.cube_absent=function() %s end; return rules",
      (int)ages_block_rules_lua_size,(const char *)ages_block_rules_lua,bodies[i]);
    ASSERT(n>0 && (size_t)n<capacity);
    GameplaySources override={.block_rules=source,.block_rules_size=(size_t)n};
    ASSERT(gameplay_load_sources(&override)); bool out=true;
    ASSERT(!gameplay_cube_absent(0,&out)); ASSERT(out);
    ASSERT(strstr(gameplay_error(),"block_rules.cube_absent"));
    ASSERT_EQ(gameplay_block_rule_calls(GAMEPLAY_RULE_cube_absent),0);
    ASSERT(!gameplay_init());
  }
  int n=snprintf(source,capacity,"local rules=(function()\n%.*s\nend)(); rules.cube_absent=function(p) "
    "return p==0 and io==nil and os==nil and math==nil and _G==nil and shared==nil end; return rules",
    (int)ages_block_rules_lua_size,(const char *)ages_block_rules_lua);
  ASSERT(n>0 && (size_t)n<capacity);
  GameplaySources override={.block_rules=source,.block_rules_size=(size_t)n};
  ASSERT(gameplay_load_sources(&override)); bool out=false;
  ASSERT(gameplay_cube_absent(0,&out)); ASSERT(out);
  ASSERT(!gameplay_cube_absent(0,NULL));
  gameplay_close(); free(source);
}

static void invalid_sync_transitions(void) {
  const char *bodies[]={"return true","return 1.0","return 99","return -1","return 1,2","return nil","return 0","return 2"};
  size_t capacity=ages_block_rules_lua_size+256; char *source=malloc(capacity); ASSERT(source);
  for(size_t i=0;i<sizeof bodies/sizeof *bodies;++i) {
    int n=snprintf(source,capacity,"local rules=(function()\n%.*s\nend)(); rules.sync_next_state=function() %s end; return rules",
      (int)ages_block_rules_lua_size,(const char *)ages_block_rules_lua,bodies[i]);
    ASSERT(n>0 && (size_t)n<capacity);
    GameplaySources override={.block_rules=source,.block_rules_size=(size_t)n};
    ASSERT(gameplay_load_sources(&override)); uint8_t next=255;
    ASSERT(!gameplay_sync_next_state(0,&next)); ASSERT_EQ(next,255);
    ASSERT(strstr(gameplay_error(),"block_rules.sync_next_state"));
  }
  gameplay_close(); ASSERT(gameplay_init()); uint8_t next=255;
  ASSERT(!gameplay_sync_next_state(3,&next)); ASSERT_EQ(next,255);
  gameplay_close(); ASSERT(gameplay_init()); ASSERT(!gameplay_sync_next_state(0,NULL));
  gameplay_close(); free(source);
}

int main(void) {
  RUN(exhaustive_contracts); RUN(failures_and_isolation); RUN(invalid_sync_transitions);
  printf("ok lua_block_rules: %llu typed decisions; SHA256 %s\n",(unsigned long long)checked,gameplay_revision());
  return 0;
}
