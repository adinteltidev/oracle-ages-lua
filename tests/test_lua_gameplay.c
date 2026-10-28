#include "unit.h"
#include "script/gameplay.h"
#include <time.h>
static double now(void) { return (double)clock()/CLOCKS_PER_SEC; }
int main(void) {
  ASSERT(gameplay_init());
  double start=now();
  for (int a=0;a<256;++a) for (int b=0;b<256;++b) {
    bool result;
    ASSERT(gameplay_decide(0,a,b,&result)); ASSERT_EQ(result,a!=b);
    ASSERT(gameplay_decide(1,a,b,&result)); ASSERT_EQ(result,(a&b)!=0);
  }
  ASSERT_EQ(gameplay_calls(0),65536); ASSERT_EQ(gameplay_calls(1),65536);
  printf("Lua: 131072 decisions, CPU %.6fs, heap %zu bytes\n",now()-start,gameplay_memory());
  ASSERT(gameplay_memory()<256*1024);
  bool result;
  const char bad[]="return {changed=function() while true do end end, alternate=function() return true end}";
  ASSERT(gameplay_load(bad,sizeof bad-1)); ASSERT(!gameplay_decide(0,0,0,&result));
  ASSERT(strstr(gameplay_error(),"limit"));
  const char invalid[]="return {changed=function() return 1 end, alternate=function() return true end}";
  ASSERT(gameplay_load(invalid,sizeof invalid-1)); ASSERT(!gameplay_decide(0,0,0,&result));
  gameplay_close(); ASSERT(gameplay_init());
  gameplay_enable(false); ASSERT(gameplay_decide(0,1,2,&result)); ASSERT(result);
  gameplay_enable(true); ASSERT(gameplay_decide(1,1,2,&result)); ASSERT(!result);
  gameplay_close(); puts("ok lua_gameplay: exhaustive byte inputs, budget, result validation, C control");
}
