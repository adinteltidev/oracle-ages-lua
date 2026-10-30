#include "script/gameplay.h"
#include "script/block_rules.h"
#include "script/colored_cube.h"
#include "script/levers_lava.h"
#include "script/doors.h"
#include "lua.h"
#include "lauxlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const unsigned char ages_switch_tiles_lua[], ages_block_puzzles_lua[];
extern const unsigned int ages_switch_tiles_lua_size, ages_block_puzzles_lua_size;
extern const unsigned char ages_block_rules_lua[];
extern const unsigned int ages_block_rules_lua_size;
extern const unsigned char ages_colored_cube_lua[];
extern const unsigned int ages_colored_cube_lua_size;
extern const unsigned char ages_levers_lava_lua[];
extern const unsigned int ages_levers_lava_lua_size;
extern const unsigned char ages_doors_lua[];
extern const unsigned int ages_doors_lua_size;
#define CAP (256u * 1024u)
enum { SWITCH_CHANGED, SWITCH_ALTERNATE, BLOCK_NEXT_STATE, BLOCK_RULE_FIRST,
       COLORED_CUBE_FIRST = BLOCK_RULE_FIRST + GAMEPLAY_BLOCK_RULE_COUNT,
       LAVA_FIRST = COLORED_CUBE_FIRST + GAMEPLAY_COLORED_CUBE_RULE_COUNT,
       DOOR_FIRST = LAVA_FIRST + GAMEPLAY_LAVA_RULE_COUNT,
       RULE_COUNT = DOOR_FIRST + GAMEPLAY_DOOR_RULE_COUNT };
static const char *const rule_names[RULE_COUNT] = {
  "ages/switch_tiles.changed", "ages/switch_tiles.alternate", "ages/block_puzzles.next_state",
#define BLOCK_RULE1(name, a) "ages/block_rules." #name,
#define BLOCK_RULE2(name, a, b) "ages/block_rules." #name,
#define BLOCK_RULE_FLAG(name, a) "ages/block_rules." #name,
#include "script/block_rules.def"
#undef BLOCK_RULE1
#undef BLOCK_RULE2
#undef BLOCK_RULE_FLAG
  "ages/block_rules.sync_next_state",
  "ages/colored_cube.present", "ages/colored_cube.visible", "ages/colored_cube.palette_offset",
  "ages/levers_lava.fully_pulled", "ages/levers_lava.released", "ages/levers_lava.timer_elapsed",
  "ages/levers_lava.has_tile", "ages/levers_lava.source_found", "ages/levers_lava.source_segment",
  "ages/levers_lava.source_delta", "ages/levers_lava.next_state", "ages/levers_lava.floor_tile",
#define DOOR_RULE1(name, a) "ages/doors." #name,
#define DOOR_RULE2(name, a, b) "ages/doors." #name,
#define DOOR_RULE_FLAG(name, a) "ages/doors." #name,
#define DOOR_BYTE(name, a, low, high) "ages/doors." #name,
#include "script/doors.def"
#undef DOOR_RULE1
#undef DOOR_RULE2
#undef DOOR_RULE_FLAG
#undef DOOR_BYTE
};
static lua_State *vm;
static size_t used;
static size_t peak;
static uint64_t allocations;
static bool enabled = true, attempted;
static int refs[RULE_COUNT], budget;
static uint64_t calls[RULE_COUNT], block_calls[4];
static char error[256];

static void *allocate(void *ctx, void *ptr, size_t old, size_t size) {
  (void)ctx;
  if (!ptr) old = 0;
  if (!size) { free(ptr); used -= old; return NULL; }
  if (size > old && size - old > CAP - used) return NULL;
  void *p = realloc(ptr, size);
  if (p) {
    used = used - old + size;
    if (used > peak) peak = used;
    ++allocations;
  }
  return p;
}

static void limit(lua_State *L, lua_Debug *ar) {
  (void)ar;
  if (--budget <= 0) luaL_error(L, "gameplay instruction limit");
}

static bool reject(const char *rule, const char *reason) {
  snprintf(error, sizeof error, "%s: %s", rule, reason);
  if (vm) lua_settop(vm, 0);
  return false;
}

