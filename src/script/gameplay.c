#include "script/gameplay.h"
#include "lua.h"
#include "lauxlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern const unsigned char ages_switch_lua[];
extern const unsigned int ages_switch_lua_size;
#define CAP (256u * 1024u)
static lua_State *vm;
static size_t used;
static bool enabled=true, attempted;
static int refs[2], budget;
static uint64_t calls[2];
static char error[160];
static void *allocate(void *ctx, void *ptr, size_t old, size_t size) {
  (void)ctx;
  if (!ptr) old=0;
  if (!size) { free(ptr); used-=old; return NULL; }
  if (size>old && size-old>CAP-used) return NULL;
  void *p=realloc(ptr,size);
  if (p) used=used-old+size;
  return p;
}
static void limit(lua_State *L,lua_Debug *ar) {
  (void)ar;
  if (--budget<=0) luaL_error(L,"gameplay instruction limit");
}
static bool failed(const char *fallback) {
  // Do not call tostring on arbitrary script objects outside a protected call.
  const char *s=lua_type(vm,-1)==LUA_TSTRING ? lua_tostring(vm,-1) : fallback;
  snprintf(error,sizeof error,"%s",s ? s : fallback);
  lua_settop(vm,0); return false;
}
void gameplay_close(void) {
  if (vm) lua_close(vm);
  vm=NULL; used=0; attempted=false; memset(calls,0,sizeof calls); error[0]=0;
}
bool gameplay_load(const char *source,size_t length) {
  gameplay_close(); attempted=true;
  vm=lua_newstate(allocate,NULL);
  if (!vm) { snprintf(error,sizeof error,"cannot allocate gameplay VM"); return false; }
  // No standard libraries, filesystem, network, randomness or C callbacks.
  budget=100; lua_sethook(vm,limit,LUA_MASKCOUNT,100);
  if (luaL_loadbufferx(vm,source,length,"ages/switch_tiles.lua","t")!=LUA_OK ||
      lua_pcall(vm,0,1,0)!=LUA_OK) return failed("invalid gameplay source");
  if (!lua_istable(vm,-1)) return failed("gameplay must return a table");
  if (used>CAP-16384) return failed("gameplay initialization memory limit");
  const char *names[]={"changed","alternate"};
  for (int i=0;i<2;++i) {
    // Raw access cannot invoke an unbounded metamethod during initialization.
    lua_pushstring(vm,names[i]); lua_rawget(vm,-2);
    if (!lua_isfunction(vm,-1)) return failed("missing gameplay decision");
    refs[i]=luaL_ref(vm,LUA_REGISTRYINDEX);
  }
  lua_settop(vm,0); return true;
}
bool gameplay_init(void) {
  if (attempted) return vm && !error[0];
  return gameplay_load((const char *)ages_switch_lua,ages_switch_lua_size);
}
void gameplay_enable(bool value) { enabled=value; }
bool gameplay_decide(int rule,uint8_t a,uint8_t b,bool *out) {
  if (rule<0 || rule>1 || !out) return false;
  if (!enabled) { *out=rule==0 ? a!=b : (a&b)!=0; return true; }
  if (!gameplay_init()) return false;
  lua_rawgeti(vm,LUA_REGISTRYINDEX,refs[rule]);
  lua_pushinteger(vm,a); lua_pushinteger(vm,b);
  budget=100;
  if (lua_pcall(vm,2,1,0)!=LUA_OK) return failed("gameplay execution failed");
  if (!lua_isboolean(vm,-1)) return failed("gameplay decision must return boolean");
  *out=lua_toboolean(vm,-1); lua_pop(vm,1); ++calls[rule]; return true;
}
const char *gameplay_error(void) { return error; }
uint64_t gameplay_calls(int rule) { return rule>=0 && rule<2 ? calls[rule] : 0; }
size_t gameplay_memory(void) { return used; }
