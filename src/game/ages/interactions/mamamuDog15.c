#include "game/game.h"
#include "game/gen.h"
#include "script/npcs_events.h"

#undef CYC
#undef CYCT
#define CYC(from, to) burn_rom(gb, bk_, (from), (to), false)
#define CYCT(from, to) burn_rom(gb, bk_, (from), (to), true)

void writeFlagsTocddb_hook(GB *gb);

// ref/oracles-disasm/scripts/ages/scriptHelper.s (INTERAC_MAMAMU_DOG), bank 0x15.

void mamamuDog_hop_hook(GB *gb);
void mamamuDog_setZPositionTo0_hook(GB *gb);

static void mamamuDog15_addAToHl_from_rst(GB *gb, uint16_t return_address) {
  push_effect(gb, return_address);
  burn_rom(gb, 0x00, 0x0010, 0x0011, false); alu_add(gb, L);
  burn_rom(gb, 0x00, 0x0011, 0x0012, false); L = A;
  if (!(F & FC)) {
    burn_rom(gb, 0x00, 0x0012, 0x0013, true); ret_effect(gb);
    return;
  }
  burn_rom(gb, 0x00, 0x0012, 0x0013, false);
  burn_rom(gb, 0x00, 0x0013, 0x0014, false); H = alu_inc8(gb, H);
  burn_rom(gb, 0x00, 0x0014, 0x0015, false); ret_effect(gb);
}

// Reverse direction if x-position gets too high or low.
void mamamuDog_checkReverseDirection_hook(GB *gb) {
  BASE(mamamuDog_checkReverseDirection);
  uint16_t sp0_ = gb->sp;
  NPC_CALL_C(b_+0, objectApplySpeed_hook, SYM(objectApplySpeed), b_+3);
  CYC(b_+3, b_+5); E = INTERACTION_BASE + OBJ_XH;
  CYC(b_+5, b_+6); A = mem_rd(gb, DE);
  CYC(b_+6, b_+8); alu_sub(gb, 0x18);
  CYC(b_+8, b_+10); NPC_CP(0x70);
  if (F & FC) { RET_TAKEN(b_+10); return; }
  CYC(b_+10, b_+11);
  CYC(b_+11, b_+12); H = D;
  CYC(b_+12, b_+14); L = INTERACTION_BASE + OBJ_ANGLE;
  CYC(b_+14, b_+15); A = mem_rd(gb, HL);
  CYC(b_+15, b_+17); NPC_XOR(0x10);
  CYC(b_+17, b_+18); EVENT_WRITE(HL, A);
  CYC(b_+18, b_+20); B = 0x01;
  CYC(b_+20, b_+22); // jr $5eac -- shared tail duplicated below (matches mamamuDog_reverseDirection)
  CYC((SYM(mamamuDog_reverseDirection) + 2), (SYM(mamamuDog_reverseDirection) + 3)); H = D;
  CYC((SYM(mamamuDog_reverseDirection) + 3), (SYM(mamamuDog_reverseDirection) + 5)); L = INTERACTION_BASE + OBJ_VAR3F;
  CYC((SYM(mamamuDog_reverseDirection) + 5), (SYM(mamamuDog_reverseDirection) + 6)); A = mem_rd(gb, HL);
  CYC((SYM(mamamuDog_reverseDirection) + 6), (SYM(mamamuDog_reverseDirection) + 7)); NPC_XOR(B);
  CYC((SYM(mamamuDog_reverseDirection) + 7), (SYM(mamamuDog_reverseDirection) + 8)); EVENT_WRITE(HL, A);
  CYC((SYM(mamamuDog_reverseDirection) + 8), (SYM(mamamuDog_reverseDirection) + 11)); TAIL(interactionSetAnimation); // jp
}

void mamamuDog_reverseDirection_hook(GB *gb) {
  BASE(mamamuDog_reverseDirection);
  uint16_t sp0_ = gb->sp;
  (void)sp0_;
  CYC(b_+0, b_+2); B = 0x02;
  CYC(b_+2, b_+3); H = D;
  CYC(b_+3, b_+5); L = INTERACTION_BASE + OBJ_VAR3F;
  CYC(b_+5, b_+6); A = mem_rd(gb, HL);
  CYC(b_+6, b_+7); NPC_XOR(B);
  CYC(b_+7, b_+8); EVENT_WRITE(HL, A);
  CYC(b_+8, b_+11); TAIL(interactionSetAnimation); // jp
}

void mamamuDog_setCounterRandomly_hook(GB *gb) {
  BASE(mamamuDog_setCounterRandomly);
  uint16_t sp0_ = gb->sp;
  NPC_CALL_C(b_+0, getRandomNumber_hook, SYM(getRandomNumber), b_+3);
  CYC(b_+3, b_+5); NPC_AND(0x07);
  CYC(b_+5, b_+8); SET_HL(SYM(mamamuDog_randomCounterValues)); // mamamuDog_randomCounterValues
  CYC(b_+8, b_+9); NPC_LOCAL_CALL(mamamuDog15_addAToHl_from_rst(gb, b_+9));
  CYC(b_+9, b_+10); A = mem_rd(gb, HL);
  CYC(b_+10, b_+12); E = INTERACTION_BASE + OBJ_VAR3E;
  CYC(b_+12, b_+13); EVENT_WRITE(DE, A);
  NPC_CALL_C(b_+13, mamamuDog_hop_hook, SYM(mamamuDog_hop), SYM(mamamuDog_setZPositionTo0));
  TAIL(mamamuDog_setZPositionTo0); // fallthrough
}

void mamamuDog_setZPositionTo0_hook(GB *gb) {
  BASE(mamamuDog_setZPositionTo0);
  uint16_t sp0_ = gb->sp;
  (void)sp0_;
  CYC(b_+0, b_+1); H = D;
  CYC(b_+1, b_+3); L = INTERACTION_BASE + OBJ_Z;
  CYC(b_+3, b_+4); alu_xor(gb, A);
  CYC(b_+4, b_+5); EVENT_WRITE(HL, A); SET_HL(HL + 1);
  CYC(b_+5, b_+6); EVENT_WRITE(HL, A);
  RET(b_+6); return;
}

void mamamuDog_updateSpeedZ_hook(GB *gb) {
  BASE(mamamuDog_updateSpeedZ);
  uint16_t sp0_ = gb->sp;
  CYC(b_+0, b_+2); C = 0x20;
  NPC_CALL_C(b_+2, objectUpdateSpeedZ_paramC_hook, SYM(objectUpdateSpeedZ_paramC), b_+5);
  if (!(F & FZ)) { RET_TAKEN(b_+5); return; }
  CYC(b_+5, b_+6);
  TAIL(mamamuDog_hop); // fallthrough
}

void mamamuDog_hop_hook(GB *gb) {
  BASE(mamamuDog_hop);
  uint16_t sp0_ = gb->sp;
  (void)sp0_;
  CYC(b_+0, b_+3); SET_BC(0xff40); // -$c0
  CYC(b_+3, b_+6); TAIL(objectSetSpeedZ); // jp
}

void mamamuDog_decCounter_hook(GB *gb) {
  BASE(mamamuDog_decCounter);
  uint16_t sp0_ = gb->sp;
  (void)sp0_;
  CYC(b_+0, b_+1); H = D;
  CYC(b_+1, b_+3); L = INTERACTION_BASE + OBJ_VAR3E;
  CYC(b_+3, b_+4); EVENT_WRITE(HL, alu_dec8(gb, mem_rd(gb, HL)));
  CYC(b_+4, b_+7); TAIL(writeFlagsTocddb); // jp
}
