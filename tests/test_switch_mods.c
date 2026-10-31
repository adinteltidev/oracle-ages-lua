#include "unit.h"
#include "platform/switch_mods.h"
#include "platform/switch_menu.h"
#include "game/features.h"
#include "mods/sandbox.h"
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
Features features;
static char data[512];
static GBSample sample; static GB gb; static uint32_t pixels[160*144];
static void file(const char *name,const char *content) {
  char path[1024]; snprintf(path,sizeof path,"%s%s",data,name);
  FILE *f=fopen(path,"wb"); ASSERT(f); ASSERT_EQ(fwrite(content,1,strlen(content),f),strlen(content)); ASSERT_EQ(fclose(f),0);
}
static void package(const char *id,const char *source,bool writes) {
  char p[1024],ini[256]; snprintf(p,sizeof p,"%smods/%s",data,id); ASSERT_EQ(mkdir(p,0700),0);
  snprintf(p,sizeof p,"mods/%s/mod.ini",id);
  snprintf(ini,sizeof ini,"id=%s\nname=%s\napi=%d\ngames=ages\nchanges_game=%s\n",id,id,writes ? 2 : 1,writes ? "yes" : "no"); file(p,ini);
  snprintf(p,sizeof p,"mods/%s/main.lua",id); file(p,source);
}
static void setup(void) {
  char tmp[]="/tmp/oracles-switch-mods-XXXXXX"; char *d=mkdtemp(tmp); ASSERT(d);
  snprintf(data,sizeof data,"%s/",d); char path[1024]; snprintf(path,sizeof path,"%smods",data); ASSERT_EQ(mkdir(path,0700),0);
  memset(&sample,0,sizeof sample); memset(&gb,0,sizeof gb); gb.sample=&sample;
}
static void cleanup(void) {
  switch_mods_stop(); ASSERT(!mods_current_frame()->wram); char cmd[1024]; snprintf(cmd,sizeof cmd,"rm -rf '%s'",data); ASSERT_EQ(system(cmd),0);
}
static void selection_is_applied_before_main_and_survives_sessions(void) {
  setup(); package("off","error('must never execute')",false); file("mods.cfg","disabled off\n");
  ASSERT(switch_mods_start(data,NULL,0)); ASSERT_EQ(mods_count(),1); ASSERT(!mods_enabled(0)); ASSERT(strstr(mods_error(0),"player"));
  ASSERT(switch_mods_toggle(0)); ASSERT(switch_mods_wanted(0)); ASSERT(!mods_enabled(0)); ASSERT(switch_mods_pending());
  ASSERT(switch_mods_start(data,NULL,0)); ASSERT(!mods_enabled(0)); ASSERT(strstr(mods_error(0),"must never execute")); cleanup();
}
static void settings_are_staged_and_restored_on_next_session(void) {
  setup(); package("options","local a=oracles.setting('Show',true) local b=oracles.setting('Size',{0,10,3})\n",false);
  ASSERT(switch_mods_start(data,NULL,0)); ASSERT_EQ(mods_setting_count(),2);
  ASSERT(switch_mods_change_setting(0,1)); ASSERT(mods_setting_value(0)); ASSERT_EQ(switch_mods_setting(0),0);
  ASSERT(switch_mods_change_setting(1,100)); ASSERT_EQ(mods_setting_number(1),3); ASSERT_EQ(switch_mods_setting(1),10);
  ASSERT(switch_mods_start(data,NULL,0)); ASSERT(!mods_setting_value(0)); ASSERT_EQ(mods_setting_number(1),10); cleanup();
}
static void snapshot_overlay_write_and_load_boundary(void) {
  setup(); package("overlay","oracles.swap_treasure(0,1,0x28,0,0x05,0) oracles.on_frame(function() oracles.draw_rect(2,3,1,1,31) oracles.wr(0xc400,17) end)",true);
  ASSERT(switch_mods_start(data,NULL,0));
  uint8_t id=0,sub=0; ASSERT(features.treasure_swap(0,1,0x28,0,&id,&sub)); ASSERT_EQ(id,5); ASSERT_EQ(sub,0);
  sample.wram[0][0xc30]=42; sample.wram[0][0xc2d]=3; sample.wram[0][0xd0a]=20; sample.wram[0][0xd0b]=16;
  sample.line_drawn[143]=1; sample.drawn_bg_pal[1][0]=9; sample.drawn_oam[1][0]=7;
  ModsFrame f=mods_snapshot(&sample,false,160,NULL,0,NULL);
  ASSERT_EQ(f.room,42); ASSERT_EQ(f.room_group,3); ASSERT_EQ(f.room_w,20); ASSERT_EQ(f.room_h,16);
  ASSERT(f.wram==sample.wram[0]); ASSERT(f.wram1==sample.wram[1]); ASSERT(f.wram3==sample.wram[3]);
  ASSERT_EQ(f.bg_pal[0],9); ASSERT_EQ(f.oam[0],7); ASSERT(!f.origin);
  sample.hram[0x1b]=2; sample.wram[0][0xd00]=1;
  sample.line_lcdc[0]=0x91; sample.line_lcdc[16]=0x81; sample.wram[0][0xd08]=16;
  f=mods_snapshot(&sample,false,160,NULL,0,NULL); ASSERT(f.origin); ASSERT_EQ(f.origin_y,0);
  sample.wram[0][0xbcb]=1; f=mods_snapshot(&sample,false,160,NULL,0,NULL); ASSERT(!f.origin);
  memset(pixels,0x55,sizeof pixels); switch_mods_present(&gb,pixels);
  ASSERT_EQ(pixels[3*160+2],0xff0000ffu); ASSERT_EQ(pixels[3*160+1],0x55555555u); ASSERT_EQ(gb.wram[0][0x400],17);
  mods_frame(&f); mods_reset_picture(); ModWrite w[256]; ASSERT_EQ(mods_take_writes(w,256),0);
  ASSERT(!mods_current_frame()->wram); ASSERT(mods_enabled(0)); ASSERT(features.treasure_swap==mods_treasure_swap);
  switch_mods_measure(.003,.0001,false,false); switch_mods_report(); cleanup(); ASSERT(!features.treasure_swap);
}
static void stores_recover_backup_and_reject_unsafe_paths(void) {
  setup(); package("store","local n=oracles.load('n') or 0 oracles.store('n',n+1) oracles.log(tostring(n+1))",false);
  ASSERT(switch_mods_start(data,NULL,0)); ASSERT(mods_enabled(0)); ASSERT_EQ(strcmp(mods_last_log(),"1"),0);
  ASSERT(switch_mods_start(data,NULL,0)); ASSERT_EQ(strcmp(mods_last_log(),"2"),0);
  char path[1024],bak[1024]; snprintf(path,sizeof path,"%smods/store/store.ini",data); snprintf(bak,sizeof bak,"%s.bak",path);
  ASSERT_EQ(remove(path),0); // Previous value remains in .bak.
  ASSERT(switch_mods_start(data,NULL,0)); ASSERT_EQ(strcmp(mods_last_log(),"2"),0);
  switch_mods_stop(); ASSERT_EQ(remove(path),0); ASSERT_EQ(symlink("/tmp",path),0);
  ASSERT(switch_mods_start(data,NULL,0)); ASSERT(!mods_enabled(0)); cleanup();
}
static void failed_config_write_rolls_back_selection(void) {
  setup(); package("simple","",false); ASSERT(switch_mods_start(data,NULL,0));
  char tmp[1024]; snprintf(tmp,sizeof tmp,"%smods.cfg.tmp",data); ASSERT_EQ(mkdir(tmp,0700),0);
  ASSERT(!switch_mods_toggle(0)); ASSERT(switch_mods_wanted(0)); ASSERT(!switch_mods_pending()); cleanup();
}
static void memory_profile_is_aggregate_and_releases_all_allocations(void) {
  size_t total=0; ModAlloc a={.cap=128,.aggregate=&total,.aggregate_cap=100},b=a;
  void *p=mod_lua_alloc(&a,NULL,9,60); ASSERT(p); ASSERT_EQ(total,60);
  ASSERT(!mod_lua_alloc(&b,NULL,9,60)); ASSERT_EQ(total,60);
  p=mod_lua_alloc(&a,p,60,40); ASSERT(p); ASSERT_EQ(total,40);
  void *q=mod_lua_alloc(&b,NULL,0,60); ASSERT(q); ASSERT_EQ(total,100);
  mod_lua_alloc(&a,p,40,0); mod_lua_alloc(&b,q,60,0); ASSERT_EQ(total,0);
}
static void runtime_failure_does_not_stop_other_mods_or_rewind_lua(void) {
  setup(); package("bad","oracles.on_frame(function() error('isolated') end)",false);
  package("good","local n=0 oracles.on_frame(function() n=n+1 oracles.log(tostring(n)) end)",false);
  ASSERT(switch_mods_start(data,NULL,0)); ASSERT(switch_mods_present(&gb,pixels));
  ASSERT(!mods_enabled(0)); ASSERT(mods_enabled(1)); ASSERT_EQ(strcmp(mods_last_log(),"1"),0);
  mods_reset_picture(); ASSERT(!switch_mods_present(&gb,pixels)); ASSERT_EQ(strcmp(mods_last_log(),"2"),0); cleanup();
}
static void aggregate_profile_disables_only_the_over_budget_package(void) {
  setup(); const char *src="keep={} for i=1,6 do keep[i]=string.rep(string.char(64+i),900000) end";
  package("first",src,false); package("second",src,false); package("third",src,false);
  ASSERT(switch_mods_start(data,NULL,0)); ASSERT_EQ(mods_count(),3);
  ASSERT(mods_enabled(0)); ASSERT(mods_enabled(1)); ASSERT(!mods_enabled(2)); ASSERT(mods_heap_bytes()<=16*1024*1024);
  ASSERT(strstr(mods_error(2),"memory"));
  file("mods.cfg","disabled first\ndisabled second\n");
  ASSERT(switch_mods_start(data,NULL,0)); ASSERT(mods_enabled(2)); cleanup();
}
static void missing_folder_and_wrong_game_are_normal(void) {
  setup(); char path[1024]; snprintf(path,sizeof path,"%smods",data); ASSERT_EQ(rmdir(path),0);
  ASSERT(switch_mods_start(data,NULL,0)); ASSERT_EQ(mods_count(),0);
  package("seasons","error('wrong game must not run')",false);
  file("mods/seasons/mod.ini","id=seasons\napi=1\ngames=seasons\n");
  ASSERT(switch_mods_start(data,NULL,0)); ASSERT_EQ(mods_count(),0); cleanup();
}
static void corrupt_selection_never_runs_main(void) {
  setup(); package("off","error('must never execute')",false);
  file("mods.cfg","setting off 53686f77 999999999999999999999999999999\n");
  ASSERT(!switch_mods_start(data,NULL,0)); ASSERT_EQ(mods_count(),0); ASSERT(!features.treasure_swap);
  file("mods.cfg","broken configuration\n"); ASSERT(!switch_mods_start(data,NULL,0)); ASSERT_EQ(mods_count(),0); cleanup();
}
static void settings_restore_before_main_and_during_dynamic_registration(void) {
  setup(); package("dynamic","local x=oracles.setting('Start',true) assert(not x()) oracles.on_frame(function() local f=oracles.setting('Dynamic',true) oracles.log(f() and 'on' or 'off') end)",false);
  file("mods.cfg","setting dynamic 5374617274 0\nsetting dynamic 44796e616d6963 0\n");
  ASSERT(switch_mods_start(data,NULL,0)); ASSERT(mods_enabled(0)); ASSERT_EQ(mods_setting_count(),1);
  ASSERT(!switch_mods_present(&gb,pixels)); ASSERT_EQ(strcmp(mods_last_log(),"off"),0); ASSERT_EQ(mods_setting_count(),2);
  ASSERT_EQ(switch_mods_setting(1),0); ASSERT(switch_mods_change_setting(1,1));
  ASSERT(!switch_mods_present(&gb,pixels)); ASSERT_EQ(strcmp(mods_last_log(),"off"),0);
  ASSERT(switch_mods_start(data,NULL,0)); ASSERT(!switch_mods_present(&gb,pixels)); ASSERT_EQ(strcmp(mods_last_log(),"on"),0); cleanup();
}
static void seasons_uses_its_own_api_context_and_mod_filter(void) {
  setup(); package("only-ages","error('Ages mod must not run in Seasons')",false);
  package("only-seasons","assert(oracles.game()=='seasons') oracles.on_frame(function() local r=oracles.room() assert(r.room==73 and r.group==2) end)",false);
  file("mods/only-seasons/mod.ini","id=only-seasons\napi=1\ngames=seasons\n");
  uint8_t rom[0x151]={0}; memcpy(rom+0x134,"ZELDA DIN",9);
  ASSERT(switch_mods_start(data,rom,sizeof rom)); ASSERT_EQ(mods_count(),1); ASSERT(!strcmp(mods_id(0),"only-seasons"));
  sample.wram[0][0xc4c]=73; sample.wram[0][0xc49]=2; sample.wram[0][0xd0a]=20; sample.wram[0][0xd0b]=16;
  ASSERT(!switch_mods_present(&gb,pixels)); ASSERT(mods_enabled(0)); ASSERT(mods_current_frame()->seasons); cleanup();
}
static void menu_controller_navigation(void) {
  SwitchMenu m={0}; switch_menu_open(&m,true); m.tab=2; m.setting=3; m.mod_count=3;
  ASSERT_EQ(switch_menu_press(&m,UI_ACCEPT,false),SWITCH_MENU_NONE); ASSERT(m.mods_page);
  switch_menu_press(&m,UI_UP,false); ASSERT_EQ(m.mod_row,2);
  ASSERT_EQ(switch_menu_press(&m,UI_LEFT,false),SWITCH_MENU_MOD_CHANGE); ASSERT_EQ(m.mod_delta,-1);
  ASSERT_EQ(switch_menu_press(&m,UI_BACK,false),SWITCH_MENU_NONE); ASSERT(!m.mods_page);
}
int main(void) {
  RUN(selection_is_applied_before_main_and_survives_sessions);
  RUN(settings_are_staged_and_restored_on_next_session);
  RUN(snapshot_overlay_write_and_load_boundary);
  RUN(stores_recover_backup_and_reject_unsafe_paths);
  RUN(failed_config_write_rolls_back_selection);
  RUN(memory_profile_is_aggregate_and_releases_all_allocations);
  RUN(runtime_failure_does_not_stop_other_mods_or_rewind_lua);
  RUN(aggregate_profile_disables_only_the_over_budget_package);
  RUN(missing_folder_and_wrong_game_are_normal);
  RUN(corrupt_selection_never_runs_main);
  RUN(settings_restore_before_main_and_during_dynamic_registration);
  RUN(seasons_uses_its_own_api_context_and_mod_filter);
  RUN(menu_controller_navigation);
  return 0;
}
