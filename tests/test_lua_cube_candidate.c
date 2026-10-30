#include "unit.h"
#include "lua_cube_candidate.h"
#include "script/gameplay.h"

static uint64_t checked;
static void check(uint8_t a, uint8_t b, bool expected) {
  bool actual;
  ASSERT(cube_candidate_query(a, b, &actual));
  ASSERT_EQ(actual, expected);
  ++checked;
}

static void staged_boundaries(void) {
  ASSERT(cube_candidate_load(CUBE_NO_CUBE, 0));
  for (int position = 0; position < 256; ++position) check(position, 0, position == 0);
  ASSERT(cube_candidate_load(CUBE_COLOR_ACTIVE, 0));
  for (int color = 0; color < 256; ++color) check(color, 0, (color & 0x80) != 0);
  ASSERT(cube_candidate_load(CUBE_TILE_MATCHES, 0));
  for (int tile = 0; tile < 256; ++tile) for (int color = 0; color < 256; ++color)
    check(tile, color, (uint8_t)(tile - 0x2c) == (color & 0x7f));
}

static void full_permission(void) {
  const uint8_t positions[] = {0, 1, 0xff};
  for (size_t p = 0; p < sizeof positions; ++p) {
    ASSERT(cube_candidate_load(CUBE_ALLOWS, positions[p]));
    for (int color = 0; color < 256; ++color) for (int tile = 0; tile < 256; ++tile) {
      bool expected = !positions[p] || ((color & 0x80) && (uint8_t)(tile - 0x2c) == (color & 0x7f));
      check(color, tile, expected);
    }
    ASSERT_EQ(gameplay_calls(0), 65536);
  }
  // All other nonzero positions share the same presence semantics.
  for (int position = 0; position < 256; ++position) {
    ASSERT(cube_candidate_load(CUBE_ALLOWS, position));
    check(0x80, 0x2c, true); // active red cube and matching block
    check(0x81, 0x2c, !position);
    check(0, 0x2c, !position);
    check(0xff, 0xab, true); // largest accepted masked color
    check(0xff, 0x2b, !position); // wrapped subtraction must not become -1
  }
  ASSERT(gameplay_memory() < 256*1024);
}

int main(void) {
  printf("Candidate SHA256: %s\n", cube_candidate_revision());
  RUN(staged_boundaries);
  RUN(full_permission);
  ASSERT_EQ(checked, 263936);
  gameplay_close();
  printf("ok lua_cube_candidate: %llu test-only decisions; no game integration\n", (unsigned long long)checked);
  return 0;
}
