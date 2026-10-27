#include "unit.h"
#include "platform/keys.h"
#include "platform/tempfile.h"

int main(void) {
  ASSERT(!oracles_command_modifier(SDL_KMOD_NONE));
  ASSERT(!oracles_command_modifier(SDL_KMOD_ALT));
#ifdef __APPLE__
  ASSERT(oracles_command_modifier(SDL_KMOD_LGUI));
  ASSERT(oracles_command_modifier(SDL_KMOD_RGUI | SDL_KMOD_SHIFT));
  ASSERT(!oracles_command_modifier(SDL_KMOD_CTRL));
#else
  ASSERT(oracles_command_modifier(SDL_KMOD_LCTRL));
  ASSERT(oracles_command_modifier(SDL_KMOD_RCTRL | SDL_KMOD_SHIFT));
  ASSERT(!oracles_command_modifier(SDL_KMOD_GUI));
#endif
  SDL_KeyboardEvent key = {0}; key.scancode = SDL_SCANCODE_F11;
  ASSERT(oracles_fullscreen_key(&key));
  key.scancode = SDL_SCANCODE_F; key.mod = SDL_KMOD_GUI;
#ifdef __APPLE__
  ASSERT(oracles_fullscreen_key(&key));
#else
  ASSERT(!oracles_fullscreen_key(&key));
#endif
  key.mod = SDL_KMOD_CTRL; ASSERT(!oracles_fullscreen_key(&key));
  char first[4096], second[4096];
  ASSERT(oracles_temp_path(first, sizeof first)); ASSERT(oracles_temp_path(second, sizeof second));
  ASSERT(strcmp(first, second) != 0);
  FILE *f = fopen(first, "wb"); ASSERT(f); ASSERT(fputs("portable", f) >= 0); ASSERT(fclose(f) == 0);
  ASSERT(remove(first) == 0); ASSERT(remove(second) == 0);
  puts("ok platform modifiers, fullscreen and unique temporary files"); return 0;
}
