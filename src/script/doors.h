#pragma once
#include "script/gameplay.h"

typedef enum {
#define DOOR_RULE1(name, a) GAMEPLAY_DOOR_##name,
#define DOOR_RULE2(name, a, b) GAMEPLAY_DOOR_##name,
#define DOOR_RULE_FLAG(name, a) GAMEPLAY_DOOR_##name,
#define DOOR_BYTE(name, a, low, high) GAMEPLAY_DOOR_##name,
#include "script/doors.def"
#undef DOOR_RULE1
#undef DOOR_RULE2
#undef DOOR_RULE_FLAG
#undef DOOR_BYTE
  GAMEPLAY_DOOR_RULE_COUNT
} GameplayDoorRule;

bool gameplay_doors_enabled(void);
#define DOOR_RULE1(name, a) bool gameplay_door_##name(uint8_t a, bool *out);
#define DOOR_RULE2(name, a, b) bool gameplay_door_##name(uint8_t a, uint8_t b, bool *out);
#define DOOR_RULE_FLAG(name, a) bool gameplay_door_##name(bool a, bool *out);
#define DOOR_BYTE(name, a, low, high) bool gameplay_door_##name(uint8_t a, uint8_t *out);
#include "script/doors.def"
#undef DOOR_RULE1
#undef DOOR_RULE2
#undef DOOR_RULE_FLAG
#undef DOOR_BYTE
uint64_t gameplay_door_calls(GameplayDoorRule rule);
const char *gameplay_door_rule_name(GameplayDoorRule rule);

#ifdef ORACLES_LUA_GAMEPLAY
#define DOOR_ACTIVE (!game_seasons && gameplay_doors_enabled())
// Script commands retain their generic C implementation. Only an interaction 1e
// owner in bank 1 opts into door predicates; no host scope survives a fiber yield.
#define DOOR_SCRIPT_ACTIVE (DOOR_ACTIVE && D >= 0xd0 && D <= 0xdf && \
  H8(hActiveObjectType) == INTERACTION_BASE && \
  gb->wram[1][((D & 15) << 8) + INTERACTION_BASE + OBJ_ID] == 0x1e)
#define DOOR_DECIDE(result, name, input) do { \
  if (DOOR_ACTIVE && !gameplay_door_##name((input), &(result))) { gb_stop(gb, gameplay_error()); return; } \
} while (0)
#define DOOR_DECIDE2(result, name, a, b) do { \
  if (DOOR_ACTIVE && !gameplay_door_##name((a), (b), &(result))) { gb_stop(gb, gameplay_error()); return; } \
} while (0)
#define DOOR_SCRIPT_DECIDE(result, name, input) do { \
  if (DOOR_SCRIPT_ACTIVE && !gameplay_door_##name((input), &(result))) { gb_stop(gb, gameplay_error()); return; } \
} while (0)
#define DOOR_SCRIPT_STATE(value) do { \
  if (DOOR_SCRIPT_ACTIVE && E == INTERACTION_BASE + OBJ_STATE && (value) <= 3 && \
      !gameplay_door_script_state((value), &(value))) { gb_stop(gb, gameplay_error()); return; } \
} while (0)
#define DOOR_FAILED_RETURN() do { if (gb->hung && DOOR_ACTIVE && gameplay_error()[0]) return; } while (0)
#define DOOR_SET_CARRY(carry) do { F = (uint8_t)((F & ~FC) | ((carry) ? FC : 0)); } while (0)
#define DOOR_SET_ZERO(zero) do { F = (uint8_t)((F & ~FZ) | ((zero) ? FZ : 0)); } while (0)
#else
#define DOOR_ACTIVE false
#define DOOR_SCRIPT_ACTIVE false
#define DOOR_DECIDE(result, name, input) ((void)0)
#define DOOR_DECIDE2(result, name, a, b) ((void)0)
#define DOOR_SCRIPT_DECIDE(result, name, input) ((void)0)
#define DOOR_SCRIPT_STATE(value) ((void)0)
#define DOOR_FAILED_RETURN() ((void)0)
#define DOOR_SET_CARRY(carry) ((void)0)
#define DOOR_SET_ZERO(zero) ((void)0)
#endif
