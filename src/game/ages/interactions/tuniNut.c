#include "game/game.h"
#include "game/gen.h"
#include "script/npcs_events.h"

#undef CYC
#undef CYCT
#define CYC(from, to) burn_rom(gb, bk_, (from), (to), false)
#define CYCT(from, to) burn_rom(gb, bk_, (from), (to), true)

void interactionCodeb1_hook(GB *gb) {
  BASE(interactionCodeb1);
  uint16_t sp0_ = gb->sp; (void)sp0_;
  CYC(b_+0, b_+3); SET_HL(RV((SYM(interactiond7_makuSeed__state4Substate1) + 5), 0x7b8f));
  CYC(b_+3, b_+5); E = 0x3f;
  CYC(b_+5, b_+8); TAIL(interBankCall);
}

void interactionCodeb2__state0_hook(GB *gb) {
  BASE(interactionCodeb2);
  uint16_t sp0_ = gb->sp; (void)sp0_;
  CYC(b_+60, b_+61); A = alu_inc8(gb, A);
  CYC(b_+61, b_+62); EVENT_WRITE(DE, A);
  CYC(b_+62, b_+65); EVENT_WRITE(wScreenShakeMagnitude, A);
  CYC(b_+65, b_+68); SET_HL(b_+117);
  CYC(b_+68, b_+71); TAIL(interactionSetMiniScript);
}

// interactionCodeb2@setRandomCounter1: counter1 = (random & var30) + var31.
static void volcano_set_random_counter1(GB *gb) {
  BASE(interactionCodeb2__setRandomCounter1);
  uint16_t sp0_ = cpu_sp(gb); (void)sp0_;
  NPC_CALL_C(b_+0, getRandomNumber_noPreserveVars_hook, SYM(getRandomNumber_noPreserveVars), b_+3);
  H = D;
  L = INTERACTION_BASE + OBJ_USE_TEXT_ID;
  CYC(b_+3, b_+6);
  CYC(b_+6, b_+7); NPC_AND(mem_rd(gb, HL));
  L = alu_inc8(gb, L);
  CYC(b_+7, b_+8);
  CYC(b_+8, b_+9); alu_add(gb, mem_rd(gb, HL));
  L = INTERACTION_BASE + OBJ_COUNTER1;
  CYC(b_+9, b_+11);
  CYC(b_+11, b_+12); EVENT_WRITE(HL, A);
  CYC(b_+12, b_+13); ret_effect(gb);
}

// interactionCodeb2@runScript: reads the next screen-shake entry from the mini script,
// restarting it at $ff, then falls into @setRandomCounter1.
static void volcano_run_script(GB *gb) {
  BASE(interactionCodeb2__runScript);
  uint16_t sp0_ = cpu_sp(gb); (void)sp0_;
  for (;;) {
    NPC_CALL_C(b_+0, interactionGetMiniScript_hook, SYM(interactionGetMiniScript), b_+3);
    CYC(b_+3, b_+4); A = mem_rd(gb, HL); SET_HL(HL + 1);
    NPC_CP(0xff);
    CYC(b_+4, b_+6);
    if (!(F & FZ)) { CYCT(b_+6, b_+8); break; }
    CYC(b_+6, b_+8);
    SET_HL(SYM(interactionCodeb2__script));
    CYC(b_+8, b_+11);
    NPC_CALL_C(b_+11, interactionSetMiniScript_hook, SYM(interactionSetMiniScript), b_+14);
    CYCT(b_+14, b_+16);
  }
  CYC(b_+16, b_+19); EVENT_WRITE(wScreenShakeCounterY, A);
  CYC(b_+19, b_+20); A = mem_rd(gb, HL); SET_HL(HL + 1);
  CYC(b_+20, b_+23); EVENT_WRITE(wScreenShakeCounterX, A);
  E = INTERACTION_BASE + OBJ_USE_TEXT_ID;
  CYC(b_+23, b_+25);
  CYC(b_+25, b_+26); A = mem_rd(gb, HL); SET_HL(HL + 1);
  CYC(b_+26, b_+27); EVENT_WRITE(DE, A);
  E = alu_inc8(gb, E);
  CYC(b_+27, b_+28);
  CYC(b_+28, b_+29); A = mem_rd(gb, HL); SET_HL(HL + 1);
  CYC(b_+29, b_+30); EVENT_WRITE(DE, A);
  NPC_CALL_C(b_+30, interactionSetMiniScript_hook, SYM(interactionSetMiniScript), b_+33);
  NPC_LOCAL_CALL(volcano_set_random_counter1(gb));
}

