#pragma once
#include "core/gb.h"
#include "platform/switch_game.h"
typedef struct { GB *gb; uint8_t *rom; SwitchGamePaths paths; } SwitchPreparedGame;
// No changes to the running game on preparation failure. SRAM is optional for replays.
bool switch_session_prepare(const char *root, SwitchGameId id, bool sram, SwitchPreparedGame *out);
// For prepared, unstarted games (no live fibers).
void switch_session_discard(SwitchPreparedGame *prepared);
// Call at the frontend frame boundary after saving the outgoing game and stopping its mods.
// Reuses the native fiber allocation; every old continuation is invalidated first.
void switch_session_commit(GB **game, uint8_t **rom, SwitchGamePaths *paths, SwitchPreparedGame *next);
