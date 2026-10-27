// Public libnx/SDL2 frontend. The desktop SDL3 frontend remains independent.
#include <switch.h>
#include <SDL.h>
#include "core/gb.h"
#include "core/sha1.h"
#include "assets/assets.h"
#include "hooks/hooks.h"
#include "rt/fibers.h"
#include "platform/setup.h"
#include "platform/tas.h"
#include "game/game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stddef.h>

#define DATA "sdmc:/switch/oracle-ages-native/"
#define ROM DATA "ages.gbc"
#define SRAM DATA "sram.sav"
#define STATE DATA "quick.state"
#define ROM_SHA1 "880374fb978b18af4aa529e2e32f7ffb4d7dd2f4"
_Static_assert(sizeof(GB) == 194656, "Revalidate the v1 boot-state layout on this target");
_Static_assert(sizeof(Apu) == 65788, "Revalidate the APU layout on this target");
_Static_assert(sizeof(void *) == 8 && offsetof(GB, rom) == 24 && offsetof(GB, wram) == 49233 &&
               offsetof(GB, hram) == 82161 && offsetof(GB, vram) == 32849 && offsetof(GB, io) == 82288 &&
               offsetof(GB, framebuffer) == 82568 && offsetof(GB, apu) == 128696 &&
               offsetof(GB, cycles) == 194488 && offsetof(GB, mcycles) == 194496,
               "Revalidate the v1 selected field offsets");
#ifndef ORACLES_SWITCH_BUILD
#define ORACLES_SWITCH_BUILD "local"
#endif
static PadState pad;
static GB *game;
static uint8_t *rom;
static uint8_t live_joy;
static bool save_pending, load_pending;
static SDL_AudioDeviceID active_audio;
static bool reset_clock;
static uint8_t input_live(void *ctx, uint64_t frame) { (void)ctx; (void)frame; return live_joy; }
static uint8_t input_tas(void *ctx, uint64_t frame) { return tas_input_at(ctx, frame); }

static bool replace_file(const char *path, const void *data, size_t size) {
  char tmp[512];
  if (snprintf(tmp, sizeof tmp, "%s.tmp", path) >= (int)sizeof tmp) return false;
  FILE *f = fopen(tmp, "wb");
  if (!f) return false;
  bool ok = fwrite(data, 1, size, f) == size;
  if (fflush(f)) ok = false;
  if (fsync(fileno(f))) ok = false;
  if (fclose(f)) ok = false;
  // Do not remove the previous save if replacement fails.
  if (ok && rename(tmp, path)) ok = false;
  if (!ok) remove(tmp);
  return ok;
}
static bool save_sram(void) { return replace_file(SRAM, game->eram, game->eram_size); }
// libnx dispatches these hooks from appletMainLoop, outside a game fiber.
static void lifecycle(AppletHookType type, void *param) {
  (void)param;
  if (type == AppletHookType_OnFocusState || type == AppletHookType_OnResume) {
    reset_clock = true;
    if (active_audio) {
      SDL_ClearQueuedAudio(active_audio);
      SDL_PauseAudioDevice(active_audio, appletGetFocusState() != AppletFocusState_InFocus);
    }
    if (type == AppletHookType_OnFocusState && appletGetFocusState() != AppletFocusState_InFocus)
      if (!save_sram()) fprintf(stderr, "SRAM save on focus loss failed\n");
  }
}
static bool save_state(const char *path) {
  if (!threads_parked(game)) return false;
  char tmp[512];
  snprintf(tmp, sizeof tmp, "%s.tmp", path);
  if (!oracles_save_boot_state(game, tmp)) { remove(tmp); return false; }
  FILE *f = fopen(tmp, "r+b");
  if (!f) return false;
  bool ok = fsync(fileno(f)) == 0;
  if (fclose(f)) ok = false;
  if (ok && rename(tmp, path)) ok = false;
  if (!ok) remove(tmp);
  return ok;
}
static bool load_state(const char *path) {
  if (!oracles_load_boot_state(game, path)) return false;
  fibers_reset(game);
  return true;
}
static bool prepare_game(bool replay) {
  size_t n = 0;
  rom = oracles_read_file(ROM, &n);
  if (!rom) { printf("Copy your Ages USA/Australia ROM to:\n%s\n", ROM); return false; }
  char hash[41]; sha1_hex(rom, n, hash);
  if (n != 1048576 || strcmp(hash, ROM_SHA1)) {
    printf("Unsupported ROM. Expected Ages USA/Australia.\nSHA1: %s\n", hash); return false;
  }
  game = calloc(1, sizeof *game);
  if (!game) return false;
  gb_init(game);
  if (!game->samples || !gb_load_rom(game, rom, n)) return false;
  game->cyctab = cyctab_alloc(rom, n);
  game->code_bits = assets_code_bits(rom, n);
  if (!game->cyctab || !game->code_bits) return false;
  printf("Code bytes removed: %zu\n", assets_zero_code(rom, n));
  gb_reset(game);
  if (replay) return load_state(DATA "tests/ages-boot.state");
  struct stat st;
  if (!stat(SRAM, &st)) {
    uint8_t *s = oracles_read_file(SRAM, &n);
    if (!s || n != game->eram_size) { free(s); printf("Invalid SRAM; preserved on SD.\n"); return false; }
    memcpy(game->eram, s, n); free(s);
  }
  game->input_at = input_live;
  return true;
}