void interactionCodeb2__state1_hook(GB *gb) {
  BASE(interactionCodeb2);
  uint16_t sp0_ = gb->sp;
  CYC(b_+5, b_+8); A = mem_rd(gb, wFrameCounter);
  CYC(b_+8, b_+10); NPC_AND(0x0f);
  CYC(b_+10, b_+12); A = 0xb3;
  if (F & FZ) NPC_CALL_C_CC(b_+12, playSound_b00_hook, SYM(playSound_b00), b_+15); else CYC(b_+12, b_+15);
  CYC(b_+15, b_+18); A = mem_rd(gb, wScreenShakeCounterY);
  CYC(b_+18, b_+19); NPC_OR(A);
  if (!(F & FZ)) { CYCT(b_+19, b_+21); goto update; }
  CYC(b_+19, b_+21);
  CYC(b_+21, b_+24); A = mem_rd(gb, wScreenShakeCounterX);
  CYC(b_+24, b_+25); NPC_OR(A);
  if (F & FZ) { CYCT(b_+25, b_+28); push_effect(gb, b_+28); NPC_LOCAL_CALL(volcano_run_script(gb)); } else CYC(b_+25, b_+28);
update:
  NPC_CALL_ZERO(b_+28, interactionDecCounter1_hook, SYM(interactionDecCounter1), b_+31);
  if (!(F & FZ)) { CYCT(b_+31, b_+32); ret_effect(gb); return; }
  CYC(b_+31, b_+32);
  CYC(b_+32, b_+35); push_effect(gb, b_+35); NPC_LOCAL_CALL(volcano_set_random_counter1(gb));
  CYC(b_+35, b_+37); C = 0x0f;
  NPC_CALL_C(b_+37, getRandomNumber_hook, SYM(getRandomNumber), b_+40);
  CYC(b_+40, b_+41); NPC_AND(C);
  CYC(b_+41, b_+43); C = alu_srl(gb, C);
  CYC(b_+43, b_+44); C = alu_inc8(gb, C);
  CYC(b_+44, b_+45); alu_sub(gb, C);
  CYC(b_+45, b_+46); C = A;
  NPC_CALL_C(b_+46, getFreePartSlot_hook, SYM(getFreePartSlot), b_+49);
  if (!(F & FZ)) { CYCT(b_+49, b_+50); ret_effect(gb); return; }
  CYC(b_+49, b_+50);
  CYC(b_+50, b_+52); EVENT_WRITE(HL, 0x11);
  CYC(b_+52, b_+53); L = alu_inc8(gb, L);
  CYC(b_+53, b_+55); EVENT_WRITE(HL, 1);
  CYC(b_+55, b_+57); B = 0;
  CYC(b_+57, b_+60); TAIL(objectCopyPositionWithOffset);
}

void interactionCodeb2_hook(GB *gb) {
  BASE(interactionCodeb2);
  uint16_t sp0_ = gb->sp;
  NPC_CALL_ZERO(b_+0, checkInteractionState_hook, SYM(checkInteractionState), b_+3);
  if (F & FZ) { CYCT(b_+3, b_+5); interactionCodeb2__state0_hook(gb); return; }
  CYC(b_+3, b_+5); TAIL(interactionCodeb2__state1);
}
