#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
// Stateless pilot; no VM pointer or new persistent data is added to GB.
bool gameplay_init(void);
void gameplay_enable(bool enabled);
bool gameplay_is_enabled(void);
bool gameplay_decide(int rule, uint8_t a, uint8_t b, bool *out);
typedef enum {
  GAMEPLAY_BLOCK_INITIALIZE,
  GAMEPLAY_BLOCK_WAIT_RELEASE,
  GAMEPLAY_BLOCK_WAIT_MOVED,
  GAMEPLAY_BLOCK_WAIT_DELAY,
  GAMEPLAY_BLOCK_FINISH,
} GameplayBlockState;
typedef struct {
  GameplayBlockState state;
  union {
    struct { uint8_t threshold, enemies; } release;
    struct { uint8_t previous, current; } tile;
    bool delay_elapsed;
  } input;
} GameplayBlockInput;
typedef struct { GameplayBlockState next; } GameplayBlockOutput;
// Only the current state or its valid successor can be returned; no side effects.
bool gameplay_block_step(const GameplayBlockInput *input, GameplayBlockOutput *out);
uint64_t gameplay_block_calls(GameplayBlockState state);
const char *gameplay_error(void);
const char *gameplay_revision(void);
uint64_t gameplay_calls(int rule);
size_t gameplay_memory(void);
typedef struct { size_t current, peak; uint64_t allocations; } GameplayMemoryStats;
GameplayMemoryStats gameplay_memory_stats(void);
void gameplay_close(void);
// Used for bounded-runtime regression tests; production always loads embedded source.
bool gameplay_load(const char *source, size_t length);
typedef struct {
  const char *switch_tiles; size_t switch_tiles_size;
  const char *block_puzzles; size_t block_puzzles_size;
  const char *block_rules; size_t block_rules_size;
  const char *colored_cube; size_t colored_cube_size;
  const char *levers_lava; size_t levers_lava_size;
} GameplaySources;
// Test seam: reset the runtime, replacing selected modules; NULL uses embedded source.
bool gameplay_load_sources(const GameplaySources *sources);
