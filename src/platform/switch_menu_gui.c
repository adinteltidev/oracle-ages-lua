// Same Clay widgets, VT323 font and theme as the Windows/macOS Home screen.
#include "platform/switch_menu.h"
#include "gui/widgets.h"
#ifdef ORACLES_SWITCH_MODS
#include "platform/switch_mods.h"
#include <string.h>
#define MOD_GROUP(index) CLAY({.id=CLAY_IDI("SwitchRow",index), \
  .layout={.sizing={CLAY_SIZING_GROW(0),CLAY_SIZING_FIT(0)},.layoutDirection=CLAY_TOP_TO_BOTTOM}})
// Long IDs and Lua filenames must not expand the controller list beyond the page.
static Clay_String mod_detail(const char *text) {
  char wrapped[1024]; size_t n=0; int word=0;
  for(;*text && n+2<sizeof wrapped;++text) {
    if(*text==' ' || *text=='\n' || *text=='\t') word=0;
    else if(word==30) { wrapped[n++]='\n'; word=0; }
    wrapped[n++]=*text; ++word;
  }
  wrapped[n]=0; return gui_str("%s",wrapped);
}
#endif

void switch_menu_draw(SwitchMenu *m, int w, int h) {
  GuiTheme theme; gui_theme(&theme, m->tab==2 ? m->game==0 : m->tab==0); const GuiTheme *t = &theme;
  GuiFrame f = {.w=w, .h=h, .safe={0,0,w,h}, .hints=true, .ok_key="A", .back_key="B"};
  static const char *const tabs[] = {"Ages", "Seasons", "Settings"};
  gui_begin(page_scale(&f));
  int selected = m->mods_page ? m->mod_row : m->files_page ? m->file : m->tab == 2 ? m->setting : m->row;
  keep_visible(CLAY_ID("SwitchList"), CLAY_IDI("SwitchRow", selected));
  PAGE(t, &f) {
    version_tabs(t,"SwitchTab",tabs,3,m->tab,false,ORACLES_SWITCH_VERSION);
    LIST("SwitchList") {
#ifdef ORACLES_SWITCH_MODS
      if(m->mods_page) {
        section(t,"Mods (API 1 / 2)");
        paragraph(CLAY_STRING("A enable/disable; options: Left/Right. Changes apply next play session."),SMALL,t->muted,20,0);
        if(switch_mods_pending()) paragraph(CLAY_STRING("Changes saved. Exit and open Play again to apply."),SMALL,t->muted,20,0);
        if(!mods_count()) paragraph(CLAY_STRING("No mods installed. See LEIA-ME.txt for SD installation."),TEXT,t->muted,20,12);
        for(int i=0;i<mods_count();++i) {
          MOD_GROUP(i) {
          row(t,(Row){.id=CLAY_IDI("SwitchModChoice",i),.enabled=true,.selected=m->mod_row==i,
            .label=gui_str("%.24s",mods_name(i)),
            .value=gui_str("%s / %s",switch_mods_wanted(i) ? "Next On" : "Next Off",mods_enabled(i) ? "On" : "Off")});
          paragraph(mod_detail(mods_id(i)),SMALL,t->muted,20,0);
          if(m->mod_row==i && strlen(mods_name(i))>24) paragraph(mod_detail(mods_name(i)),SMALL,t->muted,20,0);
          if(mods_error(i)[0]) {
            const char *error=mods_error(i),*file=strstr(error,"/main.lua:");
            paragraph(mod_detail(file ? file+1 : error),SMALL,t->muted,20,0);
          }
          }
        }
        for(int i=0;i<mods_setting_count();++i) {
          int j=mods_count()+i;
          MOD_GROUP(j) {
          row(t,(Row){.id=CLAY_IDI("SwitchModSetting",j),.enabled=true,.selected=m->mod_row==j,
            .label=gui_str("%.12s: %.12s",mods_setting_mod_id(i),mods_setting_name(i)),
            .value=mods_setting_kind(i)==MOD_SETTING_BOOL ? gui_str("%s",switch_mods_setting(i) ? "On" : "Off") : gui_str("%d",switch_mods_setting(i))});
          if(m->mod_row==j) paragraph(mod_detail(gui_str("%s: %s",mods_setting_mod_id(i),mods_setting_name(i)).chars),SMALL,t->muted,20,0);
          }
        }
      } else
#endif
      if (m->tab<2 && m->tab!=m->game) {
        section(t, "Switch game");
        paragraph(CLAY_STRING("Battery progress is saved before switching. Each game keeps its own states and mods."),TEXT,t->muted,20,12);
        row(t,(Row){.id=CLAY_IDI("SwitchRow",0),.enabled=true,.selected=true,
          .label=m->tab ? CLAY_STRING("Start Oracle of Seasons") : CLAY_STRING("Start Oracle of Ages"),.value=CLAY_STRING(">")});
      } else if (m->tab == 2) {
        section(t, "Settings");
        #ifdef ORACLES_SWITCH_MODS
        const int settings_count=4;
#else
        const int settings_count=3;
#endif
        for (int i=0; i<settings_count; ++i) {
          row(t, (Row){.id=CLAY_IDI("SwitchRow",i), .enabled=true, .selected=m->setting==i,
            .label=gui_str("%s", i==0 ? "Volume" : i==1 ? "Integer scale" : i==2 ? "Exit game" : "Mods"),
            .value=i==0 ? gui_str("%d / 10",m->volume) : i==1 ? gui_str("%s",m->integer_scale ? "On" : "Off") : CLAY_STRING(">")});
        }
        paragraph(CLAY_STRING("Left/Right change   Minus returns to game"), SMALL, t->muted, 20, 0);
      } else if (m->files_page) {
        section(t, "Save files");
        for (int i=0; i<3; ++i) {
          row(t, (Row){.id=CLAY_IDI("SwitchRow",i), .enabled=true, .selected=m->file==i,
            .label=gui_str("%d  %s",i+1,m->files[i].valid ? m->files[i].name : "Empty"),
            .value=m->files[i].valid ? gui_str("%d hearts",m->files[i].max_health/4) : CLAY_STRING("")});
        }
        paragraph(CLAY_STRING("A opens the game's file screen at the selected file."), SMALL, t->muted, 20, 0);
      } else {
        int latest=-1;
        for (int i=1;i<=4;++i) if (m->states[i].used && (latest<0 || m->states[i].time>m->states[latest].time)) latest=i;
        for (int i=0;i<=6;++i) {
          if (i==0) section(t,"Continue");
          if (i==1) section(t,"Save states");
          if (i==5) section(t,"More");
          bool sel=m->row==i;
          if (i<=4) {
            const UiSlot *s=&m->states[i];
            SDL_Texture *tex=s->used ? gui_image(i,&s->thumb[0][0][0],UI_W,UI_H,m->stamp) : NULL;
            PICTURE_ROW(t,CLAY_IDI("SwitchRow",i),sel) {
              state_info(t,tex,SMALL_THUMB_W,i==0 ? CLAY_STRING("Resume") : gui_str("Slot %d",i),i==latest,
                s->used ? when_str(s->time,now_time()) : CLAY_STRING("Empty"),sel,i==0 || s->used);
            }
          } else row(t,(Row){.id=CLAY_IDI("SwitchRow",i),.enabled=true,.selected=sel,
            .label=i==5 ? CLAY_STRING("Save files") : CLAY_STRING("Title screen"),.value=CLAY_STRING(">")});
        }
      }
    }
    hint_line(t,&f,m->mods_page ? "A toggle   Left/Right option   B back" : m->tab==2 ? "A change   B resume   Minus game" : m->tab<2 && m->tab!=m->game ? "A switch game   B resume   Left/Right tabs" : m->files_page ? "A select   B back" : "A load/play   X save   B resume   L+R menu");
  }
  gui_end();
}
