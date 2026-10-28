#pragma once
// Only the Switch menu uses this adapter. Desktop builds continue using SDL3.
#include <SDL.h>
#include <time.h>
typedef Sint64 SDL_Time;
#ifndef SDL_PRINTF_VARARG_FUNC
#define SDL_PRINTF_VARARG_FUNC(n) __attribute__((format(printf, n, n+1)))
#endif
#define SDL_EVENT_MOUSE_MOTION SDL_MOUSEMOTION
#define SDL_EVENT_MOUSE_BUTTON_DOWN SDL_MOUSEBUTTONDOWN
#define SDL_EVENT_MOUSE_BUTTON_UP SDL_MOUSEBUTTONUP
#define SDL_EVENT_MOUSE_WHEEL SDL_MOUSEWHEEL
#define SDL_EVENT_FINGER_DOWN SDL_FINGERDOWN
#define SDL_EVENT_FINGER_MOTION SDL_FINGERMOTION
#define SDL_EVENT_FINGER_UP SDL_FINGERUP
#define SDL_EVENT_FINGER_CANCELED (SDL_USEREVENT + 37)
#define fingerID fingerId
#define SDL_SetRenderClipRect SDL_RenderSetClipRect
#define SDL_SCALEMODE_NEAREST SDL_ScaleModeNearest
#define SDL_GetCurrentRenderOutputSize SDL_GetRendererOutputSize
#define SDL_GetRenderWindow SDL_RenderGetWindow
#define SDL_DestroySurface SDL_FreeSurface
#define SDL_GetTicks SDL_GetTicks64
// SDL2's software renderer rasterizes a zero-sized float rectangle as a
// line; SDL3 treats it as empty. Clay emits these for absent border sides.
static inline int switch_sdl_fill_rect(SDL_Renderer *r, const SDL_FRect *rect) {
  if (rect && (rect->w <= 0 || rect->h <= 0)) return 0;
  return SDL_RenderFillRectF(r,rect);
}
#define SDL_RenderFillRect switch_sdl_fill_rect
static inline float SDL_GetWindowDisplayScale(SDL_Window *w) { (void)w; return 1; }
static inline int SDL_GetCurrentTime(SDL_Time *out) {
  struct timespec ts;
  if (clock_gettime(CLOCK_REALTIME, &ts)) return 0;
  *out = (Sint64)ts.tv_sec * 1000000000 + ts.tv_nsec; return 1;
}
static inline SDL_Surface *SDL_CreateSurface(int w, int h, Uint32 format) {
  return SDL_CreateRGBSurfaceWithFormat(0, w, h, SDL_BITSPERPIXEL(format), format);
}
static inline void SDL_RenderCoordinatesFromWindow(SDL_Renderer *r, float x, float y, float *rx, float *ry) {
  SDL_RenderWindowToLogical(r, (int)x, (int)y, rx, ry);
}
static inline int SDL_RenderTexture(SDL_Renderer *r, SDL_Texture *t, const SDL_FRect *src, const SDL_FRect *dst) {
  SDL_Rect s;
  if (src) s = (SDL_Rect){(int)src->x, (int)src->y, (int)src->w, (int)src->h};
  return SDL_RenderCopyF(r, t, src ? &s : NULL, dst);
}

#define SDL_NS_PER_SECOND 1000000000LL
typedef struct { int year, month, day, hour, minute, second, day_of_week; } SDL_DateTime;
static inline int SDL_TimeToDateTime(SDL_Time ns, SDL_DateTime *out, int local) {
  time_t sec = ns / SDL_NS_PER_SECOND;
  struct tm value;
  if (!(local ? localtime_r(&sec, &value) : gmtime_r(&sec, &value))) return 0;
  *out=(SDL_DateTime){value.tm_year+1900,value.tm_mon+1,value.tm_mday,value.tm_hour,value.tm_min,value.tm_sec,value.tm_wday};
  return 1;
}
