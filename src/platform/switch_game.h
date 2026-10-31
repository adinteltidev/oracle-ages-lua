#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum { SWITCH_GAME_AGES, SWITCH_GAME_SEASONS } SwitchGameId;
typedef struct {
  SwitchGameId id;
  bool european;
  char data[512], rom[512], sram[512], slots[4][512], log[512];
  char boot[512], reference[512], inputs[512], observed[512], reload[512], result[512];
  uint64_t replay_frames;
  unsigned replay_checks;
} SwitchGamePaths;
extern const char switch_games_revision[];
const char *switch_game_name(SwitchGameId id);
bool switch_game_identify(SwitchGameId id, const char *sha1, size_t size, bool *european);
bool switch_game_paths(const char *root, SwitchGameId id, bool european, SwitchGamePaths *out);
// Validates the original file before any code bytes are stripped. Caller owns the result.
uint8_t *switch_game_read_rom(const char *root, SwitchGameId id, SwitchGamePaths *out);
