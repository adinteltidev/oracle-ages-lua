#include "platform/switch_menu_boot.h"
#include "rt/fibers.h"
#include "game/game.h"
#include <string.h>
// Navigate the original game's file screen using neutral input between pulses.
void switch_menu_restart(GB *game, SwitchMenuBoot *boot, int file) {
  uint8_t (*input_at)(void *,uint64_t)=game->input_at; void *input_ctx=game->input_ctx;
  uint8_t eram[sizeof game->eram]; memcpy(eram,game->eram,sizeof eram);
  // Imported boot states may retain an initial-WRAM fixture. A new title/file
  // boot must start with clean WRAM, rather than stale file-screen flags.
  game->init_ram_loaded=false;
  fibers_reset(game); gb_reset(game); memcpy(game->eram,eram,sizeof eram);
  game->input_at=input_at; game->input_ctx=input_ctx;
  boot->file=file; boot->phase=0; boot->ticks=0;
}
uint8_t switch_menu_boot_input(GB *game, SwitchMenuBoot *boot, uint8_t live_joy) {
  if (boot->file<0) return live_joy;
  bool pulse=(boot->ticks++/6)%2==0;
  if (boot->ticks>3600) { boot->file=-1; return 0; }
  unsigned mode=game->wram[0][wFileSelect_mode&0xfff];
  unsigned sub=game->wram[0][wFileSelect_mode2&0xfff];
  if (!boot->phase) {
    if (game->wram[0][0x2e6]==3 && game->wram[0][0x2e7]==3) { boot->phase=1; return 0; }
    return pulse ? JOY_START : 0;
  }
  if (mode==8) return live_joy; // European language selection belongs to the player.
  if (mode!=1 || sub!=1) return 0;
  if (game->wram[0][wFileSelect_cursorPos&0xfff]==boot->file) { boot->file=-1; return 0; }
  return pulse ? JOY_DOWN : 0;
}