static bool failed(const char *rule) {
  // Never invoke a script's tostring metamethod while reporting an error.
  const char *s = lua_type(vm, -1) == LUA_TSTRING ? lua_tostring(vm, -1) : "script failure";
  return reject(rule, s);
}

void gameplay_close(void) {
  if (vm) lua_close(vm);
  vm = NULL; used = 0; peak = 0; allocations = 0; attempted = false;
  memset(calls, 0, sizeof calls); memset(block_calls, 0, sizeof block_calls);
  error[0] = 0;
}

static void load_module(lua_State *L, const char *source, size_t length,
                        const char *name, int first, int count) {
  if (luaL_loadbufferx(L, source, length, name, "t") != LUA_OK) lua_error(L);
  // Each module has its own empty environment: no shared globals or stdlibs.
  lua_newtable(L);
  lua_setupvalue(L, -2, 1);
  lua_call(L, 0, 1);
  if (!lua_istable(L, -1)) luaL_error(L, "%s must return a table", name);
  for (int i = first; i < first + count; ++i) {
    const char *field = strrchr(rule_names[i], '.') + 1;
    lua_pushstring(L, field); lua_rawget(L, -2);
    if (!lua_isfunction(L, -1)) luaL_error(L, "missing %s", rule_names[i]);
    refs[i] = luaL_ref(L, LUA_REGISTRYINDEX);
  }
  lua_pop(L, 1);
}

// All initialization that may allocate runs inside the outer protected call.
static int initialize(lua_State *L) {
  const GameplaySources *sources = lua_touserdata(L, 1);
  load_module(L, sources && sources->switch_tiles ? sources->switch_tiles : (const char *)ages_switch_tiles_lua,
    sources && sources->switch_tiles ? sources->switch_tiles_size : ages_switch_tiles_lua_size,
    "ages/switch_tiles.lua", SWITCH_CHANGED, 2);
  load_module(L, sources && sources->block_puzzles ? sources->block_puzzles : (const char *)ages_block_puzzles_lua,
    sources && sources->block_puzzles ? sources->block_puzzles_size : ages_block_puzzles_lua_size,
    "ages/block_puzzles.lua", BLOCK_NEXT_STATE, 1);
  load_module(L, sources && sources->block_rules ? sources->block_rules : (const char *)ages_block_rules_lua,
    sources && sources->block_rules ? sources->block_rules_size : ages_block_rules_lua_size,
    "ages/block_rules.lua", BLOCK_RULE_FIRST, GAMEPLAY_BLOCK_RULE_COUNT);
  load_module(L, sources && sources->colored_cube ? sources->colored_cube : (const char *)ages_colored_cube_lua,
    sources && sources->colored_cube ? sources->colored_cube_size : ages_colored_cube_lua_size,
    "ages/colored_cube.lua", COLORED_CUBE_FIRST, GAMEPLAY_COLORED_CUBE_RULE_COUNT);
  load_module(L, sources && sources->levers_lava ? sources->levers_lava : (const char *)ages_levers_lava_lua,
    sources && sources->levers_lava ? sources->levers_lava_size : ages_levers_lava_lua_size,
    "ages/levers_lava.lua", LAVA_FIRST, GAMEPLAY_LAVA_RULE_COUNT);
  load_module(L, sources && sources->doors ? sources->doors : (const char *)ages_doors_lua,
    sources && sources->doors ? sources->doors_size : ages_doors_lua_size,
    "ages/doors.lua", DOOR_FIRST, GAMEPLAY_DOOR_RULE_COUNT);
  if (used > CAP - 16384) luaL_error(L, "gameplay initialization memory limit");
  if (!lua_checkstack(L, 16)) luaL_error(L, "gameplay stack limit");
  return 0;
}

bool gameplay_load_sources(const GameplaySources *sources) {
  gameplay_close(); attempted = true;
  vm = lua_newstate(allocate, NULL);
  if (!vm) return reject("gameplay", "cannot allocate VM");
  budget = 100; lua_sethook(vm, limit, LUA_MASKCOUNT, 100);
  lua_pushcfunction(vm, initialize); lua_pushlightuserdata(vm, (void *)sources);
  if (lua_pcall(vm, 1, 0, 0) != LUA_OK) return failed("gameplay initialization");
  return true;
}

