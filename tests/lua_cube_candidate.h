#pragma once
#include <stdbool.h>
#include <stdint.h>

// Test-only contracts. No game hook or production VM loads this module.
typedef enum {
  CUBE_NO_CUBE,       // a: position -> absent?
  CUBE_COLOR_ACTIVE,  // a: color -> bit 7 set?
  CUBE_TILE_MATCHES,  // a: tile, b: color -> matching wrapped index?
  CUBE_ALLOWS,       // bound position; a: color, b: tile -> complete permission?
} CubeCandidateRule;
bool cube_candidate_load(CubeCandidateRule rule, uint8_t position);
bool cube_candidate_query(uint8_t a, uint8_t b, bool *out);
const char *cube_candidate_revision(void);
