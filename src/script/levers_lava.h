#pragma once
#include "script/gameplay.h"

typedef enum {
  GAMEPLAY_LAVA_FULLY_PULLED, GAMEPLAY_LAVA_RELEASED,
  GAMEPLAY_LAVA_TIMER_ELAPSED, GAMEPLAY_LAVA_HAS_TILE,
  GAMEPLAY_LAVA_SOURCE_FOUND, GAMEPLAY_LAVA_SOURCE_SEGMENT,
  GAMEPLAY_LAVA_SOURCE_DELTA, GAMEPLAY_LAVA_NEXT_STATE, GAMEPLAY_LAVA_FLOOR_TILE,
  GAMEPLAY_LAVA_RULE_COUNT
} GameplayLavaRule;

bool gameplay_levers_lava_enabled(void);
bool gameplay_lava_fully_pulled(uint8_t distance,bool *out);
bool gameplay_lava_released(uint8_t distance,bool *out);
bool gameplay_lava_timer_elapsed(bool zero,bool *out);
bool gameplay_lava_has_tile(uint8_t token,bool *out);
bool gameplay_lava_source_found(bool zero,bool *out);
bool gameplay_lava_source_segment(uint8_t wrapped_relative_tile,bool *out);
bool gameplay_lava_source_delta(bool empty,uint8_t *out);
bool gameplay_lava_next_state(uint8_t current,uint8_t *out);
bool gameplay_lava_floor_tile(uint8_t state,uint8_t random_byte,uint8_t *out);
uint64_t gameplay_lava_calls(GameplayLavaRule rule);
const char *gameplay_lava_rule_name(GameplayLavaRule rule);

#ifdef ORACLES_LUA_GAMEPLAY
#define LAVA_ACTIVE (!game_seasons && gameplay_levers_lava_enabled())
#define LAVA_DECIDE(result,name,input) do { \
  if(LAVA_ACTIVE && !gameplay_lava_##name((input),&(result))) { gb_stop(gb,gameplay_error()); return; } \
} while(0)
#define LAVA_BYTE(result,name,input) LAVA_DECIDE(result,name,input)
#define LAVA_FLOOR(result,state,random_byte) do { \
  if(LAVA_ACTIVE && !gameplay_lava_floor_tile((state),(random_byte),&(result))) { gb_stop(gb,gameplay_error()); return; } \
} while(0)
#define LAVA_FAILED_RETURN() do { if(gb->hung && LAVA_ACTIVE && gameplay_error()[0]) return; } while(0)
#else
#define LAVA_ACTIVE false
#define LAVA_DECIDE(result,name,input) ((void)0)
#define LAVA_BYTE(result,name,input) ((void)0)
#define LAVA_FLOOR(result,state,random_byte) ((void)0)
#define LAVA_FAILED_RETURN() ((void)0)
#endif