// Strict reference consumption: missing/truncated checkpoints are errors too.
typedef struct {
  FILE *ref, *log;
  uint64_t next_frame, want, checked, frame;
  bool have, fail;
} Replay;
static bool next_ref(Replay *r) {
  unsigned long long frame, hash;
  int n = fscanf(r->ref, "%llu %llx", &frame, &hash);
  if (n == 2) { r->next_frame = frame; r->want = hash; return true; }
  if (!feof(r->ref)) r->fail = true;
  return false;
}
static void replay_frame(GB *gb, const GBSample *sample, void *ctx) {
  Replay *r = ctx; r->frame = sample->frame;
  if (!r->have || r->fail) return;
  if (sample->frame > r->next_frame) { r->fail = true; return; }
  if (sample->frame != r->next_frame) return;
  uint64_t got = gb_state_hash(gb);
  fprintf(r->log, "%llu %016llx\n", (unsigned long long)sample->frame, (unsigned long long)got);
  if (got != r->want) { r->fail = true; return; }
  r->checked++;
  r->have = next_ref(r);
}
static int replay(bool full) {
  if (!prepare_game(true)) return 1;
  Tas tas = {0};
  Replay r = {0};
  r.ref = fopen(DATA "tests/ages.ref", "r");
  r.log = fopen(DATA "replay-observed.txt", "w");
  if (!r.ref || !r.log || !tas_load(&tas, DATA "tests/ages-consoleverified.inputs")) {
    printf("Missing test fixtures or cannot write log.\n");
    if (r.ref) fclose(r.ref);
    if (r.log) fclose(r.log);
    tas_free(&tas); return 1;
  }
  unsigned long long ref_frame, ref_hash, previous = 0;
  unsigned records = 0;
  int fields;
  while ((fields = fscanf(r.ref, "%llu %llx", &ref_frame, &ref_hash)) == 2) {
    if (ref_frame <= previous || ref_frame % 60) r.fail = true;
    previous = ref_frame; records++;
  }
  if (fields != EOF || ferror(r.ref) || records != 4825) r.fail = true;
  rewind(r.ref);
  r.have = next_ref(&r);
  if (!r.have) r.fail = true;
  game->input_at = input_tas; game->input_ctx = &tas;
  game->frame_cb = replay_frame; game->frame_ctx = &r;
  const uint64_t limit = full ? 289518 : 20000;
  const uint64_t expected = full ? 4825 : 333;
  bool cancelled = false, reload_pending = false;
  unsigned reloads = 0;
  uint64_t start = armGetSystemTick(), frames = 0;
  for (; frames < limit && !r.fail && !game->hung; ++frames) {
    if (frames && frames % 1013 == 0) reload_pending = true;
    if (reload_pending && threads_parked(game)) {
      if (!save_state(DATA "replay-reload.state") || !load_state(DATA "replay-reload.state")) { r.fail = true; break; }
      reload_pending = false; reloads++;
    }
    gb_run_frame(game);
    game->apu.ring_r = game->apu.ring_w;
    if (frames % 120 == 0) {
      if (!appletMainLoop()) { cancelled = true; break; }
      padUpdate(&pad);
      if (padGetButtonsDown(&pad) & HidNpadButton_Plus) { cancelled = true; break; }
      printf("\x1b[10;1HFrames %llu/%llu; checks %llu; reloads %u\n", (unsigned long long)frames,
             (unsigned long long)limit, (unsigned long long)r.checked, reloads);
      consoleUpdate(NULL);
    }
  }
  bool ok = !cancelled && !r.fail && !game->hung && frames == limit && r.checked == expected && reloads > 0;
  if (full && r.have) ok = false;
  if (fflush(r.log) || ferror(r.log)) ok = false;
  if (fclose(r.log)) ok = false;
  fclose(r.ref); tas_free(&tas);
  double seconds = (armGetSystemTick() - start) / 19200000.0;
  FILE *result = fopen(DATA "replay-result.txt", "w");
  if (result) {
    fprintf(result, "build=%s\nstatus=%s\nframes=%llu\ncheckpoints=%llu\nexpected=%llu\nreloads=%u\nseconds=%.3f\nsizeof_GB=%zu\nsizeof_Apu=%zu\nreason=%s\n",
      ORACLES_SWITCH_BUILD, ok ? "PASS" : cancelled ? "CANCELLED" : "FAIL", (unsigned long long)frames,
      (unsigned long long)r.checked, (unsigned long long)expected, reloads, seconds, sizeof(GB), sizeof(Apu), gb_stop_reason);
    fclose(result);
  }
  printf("\n%s: %llu/%llu checkpoints, %.2fs\n", ok ? "PASS" : "FAIL / CANCELLED",
         (unsigned long long)r.checked, (unsigned long long)expected, seconds);
  return ok ? 0 : 1;
}

