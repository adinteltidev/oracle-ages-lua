#pragma once
#include "script/gameplay.h"
bool gameplay_items_shops_enabled(void);
typedef enum {
  GAMEPLAY_ITEMS_compare,
  GAMEPLAY_ITEMS_zero,
  GAMEPLAY_ITEMS_bit_zero,
  GAMEPLAY_ITEMS_carry,
  GAMEPLAY_ITEMS_reward_id,
  GAMEPLAY_ITEMS_parameter_mode,
  GAMEPLAY_ITEMS_COUNT
} GameplayItemsRule;
bool gameplay_item_compare(uint8_t a, uint8_t b, uint8_t *out);
bool gameplay_item_zero(uint8_t a, bool *out);
bool gameplay_item_bit_zero(uint8_t a, uint8_t b, bool *out);
bool gameplay_item_carry(uint8_t a, bool *out);
bool gameplay_item_reward_id(uint8_t a, uint8_t *out);
bool gameplay_item_parameter_mode(uint8_t a, uint8_t *out);
uint64_t gameplay_item_calls(GameplayItemsRule rule);
const char *gameplay_item_rule_name(GameplayItemsRule rule);
typedef enum {
  GAMEPLAY_SHOPS_compare,
  GAMEPLAY_SHOPS_zero,
  GAMEPLAY_SHOPS_bit_zero,
  GAMEPLAY_SHOPS_carry,
  GAMEPLAY_SHOPS_reward_id,
  GAMEPLAY_SHOPS_price_index,
  GAMEPLAY_SHOPS_table_index,
  GAMEPLAY_SHOPS_purchase_outcome,
  GAMEPLAY_SHOPS_affordable,
  GAMEPLAY_SHOPS_COUNT
} GameplayShopsRule;
bool gameplay_shop_compare(uint8_t a, uint8_t b, uint8_t *out);
bool gameplay_shop_zero(uint8_t a, bool *out);
bool gameplay_shop_bit_zero(uint8_t a, uint8_t b, bool *out);
bool gameplay_shop_carry(uint8_t a, bool *out);
bool gameplay_shop_reward_id(uint8_t a, uint8_t *out);
bool gameplay_shop_price_index(uint8_t a, uint8_t *out);
bool gameplay_shop_table_index(uint8_t a, uint8_t *out);
bool gameplay_shop_purchase_outcome(uint8_t a, uint8_t *out);
bool gameplay_shop_affordable(uint8_t a, uint8_t b, uint8_t c, uint8_t d, bool *out);
uint64_t gameplay_shop_calls(GameplayShopsRule rule);
const char *gameplay_shop_rule_name(GameplayShopsRule rule);

#ifdef ORACLES_LUA_GAMEPLAY
#define TRADE_ACTIVE (!game_seasons && gameplay_items_shops_enabled())
#define SHOP_OWNER (D>=0xd0 && D<=0xdf && H8(hActiveObjectType)==INTERACTION_BASE && \
  (gb->wram[1][((D&15)<<8)+0x41]==0x46 || gb->wram[1][((D&15)<<8)+0x41]==0x81))
#define SHOP_SCRIPT_ACTIVE (TRADE_ACTIVE && SHOP_OWNER)
#define TRADE_STOP() do { gb_stop(gb,gameplay_error()); return; } while(0)
#define TRADE_FAILED_RETURN() do { if(gb->hung && TRADE_ACTIVE && gameplay_error()[0]) return; } while(0)
#define TRADE_CP(scope,fn,value) do { \
  uint8_t rhs_=(value),lhs_=A; alu_cp(gb,rhs_); \
  if(scope) { uint8_t relation_;if(!fn(lhs_,rhs_,&relation_)) TRADE_STOP(); \
    F=(uint8_t)((F&~(FZ|FC))|(relation_==0?FZ:0)|(relation_==1?FC:0)); } \
} while(0)
#define TRADE_ZERO(scope,fn) do { if(scope) { bool zero_;if(!fn(A,&zero_)) TRADE_STOP(); \
  F=(uint8_t)((F&~FZ)|(zero_?FZ:0)); } } while(0)
