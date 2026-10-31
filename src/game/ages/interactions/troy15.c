#include "game/game.h"
#include "game/gen.h"
#include "script/npcs_events.h"

#undef CYC
#undef CYCT
#define CYC(from, to) burn_rom(gb, bk_, (from), (to), false)
#define CYCT(from, to) burn_rom(gb, bk_, (from), (to), true)

void troy_chooseRandomAnimalText_hook(GB *gb) {
  BASE(troy_chooseRandomAnimalText);
  uint16_t sp0_ = gb->sp;
  NPC_CALL_C(b_+0, getRandomNumber_hook, SYM(getRandomNumber), b_+3);
  CYC(b_+3, b_+5); NPC_AND(0x0f);
  CYC(b_+5, b_+7); alu_add(gb, 0x13);
  CYC(b_+7, b_+10); EVENT_WRITE(wTextSubstitutions, A);
  CYC(b_+10, b_+11); ret_effect(gb);
}
