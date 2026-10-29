#include "unit.h"
#include "core/gb.h"
#include "core/bus.h"
#include "assets/assets.h"
#include "platform/setup.h"
#include "platform/tempfile.h"
#include "game/game.h"
#include "game/gen.h"
#include "script/gameplay.h"

static GB *g, *seed, *expected;
static GBSample *samples;
static unsigned cases;

static void enter(bool dispatched) {
  g->d = 0xd0; g->sp = 0xdff0;
  g->rom_bank = SYMBANK(interactionCode13); g->pc = SYM(interactionCode13);
  if (dispatched) ASSERT(hook_dispatch(g));
  else hook_run(g, interactionCode13_hook, SYM(interactionCode13));
  ASSERT(!g->hung);
}

static void prepare(int state, uint8_t a, uint8_t b) {
  *g = *seed;
  memset(g->samples, 0, 16*sizeof *g->samples);
  mem_wr(g, wScreenVariables, 1);
  mem_wr(g, hActiveObjectType, INTERACTION_BASE);
  mem_wr(g, 0xd040, 1); // enabled interaction; $02 means delete
  mem_wr(g, 0xd040 + OBJ_STATE, state);
  mem_wr(g, 0xd040 + OBJ_XH, 0x48); mem_wr(g, 0xd040 + OBJ_YH, 0x48);
  mem_wr(g, 0xd040 + OBJ_VAR18, 0x44);
  mem_wr(g, 0xd040 + OBJ_SUBID, a); mem_wr(g, wNumEnemies, b);
  mem_wr(g, 0xd040 + OBJ_VAR19, a); mem_wr(g, wRoomLayout + 0x44, b);
  mem_wr(g, 0xd040 + OBJ_COUNTER1, a);
}

static void compare_case(int state, uint8_t a, uint8_t b, bool dispatched) {
  for (int mode = 0; mode < 2; ++mode) {
    prepare(state, a, b); gameplay_enable(mode != 0); enter(dispatched);
    if (!mode) { *expected = *g; memcpy(samples, g->samples, 16*sizeof *samples); }
    else { ASSERT(!memcmp(expected, g, sizeof *g)); ASSERT(!memcmp(samples, g->samples, 16*sizeof *samples)); }
  }
  ++cases;
}

static void directed_states(void) {
  const uint8_t edges[] = {0, 1, 2, 3, 0x1e, 0x7f, 0xfe, 0xff};
  for (int dispatch = 0; dispatch < 2; ++dispatch) {
    for (int state = 0; state < 3; ++state)
      for (size_t a = 0; a < sizeof edges; ++a) for (size_t b = 0; b < sizeof edges; ++b)
        compare_case(state, edges[a], edges[b], dispatch != 0);
    for (int counter = 0; counter < 256; ++counter) compare_case(3, counter, 0xa5, dispatch != 0);
  }
  for (int state = 0; state < 4; ++state) ASSERT(gameplay_block_calls(state) > 0);
  // Original early-outs must not consult Lua or run another state's effects.
  for (int guard = 0; guard < 2; ++guard) {
    prepare(1, 0, 0);
    if (guard) mem_wr(g, wScreenVariables, 0);
    else mem_wr(g, 0xd040, 2);
    uint64_t before = gameplay_block_calls(GAMEPLAY_BLOCK_WAIT_RELEASE);
    gameplay_enable(true); enter(true);
    ASSERT_EQ(gameplay_block_calls(GAMEPLAY_BLOCK_WAIT_RELEASE), before);
  }
}

static void resume_each_state(void) {
  char path[4096]; ASSERT(oracles_temp_path(path, sizeof path));
  for (int state = 0; state < 4; ++state) {
    prepare(state, 1, 1);
    ASSERT(oracles_save_boot_state(g, path));
    for (int mode = 0; mode < 2; ++mode) {
      *g = *seed;
      ASSERT(oracles_load_boot_state(g, path));
      memset(g->samples, 0, 16*sizeof *g->samples);
      // A fresh VM models closing/reopening the app: no Lua state is persisted.
      gameplay_close(); ASSERT(gameplay_init()); gameplay_enable(mode != 0);
      enter(true);
      if (!mode) { *expected = *g; memcpy(samples, g->samples, 16*sizeof *samples); }
      else { ASSERT(!memcmp(expected, g, sizeof *g)); ASSERT(!memcmp(samples, g->samples, 16*sizeof *samples)); }
    }
  }
  ASSERT_EQ(remove(path), 0);
}

