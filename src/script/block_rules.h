#pragma once
#include "script/gameplay.h"

typedef enum {
#define BLOCK_RULE1(name, a) GAMEPLAY_RULE_##name,
#define BLOCK_RULE2(name, a, b) GAMEPLAY_RULE_##name,
#define BLOCK_RULE_FLAG(name, a) GAMEPLAY_RULE_##name,
#include "script/block_rules.def"
#undef BLOCK_RULE1
#undef BLOCK_RULE2
#undef BLOCK_RULE_FLAG
  GAMEPLAY_RULE_sync_next_state,
  GAMEPLAY_BLOCK_RULE_COUNT
} GameplayBlockRule;

// Each function returns success and writes exactly one validated boolean.
// Flag inputs cross the Lua boundary as the integers 0/1, like the older pilot.
// No memory address, GB pointer or persistent object state crosses this API.
#define BLOCK_RULE1(name, a) bool gameplay_##name(uint8_t a, bool *out);
#define BLOCK_RULE2(name, a, b) bool gameplay_##name(uint8_t a, uint8_t b, bool *out);
#define BLOCK_RULE_FLAG(name, a) bool gameplay_##name(bool a, bool *out);
#include "script/block_rules.def"
#undef BLOCK_RULE1
#undef BLOCK_RULE2
#undef BLOCK_RULE_FLAG
uint64_t gameplay_block_rule_calls(GameplayBlockRule rule);
const char *gameplay_block_rule_name(GameplayBlockRule rule);
bool gameplay_sync_next_state(uint8_t current, uint8_t *out);

// The caller computes original CPU effects/flags first. C-reference tests bypass
// Lua here, keeping the actual original branch result rather than a second model.
#ifdef ORACLES_LUA_BLOCK_PUZZLES
#define BLOCK_DECIDE(result, name, ...) do { \
  if (!game_seasons && gameplay_is_enabled() && \
      !gameplay_##name(__VA_ARGS__, &(result))) { \
    gb_stop(gb, gameplay_error()); return; \
  } \
} while (0)
#define BLOCK_FAILED_RETURN() do { if (gb->hung) return; } while (0)
#define BLOCK_SET_Z(result) do { \
  if (!game_seasons && gameplay_is_enabled()) F = (uint8_t)((F & ~FZ) | ((result) ? FZ : 0)); \
} while (0)
#define BLOCK_SYNC_STATE(result, current) do { \
  if (!game_seasons && gameplay_is_enabled() && \
      !gameplay_sync_next_state((current), &(result))) { \
    gb_stop(gb, gameplay_error()); return; \
  } \
} while (0)
#else
#define BLOCK_DECIDE(result, name, ...) ((void)0)
#define BLOCK_FAILED_RETURN() ((void)0)
#define BLOCK_SET_Z(result) ((void)0)
#define BLOCK_SYNC_STATE(result, current) ((void)0)
#endif
