#include "unit.h"
#include "script/npcs_events.h"
extern const unsigned char ages_npcs_rules_lua[],ages_events_flow_lua[];
extern const unsigned int ages_npcs_rules_lua_size,ages_events_flow_lua_size;
static void contracts(void){
  ASSERT(gameplay_init());ASSERT(gameplay_npcs_events_enabled());
  for(unsigned a=0;a<256;++a){bool flag;uint8_t byte;uint16_t word;
    ASSERT(gameplay_npc_zero(a,&flag));ASSERT_EQ(flag,a==0);ASSERT(gameplay_event_zero(a,&flag));ASSERT_EQ(flag,a==0);
    ASSERT(gameplay_npc_carry(a,&flag));ASSERT_EQ(flag,a==1);ASSERT(gameplay_event_carry(a,&flag));ASSERT_EQ(flag,a==1);
    ASSERT(gameplay_event_next_state(a,&byte));ASSERT_EQ(byte,(a+1)&255);
    ASSERT(gameplay_event_table_index(a,&byte));ASSERT_EQ(byte,a);
    ASSERT(gameplay_event_script_ready(a,&flag));ASSERT_EQ(flag,a==0 || (a&128)!=0);
    ASSERT(gameplay_event_script_ended(a,&flag));ASSERT_EQ(flag,a==0);
    ASSERT(gameplay_event_choose_linked(a,&flag));ASSERT_EQ(flag,a!=0);
    ASSERT(gameplay_event_condition(a,&flag));ASSERT_EQ(flag,a!=0);
    for(unsigned b=0;b<256;++b){
      ASSERT(gameplay_npc_compare(a,b,&byte));ASSERT_EQ(byte,a==b?0:a<b?1:2);ASSERT(gameplay_event_compare(a,b,&byte));ASSERT_EQ(byte,a==b?0:a<b?1:2);
      ASSERT(gameplay_npc_bit_zero(a,b,&flag));ASSERT_EQ(flag,(a&b)==0);ASSERT(gameplay_event_bit_zero(a,b,&flag));ASSERT_EQ(flag,(a&b)==0);
      ASSERT(gameplay_event_transition(a,b,&byte));ASSERT_EQ(byte,b);ASSERT(gameplay_event_dialogue(a,b,&word));ASSERT_EQ(word,a|(b<<8));
    }
  }
  for(unsigned subid=0;subid<2;++subid)for(unsigned saved=0;saved<2;++saved)for(unsigned progress=0;progress<256;++progress){bool flag;uint8_t index;
    ASSERT(gameplay_npc_old_lady_available(subid,saved,progress,&flag));ASSERT_EQ(flag,subid?progress<4:saved==0);
    if(!subid || progress<4){ASSERT(gameplay_npc_old_lady_script(subid,progress,&index));ASSERT_EQ(index,subid?progress:0);}
  }
  for(unsigned subid=0;subid<13;++subid)for(unsigned nut=0;nut<2;++nut){bool flag;ASSERT(gameplay_npc_symmetry_available(subid,nut,&flag));ASSERT_EQ(flag,subid!=12 || nut);}
  for(unsigned state=0;state<4;++state)for(unsigned flags=0;flags<256;++flags){bool flag;ASSERT(gameplay_npc_symmetry_restart(state,flags,&flag));ASSERT_EQ(flag,state==2 && (flags&1));}
  GameplayMemoryStats before=gameplay_memory_stats();for(unsigned i=0;i<1000;++i){uint8_t state;ASSERT(gameplay_event_transition(1,2,&state));}
  GameplayMemoryStats after=gameplay_memory_stats();ASSERT_EQ(before.allocations,after.allocations);ASSERT(after.peak<256*1024);
  printf("20 NPC/event rules: byte domains, state machines, text IDs, flags; heap=%zu peak=%zu SHA256=%s\n",after.current,after.peak,gameplay_revision());
}
static void override(const char *module,const char *field,const char *body){
  bool npc=!strcmp(module,"npcs");const unsigned char *base=npc?ages_npcs_rules_lua:ages_events_flow_lua;unsigned size=npc?ages_npcs_rules_lua_size:ages_events_flow_lua_size;
  size_t cap=size+1024;char *source=malloc(cap);ASSERT(source);int n=snprintf(source,cap,"local r=(function() %.*s end)();r.%s=function(...) %s end;return r",size,base,field,body);ASSERT(n>0 && (size_t)n<cap);
  GameplaySources sources={0};if(npc){sources.npcs=source;sources.npcs_size=n;}else{sources.events=source;sources.events_size=n;}ASSERT(gameplay_load_sources(&sources));free(source);
}
static void rejects(void){
  const char *bad[]={"return nil","return {}","return","return 1,2","return '1'","return 1.0","while true do end"};
  for(unsigned i=0;i<sizeof bad/sizeof *bad;++i){override("npcs","compare",bad[i]);uint8_t out=77;ASSERT(!gameplay_npc_compare(1,2,&out));ASSERT_EQ(out,77);override("events","transition",bad[i]);ASSERT(!gameplay_event_transition(1,2,&out));ASSERT_EQ(out,77);}
  override("events","transition","return 3");uint8_t byte=77;ASSERT(!gameplay_event_transition(1,2,&byte));ASSERT_EQ(byte,77);
  override("events","next_state","return 3");ASSERT(!gameplay_event_next_state(1,&byte));
  override("events","table_index","return 2");ASSERT(!gameplay_event_table_index(1,&byte));
  override("events","dialogue","return 65536");uint16_t word=77;ASSERT(!gameplay_event_dialogue(1,2,&word));ASSERT_EQ(word,77);
  override("events","dialogue","return 3");ASSERT(!gameplay_event_dialogue(1,2,&word));
  override("npcs","old_lady_script","return 1");ASSERT(!gameplay_npc_old_lady_script(0,0,&byte));
  override("npcs","old_lady_script","return 4");ASSERT(!gameplay_npc_old_lady_script(1,0,&byte));
  override("events","script_ready","return 0");bool flag;ASSERT(!gameplay_event_script_ready(0,&flag));
  const char *source="return io.open('x')";GameplaySources sources={.events=source,.events_size=strlen(source)};ASSERT(!gameplay_load_sources(&sources));
  gameplay_close();ASSERT(gameplay_init());
}
int main(void){RUN(contracts);RUN(rejects);gameplay_close();return 0;}
