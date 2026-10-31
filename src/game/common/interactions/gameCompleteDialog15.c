#include "game/game.h"
#include "game/gen.h"
#include "script/npcs_events.h"

#undef CYC
#undef CYCT
#define CYC(from, to) burn_rom(gb, bk_, (from), (to), false)
#define CYCT(from, to) burn_rom(gb, bk_, (from), (to), true)

void gameCompleteDialog_markGameAsComplete_hook(GB *gb);

void gameCompleteDialog_markGameAsComplete_hook(GB *gb) {
  BASE(gameCompleteDialog_markGameAsComplete);
  CYC(b_+0, b_+1); alu_xor(gb, A);
  CYC(b_+1, b_+4); EVENT_WRITE(wMapleKillCounter, A);
  CYC(b_+4, b_+5); A = alu_inc8(gb, A);
  CYC(b_+5, b_+8); EVENT_WRITE(wFileIsCompleted, A);
  if (game_seasons) {
    CYC(b_+S(8), b_+S(10)); A = 0x28;
    CYC(b_+S(10), b_+S(13)); TAIL(setGlobalFlag);
  }
  CYC(b_+8, b_+10); A = 0x1c;
  CYC(b_+10, b_+13); EVENT_WRITE(wMakuMapTextPresent, A);
  CYC(b_+13, b_+15); A = 0x8c;
  CYC(b_+15, b_+18); EVENT_WRITE(wMakuMapTextPast, A);
  CYC(b_+18, b_+20); A = 0x14;
  CYC(b_+20, b_+23); TAIL(setGlobalFlag);
}
