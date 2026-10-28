#pragma once
#include "platform/switch_menu.h"
#include "core/gb.h"
// Return a preview only for a complete state of the current format. No pointer
// from the serialized GB is dereferenced. Files are never modified here.
bool switch_menu_read_state(const char *path, UiSlot *out);
void switch_menu_preview(const GB *gb, UiSlot *out);