static uint8_t joy_from_pad(u64 b) {
  uint8_t j = 0;
  if (b & HidNpadButton_A) j |= JOY_A;
  if (b & HidNpadButton_B) j |= JOY_B;
  if (b & HidNpadButton_Plus) j |= JOY_START;
  if (b & HidNpadButton_Minus) j |= JOY_SELECT;
  if (b & (HidNpadButton_Right | HidNpadButton_StickLRight)) j |= JOY_RIGHT;
  if (b & (HidNpadButton_Left | HidNpadButton_StickLLeft)) j |= JOY_LEFT;
  if (b & (HidNpadButton_Up | HidNpadButton_StickLUp)) j |= JOY_UP;
  if (b & (HidNpadButton_Down | HidNpadButton_StickLDown)) j |= JOY_DOWN;
  return j;
}
static int play(void) {
  if (!prepare_game(false)) return 1;
  consoleExit(NULL);
  SDL_Window *window = NULL; SDL_Renderer *renderer = NULL; SDL_Texture *texture = NULL;
  SDL_AudioDeviceID audio = 0;
  AppletHookCookie hook = {0};
  bool hooked = false;
  int result = 1;
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) goto done;
  window = SDL_CreateWindow("Oracle of Ages", 0, 0, 1280, 720, SDL_WINDOW_FULLSCREEN_DESKTOP);
  if (!window) goto done;
  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  if (!renderer) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
  if (!renderer) goto done;
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
  texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, FB_W, FB_H);
  if (!texture) goto done;
  SDL_AudioSpec spec = {0}; spec.freq = APU_SAMPLE_RATE; spec.format = AUDIO_S16SYS;
  spec.channels = 2; spec.samples = 1024;
  audio = SDL_OpenAudioDevice(NULL, 0, &spec, NULL, 0);
  if (!audio) goto done;
  active_audio = audio;
  appletHook(&hook, lifecycle, NULL); hooked = true;
  if (appletGetAppletType() == AppletType_Application)
    appletSetFocusHandlingMode(AppletFocusHandlingMode_SuspendHomeSleepNotify);
  SDL_PauseAudioDevice(audio, 0);
  static uint32_t pixels[FB_W * FB_H];
  static int16_t pcm[4096 * 2];
  uint64_t frames = 0, late = 0, underruns = 0;
  uint64_t frequency = SDL_GetPerformanceFrequency();
  uint64_t due = SDL_GetPerformanceCounter();
  // Fractional clock accumulator; never replace the GB cadence with 60 Hz.
  uint64_t remainder = 0;
  while (appletMainLoop() && !game->hung) {
    if (reset_clock) { due = SDL_GetPerformanceCounter(); remainder = 0; reset_clock = false; }
    SDL_Event event; bool quit = false;
    while (SDL_PollEvent(&event)) if (event.type == SDL_QUIT) quit = true;
    padUpdate(&pad); u64 held = padGetButtons(&pad), down = padGetButtonsDown(&pad);
    if (quit || ((held & (HidNpadButton_L | HidNpadButton_R | HidNpadButton_Plus)) ==
                         (HidNpadButton_L | HidNpadButton_R | HidNpadButton_Plus))) break;
    live_joy = joy_from_pad(held);
    if (down & HidNpadButton_ZL) save_pending = true;
    if (down & HidNpadButton_ZR) load_pending = true;
    if (threads_parked(game)) {
      if (save_pending) { if (!save_state(STATE)) fprintf(stderr, "Quicksave failed\n"); save_pending = false; }
      if (load_pending) { if (!load_state(STATE)) fprintf(stderr, "Quickload failed\n"); SDL_ClearQueuedAudio(audio); load_pending = false; }
    }
    gb_run_frame(game);
    uint32_t count;
    if (frames > 10 && !SDL_GetQueuedAudioSize(audio)) underruns++;
    while ((count = apu_read_samples(&game->apu, pcm, 4096))) {
      if (SDL_QueueAudio(audio, pcm, count * 4)) goto done;
    }
    // Bound latency, including after suspend or a slow SD write.
    if (SDL_GetQueuedAudioSize(audio) > APU_SAMPLE_RATE * 4 / 5) SDL_ClearQueuedAudio(audio);
    const uint16_t *fb = game->sample ? game->sample->framebuffer : game->framebuffer;
    for (int i = 0; i < FB_W * FB_H; ++i) {
      unsigned c = fb[i], r = c & 31, g = (c >> 5) & 31, b = (c >> 10) & 31;
      pixels[i] = 0xff000000u | ((b * 255 / 31) << 16) | ((g * 255 / 31) << 8) | (r * 255 / 31);
    }
    SDL_UpdateTexture(texture, NULL, pixels, FB_W * 4);
    int w, h; SDL_GetRendererOutputSize(renderer, &w, &h);
    int scale = w / FB_W < h / FB_H ? w / FB_W : h / FB_H;
    if (scale < 1) scale = 1;
    SDL_Rect dst = {(w - FB_W * scale) / 2, (h - FB_H * scale) / 2, FB_W * scale, FB_H * scale};
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, &dst); SDL_RenderPresent(renderer);
    if (++frames % 600 == 0 && !save_sram()) fprintf(stderr, "SRAM save failed\n");
    remainder += frequency * FRAME_CYCLES;
    due += remainder / 4194304; remainder %= 4194304;
    uint64_t now = SDL_GetPerformanceCounter();
    if (now > due) { late++; if (now - due > frequency / 4) { due = now; SDL_ClearQueuedAudio(audio); } }
    else if (due - now > frequency / 1000) SDL_Delay((Uint32)((due - now) * 1000 / frequency));
  }
  result = game->hung ? 1 : 0;
  fprintf(stderr, "frames=%llu late=%llu underruns=%llu reason=%s\n", (unsigned long long)frames,
          (unsigned long long)late, (unsigned long long)underruns, gb_stop_reason);
  if (!save_sram()) { fprintf(stderr, "Final SRAM save failed\n"); result = 1; }
 done:
  if (result) fprintf(stderr, "SDL: %s; engine: %s\n", SDL_GetError(), gb_stop_reason);
  if (hooked) appletUnhook(&hook);
  active_audio = 0;
  if (audio) SDL_CloseAudioDevice(audio);
  if (texture) SDL_DestroyTexture(texture);
  if (renderer) SDL_DestroyRenderer(renderer);
  if (window) SDL_DestroyWindow(window);
  SDL_Quit(); consoleInit(NULL);
  return result;
}
int main(int argc, char **argv) {
  (void)argc; (void)argv;
  padConfigureInput(1, HidNpadStyleSet_NpadStandard); padInitializeDefault(&pad);
  consoleInit(NULL);
  mkdir("sdmc:/switch", 0777); mkdir(DATA, 0777);
  freopen(DATA "runtime.log", "w", stderr);
  hooks_init(); hook_mode = HOOK_MODE_REPLACE;
  printf("Oracle of Ages - Switch\nBuild: %s\nNative C / SDL2 / libnx\n\nA: Play\nX: Short replay + reload\nY: Full replay + reload\nPlus: Exit\n\nROM: %s\n", ORACLES_SWITCH_BUILD, ROM);
  int mode = -1;
  while (appletMainLoop()) {
    padUpdate(&pad); u64 down = padGetButtonsDown(&pad);
    if (down & HidNpadButton_Plus) break;
    if (down & HidNpadButton_A) { mode = 0; break; }
    if (down & HidNpadButton_X) { mode = 1; break; }
    if (down & HidNpadButton_Y) { mode = 2; break; }
    consoleUpdate(NULL);
  }
  int result = mode == 0 ? play() : mode > 0 ? replay(mode == 2) : 0;
  if (mode >= 0) {
    printf("\nFinished (%d). Plus to exit.\nSee runtime.log / replay-result.txt on SD.\n", result);
    while (appletMainLoop()) { padUpdate(&pad); if (padGetButtonsDown(&pad) & HidNpadButton_Plus) break; consoleUpdate(NULL); }
  }
  consoleExit(NULL);
  return result;
}
