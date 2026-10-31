#pragma once
#include "core/gb.h"
#include "mods/mods.h"
ModsFrame mods_snapshot(const GBSample *sample, bool seasons, int width,
                        const uint8_t *rom, size_t size, const UiFont *font);
void mods_compose_abgr(uint32_t *pixels, int width);
void mods_apply_writes(GB *gb);
