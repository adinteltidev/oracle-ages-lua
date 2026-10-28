#include "platform/switch_menu.h"

bool switch_menu_chord(bool both, bool *previous) {
  bool edge = both && !*previous; *previous = both; return edge;
}
void switch_menu_open(SwitchMenu *m, bool can_save) {
  m->tab = m->row = m->setting = m->file = 0;
  m->files_page = false; m->can_save = can_save; ++m->stamp;
}
SwitchMenuAction switch_menu_press(SwitchMenu *m, UiButton b, bool save) {
  if (save && (m->tab != 0 || m->files_page || m->row < 1 || m->row > 4)) return SWITCH_MENU_NONE;
  if (b == UI_BACK) {
    if (m->files_page) { m->files_page = false; return SWITCH_MENU_NONE; }
    return SWITCH_MENU_RESUME;
  }
  if (m->tab == 2) {
    if (b == UI_UP || b == UI_DOWN) m->setting = (m->setting + (b == UI_UP ? 2 : 1)) % 3;
    else if (b == UI_LEFT || b == UI_RIGHT || b == UI_ACCEPT) {
      if (m->setting == 0) {
        m->volume += b == UI_LEFT ? -1 : 1;
        if (m->volume < 0) m->volume = 0;
        if (m->volume > 10) m->volume = 10;
      } else if (m->setting == 1) m->integer_scale = !m->integer_scale;
      else if (b == UI_ACCEPT) return SWITCH_MENU_EXIT;
    }
    return SWITCH_MENU_NONE;
  }
  if (!m->files_page && (b == UI_LEFT || b == UI_RIGHT)) {
    m->tab = (m->tab + (b == UI_LEFT ? 2 : 1)) % 3;
    return SWITCH_MENU_NONE;
  }
  if (m->tab == 1) return SWITCH_MENU_NONE; // Only Ages is supported by this port.
  if (m->files_page) {
    if (b == UI_UP || b == UI_DOWN) m->file = (m->file + (b == UI_UP ? 2 : 1)) % 3;
    if (b == UI_ACCEPT) return SWITCH_MENU_FILE;
    return SWITCH_MENU_NONE;
  }
  if (b == UI_UP || b == UI_DOWN) m->row = (m->row + (b == UI_UP ? 6 : 1)) % 7;
  if (m->row >= 1 && m->row <= 4 && save && m->can_save) return SWITCH_MENU_SAVE;
  if (b != UI_ACCEPT || save) return SWITCH_MENU_NONE;
  if (m->row == 0) return SWITCH_MENU_RESUME;
  if (m->row <= 4) return m->states[m->row].used ? SWITCH_MENU_LOAD : SWITCH_MENU_NONE;
  if (m->row == 5) m->files_page = true;
  if (m->row == 6) return SWITCH_MENU_TITLE;
  return SWITCH_MENU_NONE;
}