#define TRADE_BIT(scope,fn,bit,value) do { \
  uint8_t raw_=(value);alu_bit(gb,(bit),raw_); \
  if(scope) { bool zero_;if(!fn(raw_,(uint8_t)(1u<<(bit)),&zero_)) TRADE_STOP(); \
    F=(uint8_t)((F&~FZ)|(zero_?FZ:0)); } \
} while(0)
#define TRADE_CARRY(fn) do { if(TRADE_ACTIVE) { bool carry_; \
  if(!fn((F&FC)!=0,&carry_)) TRADE_STOP();F=(uint8_t)((F&~FC)|(carry_?FC:0)); } } while(0)
#define ITEM_CP(value) TRADE_CP(TRADE_ACTIVE,gameplay_item_compare,value)
#define SHOP_CP(value) TRADE_CP(TRADE_ACTIVE,gameplay_shop_compare,value)
#define SHOP_SCRIPT_CP(value) TRADE_CP(SHOP_SCRIPT_ACTIVE,gameplay_shop_compare,value)
#define ITEM_OR(value) do {alu_or(gb,(value));TRADE_ZERO(TRADE_ACTIVE,gameplay_item_zero);}while(0)
#define ITEM_AND(value) do {alu_and(gb,(value));TRADE_ZERO(TRADE_ACTIVE,gameplay_item_zero);}while(0)
#define SHOP_OR(value) do {alu_or(gb,(value));TRADE_ZERO(TRADE_ACTIVE,gameplay_shop_zero);}while(0)
#define SHOP_AND(value) do {alu_and(gb,(value));TRADE_ZERO(TRADE_ACTIVE,gameplay_shop_zero);}while(0)
#define ITEM_BIT(bit,value) TRADE_BIT(TRADE_ACTIVE,gameplay_item_bit_zero,bit,value)
#define SHOP_BIT(bit,value) TRADE_BIT(TRADE_ACTIVE,gameplay_shop_bit_zero,bit,value)
#define ITEM_FACT_ZERO() TRADE_ZERO(TRADE_ACTIVE,gameplay_item_zero)
#define SHOP_FACT_ZERO() TRADE_ZERO(TRADE_ACTIVE,gameplay_shop_zero)
#define ITEM_CARRY() TRADE_CARRY(gameplay_item_carry)
#define SHOP_CARRY() TRADE_CARRY(gameplay_shop_carry)
#define ITEM_BYTE(value,name) do {if(TRADE_ACTIVE && !gameplay_item_##name(value,&(value))) TRADE_STOP();}while(0)
#define SHOP_BYTE(value,name) do {if(TRADE_ACTIVE && !gameplay_shop_##name(value,&(value))) TRADE_STOP();}while(0)
#else
#define TRADE_ACTIVE false
#define SHOP_SCRIPT_ACTIVE false
#define TRADE_FAILED_RETURN() ((void)0)
#define ITEM_CP(value) alu_cp(gb,(value))
#define SHOP_CP(value) alu_cp(gb,(value))
#define SHOP_SCRIPT_CP(value) alu_cp(gb,(value))
#define ITEM_OR(value) alu_or(gb,(value))
#define ITEM_AND(value) alu_and(gb,(value))
#define SHOP_OR(value) alu_or(gb,(value))
#define SHOP_AND(value) alu_and(gb,(value))
#define ITEM_BIT(bit,value) alu_bit(gb,bit,(value))
#define SHOP_BIT(bit,value) alu_bit(gb,bit,(value))
#define ITEM_FACT_ZERO() ((void)0)
#define SHOP_FACT_ZERO() ((void)0)
#define ITEM_CARRY() ((void)0)
#define SHOP_CARRY() ((void)0)
#define ITEM_BYTE(value,name) ((void)0)
#define SHOP_BYTE(value,name) ((void)0)
#endif
