#include "unit.h"
#include "core/gb.h"
#include "core/bus.h"
#include "core/sha1.h"
#include "assets/assets.h"
#include "platform/setup.h"
#include "platform/tempfile.h"
#include "game/game.h"
#include "game/gen.h"
#include "script/gameplay.h"
#include "script/block_rules.h"

static GB *g, *seed, *expected;
static GBSample *samples;
static unsigned cases;
static char context[200];
#define CONTROLLER 0xd340

static void prepare(uint8_t state, uint8_t enabled, uint8_t tile,
                    unsigned count, bool blocked, bool full, uint8_t angle) {
  *g = *seed; memset(g->samples, 0, 16*sizeof *g->samples);
  for (unsigned i = 0; i < 256; ++i) {
    mem_wr(g, wRoomLayout + i, 1); // filler differs from every test tile
    mem_wr(g, wRoomCollisions + i, blocked ? 0xff : 0);
  }
  for (uint16_t page = 0xd2; page < 0xe0; ++page) {
    mem_wr(g, (page << 8) + INTERACTION_BASE, full ? 1 : 0);
    mem_wr(g, (page << 8) + INTERACTION_BASE + OBJ_ID, 0x77);
  }
  mem_wr(g, CONTROLLER, 1); mem_wr(g, CONTROLLER + OBJ_ID, 0xbd);
  mem_wr(g, CONTROLLER + OBJ_STATE, state);
  mem_wr(g, CONTROLLER + OBJ_YH, 0x48); mem_wr(g, CONTROLLER + OBJ_XH, 0x48);
  mem_wr(g, w1ReservedInteraction1_enabled, enabled);
  mem_wr(g, w1ReservedInteraction1_var31, tile);
  mem_wr(g, wBlockPushAngle, angle);
  for (unsigned i = 0; i < count; ++i) mem_wr(g, wRoomLayout + 0x33 + i*0x22, tile);
}

static void enter(bool candidate, bool dispatched) {
  g->d = CONTROLLER >> 8; g->sp = 0xdff0;
  // The real dispatcher performs lazy ROM validation; the unregistered test
  // hook does not. Probe a non-ROM address to perform validation for both
  // entries without executing a game routine or modifying virtual CPU state.
  g->pc = 0xffff; ASSERT(!hook_dispatch(g));
  g->rom_bank = SYMBANK(interactionCodebd); g->pc = SYM(interactionCodebd);
  ASSERT(hook_is(g, SYM(interactionCodebd), interactionCodebd_hook));
  gameplay_enable(candidate);
  if (dispatched) ASSERT(hook_dispatch(g));
  else hook_run(g, interactionCodebd_hook, SYM(interactionCodebd));
  ASSERT(!g->hung); ASSERT(!g->sample_overflow);
}

static void compare(bool candidate) {
  if (!candidate) { *expected = *g; memcpy(samples, g->samples, 16*sizeof *samples); }
  else {
    if (memcmp(expected, g, sizeof *g)) {
      fprintf(stderr, "GB mismatch: %s\n", context);
      unsigned shown = 0;
      for (size_t i = 0; i < sizeof *g && shown < 12; ++i)
        if (((const unsigned char *)expected)[i] != ((const unsigned char *)g)[i]) {
          fprintf(stderr, "byte %zu: C=%02x candidate=%02x\n", i,
            ((const unsigned char *)expected)[i], ((const unsigned char *)g)[i]); ++shown;
        }
    }
    ASSERT(!memcmp(expected, g, sizeof *g));
    ASSERT(!memcmp(samples, g->samples, 16*sizeof *samples));
  }
}

static unsigned spawned(void) {
  unsigned n = 0;
  for (uint16_t page = 0xd2; page < 0xe0; ++page) {
    uint16_t addr = (page << 8) + INTERACTION_BASE;
    if (mem_rd(g, addr) && mem_rd(g, addr + OBJ_ID) == 0x14) ++n;
  }
  return n;
}

