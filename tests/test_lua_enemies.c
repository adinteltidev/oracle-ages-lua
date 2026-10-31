#include "unit.h"
#include "script/enemies.h"
extern const unsigned char ages_enemies_ai_lua[];
extern const unsigned int ages_enemies_ai_lua_size;
static void override(const char *field,const char *body){
 char *source=malloc(ages_enemies_ai_lua_size+1024);ASSERT(source);
 int n=snprintf(source,ages_enemies_ai_lua_size+1024,"local r=(function() %.*s end)();r.%s=function(...) %s end;return r",ages_enemies_ai_lua_size,ages_enemies_ai_lua,field,body);
 GameplaySources src={.enemies=source,.enemies_size=n};ASSERT(gameplay_load_sources(&src));free(source);
}
static void contracts(void){
 ASSERT(gameplay_init());ASSERT(gameplay_enemies_enabled());
 for(unsigned a=0;a<256;++a){bool flag;ASSERT(gameplay_enemy_zero(a,&flag));ASSERT_EQ(flag,a==0);ASSERT(gameplay_enemy_octorok_aim(a,&flag));ASSERT_EQ(flag,a>=2);
  for(unsigned b=0;b<256;++b){uint8_t out;
   ASSERT(gameplay_enemy_compare(a,b,&out));ASSERT_EQ(out,a==b?0:a<b?1:2);
   ASSERT(gameplay_enemy_bit_zero(a,b,&flag));ASSERT_EQ(flag,(a&b)==0);
   ASSERT(gameplay_enemy_condition(a,b,&flag));ASSERT_EQ(flag,(a&b)!=0);
   ASSERT(gameplay_enemy_octorok_attack(a,b,&flag));ASSERT_EQ(flag,(a&b)==0);
   ASSERT(gameplay_enemy_transition(a,b,&out));ASSERT_EQ(out,b);
   ASSERT(gameplay_enemy_angle(a,b,&out));ASSERT_EQ(out,b);
   ASSERT(gameplay_enemy_direction(a,b,&out));ASSERT_EQ(out,b);
   ASSERT(gameplay_enemy_speed(a,b,&out));ASSERT_EQ(out,b);
  }
 }
 GameplayMemoryStats before=gameplay_memory_stats();for(unsigned i=0;i<1000;++i){uint8_t out;ASSERT(gameplay_enemy_transition(1,2,&out));}GameplayMemoryStats after=gameplay_memory_stats();ASSERT_EQ(before.allocations,after.allocations);ASSERT(after.peak<256*1024);
 printf("10 enemy rules; heap=%zu peak=%zu SHA256=%s\n",after.current,after.peak,gameplay_revision());
}
static void rejects(void){
 const char *bad[]={"return nil","return {}","return","return 1,2","return '1'","return 1.0","while true do end","return 256","return -1"};
 for(unsigned i=0;i<sizeof bad/sizeof *bad;++i){override("transition",bad[i]);uint8_t out=77;ASSERT(!gameplay_enemy_transition(1,2,&out));ASSERT_EQ(out,77);}
 override("transition","return 3");uint8_t out=77;ASSERT(!gameplay_enemy_transition(1,2,&out));
 override("condition","return 0");bool flag;ASSERT(!gameplay_enemy_condition(128,128,&flag));
 override("angle","return 32");ASSERT(!gameplay_enemy_angle(1,2,&out));
 override("direction","return 4");ASSERT(!gameplay_enemy_direction(1,2,&out));
 override("speed","return 3");ASSERT(!gameplay_enemy_speed(1,2,&out));
 override("condition","return io.open('x')");ASSERT(!gameplay_enemy_condition(1,1,&flag));
 gameplay_close();ASSERT(gameplay_init());
}
int main(void){RUN(contracts);RUN(rejects);gameplay_close();return 0;}
