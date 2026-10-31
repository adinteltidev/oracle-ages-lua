#include "unit.h"
#include "script/colored_cube.h"

static void exhaustive_bytes(void) {
  ASSERT(gameplay_init()); ASSERT(gameplay_colored_cube_enabled());
  for(unsigned byte=0;byte<256;++byte) {
    bool value; uint8_t offset;
    ASSERT(gameplay_colored_cube_present(byte,&value)); ASSERT_EQ(value,byte!=0);
    ASSERT(gameplay_colored_cube_visible(byte,&value)); ASSERT_EQ(value,(byte&0x80)!=0);
    ASSERT(gameplay_colored_cube_palette(byte,&offset)); ASSERT_EQ(offset,byte&0x7f);
  }
  for(int i=0;i<GAMEPLAY_COLORED_CUBE_RULE_COUNT;++i) ASSERT_EQ(gameplay_colored_cube_calls(i),256);
  GameplayMemoryStats memory=gameplay_memory_stats(); ASSERT(memory.peak<256*1024);
  printf("768 typed byte cases; heap=%zu peak=%zu SHA256=%s\n",memory.current,memory.peak,gameplay_revision());
  gameplay_enable(false); ASSERT(!gameplay_colored_cube_enabled()); gameplay_enable(true);
}

static void bad_results(void) {
  const char *boolean_bodies[]={"return 1","return 1.0","return nil","return {}","return","return true,false","while true do end"};
  const char *integer_bodies[]={"return -1","return 128","return 256","return 4294967296","return 1.0","return true","return nil","return {}","return","return 1,2","return '1'","while true do end"};
  for(int rule=0;rule<3;++rule) {
    const char **bodies=rule==2?integer_bodies:boolean_bodies;
    size_t count=rule==2?sizeof integer_bodies/sizeof *integer_bodies:sizeof boolean_bodies/sizeof *boolean_bodies;
    for(size_t i=0;i<count;++i) {
      char source[1024]; int n=snprintf(source,sizeof source,
        "return {present=function() %s end,visible=function() %s end,palette_offset=function() %s end}",
        rule==0?bodies[i]:"return true",rule==1?bodies[i]:"return true",rule==2?bodies[i]:"return 1");
      GameplaySources override={.colored_cube=source,.colored_cube_size=(size_t)n}; ASSERT(gameplay_load_sources(&override));
      bool value=true; uint8_t offset=0xa5;
      bool ok=rule==0?gameplay_colored_cube_present(0,&value):rule==1?gameplay_colored_cube_visible(0,&value):gameplay_colored_cube_palette(0,&offset);
      ASSERT(!ok); ASSERT(value); ASSERT_EQ(offset,0xa5); ASSERT(strstr(gameplay_error(),"ages/colored_cube."));
      ASSERT_EQ(gameplay_colored_cube_calls(rule),0);
    }
  }
  gameplay_close(); ASSERT(gameplay_init()); ASSERT(!gameplay_colored_cube_palette(0,NULL));
  gameplay_close(); ASSERT(gameplay_init()); ASSERT(!gameplay_colored_cube_present(0,NULL));
  gameplay_close(); ASSERT(gameplay_init()); ASSERT(!gameplay_colored_cube_visible(0,NULL));
  ASSERT_EQ(gameplay_colored_cube_calls(-1),0); ASSERT_EQ(gameplay_colored_cube_calls(3),0);
}

static void isolation_and_limits(void) {
  const char switches[]="leak=123; return {changed=function() return true end,alternate=function() return false end}";
  const char cube[]="return {present=function() return leak==nil and _G==nil and os==nil and io==nil and math==nil and debug==nil end,visible=function() return true end,palette_offset=function() return 127 end}";
  GameplaySources override={.switch_tiles=switches,.switch_tiles_size=sizeof switches-1,.colored_cube=cube,.colored_cube_size=sizeof cube-1};
  ASSERT(gameplay_load_sources(&override)); bool value=false; ASSERT(gameplay_colored_cube_present(1,&value)); ASSERT(value);
  const char runaway[]="while true do end";
  override=(GameplaySources){.colored_cube=runaway,.colored_cube_size=sizeof runaway-1}; ASSERT(!gameplay_load_sources(&override)); ASSERT(strstr(gameplay_error(),"instruction limit"));
  size_t length=300000; char *large=malloc(length+64); ASSERT(large);
  const char prefix[]="local s='"; memcpy(large,prefix,sizeof prefix-1); memset(large+sizeof prefix-1,'x',length);
  const char suffix[]="'; return {}"; memcpy(large+sizeof prefix-1+length,suffix,sizeof suffix);
  override=(GameplaySources){.colored_cube=large,.colored_cube_size=sizeof prefix-1+length+sizeof suffix-1}; ASSERT(!gameplay_load_sources(&override));
  ASSERT(gameplay_memory_stats().peak<=256*1024); ASSERT(gameplay_error()[0]); free(large);
  uint8_t offset=0xa5; ASSERT(!gameplay_colored_cube_palette(0,&offset)); ASSERT_EQ(offset,0xa5);
  gameplay_close(); ASSERT(gameplay_init());
}

int main(void) {
  RUN(exhaustive_bytes); RUN(bad_results); RUN(isolation_and_limits);
  gameplay_close(); puts("ok lua_colored_cube"); return 0;
}
