#include "unit.h"
#include "platform/switch_menu_state.h"
#include "platform/setup.h"
#include <unistd.h>

int main(void) {
  SwitchMenu *m=calloc(1,sizeof *m); ASSERT(m); m->volume=10;
  bool previous=false;
  ASSERT(!switch_menu_chord(false,&previous));
  ASSERT(switch_menu_chord(true,&previous));
  for (int i=0;i<120;++i) ASSERT(!switch_menu_chord(true,&previous));
  ASSERT(!switch_menu_chord(false,&previous));
  ASSERT(switch_menu_chord(true,&previous));
  switch_menu_open(m,true);
  ASSERT_EQ(switch_menu_press(m,UI_ACCEPT,false),SWITCH_MENU_RESUME);
  for (int i=1;i<=4;++i) {
    switch_menu_press(m,UI_DOWN,false); ASSERT_EQ(m->row,i);
    ASSERT_EQ(switch_menu_press(m,UI_ACCEPT,false),SWITCH_MENU_NONE);
    ASSERT_EQ(switch_menu_press(m,UI_ACCEPT,true),SWITCH_MENU_SAVE);
    m->states[i].used=true;
    ASSERT_EQ(switch_menu_press(m,UI_ACCEPT,false),SWITCH_MENU_LOAD);
  }
  m->can_save=false;
  ASSERT_EQ(switch_menu_press(m,UI_ACCEPT,true),SWITCH_MENU_NONE);
  switch_menu_press(m,UI_DOWN,false); switch_menu_press(m,UI_ACCEPT,false);
  ASSERT(m->files_page);
  ASSERT_EQ(switch_menu_press(m,UI_ACCEPT,true),SWITCH_MENU_NONE);
  switch_menu_press(m,UI_DOWN,false); ASSERT_EQ(m->file,1);
  ASSERT_EQ(switch_menu_press(m,UI_ACCEPT,false),SWITCH_MENU_FILE);
  ASSERT_EQ(switch_menu_press(m,UI_BACK,false),SWITCH_MENU_NONE); ASSERT(!m->files_page);
  switch_menu_press(m,UI_DOWN,false);
  ASSERT_EQ(switch_menu_press(m,UI_ACCEPT,false),SWITCH_MENU_TITLE);
  switch_menu_press(m,UI_RIGHT,false); ASSERT_EQ(m->tab,1);
  ASSERT_EQ(switch_menu_press(m,UI_ACCEPT,false),SWITCH_MENU_NONE);
  switch_menu_press(m,UI_RIGHT,false); ASSERT_EQ(m->tab,2);
  ASSERT_EQ(switch_menu_press(m,UI_ACCEPT,true),SWITCH_MENU_NONE);
  for (int i=0;i<30;++i) switch_menu_press(m,UI_LEFT,false);
  ASSERT_EQ(m->volume,0);
  for (int i=0;i<30;++i) switch_menu_press(m,UI_RIGHT,false);
  ASSERT_EQ(m->volume,10);
  switch_menu_press(m,UI_DOWN,false); bool scale=m->integer_scale;
  switch_menu_press(m,UI_ACCEPT,false); ASSERT(m->integer_scale!=scale);
  switch_menu_press(m,UI_DOWN,false);
  ASSERT_EQ(switch_menu_press(m,UI_ACCEPT,false),SWITCH_MENU_EXIT);
  ASSERT_EQ(switch_menu_press(m,UI_BACK,false),SWITCH_MENU_RESUME);

  // Slot previews must validate an entire state and never follow its pointers.
  char path[]="/tmp/oracles-menu-preview-XXXXXX";
  int fd=mkstemp(path); ASSERT(fd>=0); close(fd);
  GB *gb=calloc(1,sizeof *gb); ASSERT(gb);
  gb->framebuffer[0]=31; gb->framebuffer[1]=31<<5; gb->framebuffer[2]=31<<10;
  gb->rom=(void *)(uintptr_t)0xdeadbeef; gb->sample=(void *)(uintptr_t)0xdeadbeef;
  ASSERT(oracles_save_boot_state(gb,path));
  ASSERT(switch_menu_read_state(path,&m->states[1]));
  ASSERT(m->states[1].used); ASSERT(m->states[1].time>0);
  ASSERT_EQ(m->states[1].thumb[0][0][0],255);
  ASSERT_EQ(m->states[1].thumb[0][1][1],255);
  ASSERT_EQ(m->states[1].thumb[0][2][2],255);
  FILE *f=fopen(path,"ab"); ASSERT(f); fputc(0,f); fclose(f);
  ASSERT(!switch_menu_read_state(path,&m->states[1])); ASSERT(!m->states[1].used);
  ASSERT_EQ(truncate(path,24),0);
  ASSERT(!switch_menu_read_state(path,&m->states[1]));
  f=fopen(path,"wb"); ASSERT(f); fputs("invalid",f); fclose(f);
  ASSERT(!switch_menu_read_state(path,&m->states[1]));
  remove(path); ASSERT(!switch_menu_read_state(path,&m->states[1]));
  free(gb); free(m);
  puts("ok switch_menu: chord edge, four slots, disabled save, tabs, files, settings, corrupt previews");
  return 0;
}
