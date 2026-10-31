#include "game/game.h"
#include "game/gen.h"
#include "script/npcs_events.h"

#undef CYC
#undef CYCT
#define CYC(from, to) burn_rom(gb, bk_, (from), (to), false)
#define CYCT(from, to) burn_rom(gb, bk_, (from), (to), true)

void patch_setStairTile_hook(GB *gb);

void patch_jump_hook(GB *gb) {
  BASE(patch_jump);
  CYC(b_+0, b_+1); H = D;
  CYC(b_+1, b_+3); L = 0x54;
  CYC(b_+3, b_+5); A = 0x80;
  CYC(b_+5, b_+6); EVENT_WRITE(HL, A); SET_HL(HL + 1);
  CYC(b_+6, b_+8); EVENT_WRITE(HL, 0xfe);
  CYC(b_+8, b_+10); A = 0x01;
  CYC(b_+10, b_+13); EVENT_WRITE(wDisabledObjects, A);
  CYC(b_+13, b_+16); EVENT_WRITE(wMenuDisabled, A);
  CYC(b_+16, b_+19); EVENT_WRITE(wOamEnd, A);
  CYC(b_+19, b_+21); A = 0x8f;
  CYC(b_+21, b_+24); TAIL(playSound_b00);
}

void patch_updateTextSubstitution_hook(GB *gb) {
  BASE(patch_updateTextSubstitution);
  CYC(b_+0, b_+3); A = mem_rd(gb, wTmpcfc0_patchMinigame_itemNameText);
  CYC(b_+3, b_+6); EVENT_WRITE(wTextSubstitutions, A);
  CYC(b_+6, b_+7); ret_effect(gb);
}

void patch_restoreControlAndStairs_hook(GB *gb) {
  BASE(patch_restoreControlAndStairs);
  CYC(b_+0, b_+1); alu_xor(gb, A);
  CYC(b_+1, b_+4); EVENT_WRITE(wDisabledObjects, A);
  CYC(b_+4, b_+7); EVENT_WRITE(wMenuDisabled, A);
  CYC(b_+7, b_+9); A = 0x44;
  TAIL(patch_setStairTile);
}

void patch_setStairTile_hook(GB *gb) {
  BASE(patch_setStairTile);
  uint16_t sp0_ = gb->sp;
  CYC(b_+0, b_+2); C = 0x49;
  NPC_CALL_C(b_+2, setTile_hook, SYM(setTile), b_+5);
  NPC_CALL_C(b_+5, getFreeInteractionSlot_hook, SYM(getFreeInteractionSlot), b_+8);
  if (!(F & FZ)) { RET_TAKEN(b_+8); return; }
  CYC(b_+8, b_+9);
  CYC(b_+9, b_+11); EVENT_WRITE(HL, 0x05);
  CYC(b_+11, b_+13); L = 0x4b;
  CYC(b_+13, b_+15); EVENT_WRITE(HL, 0x48);
  CYC(b_+15, b_+17); L = 0x4d;
  CYC(b_+17, b_+19); EVENT_WRITE(HL, 0x98);
  CYC(b_+19, b_+20); ret_effect(gb);
}

void patch_moveLinkPositionAtMinigameEnd_hook(GB *gb) {
  BASE(patch_moveLinkPositionAtMinigameEnd);
  uint16_t sp0_ = gb->sp;
  CYC(b_+0, b_+1); push_effect(gb, DE);
  NPC_CALL_C(b_+1, clearAllItemsAndPutLinkOnGround_hook, SYM(clearAllItemsAndPutLinkOnGround), b_+4);
  CYC(b_+4, b_+5); SET_DE(pop_effect(gb));
  NPC_CALL_C(b_+5, setLinkForceStateToState08_hook, SYM(setLinkForceStateToState08), b_+8);
  NPC_CALL_C(b_+8, resetLinkInvincibility_hook, SYM(resetLinkInvincibility), b_+11);
  CYC(b_+11, b_+13); L = 0x0b;
  CYC(b_+13, b_+15); EVENT_WRITE(HL, 0x48);
  CYC(b_+15, b_+17); L = 0x0d;
  CYC(b_+17, b_+19); EVENT_WRITE(HL, 0x78);
  CYC(b_+19, b_+21); L = 0x08;
  CYC(b_+21, b_+22); EVENT_WRITE(HL, A);
  CYC(b_+22, b_+23); A = alu_inc8(gb, A);
  CYC(b_+23, b_+26); EVENT_WRITE(wTmpcfc0_bigBangGame_prizeIndex, A);
  CYC(b_+26, b_+29); TAIL(resetCamera);
}

void patch_turnToFaceLink_hook(GB *gb) {
  BASE(patch_turnToFaceLink);
  uint16_t sp0_ = gb->sp;
  NPC_CALL_C(b_+0, objectGetAngleTowardLink_hook, SYM(objectGetAngleTowardLink), b_+3);
  CYC(b_+3, b_+5); alu_add(gb, 0x04);
  CYC(b_+5, b_+7); NPC_AND(0x18);
  CYC(b_+7, b_+9); alu_swap_a(gb);
  CYC(b_+9, b_+10); alu_rlca(gb);
  CYC(b_+10, b_+12); E = 0x48;
  CYC(b_+12, b_+13); EVENT_WRITE(HL, A);
  CYC(b_+13, b_+16); TAIL(interactionSetAnimation);
}
