#pragma once
#include "script/gameplay.h"
struct GB;
void gameplay_story_note(struct GB *gb);
uint64_t gameplay_npc_owner_calls(uint8_t owner);
bool gameplay_npcs_events_enabled(void);
typedef enum {
  GAMEPLAY_NPCS_compare,
  GAMEPLAY_NPCS_zero,
  GAMEPLAY_NPCS_bit_zero,
  GAMEPLAY_NPCS_carry,
  GAMEPLAY_NPCS_old_lady_available,
  GAMEPLAY_NPCS_old_lady_script,
  GAMEPLAY_NPCS_symmetry_available,
  GAMEPLAY_NPCS_symmetry_restart,
  GAMEPLAY_NPCS_COUNT
} GameplayNpcsRule;
bool gameplay_npc_compare(uint8_t a, uint8_t b, uint8_t *out);
bool gameplay_npc_zero(uint8_t a, bool *out);
bool gameplay_npc_bit_zero(uint8_t a, uint8_t b, bool *out);
bool gameplay_npc_carry(uint8_t a, bool *out);
bool gameplay_npc_old_lady_available(uint8_t a, uint8_t b, uint8_t c, bool *out);
bool gameplay_npc_old_lady_script(uint8_t a, uint8_t b, uint8_t *out);
bool gameplay_npc_symmetry_available(uint8_t a, uint8_t b, bool *out);
bool gameplay_npc_symmetry_restart(uint8_t a, uint8_t b, bool *out);
uint64_t gameplay_npc_calls(GameplayNpcsRule rule);
const char *gameplay_npc_rule_name(GameplayNpcsRule rule);
typedef enum {
  GAMEPLAY_EVENTS_compare,
  GAMEPLAY_EVENTS_zero,
  GAMEPLAY_EVENTS_bit_zero,
  GAMEPLAY_EVENTS_carry,
  GAMEPLAY_EVENTS_table_index,
  GAMEPLAY_EVENTS_transition,
  GAMEPLAY_EVENTS_next_state,
  GAMEPLAY_EVENTS_dialogue,
  GAMEPLAY_EVENTS_choose_linked,
  GAMEPLAY_EVENTS_script_ready,
  GAMEPLAY_EVENTS_script_ended,
  GAMEPLAY_EVENTS_condition,
  GAMEPLAY_EVENTS_COUNT
} GameplayEventsRule;
bool gameplay_event_compare(uint8_t a, uint8_t b, uint8_t *out);
bool gameplay_event_zero(uint8_t a, bool *out);
bool gameplay_event_bit_zero(uint8_t a, uint8_t b, bool *out);
bool gameplay_event_carry(uint8_t a, bool *out);
bool gameplay_event_table_index(uint8_t a, uint8_t *out);
bool gameplay_event_transition(uint8_t a, uint8_t b, uint8_t *out);
bool gameplay_event_next_state(uint8_t a, uint8_t *out);
bool gameplay_event_dialogue(uint8_t a, uint8_t b, uint16_t *out);
bool gameplay_event_choose_linked(uint8_t a, bool *out);
bool gameplay_event_script_ready(uint8_t a, bool *out);
bool gameplay_event_script_ended(uint8_t a, bool *out);
bool gameplay_event_condition(uint8_t a, bool *out);
uint64_t gameplay_event_calls(GameplayEventsRule rule);
const char *gameplay_event_rule_name(GameplayEventsRule rule);

