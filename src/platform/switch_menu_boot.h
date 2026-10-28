#pragma once
#include "core/gb.h"
typedef struct { int file; unsigned phase,ticks; } SwitchMenuBoot;
void switch_menu_restart(GB *gb, SwitchMenuBoot *boot, int file);
uint8_t switch_menu_boot_input(GB *gb, SwitchMenuBoot *boot, uint8_t live);
