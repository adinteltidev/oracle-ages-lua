// Reproduce the previously observed room through natural TAS inputs, then walk into its wall.
#include "unit.h"
#include "core/gb.h"
#include "platform/tas.h"
#include "platform/setup.h"
#include "assets/assets.h"
#include "hw/render.h"
#include "platform/png.h"
#include "game/game.h"
#include <stdlib.h>

typedef struct { Tas tas; int override; } Input;
static uint8_t input_at(void *ctx, uint64_t frame) {
  Input *in = ctx;
  return in->override < 0 ? tas_input_at(&in->tas, frame) : (uint8_t)in->override;
}
static unsigned x(const GB *gb) { return gb->wram[1][0x0c] | gb->wram[1][0x0d] << 8; }
static unsigned y(const GB *gb) { return gb->wram[1][0x0a] | gb->wram[1][0x0b] << 8; }
static bool in_room(const GB *gb) {
  return gb->wram[0][0xc2d] == 0 && gb->wram[0][0xc30] == 0x8a;
}
static void tick(GB *gb) { gb_run_frame(gb); ASSERT(!gb->hung); }
static void screenshot(GB *gb, const char *name) {
  const char *dir = getenv("ORACLES_TEST_ARTIFACTS");
  if (!dir) return;
  char path[4096]; uint8_t rgb[FB_W * FB_H * 3];
  snprintf(path, sizeof path, "%s/%s.png", dir, name);
  framebuffer_to_rgb(gb->sample ? gb->sample->framebuffer : gb->framebuffer, rgb);
  ASSERT(png_write_rgb(path, rgb, FB_W, FB_H));
}

int main(void) {
  size_t size;
  uint8_t *rom = oracles_read_file(GAME_ROM_DIR "/Legend of Zelda, The - Oracle of Ages (USA, Australia).gbc", &size);
  if (!rom) SKIP("Ages ROM missing");
  Input in = {.override = -1};
  ASSERT(tas_load(&in.tas, TAS_DIR "/ages-consoleverified.inputs"));
  GB *gb = calloc(1, sizeof *gb);
  gb_init(gb);
  ASSERT(gb_load_rom(gb, rom, size));
  gb->cyctab = cyctab_alloc(rom, size);
  gb->code_bits = assets_code_bits(rom, size);
  assets_zero_code(rom, size);
  gb_reset(gb);
  ASSERT(oracles_load_boot_state(gb, TAS_DIR "/ages-boot.state"));
  gb->input_at = input_at; gb->input_ctx = &in;
  bool found = false;
  for (unsigned i = 0; i < 20000; ++i) {
    tick(gb);
    // The TAS presses Up on the very first normal update. Capture the preceding
    // initialization frame too, then release input while normal logic settles.
    if (in_room(gb) && gb->wram[0][0x2ee] == 2 && gb->wram[0][0x2ef] <= 1 &&
        gb->wram[1][4] == 1 && !gb->wram[0][0xba0] && !(gb->wram[0][0xc8a] & 0x81) &&
        !gb->wram[0][0x4ab] && !(gb->wram[0][0xd00] & 0x0e) && !gb->wram[0][0xc4f] &&
        y(gb) == 56 * 256 && x(gb) >= 70 * 256 && x(gb) <= 76 * 256) {
      found = true; break;
    }
  }
  ASSERT(found);
  in.override = 0;
  for (unsigned i = 0; i < 10; ++i) tick(gb);
  ASSERT(in_room(gb)); ASSERT_EQ(y(gb), 56 * 256);
  const unsigned initial = x(gb);
  screenshot(gb, "native-start");
  unsigned moves = 0;
  in.override = JOY_RIGHT;
  for (unsigned i = 0; i < 20; ++i) {
    unsigned before = x(gb);
    tick(gb);
    unsigned dx = x(gb) - before;
    ASSERT(dx == 0 || dx == 256);
    if (dx) ++moves;
    ASSERT_EQ(y(gb), 56 * 256); ASSERT(in_room(gb));
  }
  ASSERT(moves >= 18);
  screenshot(gb, "native-walk");
  printf("free movement: X=%.2f -> %.2f, %u one-pixel updates, Y=56\n",
         initial / 256.0, x(gb) / 256.0, moves);
  for (unsigned i = 0; i < 100; ++i) tick(gb);
  ASSERT_EQ(x(gb), 124 * 256); ASSERT_EQ(y(gb), 56 * 256);
  ASSERT_EQ(gb->wram[1][0x33], 3);
  for (unsigned i = 0; i < 10; ++i) { tick(gb); ASSERT_EQ(x(gb), 124 * 256); }
  ASSERT_EQ(gb->wram[0][0x6aa], 12);
  screenshot(gb, "native-wall");
  printf("collision: X=124, Y=56, walls=03, health=12; ten blocked frames\n");
  printf("ok native_movement (natural TAS setup, no game RAM edits)\n");
  const char *dir = getenv("ORACLES_TEST_ARTIFACTS");
  if (dir) {
    in.override = 0;
    for (unsigned i = 0; i < 60 && !threads_parked(gb); ++i) tick(gb);
    ASSERT(threads_parked(gb));
    char path[4096];
    snprintf(path, sizeof path, "%s/native-wall.state", dir);
    ASSERT(oracles_save_boot_state(gb, path));
    snprintf(path, sizeof path, "%s/native-sram.sav", dir);
    FILE *save = fopen(path, "wb"); ASSERT(save);
    ASSERT(fwrite(gb->eram, 1, gb->eram_size, save) == gb->eram_size);
    ASSERT_EQ(fclose(save), 0);
  }
  free(gb); free(rom); tas_free(&in.tas);
  return 0;
}
