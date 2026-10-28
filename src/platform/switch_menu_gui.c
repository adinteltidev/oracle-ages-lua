// Same Clay widgets, VT323 font and theme as the Windows/macOS Home screen.
#include "platform/switch_menu.h"
#include "gui/widgets.h"

void switch_menu_draw(SwitchMenu *m, int w, int h) {
  GuiTheme theme; gui_theme(&theme, true); const GuiTheme *t = &theme;
  GuiFrame f = {.w=w, .h=h, .safe={0,0,w,h}, .hints=true, .ok_key="A", .back_key="B"};
  static const char *const tabs[] = {"Ages", "Seasons", "Settings"};
  gui_begin(page_scale(&f));
  int selected = m->files_page ? m->file : m->tab == 2 ? m->setting : m->row;
  keep_visible(CLAY_ID("SwitchList"), CLAY_IDI("SwitchRow", selected));
  PAGE(t, &f) {
    CLAY({.layout={.sizing={CLAY_SIZING_GROW(0),CLAY_SIZING_FIXED(gui_px(52))},
                    .childGap=gui_px(8)},
          .border={.color=gui_rgb(t->line),.width={.bottom=1}}}) {
      tab_bar(t, "SwitchTab", tabs, 3, m->tab, false);
      CLAY({.id=CLAY_ID("SwitchVersion"),
            .layout={.sizing={CLAY_SIZING_FIT(0),CLAY_SIZING_GROW(0)},
                     .padding={0,gui_px(14),0,gui_px(4)},
                     .childAlignment={.y=CLAY_ALIGN_Y_CENTER}}}) {
        label("v" ORACLES_SWITCH_VERSION,TAG,t->muted);
      }
    }
    LIST("SwitchList") {
      if (m->tab == 1) {
        section(t, "Not available");
        paragraph(CLAY_STRING("This Switch build supports Oracle of Ages (USA/Australia)."), TEXT, t->muted, 20, 12);
      } else if (m->tab == 2) {
        section(t, "Settings");
        for (int i=0; i<3; ++i) {
          row(t, (Row){.id=CLAY_IDI("SwitchRow",i), .enabled=true, .selected=m->setting==i,
            .label=gui_str("%s", i==0 ? "Volume" : i==1 ? "Integer scale" : "Exit game"),
            .value=i==0 ? gui_str("%d / 10",m->volume) : i==1 ? gui_str("%s",m->integer_scale ? "On" : "Off") : CLAY_STRING(">")});
        }
        paragraph(CLAY_STRING("Left/Right change   Minus returns to Ages"), SMALL, t->muted, 20, 0);
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
    hint_line(t,&f,m->tab==2 ? "A change   B resume   Minus Ages" : m->files_page ? "A select   B back" : "A load/play   X save   B resume   L+R menu");
  }
  gui_end();
}
