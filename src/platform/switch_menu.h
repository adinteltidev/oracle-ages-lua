#pragma once
#include "ui/menu.h"

typedef enum { SWITCH_MENU_NONE, SWITCH_MENU_RESUME, SWITCH_MENU_LOAD,
  SWITCH_MENU_SAVE, SWITCH_MENU_CHANGE_GAME, SWITCH_MENU_MOD_CHANGE, SWITCH_MENU_TITLE, SWITCH_MENU_FILE, SWITCH_MENU_EXIT } SwitchMenuAction;
typedef struct {
  UiSlot states[5]; // Resume followed by four persistent slots.
  UiFileInfo files[3];
  int tab, row, setting, file, volume;
  int game; // Currently running game: 0 Ages, 1 Seasons.
  int mod_row, mod_count, mod_delta;
  bool mods_page;
  bool files_page, integer_scale, can_save;
  uint32_t stamp;
} SwitchMenu;
// One edge when both shoulders become held. Holding the chord cannot repeat.
bool switch_menu_chord(bool both, bool *previous);
void switch_menu_open(SwitchMenu *m, bool can_save);
SwitchMenuAction switch_menu_press(SwitchMenu *m, UiButton button, bool save);
void switch_menu_draw(SwitchMenu *m, int width, int height);
