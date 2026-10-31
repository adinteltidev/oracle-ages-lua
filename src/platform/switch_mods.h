#pragma once
#include "mods/bridge.h"
// Persistent frontend choices; changes take effect only in the next play session.
bool switch_mods_start(const char *data, const uint8_t *rom, size_t size);
void switch_mods_stop(void);
bool switch_mods_present(GB *game, uint32_t *pixels);
bool switch_mods_wanted(int index);
bool switch_mods_toggle(int index);
int switch_mods_setting(int index);
bool switch_mods_change_setting(int index, int delta);
bool switch_mods_pending(void);
// Aggregate measurements are separate from the engine and save-state format.
void switch_mods_measure(double frame_seconds, double mods_seconds, bool late, bool underrun);
void switch_mods_report(void);
