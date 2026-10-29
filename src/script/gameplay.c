#include "script/gameplay.h"
#include "lua.h"
#include "lauxlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const unsigned char ages_switch_tiles_lua[], ages_block_puzzles_lua[];
extern const unsigned int ages_switch_tiles_lua_size, ages_block_puzzles_lua_size;
#define CAP (256u * 1024u)
enum { SWITCH_CHANGED, SWITCH_ALTERNATE, BLOCK_NEXT_STATE, RULE_COUNT };
static const char *const rule_names[RULE_COUNT] = {
  "ages/switch_tiles.changed", "ages/switch_tiles.alternate", "ages/block_puzzles.next_state",
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

const char *gameplay_error(void) { return error; }
const char *gameplay_revision(void) { return ORACLES_GAMEPLAY_REVISION; }
uint64_t gameplay_calls(int rule) { return rule >= 0 && rule < RULE_COUNT ? calls[rule] : 0; }
uint64_t gameplay_block_calls(GameplayBlockState state) { return state >= 0 && state < 4 ? block_calls[state] : 0; }
size_t gameplay_memory(void) { return used; }
GameplayMemoryStats gameplay_memory_stats(void) {
  return (GameplayMemoryStats){.current = used, .peak = peak, .allocations = allocations};
}
