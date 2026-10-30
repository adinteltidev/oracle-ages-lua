#include "unit.h"
#include "lua_cube_candidate.h"
#include "core/gb.h"
#include "core/bus.h"
#include "core/sha1.h"
#include "assets/assets.h"
#include "platform/setup.h"
#include "script/gameplay.h"

// White-box reference: call the actual private C gate without changing its
// visibility or any production source. Rename only this test copy's public hook
// so the normal native library keeps its original symbol and dispatch table.
#define interactionCode14_hook cube_reference_interaction_hook
#include "../src/game/common/interactions/pushblock.c"
#undef interactionCode14_hook

int main(void) {
  size_t size;
  uint8_t *rom = oracles_read_file(GAME_ROM_DIR "/Legend of Zelda, The - Oracle of Ages (USA, Australia).gbc", &size);
  if (!rom) SKIP("Ages ROM missing");
  char digest[41]; sha1_hex(rom, size, digest);
  ASSERT(size == 1048576 && !strcmp(digest, "880374fb978b18af4aa529e2e32f7ffb4d7dd2f4"));
  GB *g = calloc(1, sizeof *g), *seed = malloc(sizeof *seed);
  ASSERT(g && seed); gb_init(g); ASSERT(gb_load_rom(g, rom, size));
  g->cyctab = cyctab_alloc(rom, size); g->code_bits = assets_code_bits(rom, size);
  assets_zero_code(rom, size); gb_reset(g);
  g->ime = false; g->ie = 0; g->d = 0xd0; g->sp = 0xdff0;
  g->rom_bank = SYMBANK(interactionCode14); g->pc = SYM(interactionCode14) + 194;
  *seed = *g;
  const uint8_t positions[] = {0, 1, 0xff};
  const uint8_t tiles[] = {0, 1, 0x2b, 0x2c, 0x2d, 0x2e, 0x7f, 0x80, 0xab, 0xac, 0xad, 0xff};
  unsigned checked = 0;
  for (size_t p = 0; p < sizeof positions; ++p) {
    ASSERT(cube_candidate_load(CUBE_ALLOWS, positions[p]));
    for (int color = 0; color < 256; ++color) for (size_t t = 0; t < sizeof tiles; ++t) {
      bool permission; ASSERT(cube_candidate_query(color, tiles[t], &permission));
      for (int flags = 0; flags < 2; ++flags) {
        *g = *seed; g->f = flags ? 0xf0 : 0;
        mem_wr(g, wRotatingCubePos, positions[p]); mem_wr(g, wRotatingCubeColor, color);
        mem_wr(g, 0xd040 + OBJ_VAR31, tiles[t]);
        pushblock_checkRotatingCubePermitsPushing(g);
        ASSERT(!g->hung);
        ASSERT_EQ(permission, !(g->f & FC));
        ASSERT_EQ(g->sp, seed->sp + 2);
        ++checked;
      }
    }
  }
  ASSERT_EQ(checked, 18432);
  printf("ok lua_cube_candidate_rom: %u actual C gate results; candidate SHA256 %s\n", checked, cube_candidate_revision());
  puts("Logical permission comparison only; candidate is not integrated into gameplay.");
  gameplay_close(); free(seed); free(g); free(rom);
  return 0;
}
