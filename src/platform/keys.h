#pragma once
#include <SDL3/SDL.h>

static inline bool oracles_command_modifier(SDL_Keymod modifiers) {
#ifdef __APPLE__
  return (modifiers & SDL_KMOD_GUI) != 0;
#else
  return (modifiers & SDL_KMOD_CTRL) != 0;
#endif
}

static inline bool oracles_fullscreen_key(const SDL_KeyboardEvent *key) {
  if (key->scancode == SDL_SCANCODE_F11) return true;
#ifdef __APPLE__
  return key->scancode == SDL_SCANCODE_F && (key->mod & SDL_KMOD_GUI) != 0;
#else
  return false;
#endif
}