static void complete_lifecycle(void) {
  GB *trajectory = malloc(36*sizeof *trajectory); ASSERT(trajectory);
  GBSample *trajectory_samples = malloc(36*16*sizeof *trajectory_samples); ASSERT(trajectory_samples);
  for (int mode = 0; mode < 2; ++mode) {
    prepare(0, 1, 1); gameplay_enable(mode != 0);
    for (int tick = 0; tick < 36; ++tick) {
      if (tick == 2) mem_wr(g, wNumEnemies, 1); // satisfy release threshold
      if (tick == 4) mem_wr(g, wRoomLayout + 0x44, 0x55); // block moved
      enter(tick % 2 != 0);
      if (!mode) {
        trajectory[tick] = *g;
        memcpy(trajectory_samples + tick*16, g->samples, 16*sizeof *g->samples);
      } else {
        ASSERT(!memcmp(&trajectory[tick], g, sizeof *g));
        ASSERT(!memcmp(trajectory_samples + tick*16, g->samples, 16*sizeof *g->samples));
      }
      if (tick == 0) {
        ASSERT_EQ(mem_rd(g, 0xd040 + OBJ_STATE), 1);
        ASSERT_EQ(mem_rd(g, 0xd040 + OBJ_VAR18), 0x44);
      }
      if (tick == 2) ASSERT_EQ(mem_rd(g, 0xd040 + OBJ_STATE), 2);
      if (tick == 4) ASSERT_EQ(mem_rd(g, 0xd040 + OBJ_STATE), 3);
      if (tick == 34) { ASSERT_EQ(mem_rd(g, 0xd040), 0); ASSERT_EQ(mem_rd(g, wNumEnemies), 0); break; }
    }
  }
  free(trajectory_samples); free(trajectory);
}

static void script_failure_stops_interaction(void) {
  const char source[] = "return {next_state=function() return 99 end}";
  GameplaySources override = {.block_puzzles = source, .block_puzzles_size = sizeof source-1};
  ASSERT(gameplay_load_sources(&override)); gameplay_enable(true);
  prepare(1, 1, 1); uint8_t tile = mem_rd(g, wRoomLayout + 0x44);
  g->d = 0xd0; g->sp = 0xdff0; g->pc = SYM(interactionCode13);
  hook_run(g, interactionCode13_hook, SYM(interactionCode13));
  ASSERT(g->hung);
  ASSERT_EQ(mem_rd(g, 0xd040 + OBJ_STATE), 1);
  ASSERT_EQ(mem_rd(g, wRoomLayout + 0x44), tile);
  ASSERT(strstr(gameplay_error(), "invalid state transition"));
  gameplay_close(); ASSERT(gameplay_init());
}

int main(void) {
  size_t size;
  uint8_t *rom = oracles_read_file(GAME_ROM_DIR "/Legend of Zelda, The - Oracle of Ages (USA, Australia).gbc", &size);
  if (!rom) SKIP("Ages ROM missing");
  g = calloc(1, sizeof *g); seed = malloc(sizeof *seed); expected = malloc(sizeof *expected);
  samples = malloc(16*sizeof *samples); ASSERT(g && seed && expected && samples);
  gb_init(g); ASSERT(gb_load_rom(g, rom, size));
  g->cyctab = cyctab_alloc(rom, size); g->code_bits = assets_code_bits(rom, size);
  assets_zero_code(rom, size); gb_reset(g);
  g->ime = false; g->ie = 0; g->rom_bank = SYMBANK(interactionCode13);
  *seed = *g; hook_mode = HOOK_MODE_REPLACE;
  ASSERT(gameplay_init());
  RUN(directed_states);
  printf("verified %u real block cases; Lua states: %llu/%llu/%llu/%llu\n", cases,
    (unsigned long long)gameplay_block_calls(0), (unsigned long long)gameplay_block_calls(1),
    (unsigned long long)gameplay_block_calls(2), (unsigned long long)gameplay_block_calls(3));
  RUN(resume_each_state);
  RUN(complete_lifecycle);
  RUN(script_failure_stops_interaction);
  gameplay_close(); free(samples); free(seed); free(expected);
  puts("ok lua_blocks_rom: full GB and sample equality, direct/dispatch, lifecycle, reload, error stop");
  return 0;
}