static void directed_cases(void) {
  const uint8_t enabled_values[] = {0, 1, 2, 0x80, 0xff};
  const uint8_t tiles[] = {0x2c, 0x2d, 0xda, 0xff};
  const uint8_t counts[] = {0, 1, 3};
  const uint8_t angles[] = {0, 8, 16, 24, 0x98};
  for (int dispatch = 0; dispatch < 2; ++dispatch)
    for (int state = 0; state < 3; ++state)
      for (size_t e = 0; e < sizeof enabled_values; ++e)
        for (size_t t = 0; t < sizeof tiles; ++t)
          for (size_t n = 0; n < sizeof counts; ++n)
            for (int blocked = 0; blocked < 2; ++blocked)
              for (int full = 0; full < 2; ++full)
                for (size_t a = 0; a < sizeof angles; ++a) {
                  snprintf(context, sizeof context, "dispatch=%d state=%d enabled=%u tile=%02x count=%u blocked=%d full=%d angle=%02x",
                    dispatch, state, enabled_values[e], tiles[t], counts[n], blocked, full, angles[a]);
                  for (int mode = 0; mode < 2; ++mode) {
                    prepare(state, enabled_values[e], tiles[t], counts[n], blocked, full, angles[a]);
                    uint64_t awake_before = gameplay_block_rule_calls(GAMEPLAY_RULE_sync_awake);
                    uint64_t eligible_before = gameplay_block_rule_calls(GAMEPLAY_RULE_sync_eligible);
                    enter(mode != 0, dispatch != 0); compare(mode != 0);
                    ASSERT_EQ(gameplay_block_rule_calls(GAMEPLAY_RULE_sync_awake) - awake_before, mode && state == 1 ? 1 : 0);
                    ASSERT_EQ(gameplay_block_rule_calls(GAMEPLAY_RULE_sync_eligible) - eligible_before,
                      mode && state == 1 && enabled_values[e] ? counts[n] : 0);
                    ASSERT_EQ(mem_rd(g, CONTROLLER + OBJ_STATE), state == 1 && enabled_values[e] ? 2 : 1);
                    unsigned want = state == 1 && enabled_values[e] && tiles[t] != 0xda && !blocked && !full ? counts[n] : 0;
                    ASSERT_EQ(spawned(), want);
                  }
                  ++cases;
                }
  ASSERT_EQ(cases, 7200);
  ASSERT_EQ(gameplay_block_rule_calls(GAMEPLAY_RULE_sync_awake), 2400);
  ASSERT_EQ(gameplay_block_rule_calls(GAMEPLAY_RULE_sync_eligible), 2560);
  printf("directed: %u full GB/sample comparisons; Lua awake=%llu eligible=%llu\n", cases,
    (unsigned long long)gameplay_block_rule_calls(GAMEPLAY_RULE_sync_awake), (unsigned long long)gameplay_block_rule_calls(GAMEPLAY_RULE_sync_eligible));
}

static void resume_each_state(void) {
  char path[4096]; ASSERT(oracles_temp_path(path, sizeof path));
  for (int state = 0; state < 3; ++state) {
    snprintf(context, sizeof context, "resume state=%d", state);
    prepare(state, 1, 0x2c, 3, false, false, 8);
    ASSERT(oracles_save_boot_state(g, path));
    for (int mode = 0; mode < 2; ++mode) {
      *g = *seed; ASSERT(oracles_load_boot_state(g, path));
      memset(g->samples, 0, 16*sizeof *g->samples);
      gameplay_close(); ASSERT(gameplay_init()); // fresh VM after a simulated reopen
      enter(mode != 0, true); compare(mode != 0);
    }
  }
  ASSERT_EQ(remove(path), 0);
}

int main(void) {
  size_t size;
  uint8_t *rom = oracles_read_file(GAME_ROM_DIR "/Legend of Zelda, The - Oracle of Ages (USA, Australia).gbc", &size);
  if (!rom) SKIP("Ages ROM missing");
  char digest[41]; sha1_hex(rom, size, digest);
  ASSERT(size == 1048576 && !strcmp(digest, "880374fb978b18af4aa529e2e32f7ffb4d7dd2f4"));
  g = calloc(1, sizeof *g); seed = malloc(sizeof *seed); expected = malloc(sizeof *expected);
  samples = malloc(16*sizeof *samples); ASSERT(g && seed && expected && samples);
  gb_init(g); ASSERT(gb_load_rom(g, rom, size));
  g->cyctab = cyctab_alloc(rom, size); g->code_bits = assets_code_bits(rom, size);
  assets_zero_code(rom, size); gb_reset(g); g->ime = false; g->ie = 0;
  mem_wr(g, hActiveObjectType, INTERACTION_BASE);
  *seed = *g; hook_mode = HOOK_MODE_REPLACE;
  gameplay_close(); ASSERT(gameplay_init());
  RUN(directed_cases); RUN(resume_each_state);
  printf("ok lua_sync_candidate_rom: production hook comparison; SHA256 %s\n", gameplay_revision());
  gameplay_close(); free(samples); free(expected); free(seed); free(g); free(rom);
  return 0;
}
