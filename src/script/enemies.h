#pragma once
#include "script/gameplay.h"
struct GB;
typedef enum {
  GAMEPLAY_ENEMY_condition, GAMEPLAY_ENEMY_transition, GAMEPLAY_ENEMY_angle,
  GAMEPLAY_ENEMY_direction, GAMEPLAY_ENEMY_speed, GAMEPLAY_ENEMY_octorok_attack,
  GAMEPLAY_ENEMY_octorok_aim, GAMEPLAY_ENEMY_compare, GAMEPLAY_ENEMY_zero, GAMEPLAY_ENEMY_bit_zero, GAMEPLAY_ENEMY_COUNT
} GameplayEnemyRule;
bool gameplay_enemy_compare(uint8_t a,uint8_t b,uint8_t *out);
bool gameplay_enemy_zero(uint8_t a,bool *out);
bool gameplay_enemy_bit_zero(uint8_t a,uint8_t b,bool *out);
bool gameplay_enemies_enabled(void);
bool gameplay_enemy_condition(uint8_t flags,uint8_t mask,bool *out);
bool gameplay_enemy_transition(uint8_t current,uint8_t requested,uint8_t *out);
bool gameplay_enemy_angle(uint8_t current,uint8_t requested,uint8_t *out);
bool gameplay_enemy_direction(uint8_t current,uint8_t requested,uint8_t *out);
bool gameplay_enemy_speed(uint8_t current,uint8_t requested,uint8_t *out);
bool gameplay_enemy_octorok_attack(uint8_t random,uint8_t mask,bool *out);
bool gameplay_enemy_octorok_aim(uint8_t subid,bool *out);
uint64_t gameplay_enemy_calls(GameplayEnemyRule rule);
const char *gameplay_enemy_rule_name(GameplayEnemyRule rule);
void gameplay_enemy_note(struct GB *gb);
uint64_t gameplay_enemy_owner_calls(uint8_t owner);
#ifdef ORACLES_LUA_GAMEPLAY
#define ENEMY_ACTIVE (!game_seasons && gameplay_enemies_enabled())
#define ENEMY_STOP() do {gb_stop(gb,gameplay_error());return;}while(0)
#define ENEMY_FAILED() do {if(gb->hung && ENEMY_ACTIVE && gameplay_error()[0])return;}while(0)
// The expression is used only in void controllers, never ROM-address helpers.
#define ENEMY_CP(value) do {uint8_t oe_a_=A,oe_b_=(value);alu_cp(gb,oe_b_);if(ENEMY_ACTIVE){uint8_t oe_relation_;if(!gameplay_enemy_compare(oe_a_,oe_b_,&oe_relation_))ENEMY_STOP();F=(uint8_t)((F&~(FZ|FC))|(oe_relation_==0?FZ:0)|(oe_relation_==1?FC:0));gameplay_enemy_note(gb);}}while(0)
#define ENEMY_ZERO() do {if(ENEMY_ACTIVE){bool oe_zero_;if(!gameplay_enemy_zero(A,&oe_zero_))ENEMY_STOP();F=(uint8_t)((F&~FZ)|(oe_zero_?FZ:0));gameplay_enemy_note(gb);}}while(0)
#define ENEMY_OR(value) do {alu_or(gb,value);ENEMY_ZERO();}while(0)
#define ENEMY_AND(value) do {alu_and(gb,value);ENEMY_ZERO();}while(0)
#define ENEMY_XOR(value) do {alu_xor(gb,value);ENEMY_ZERO();}while(0)
#define ENEMY_BIT(bit,value) do {uint8_t oe_byte_=(value);alu_bit(gb,bit,oe_byte_);if(ENEMY_ACTIVE){bool oe_zero_;if(!gameplay_enemy_bit_zero(oe_byte_,(uint8_t)(1u<<(bit)),&oe_zero_))ENEMY_STOP();F=(uint8_t)((F&~FZ)|(oe_zero_?FZ:0));gameplay_enemy_note(gb);}}while(0)
#define ENEMY_FLAG(mask) __extension__ ({uint8_t oe_mask_=(mask),oe_flag_=(F&oe_mask_);if(ENEMY_ACTIVE){bool oe_taken_;if(!gameplay_enemy_condition(F,oe_mask_,&oe_taken_))ENEMY_STOP();gameplay_enemy_note(gb);oe_flag_=oe_taken_?oe_mask_:0;F=(uint8_t)((F&~oe_mask_)|oe_flag_);}oe_flag_;})
#define ENEMY_WRITE(addr,value) do {uint16_t oe_dest_=(addr);uint8_t oe_next_=(value);if(oe_dest_>=0xd000 && oe_dest_<0xe000 && ENEMY_ACTIVE){unsigned oe_field_=oe_dest_&255;if(oe_field_==0x84 || oe_field_==0x85 || oe_field_==0x88 || oe_field_==0x89 || oe_field_==0x90){uint8_t oe_old_=mem_rd(gb,oe_dest_);bool oe_ok_=oe_field_==0x84 || oe_field_==0x85?gameplay_enemy_transition(oe_old_,oe_next_,&oe_next_):oe_field_==0x88?gameplay_enemy_direction(oe_old_,oe_next_,&oe_next_):oe_field_==0x89?gameplay_enemy_angle(oe_old_,oe_next_,&oe_next_):gameplay_enemy_speed(oe_old_,oe_next_,&oe_next_);if(!oe_ok_)ENEMY_STOP();gameplay_enemy_note(gb);}}mem_wr(gb,oe_dest_,oe_next_);}while(0)
#define ENEMY_CALL_C(...) do {CALL_C(__VA_ARGS__);ENEMY_FAILED();}while(0)
#define ENEMY_CALL_C_CC(...) do {CALL_C_CC(__VA_ARGS__);ENEMY_FAILED();}while(0)
#define ENEMY_CALL_ROM(...) do {CALL_ROM(__VA_ARGS__);ENEMY_FAILED();}while(0)
#define ENEMY_LOCAL_CALL(expr) do {expr;ENEMY_FAILED();}while(0)
#define ENEMY_OCTOROK_ROLL(value) do {uint8_t oe_random_=A,oe_mask_=(value);alu_and(gb,oe_mask_);if(ENEMY_ACTIVE){bool oe_attack_;if(!gameplay_enemy_octorok_attack(oe_random_,oe_mask_,&oe_attack_))ENEMY_STOP();F=(uint8_t)((F&~FZ)|(oe_attack_?FZ:0));gameplay_enemy_note(gb);}}while(0)
#define ENEMY_OCTOROK_AIM() do {uint8_t oe_subid_=A;alu_cp(gb,2);if(ENEMY_ACTIVE){bool oe_aim_;if(!gameplay_enemy_octorok_aim(oe_subid_,&oe_aim_))ENEMY_STOP();F=(uint8_t)((F&~FC)|(oe_aim_?0:FC));gameplay_enemy_note(gb);}}while(0)
#else
#define ENEMY_CP(v) alu_cp(gb,v)
#define ENEMY_OR(v) alu_or(gb,v)
#define ENEMY_AND(v) alu_and(gb,v)
#define ENEMY_XOR(v) alu_xor(gb,v)
#define ENEMY_BIT(b,v) alu_bit(gb,b,v)
#define ENEMY_ACTIVE false
#define ENEMY_FLAG(mask) (F&(mask))
#define ENEMY_WRITE(addr,value) mem_wr(gb,addr,value)
#define ENEMY_CALL_C CALL_C
#define ENEMY_CALL_C_CC CALL_C_CC
#define ENEMY_CALL_ROM CALL_ROM
#define ENEMY_LOCAL_CALL(expr) expr
#define ENEMY_OCTOROK_ROLL(v) alu_and(gb,v)
#define ENEMY_OCTOROK_AIM() alu_cp(gb,2)
#endif