#ifdef ORACLES_LUA_GAMEPLAY
bool gameplay_npc_event_owner(struct GB *gb);
#define STORY_NOTE() gameplay_story_note(gb)
#define EVENT_OBJECT_STATE(target) (((target)&255)==0x44 || ((target)&255)==0x45)
#define NPC_ACTIVE (!game_seasons && gameplay_npcs_events_enabled())
#define EVENT_ACTIVE (NPC_ACTIVE && gameplay_npc_event_owner(gb))
#define NPC_STOP() do {gb_stop(gb,gameplay_error());return;}while(0)
#define NPC_FAILED_RETURN() do {if(gb->hung && NPC_ACTIVE && gameplay_error()[0])return;}while(0)
#define NPC_POLICY_CP(scope,fn,value) do {uint8_t left_=A,right_=(value);alu_cp(gb,right_);if(scope){uint8_t result_;if(!fn(left_,right_,&result_))NPC_STOP();STORY_NOTE();F=(uint8_t)((F&~(FZ|FC))|(result_==0?FZ:0)|(result_==1?FC:0));}}while(0)
#define NPC_POLICY_ZERO(scope,fn) do {if(scope){bool result_;if(!fn(A,&result_))NPC_STOP();STORY_NOTE();F=(uint8_t)((F&~FZ)|(result_?FZ:0));}}while(0)
#define NPC_POLICY_BIT(scope,fn,bit,value) do {uint8_t byte_=(value);alu_bit(gb,bit,byte_);if(scope){bool result_;if(!fn(byte_,(uint8_t)(1u<<(bit)),&result_))NPC_STOP();STORY_NOTE();F=(uint8_t)((F&~FZ)|(result_?FZ:0));}}while(0)
#define NPC_CP(v) NPC_POLICY_CP(NPC_ACTIVE,gameplay_npc_compare,v)
#define NPC_OR(v) do {alu_or(gb,v);NPC_POLICY_ZERO(NPC_ACTIVE,gameplay_npc_zero);}while(0)
#define NPC_AND(v) do {alu_and(gb,v);NPC_POLICY_ZERO(NPC_ACTIVE,gameplay_npc_zero);}while(0)
#define NPC_XOR(v) do {alu_xor(gb,v);NPC_POLICY_ZERO(NPC_ACTIVE,gameplay_npc_zero);}while(0)
#define NPC_BIT(b,v) NPC_POLICY_BIT(NPC_ACTIVE,gameplay_npc_bit_zero,b,v)
#define NPC_FACT_ZERO() do {if(NPC_ACTIVE){bool out_;if(!gameplay_npc_zero((F&FZ)?0:1,&out_))NPC_STOP();F=(uint8_t)((F&~FZ)|(out_?FZ:0));}}while(0)
#define NPC_FACT_CARRY() do {if(NPC_ACTIVE){bool out_;if(!gameplay_npc_carry((F&FC)?1:0,&out_))NPC_STOP();F=(uint8_t)((F&~FC)|(out_?FC:0));}}while(0)
#define EVENT_CP(v) NPC_POLICY_CP(EVENT_ACTIVE,gameplay_event_compare,v)
#define EVENT_OR(v) do {alu_or(gb,v);NPC_POLICY_ZERO(EVENT_ACTIVE,gameplay_event_zero);}while(0)
#define EVENT_AND(v) do {alu_and(gb,v);NPC_POLICY_ZERO(EVENT_ACTIVE,gameplay_event_zero);}while(0)
#define EVENT_XOR(v) do {alu_xor(gb,v);NPC_POLICY_ZERO(EVENT_ACTIVE,gameplay_event_zero);}while(0)
#define EVENT_BIT(b,v) NPC_POLICY_BIT(EVENT_ACTIVE,gameplay_event_bit_zero,b,v)
#define EVENT_FACT_ZERO() do {if(EVENT_ACTIVE){bool out_;if(!gameplay_event_zero((F&FZ)?0:1,&out_))NPC_STOP();F=(uint8_t)((F&~FZ)|(out_?FZ:0));}}while(0)
#define EVENT_FACT_CARRY() do {if(EVENT_ACTIVE){bool out_;if(!gameplay_event_carry((F&FC)?1:0,&out_))NPC_STOP();F=(uint8_t)((F&~FC)|(out_?FC:0));}}while(0)
#define EVENT_INDEX(value) do {if(EVENT_ACTIVE && !gameplay_event_table_index(value,&(value)))NPC_STOP();}while(0)
#define EVENT_WRITE(addr,value) do {uint16_t target_=(addr);uint8_t requested_=(value);if(EVENT_ACTIVE && ((target_>=0xd000 && target_<0xe000 && EVENT_OBJECT_STATE(target_)) || target_==wCutsceneState || target_==wTmpcbb3 || target_==wTmpcbb4 || target_==wTmpcfc0_genericCutscene_state)){uint8_t current_=mem_rd(gb,target_);if(!gameplay_event_transition(current_,requested_,&requested_))NPC_STOP();}mem_wr(gb,target_,requested_);}while(0)
#define EVENT_INC_WRITE(addr) do {uint16_t target_=(addr);uint8_t current_=mem_rd(gb,target_);uint8_t next_=alu_inc8(gb,current_);if(EVENT_ACTIVE && ((target_&255)==0x44 || (target_&255)==0x45) && !gameplay_event_next_state(current_,&next_))NPC_STOP();mem_wr(gb,target_,next_);}while(0)
#define EVENT_DIALOGUE() do {if(EVENT_ACTIVE){uint16_t selected_;if(!gameplay_event_dialogue(C,B,&selected_))NPC_STOP();SET_BC(selected_);}}while(0)
#define EVENT_LINKED() do {if(EVENT_ACTIVE){bool linked_;if(!gameplay_event_choose_linked(A,&linked_))NPC_STOP();F=(uint8_t)((F&~FZ)|(linked_?0:FZ));}}while(0)
#define EVENT_SCRIPT_READY(active) do {if(EVENT_ACTIVE){bool ready_;if(!gameplay_event_script_ready(active,&ready_))NPC_STOP();bool original_=((F&FC)!=0)||((F&FZ)!=0);if(ready_!=original_)F=(uint8_t)((F&~(FZ|FC))|(ready_?FZ:0));}}while(0)
#define EVENT_ENDED() do {if(EVENT_ACTIVE){bool ended_;if(!gameplay_event_script_ended(A,&ended_))NPC_STOP();F=(uint8_t)((F&~FZ)|(ended_?FZ:0));}}while(0)
#define EVENT_CONDITION() do {if(EVENT_ACTIVE){bool condition_;if(!gameplay_event_condition((F&FZ)?0:1,&condition_))NPC_STOP();F=(uint8_t)((F&~FZ)|(condition_?0:FZ));}}while(0)
#define EVENT_SHOP_CP(value) do {if(EVENT_ACTIVE){EVENT_CP(value);}else{SHOP_SCRIPT_CP(value);}}while(0)
#else
#define EVENT_DIALOGUE() ((void)0)
#define EVENT_LINKED() ((void)0)
#define EVENT_SCRIPT_READY(active) ((void)0)
#define EVENT_ENDED() ((void)0)
#define EVENT_CONDITION() ((void)0)
#define EVENT_SHOP_CP(value) SHOP_SCRIPT_CP(value)
#define NPC_ACTIVE false
#define EVENT_ACTIVE false
#define NPC_FAILED_RETURN() ((void)0)
#define NPC_CP(v) alu_cp(gb,v)
#define NPC_OR(v) alu_or(gb,v)
#define NPC_AND(v) alu_and(gb,v)
#define NPC_XOR(v) alu_xor(gb,v)
#define NPC_BIT(b,v) alu_bit(gb,b,v)
#define NPC_FACT_ZERO() ((void)0)
#define NPC_FACT_CARRY() ((void)0)
#define EVENT_CP(v) alu_cp(gb,v)
#define EVENT_OR(v) alu_or(gb,v)
#define EVENT_AND(v) alu_and(gb,v)
#define EVENT_XOR(v) alu_xor(gb,v)
#define EVENT_BIT(b,v) alu_bit(gb,b,v)
#define EVENT_FACT_ZERO() ((void)0)
#define EVENT_FACT_CARRY() ((void)0)
#define EVENT_INDEX(v) ((void)0)
#define EVENT_WRITE(addr,value) mem_wr(gb,addr,value)
#define EVENT_INC_WRITE(addr) mem_wr(gb,addr,alu_inc8(gb,mem_rd(gb,addr)))
#endif
#ifdef ORACLES_LUA_GAMEPLAY
#define NPC_CALL_C(...) do {CALL_C(__VA_ARGS__);NPC_FAILED_RETURN();}while(0)
#define NPC_CALL_C_CC(...) do {CALL_C_CC(__VA_ARGS__);NPC_FAILED_RETURN();}while(0)
#define NPC_CALL_ROM(...) do {CALL_ROM(__VA_ARGS__);NPC_FAILED_RETURN();}while(0)
#define NPC_CALL_ZERO(...) do {NPC_CALL_C(__VA_ARGS__);NPC_FACT_ZERO();}while(0)
#define NPC_CALL_CARRY(...) do {NPC_CALL_C(__VA_ARGS__);NPC_FACT_CARRY();}while(0)
#define EVENT_CALL_C(...) do {CALL_C(__VA_ARGS__);NPC_FAILED_RETURN();}while(0)
#define EVENT_CALL_C_CC(...) do {CALL_C_CC(__VA_ARGS__);NPC_FAILED_RETURN();}while(0)
#define EVENT_CALL_ZERO(...) do {EVENT_CALL_C(__VA_ARGS__);EVENT_FACT_ZERO();}while(0)
#define EVENT_CALL_CARRY(...) do {EVENT_CALL_C(__VA_ARGS__);EVENT_FACT_CARRY();}while(0)

#define NPC_LOCAL_CALL(expr) do {expr;NPC_FAILED_RETURN();}while(0)
#else
#define NPC_CALL_C CALL_C
#define NPC_CALL_C_CC CALL_C_CC
#define NPC_CALL_ROM CALL_ROM
#define NPC_CALL_ZERO CALL_C
#define NPC_CALL_CARRY CALL_C
#define EVENT_CALL_C CALL_C
#define EVENT_CALL_C_CC CALL_C_CC
#define EVENT_CALL_ZERO CALL_C
#define EVENT_CALL_CARRY CALL_C
#define NPC_LOCAL_CALL(expr) expr
#endif