bool gameplay_load(const char *source, size_t length) {
  GameplaySources sources = {.switch_tiles = source, .switch_tiles_size = length};
  return gameplay_load_sources(&sources);
}

bool gameplay_init(void) {
  if (attempted) return vm && !error[0];
  return gameplay_load_sources(NULL);
}

void gameplay_enable(bool value) { enabled = value; }
bool gameplay_is_enabled(void) { return enabled; }

static bool call_rule(int rule, const uint8_t *args, int count) {
  if (!gameplay_init()) return false;
  lua_rawgeti(vm, LUA_REGISTRYINDEX, refs[rule]);
  for (int i = 0; i < count; ++i) lua_pushinteger(vm, args[i]);
  budget = 100;
  if (lua_pcall(vm, count, LUA_MULTRET, 0) != LUA_OK) return failed(rule_names[rule]);
  if (lua_gettop(vm) != 1) return reject(rule_names[rule], "expected exactly one result");
  return true;
}

bool gameplay_decide(int rule, uint8_t a, uint8_t b, bool *out) {
  if (rule < SWITCH_CHANGED || rule > SWITCH_ALTERNATE || !out)
    return reject("switch_tiles", "invalid decision arguments");
  if (!enabled) { *out = rule == SWITCH_CHANGED ? a != b : (a & b) != 0; return true; }
  uint8_t args[] = {a, b};
  if (!call_rule(rule, args, 2)) return false;
  if (!lua_isboolean(vm, -1)) return reject(rule_names[rule], "expected boolean result");
  *out = lua_toboolean(vm, -1); lua_pop(vm, 1); ++calls[rule];
  return true;
}

bool gameplay_block_step(const GameplayBlockInput *in, GameplayBlockOutput *out) {
  if (!in || !out || in->state < GAMEPLAY_BLOCK_INITIALIZE || in->state > GAMEPLAY_BLOCK_WAIT_DELAY)
    return reject(rule_names[BLOCK_NEXT_STATE], "invalid block state or arguments");
  uint8_t args[] = {(uint8_t)in->state, 0, 0};
  int next;
  switch (in->state) {
    case GAMEPLAY_BLOCK_INITIALIZE: next = GAMEPLAY_BLOCK_WAIT_RELEASE; break;
    case GAMEPLAY_BLOCK_WAIT_RELEASE:
      args[1] = in->input.release.threshold; args[2] = in->input.release.enemies;
      next = args[1] >= args[2] ? GAMEPLAY_BLOCK_WAIT_MOVED : GAMEPLAY_BLOCK_WAIT_RELEASE;
      break;
    case GAMEPLAY_BLOCK_WAIT_MOVED:
      args[1] = in->input.tile.previous; args[2] = in->input.tile.current;
      next = args[1] != args[2] ? GAMEPLAY_BLOCK_WAIT_DELAY : GAMEPLAY_BLOCK_WAIT_MOVED;
      break;
    case GAMEPLAY_BLOCK_WAIT_DELAY:
      args[1] = in->input.delay_elapsed ? 1 : 0;
      next = args[1] ? GAMEPLAY_BLOCK_FINISH : GAMEPLAY_BLOCK_WAIT_DELAY;
      break;
    default: return false; // validated above
  }
  if (enabled) {
    if (!call_rule(BLOCK_NEXT_STATE, args, 3)) return false;
    if (!lua_isinteger(vm, -1)) return reject(rule_names[BLOCK_NEXT_STATE], "expected integer state");
    lua_Integer result = lua_tointeger(vm, -1);
    int minimum = in->state == GAMEPLAY_BLOCK_INITIALIZE ? GAMEPLAY_BLOCK_WAIT_RELEASE : in->state;
    int maximum = in->state + 1;
    if (result < minimum || result > maximum)
      return reject(rule_names[BLOCK_NEXT_STATE], "invalid state transition");
    next = (int)result; lua_pop(vm, 1);
    ++calls[BLOCK_NEXT_STATE]; ++block_calls[in->state];
  }
  out->next = (GameplayBlockState)next;
  return true;
}

