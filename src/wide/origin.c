#include "wide/wide.h"
#include "wide/view.h"
static void room_position(const GBSample *s, int x, int y, int *rx, int *ry) {
  *rx = wrap_offset(s->line_scx[y] - rd(s, W_OFFSET_X)) + x;
  *ry = wrap_offset(s->line_scy[y] - rd(s, W_OFFSET_Y)) + y;
}
bool wide_play_origin(const GBSample *s, bool seasons, int *rx, int *ry) {
  const GameRam *r = seasons ? &seasons_ram : &ages_ram;
  if (!room_shown(s, r) || (s->line_lcdc[STATUS_BAR_LINES] & 0x81) != 0x81) return false;
  room_position(s, 0, STATUS_BAR_LINES, rx, ry);
  // the play area must show the room, give or take the screen shake: the scroll can also point
  // at a picture that is not the room (a cutscene panning over its art) while the mode says play
  int w = rd(s, W_ROOM_WIDTH) * 8, h = rd(s, W_ROOM_HEIGHT) * 8;
  return w >= FB_W && h >= FB_H - STATUS_BAR_LINES && w <= 256 && h <= 256 &&
         *rx >= -SHAKE && *rx <= w - FB_W + SHAKE && *ry >= -SHAKE && *ry <= h - (FB_H - STATUS_BAR_LINES) + SHAKE;
}
