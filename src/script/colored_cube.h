#pragma once
#include "script/gameplay.h"

typedef enum {
  GAMEPLAY_COLORED_CUBE_PRESENT,
  GAMEPLAY_COLORED_CUBE_VISIBLE,
  GAMEPLAY_COLORED_CUBE_PALETTE,
  GAMEPLAY_COLORED_CUBE_RULE_COUNT
} GameplayColoredCubeRule;

bool gameplay_colored_cube_enabled(void);
bool gameplay_colored_cube_present(uint8_t position, bool *out);
bool gameplay_colored_cube_visible(uint8_t color, bool *out);
bool gameplay_colored_cube_palette(uint8_t color, uint8_t *out);
uint64_t gameplay_colored_cube_calls(GameplayColoredCubeRule rule);
const char *gameplay_colored_cube_rule_name(GameplayColoredCubeRule rule);
