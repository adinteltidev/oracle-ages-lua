#include "unit.h"
#ifdef ORACLES_LUA_GAMEPLAY
#include "script/gameplay.h"
#include "script/block_rules.h"
#include "script/colored_cube.h"
#include "script/levers_lava.h"
#include "script/doors.h"
#include "script/items_shops.h"
#endif
#include "platform/tempfile.h"
#include "core/gb.h"
#include "core/sha1.h"
#include "platform/tas.h"
#include "platform/setup.h"
#include "hooks/hooks.h"
#include "game/game.h"
#include "assets/assets.h"
#include "rt/fibers.h"
#include <stdlib.h>

static uint8_t tas_cb(void *ctx, uint64_t frame) { return tas_input_at((const Tas *)ctx, frame); }
static const char *ages_rom = GAME_ROM_DIR "/Legend of Zelda, The - Oracle of Ages (USA, Australia).gbc";
static char ages_inputs[4096], ages_reference[4096], ages_boot[4096];
static uint64_t ages_frames_override;

typedef struct {
  FILE *ref; unsigned long long f, want; bool have;
  uint64_t mismatch, checked, skipped, first_sample, last_sample, samples;
} RefCheck;
static void ref_cb(GB *gb, const GBSample *sm, void *ctx) {
  RefCheck *r = ctx;
  if (!r->samples) r->first_sample = sm->frame;
  r->last_sample = sm->frame; ++r->samples;
  if (r->mismatch) return;
  while (r->have && r->f < sm->frame) {
    ++r->skipped;
    fprintf(stderr, "checkpoint skipped: reference=%llu sample=%llu\n", r->f, (unsigned long long)sm->frame);
    r->have = fscanf(r->ref, "%llu %llx", &r->f, &r->want) == 2;
  }
  if (r->have && r->f == sm->frame) {
    r->checked++;
    if (r->want != gb_state_hash(gb)) r->mismatch = sm->frame;
    r->have = fscanf(r->ref, "%llu %llx", &r->f, &r->want) == 2;
  }
}

// The native build has no interpreter, so it cannot run the CGB boot ROM: it starts from the
// post-boot state that oracles-run --boot-state-out recorded (tas/ages-boot.state) and must then
// produce the same state hashes as the emulator-hosted build.
static void native_run_reloading(const char *rom_path, const char *inputs, const char *ref_path, const char *state_path, uint64_t frames, uint64_t reload_every);
static void native_run(const char *rom_path, const char *inputs, const char *ref_path, const char *state_path, uint64_t frames) {
  native_run_reloading(rom_path, inputs, ref_path, state_path, frames, 0);
}