static bool block_rule_boolean(GameplayBlockRule rule, const uint8_t *args, int count, bool *out) {
  if (!out || rule < 0 || rule >= GAMEPLAY_BLOCK_RULE_COUNT)
    return reject("block_rules", "invalid typed rule arguments");
  int index = BLOCK_RULE_FIRST + rule;
  if (!call_rule(index, args, count)) return false;
  if (!lua_isboolean(vm, -1)) return reject(rule_names[index], "expected boolean result");
  *out = lua_toboolean(vm, -1); lua_pop(vm, 1); ++calls[index];
  return true;
}
#define BLOCK_RULE1(name, a) bool gameplay_##name(uint8_t a, bool *out) { \
  uint8_t args[] = {a}; return block_rule_boolean(GAMEPLAY_RULE_##name, args, 1, out); }
#define BLOCK_RULE2(name, a, b) bool gameplay_##name(uint8_t a, uint8_t b, bool *out) { \
  uint8_t args[] = {a, b}; return block_rule_boolean(GAMEPLAY_RULE_##name, args, 2, out); }
#define BLOCK_RULE_FLAG(name, a) bool gameplay_##name(bool a, bool *out) { \
  uint8_t args[] = {a ? 1 : 0}; return block_rule_boolean(GAMEPLAY_RULE_##name, args, 1, out); }
#include "script/block_rules.def"
#undef BLOCK_RULE1
#undef BLOCK_RULE2
#undef BLOCK_RULE_FLAG
uint64_t gameplay_block_rule_calls(GameplayBlockRule rule) {
  return rule >= 0 && rule < GAMEPLAY_BLOCK_RULE_COUNT ? calls[BLOCK_RULE_FIRST + rule] : 0;
}
const char *gameplay_block_rule_name(GameplayBlockRule rule) {
  return rule >= 0 && rule < GAMEPLAY_BLOCK_RULE_COUNT ? rule_names[BLOCK_RULE_FIRST + rule] : "invalid";
}
bool gameplay_sync_next_state(uint8_t current, uint8_t *out) {
  int index = BLOCK_RULE_FIRST + GAMEPLAY_RULE_sync_next_state;
  if (!out || current > 2) return reject(rule_names[index], "invalid sync state arguments");
  uint8_t args[] = {current};
  if (!call_rule(index, args, 1)) return false;
  if (!lua_isinteger(vm, -1)) return reject(rule_names[index], "expected integer state");
  lua_Integer value=lua_tointeger(vm,-1);
  if (value != (current == 1 ? 2 : 1))
    return reject(rule_names[index], "invalid sync state transition");
  *out=(uint8_t)value; lua_pop(vm,1); ++calls[index]; return true;
}

bool gameplay_colored_cube_enabled(void) {
#ifdef ORACLES_LUA_COLORED_CUBE
  return enabled;
#else
  return false;
#endif
}
static bool colored_cube_boolean(GameplayColoredCubeRule rule, uint8_t value, bool *out) {
  int index=COLORED_CUBE_FIRST+rule;
  if (!out) return reject(rule_names[index], "invalid output argument");
  if (!call_rule(index,&value,1)) return false;
  if (!lua_isboolean(vm,-1)) return reject(rule_names[index], "expected boolean result");
  *out=lua_toboolean(vm,-1); lua_pop(vm,1); ++calls[index]; return true;
}
bool gameplay_colored_cube_present(uint8_t position, bool *out) {
  return colored_cube_boolean(GAMEPLAY_COLORED_CUBE_PRESENT,position,out);
}
bool gameplay_colored_cube_visible(uint8_t color, bool *out) {
  return colored_cube_boolean(GAMEPLAY_COLORED_CUBE_VISIBLE,color,out);
}
bool gameplay_colored_cube_palette(uint8_t color, uint8_t *out) {
  int index=COLORED_CUBE_FIRST+GAMEPLAY_COLORED_CUBE_PALETTE;
  if (!out) return reject(rule_names[index],"invalid output argument");
  if (!call_rule(index,&color,1)) return false;
  if (!lua_isinteger(vm,-1)) return reject(rule_names[index],"expected integer palette offset");
  lua_Integer result=lua_tointeger(vm,-1);
  if (result<0 || result>127) return reject(rule_names[index],"invalid palette offset");
  *out=(uint8_t)result; lua_pop(vm,1); ++calls[index]; return true;
}
uint64_t gameplay_colored_cube_calls(GameplayColoredCubeRule rule) {
  return rule>=0 && rule<GAMEPLAY_COLORED_CUBE_RULE_COUNT ? calls[COLORED_CUBE_FIRST+rule] : 0;
}
const char *gameplay_colored_cube_rule_name(GameplayColoredCubeRule rule) {
  return rule>=0 && rule<GAMEPLAY_COLORED_CUBE_RULE_COUNT ? rule_names[COLORED_CUBE_FIRST+rule] : "invalid";
}
bool gameplay_levers_lava_enabled(void) {
#ifdef ORACLES_LUA_LEVERS_LAVA
  return enabled;
#else
  return false;
#endif
}
static bool lava_boolean(GameplayLavaRule rule,uint8_t value,bool *out) {
  int index=LAVA_FIRST+rule;
  if(!out) return reject(rule_names[index],"invalid output argument");
  if(!call_rule(index,&value,1)) return false;
  if(!lua_isboolean(vm,-1)) return reject(rule_names[index],"expected boolean result");
  *out=lua_toboolean(vm,-1);lua_pop(vm,1);++calls[index];return true;
}
bool gameplay_lava_fully_pulled(uint8_t distance,bool *out) { return lava_boolean(GAMEPLAY_LAVA_FULLY_PULLED,distance,out); }
bool gameplay_lava_released(uint8_t distance,bool *out) { return lava_boolean(GAMEPLAY_LAVA_RELEASED,distance,out); }
bool gameplay_lava_timer_elapsed(bool zero,bool *out) { return lava_boolean(GAMEPLAY_LAVA_TIMER_ELAPSED,zero?1:0,out); }
bool gameplay_lava_has_tile(uint8_t token,bool *out) { return lava_boolean(GAMEPLAY_LAVA_HAS_TILE,token,out); }
bool gameplay_lava_source_found(bool zero,bool *out) { return lava_boolean(GAMEPLAY_LAVA_SOURCE_FOUND,zero?1:0,out); }
bool gameplay_lava_source_segment(uint8_t relative,bool *out) { return lava_boolean(GAMEPLAY_LAVA_SOURCE_SEGMENT,relative,out); }
static bool lava_integer(GameplayLavaRule rule,const uint8_t *args,int count,uint8_t *out) {
  int index=LAVA_FIRST+rule;
  if(!out) return reject(rule_names[index],"invalid output argument");
  if(!call_rule(index,args,count)) return false;
  if(!lua_isinteger(vm,-1)) return reject(rule_names[index],"expected integer result");
  lua_Integer value=lua_tointeger(vm,-1);bool valid=false;
  if(rule==GAMEPLAY_LAVA_SOURCE_DELTA) valid=value==6 || value==250;
  else if(rule==GAMEPLAY_LAVA_NEXT_STATE) valid=value==(args[0]==4?1:args[0]+1);
  else if(rule==GAMEPLAY_LAVA_FLOOR_TILE) valid=value==1 || (value>=0x61 && value<=0x64);
  if(!valid) return reject(rule_names[index],"invalid result domain");
  *out=(uint8_t)value;lua_pop(vm,1);++calls[index];return true;
}
bool gameplay_lava_source_delta(bool empty,uint8_t *out) {
  uint8_t value=empty?1:0;return lava_integer(GAMEPLAY_LAVA_SOURCE_DELTA,&value,1,out);
}
bool gameplay_lava_next_state(uint8_t current,uint8_t *out) {
  if(current>4) return reject(rule_names[LAVA_FIRST+GAMEPLAY_LAVA_NEXT_STATE],"invalid state argument");
  return lava_integer(GAMEPLAY_LAVA_NEXT_STATE,&current,1,out);
}
bool gameplay_lava_floor_tile(uint8_t state,uint8_t random_byte,uint8_t *out) {
  if(state!=2 && state!=4) return reject(rule_names[LAVA_FIRST+GAMEPLAY_LAVA_FLOOR_TILE],"invalid phase argument");
  uint8_t args[]={state,random_byte};return lava_integer(GAMEPLAY_LAVA_FLOOR_TILE,args,2,out);
}
uint64_t gameplay_lava_calls(GameplayLavaRule rule) { return rule>=0 && rule<GAMEPLAY_LAVA_RULE_COUNT?calls[LAVA_FIRST+rule]:0; }
const char *gameplay_lava_rule_name(GameplayLavaRule rule) { return rule>=0 && rule<GAMEPLAY_LAVA_RULE_COUNT?rule_names[LAVA_FIRST+rule]:"invalid"; }
bool gameplay_doors_enabled(void) {
#ifdef ORACLES_LUA_DOORS
  return enabled;
#else
  return false;
#endif
}
static bool door_boolean(GameplayDoorRule rule, const uint8_t *args, int count, bool *out) {
  int index = DOOR_FIRST + rule;
  if (!out) return reject(rule_names[index], "invalid output argument");
  if (!call_rule(index, args, count)) return false;
  if (!lua_isboolean(vm, -1)) return reject(rule_names[index], "expected boolean result");
  *out = lua_toboolean(vm, -1); lua_pop(vm, 1); ++calls[index]; return true;
}
static bool door_integer(GameplayDoorRule rule, uint8_t arg, int low, int high, uint8_t *out) {
  int index = DOOR_FIRST + rule;
  if (!out) return reject(rule_names[index], "invalid output argument");
  if (rule == GAMEPLAY_DOOR_script_state && arg > 3)
    return reject(rule_names[index], "invalid state argument");
  if (!call_rule(index, &arg, 1)) return false;
  if (!lua_isinteger(vm, -1)) return reject(rule_names[index], "expected integer result");
  lua_Integer value = lua_tointeger(vm, -1);
  if (value < low || value > high) return reject(rule_names[index], "invalid result domain");
  *out = (uint8_t)value; lua_pop(vm, 1); ++calls[index]; return true;
}
#define DOOR_RULE1(name, a) bool gameplay_door_##name(uint8_t a, bool *out) { \
  uint8_t args[] = {a}; return door_boolean(GAMEPLAY_DOOR_##name, args, 1, out); }
#define DOOR_RULE2(name, a, b) bool gameplay_door_##name(uint8_t a, uint8_t b, bool *out) { \
  uint8_t args[] = {a, b}; return door_boolean(GAMEPLAY_DOOR_##name, args, 2, out); }
#define DOOR_RULE_FLAG(name, a) bool gameplay_door_##name(bool a, bool *out) { \
  uint8_t args[] = {a ? 1 : 0}; return door_boolean(GAMEPLAY_DOOR_##name, args, 1, out); }
#define DOOR_BYTE(name, a, low, high) bool gameplay_door_##name(uint8_t a, uint8_t *out) { \
  return door_integer(GAMEPLAY_DOOR_##name, a, low, high, out); }
#include "script/doors.def"
#undef DOOR_RULE1
#undef DOOR_RULE2
#undef DOOR_RULE_FLAG
#undef DOOR_BYTE
uint64_t gameplay_door_calls(GameplayDoorRule rule) {
  return rule >= 0 && rule < GAMEPLAY_DOOR_RULE_COUNT ? calls[DOOR_FIRST + rule] : 0;
}
const char *gameplay_door_rule_name(GameplayDoorRule rule) {
  return rule >= 0 && rule < GAMEPLAY_DOOR_RULE_COUNT ? rule_names[DOOR_FIRST + rule] : "invalid";
}
const char *gameplay_error(void) { return error; }
const char *gameplay_revision(void) { return ORACLES_GAMEPLAY_REVISION; }
uint64_t gameplay_calls(int rule) { return rule >= 0 && rule < RULE_COUNT ? calls[rule] : 0; }
uint64_t gameplay_block_calls(GameplayBlockState state) { return state >= 0 && state < 4 ? block_calls[state] : 0; }
size_t gameplay_memory(void) { return used; }
GameplayMemoryStats gameplay_memory_stats(void) {
  return (GameplayMemoryStats){.current = used, .peak = peak, .allocations = allocations};
}
