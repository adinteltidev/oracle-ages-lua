#include "game/game.h"
#include "game/gen.h"
#include "script/npcs_events.h"

#undef CYC
#undef CYCT
#define CYC(from, to) burn_rom(gb, bk_, (from), (to), false)
#define CYCT(from, to) burn_rom(gb, bk_, (from), (to), true)

void greatFairyOctorok_createMagicPowderAnimation_hook(GB *gb);

void greatFairyOctorok_createMagicPowderAnimation_hook(GB *gb) {
  BASE(greatFairyOctorok_createMagicPowderAnimation);
  uint16_t sp0_ = gb->sp;
  CYC(b_+0, b_+2); A = 0x83;
  NPC_CALL_C(b_+2, playSound_b00_hook, SYM(playSound_b00), b_+5);
  CYC(b_+5, b_+8); SET_BC(0x00f8);

next:
  NPC_CALL_C(b_+8, getFreePartSlot_hook, SYM(getFreePartSlot), b_+11);
  if (!(F & FZ)) {
    RET_TAKEN(b_+11); return;
  }
  CYC(b_+11, b_+12);
  CYC(b_+12, b_+14); EVENT_WRITE(HL, 0x26);
  CYC(b_+14, b_+16); L = 0xc3;
  CYC(b_+16, b_+17); EVENT_WRITE(HL, alu_inc8(gb, mem_rd(gb, HL)));
  NPC_CALL_C(b_+17, objectCopyPositionWithOffset_hook, SYM(objectCopyPositionWithOffset), b_+20);
  CYC(b_+20, b_+21); A = C;
  CYC(b_+21, b_+23); alu_add(gb, 0x08);
  CYC(b_+23, b_+24); C = A;
  CYC(b_+24, b_+26); NPC_CP(0x18);
  if (!(F & FZ)) {
    CYCT(b_+26, b_+28);
    goto next;
  }
  CYC(b_+26, b_+28);
  RET(b_+28);
}