// reload_every: a save state round trip (every thread fiber dropped, as when the app loads a
// state) at the first frame after each multiple whose threads are parked, as the app saves.
static void native_run_reloading(const char *rom_path, const char *inputs, const char *ref_path, const char *state_path, uint64_t frames, uint64_t reload_every) {
#ifdef ORACLES_LUA_GAMEPLAY
  uint64_t rules_before[GAMEPLAY_BLOCK_RULE_COUNT];
  uint64_t colors_before[GAMEPLAY_COLORED_CUBE_RULE_COUNT];
  uint64_t items_before[GAMEPLAY_ITEMS_COUNT],shops_before[GAMEPLAY_SHOPS_COUNT];
  for(int i=0;i<GAMEPLAY_ITEMS_COUNT;++i)items_before[i]=gameplay_item_calls(i);
  for(int i=0;i<GAMEPLAY_SHOPS_COUNT;++i)shops_before[i]=gameplay_shop_calls(i);
  uint64_t doors_before[GAMEPLAY_DOOR_RULE_COUNT];
  for(int i=0;i<GAMEPLAY_DOOR_RULE_COUNT;++i) doors_before[i]=gameplay_door_calls(i);
  uint64_t lava_before[GAMEPLAY_LAVA_RULE_COUNT];
  for(int i=0;i<GAMEPLAY_LAVA_RULE_COUNT;++i) lava_before[i]=gameplay_lava_calls(i);
  for(int i=0;i<GAMEPLAY_COLORED_CUBE_RULE_COUNT;++i) colors_before[i]=gameplay_colored_cube_calls(i);
  for (int i=0;i<GAMEPLAY_BLOCK_RULE_COUNT;++i) rules_before[i]=gameplay_block_rule_calls(i);
#endif
  size_t n;
  uint8_t *rom = oracles_read_file(rom_path, &n);
  if (!rom) SKIP("ROM not present");
  if (frames == 20000) {
    char sha1[41]; sha1_hex(rom, n, sha1);
    ASSERT(n == 1048576 && !strcmp(sha1, "880374fb978b18af4aa529e2e32f7ffb4d7dd2f4"));
  }
  Tas t;
  if (!tas_load(&t, inputs)) SKIP("inputs missing");
  FILE *ref = fopen(ref_path, "r");
  if (!ref) SKIP("reference missing");
  GB *gb = calloc(1, sizeof *gb);
  gb_init(gb);
  ASSERT(gb_load_rom(gb, rom, n));
  gb->cyctab = cyctab_alloc(rom, n);
  gb->code_bits = assets_code_bits(rom, n);
  assets_zero_code(rom, n);
  gb_reset(gb);
  if (!oracles_load_boot_state(gb, state_path)) SKIP("boot state missing");
  const char *limit_env = getenv("TAS_FRAMES");     // lifts the Ages default; a whole-movie value must not outrun the Seasons reference
  uint64_t limit = frames;
  if (limit_env && frames == 20000) limit = strtoull(limit_env, NULL, 10);
  if (ages_frames_override && frames == 20000) limit = ages_frames_override;
  RefCheck rc = {.ref = ref};
  rc.have = fscanf(ref, "%llu %llx", &rc.f, &rc.want) == 2;
  gb->input_at = tas_cb; gb->input_ctx = &t;
  gb->frame_cb = ref_cb; gb->frame_ctx = &rc;
  bool pending = false;
  unsigned reloads = 0;
  char reload_path[4096];
  if (reload_every) ASSERT(oracles_temp_path(reload_path, sizeof reload_path));
  for (uint64_t i = 0; i < limit && !rc.mismatch && !gb->hung; i++) {
    if (reload_every && i && i % reload_every == 0) pending = true;
    // Load resets the host sample ring. Consume queued samples before saving,
    // otherwise a checkpoint in that queue is silently lost by the test itself.
    if (pending && !gb->sample_count && threads_parked(gb)) {
      ASSERT(oracles_save_boot_state(gb, reload_path));
      ASSERT(oracles_load_boot_state(gb, reload_path));
      fibers_reset(gb);
      ++reloads;
      pending = false;
    }
    gb_run_frame(gb);
    if ((i + 1) % 25000 == 0) {
      fprintf(stderr, "replay%s: %llu/%llu frames, checked=%llu skipped=%llu reloads=%u\n",
        reload_every ? " reload" : " normal", (unsigned long long)(i + 1),
        (unsigned long long)limit, (unsigned long long)rc.checked,
        (unsigned long long)rc.skipped, reloads);
      fflush(stderr);
    }
  }
  if (gb->hung) { fprintf(stderr, "native build stopped at frame %llu\n", (unsigned long long)GRID_FRAME(gb->cycles)); ASSERT(0); }
  if (rc.mismatch) { fprintf(stderr, "state mismatch at frame %llu\n", rc.mismatch); ASSERT(0); }
  ASSERT(rc.checked > 0);
  if (frames == 20000) ASSERT_EQ(rc.skipped, 0); // Ages: never silently pass a dropped checkpoint.
  printf("verified %llu checkpoints over %llu frames%s\n",
         (unsigned long long)rc.checked, (unsigned long long)limit,
         reload_every ? " with state reloads" : "");
  printf("checkpoint diagnostics: skipped=%llu reloads=%u\n", (unsigned long long)rc.skipped, reloads);
  printf("sample interval: first=%llu last=%llu samples=%llu\n",
    (unsigned long long)rc.first_sample, (unsigned long long)rc.last_sample, (unsigned long long)rc.samples);
  for(int i=0;i<GAMEPLAY_DOOR_RULE_COUNT;++i)
    printf("Lua rule %s: %llu\n",gameplay_door_rule_name(i),(unsigned long long)(gameplay_door_calls(i)-doors_before[i]));
  for(int i=0;i<GAMEPLAY_ITEMS_COUNT;++i)printf("Lua rule %s: %llu\n",gameplay_item_rule_name(i),(unsigned long long)(gameplay_item_calls(i)-items_before[i]));
  for(int i=0;i<GAMEPLAY_SHOPS_COUNT;++i)printf("Lua rule %s: %llu\n",gameplay_shop_rule_name(i),(unsigned long long)(gameplay_shop_calls(i)-shops_before[i]));
  fflush(stdout);
#ifdef ORACLES_LUA_GAMEPLAY
  printf("Lua cumulative block coverage: %llu/%llu/%llu/%llu\n",
    (unsigned long long)gameplay_block_calls(0), (unsigned long long)gameplay_block_calls(1),
    (unsigned long long)gameplay_block_calls(2), (unsigned long long)gameplay_block_calls(3));
  for (int i=0;i<GAMEPLAY_BLOCK_RULE_COUNT;++i)
    printf("Lua rule %s: %llu\n",gameplay_block_rule_name(i),
      (unsigned long long)(gameplay_block_rule_calls(i)-rules_before[i]));
  for(int i=0;i<GAMEPLAY_COLORED_CUBE_RULE_COUNT;++i)
    printf("Lua rule %s: %llu\n",gameplay_colored_cube_rule_name(i),(unsigned long long)(gameplay_colored_cube_calls(i)-colors_before[i]));
  for(int i=0;i<GAMEPLAY_LAVA_RULE_COUNT;++i)
    printf("Lua rule %s: %llu\n",gameplay_lava_rule_name(i),(unsigned long long)(gameplay_lava_calls(i)-lava_before[i]));
  fflush(stdout);
#endif
  fclose(ref); free(gb); free(rom); tas_free(&t);
  if (reload_every) ASSERT(remove(reload_path) == 0);
}

