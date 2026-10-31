#include "unit.h"
#include "mods/sandbox.h"
#include "lua.h"
#include "lauxlib.h"
#include <time.h>
int oracles_mods_test_clock(struct timespec *unused) { (void)unused; return -1; }
int main(void) {
  lua_State *L=luaL_newstate(); ASSERT(L);
  ASSERT_EQ(luaL_loadstring(L,"return 123"),LUA_OK);
  bool exhausted=false,deadline=false;
  ASSERT_EQ(mod_resume_budgeted(L,0,10000,.004,&exhausted,&deadline),LUA_ERRRUN);
  ASSERT(deadline); ASSERT(!exhausted); ASSERT(strstr(lua_tostring(L,-1),"clock"));
  lua_close(L); return 0;
}
