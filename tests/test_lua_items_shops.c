#include "unit.h"
#include "script/items_shops.h"
extern const unsigned char ages_items_lua[],ages_shops_lua[];
extern const unsigned int ages_items_lua_size,ages_shops_lua_size;
static void contracts(void) {
  ASSERT(gameplay_init());ASSERT(gameplay_items_shops_enabled());
  for(unsigned a=0;a<256;++a) {
    bool out;uint8_t value;
    ASSERT(gameplay_item_zero(a,&out));ASSERT_EQ(out,a==0);
    ASSERT(gameplay_shop_zero(a,&out));ASSERT_EQ(out,a==0);
    ASSERT(gameplay_item_carry(a,&out));ASSERT_EQ(out,a==1);
    ASSERT(gameplay_shop_carry(a,&out));ASSERT_EQ(out,a==1);
    ASSERT(gameplay_shop_table_index(a,&value));ASSERT_EQ(value,a);
    for(unsigned b=0;b<256;++b) {
      ASSERT(gameplay_item_compare(a,b,&value));ASSERT_EQ(value,a==b?0:a<b?1:2);
      ASSERT(gameplay_shop_compare(a,b,&value));ASSERT_EQ(value,a==b?0:a<b?1:2);
      ASSERT(gameplay_item_bit_zero(a,b,&out));ASSERT_EQ(out,(a&b)==0);
      ASSERT(gameplay_shop_bit_zero(a,b,&out));ASSERT_EQ(out,(a&b)==0);
      ASSERT(gameplay_shop_affordable(a,9,b,9,&out));ASSERT_EQ(out,a>=b);
      ASSERT(gameplay_shop_affordable(a,8,b,9,&out));ASSERT(!out);
      ASSERT(gameplay_shop_affordable(a,10,b,9,&out));ASSERT(out);
    }
  }
  for(unsigned id=0;id<=98;++id){if(id==0x13 || id==0x40)continue;uint8_t out;ASSERT(gameplay_item_reward_id(id,&out));ASSERT_EQ(out,id);ASSERT(gameplay_shop_reward_id(id,&out));ASSERT_EQ(out,id);}
  for(unsigned mode=0;mode<16;++mode){uint8_t out;ASSERT(gameplay_item_parameter_mode(mode,&out));ASSERT_EQ(out,mode);}
  for(unsigned price=0;price<=20;++price){uint8_t out;ASSERT(gameplay_shop_price_index(price,&out));ASSERT_EQ(out,price);}
  uint8_t out;ASSERT(gameplay_shop_purchase_outcome(1,&out));ASSERT_EQ(out,1);ASSERT(gameplay_shop_purchase_outcome(255,&out));ASSERT_EQ(out,255);
  GameplayMemoryStats before=gameplay_memory_stats();for(unsigned i=0;i<1000;++i){bool out;ASSERT(gameplay_shop_affordable(0,5,0,1,&out));}
  GameplayMemoryStats after=gameplay_memory_stats();ASSERT_EQ(before.allocations,after.allocations);ASSERT(after.peak<256*1024);
  printf("15 rules: exhaustive byte comparisons, masks and currency boundaries; heap=%zu peak=%zu SHA256=%s\n",after.current,after.peak,gameplay_revision());
}
static void override(const char *module,const char *name,const char *body) {
  const unsigned char *base=!strcmp(module,"items")?ages_items_lua:ages_shops_lua;unsigned size=!strcmp(module,"items")?ages_items_lua_size:ages_shops_lua_size;
  size_t cap=size+1024;char *s=malloc(cap);ASSERT(s);int n=snprintf(s,cap,"local r=(function() %.*s end)();r.%s=function(...) %s end;return r",size,base,name,body);ASSERT(n>0 && (size_t)n<cap);
  GameplaySources sources={0};if(!strcmp(module,"items")){sources.items=s;sources.items_size=n;}else{sources.shops=s;sources.shops_size=n;}
  ASSERT(gameplay_load_sources(&sources));free(s);
}
static void rejects(void) {
  const char *bad[]={"return nil","return {}","return","return 1,2","return '1'","while true do end"};
  for(unsigned i=0;i<sizeof bad/sizeof *bad;++i) {
    override("items","compare",bad[i]);uint8_t out=77;ASSERT(!gameplay_item_compare(1,2,&out));ASSERT_EQ(out,77);ASSERT(gameplay_error()[0]);
    override("shops","affordable",bad[i]);bool ok=false;ASSERT(!gameplay_shop_affordable(0,1,0,1,&ok));
  }
  const char *fields[]={"compare","reward_id","parameter_mode","price_index","table_index","purchase_outcome"};
  const char *modules[]={"items","items","items","shops","shops","shops"};
  const char *values[]={"3","100","16","21","2","2"};
  for(unsigned i=0;i<6;++i){char body[80];snprintf(body,sizeof body,"return %s",values[i]);override(modules[i],fields[i],body);uint8_t out=77;bool ok;
    if(i==0)ok=gameplay_item_compare(1,1,&out);else if(i==1)ok=gameplay_item_reward_id(1,&out);else if(i==2)ok=gameplay_item_parameter_mode(1,&out);else if(i==3)ok=gameplay_shop_price_index(1,&out);else if(i==4)ok=gameplay_shop_table_index(1,&out);else ok=gameplay_shop_purchase_outcome(1,&out);
    ASSERT(!ok);ASSERT_EQ(out,77);
  }
  for(unsigned i=0;i<2;++i)for(unsigned module=0;module<2;++module){
    char body[40];snprintf(body,sizeof body,"return %u",i?0x40:0x13);override(module?"shops":"items","reward_id",body);uint8_t value=77;
    ASSERT(!(module?gameplay_shop_reward_id(1,&value):gameplay_item_reward_id(1,&value)));ASSERT_EQ(value,77);
  }
  override("items","compare","return 1.0");uint8_t out;ASSERT(!gameplay_item_compare(1,2,&out));
  override("shops","zero","return 0");bool result;ASSERT(!gameplay_shop_zero(1,&result));
  const char *bad_source="return io.open('x')";GameplaySources sources={.items=bad_source,.items_size=strlen(bad_source)};ASSERT(!gameplay_load_sources(&sources));
  gameplay_close();ASSERT(gameplay_init());
}
int main(void){RUN(contracts);RUN(rejects);gameplay_close();return 0;}