static void native_full_tas_matches_reference(void) {
  native_run(ages_rom, ages_inputs, ages_reference, ages_boot, 20000);
}

// Seasons: the recorded playthrough, whole (33,337 frames), on the same binary
// Seasons: the console-verified TAS, the whole game, from a boot with RAM cleared
static void native_seasons_tas_matches_reference(void) {
  native_run(GAME_ROM_DIR "/Legend of Zelda, The - Oracle of Seasons (USA, Australia).gbc", TAS_DIR "/seasons-consoleverified.inputs", TAS_DIR "/seasons.ref", TAS_DIR "/seasons-consoleverified-boot.state", 321712);
}

static void native_seasons_play_matches_reference(void) {
  native_run(GAME_ROM_DIR "/Legend of Zelda, The - Oracle of Seasons (USA, Australia).gbc", TAS_DIR "/seasons-play.inputs", TAS_DIR "/seasons-play.ref", TAS_DIR "/seasons-boot.state", 265064);
}

// Ages with a save state loaded about every 1000 frames: the native build resumes every thread
static void native_ages_survives_save_states(void) {
  native_run_reloading(ages_rom, ages_inputs, ages_reference, ages_boot, 20000, 1013);
}

int main(int argc, char **argv) {
  const char *run = getenv("TAS_RUN");
  const char *tas_dir = TAS_DIR;
  const char *usage = "usage: test_native_ages [--rom FILE] [--tas-dir DIR] [--frames N] [--run normal|reload|both]\n";
  for (int i = 1; i < argc; ++i) {
    if (!strcmp(argv[i], "--help")) { fputs(usage, stdout); return 0; }
    if (i + 1 >= argc) { fputs(usage, stderr); return 2; }
    if (!strcmp(argv[i], "--rom")) ages_rom = argv[++i];
    else if (!strcmp(argv[i], "--tas-dir")) tas_dir = argv[++i];
    else if (!strcmp(argv[i], "--run")) run = argv[++i];
    else if (!strcmp(argv[i], "--frames")) {
      char *end; const char *value = argv[++i];
      ages_frames_override = strtoull(value, &end, 10);
      if (!*value || *value == '-' || *end || !ages_frames_override || ages_frames_override > 1000000) {
        fputs("frames must be between 1 and 1000000\n", stderr); return 2;
      }
    } else { fputs(usage, stderr); return 2; }
  }
  ASSERT(!run || !strcmp(run, "both") || !strcmp(run, "normal") || !strcmp(run, "reload"));
  ASSERT(snprintf(ages_inputs, sizeof ages_inputs, "%s/ages-consoleverified.inputs", tas_dir) < (int)sizeof ages_inputs);
  ASSERT(snprintf(ages_reference, sizeof ages_reference, "%s/ages.ref", tas_dir) < (int)sizeof ages_reference);
  ASSERT(snprintf(ages_boot, sizeof ages_boot, "%s/ages-boot.state", tas_dir) < (int)sizeof ages_boot);
#ifdef ORACLES_LUA_GAMEPLAY
  const char *mode = getenv("TAS_GAMEPLAY");
  ASSERT(!mode || !strcmp(mode, "c") || !strcmp(mode, "lua"));
  gameplay_enable(!mode || strcmp(mode, "c"));
  printf("Replay gameplay mode: %s\n", mode ? mode : "lua"); fflush(stdout);
#endif
  if (!run || strcmp(run, "reload")) {
    RUN(native_full_tas_matches_reference);
#ifndef ORACLES_TEST_AGES_ONLY
    RUN(native_seasons_play_matches_reference);
    RUN(native_seasons_tas_matches_reference);
#endif
  }
  if (!run || strcmp(run, "normal")) RUN(native_ages_survives_save_states);
#ifdef ORACLES_LUA_GAMEPLAY
  printf("Lua coverage: changed=%llu alternate=%llu\n",(unsigned long long)gameplay_calls(0),(unsigned long long)gameplay_calls(1));
  printf("Lua scripts SHA256: %s; block states=%llu/%llu/%llu/%llu\n", gameplay_revision(),
    (unsigned long long)gameplay_block_calls(0), (unsigned long long)gameplay_block_calls(1),
    (unsigned long long)gameplay_block_calls(2), (unsigned long long)gameplay_block_calls(3));
#endif
  return 0;
}
