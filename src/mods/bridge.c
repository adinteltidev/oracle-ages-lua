#include "mods/bridge.h"
#include "mods/picture.h"
#include "wide/wide.h"
ModsFrame mods_snapshot(const GBSample *s, bool seasons, int width,
                        const uint8_t *rom, size_t size, const UiFont *font) {
  ModsFrame f = {.seasons=seasons,.width=width,.rom=rom,.rom_size=size,.font=font};
  if (!s) return f;
  f.wram=s->wram[0]; f.wram3=s->wram[3]; f.wram1=s->wram[1]; f.hram=s->hram;
  f.room=s->wram[0][seasons ? 0xc4c : 0xc30];
  f.room_group=s->wram[0][seasons ? 0xc49 : 0xc2d];
  f.room_w=s->wram[0][0xd0a]; f.room_h=s->wram[0][0xd0b];
  f.bg_pal=mods_picture_bg_pal(s); f.oam=mods_picture_oam(s);
  f.scroll=true; mods_picture_scroll(s,&f.scroll_x,&f.scroll_y);
  f.origin=wide_play_origin(s,seasons,&f.origin_x,&f.origin_y);
  return f;
}
void mods_compose_abgr(uint32_t *pixels, int width) {
  const ModOverlay *o=mods_overlay();
  if (!pixels || width!=o->width || width<1 || width>MOD_OVERLAY_MAX_W) return;
  for (int y=0;y<MOD_OVERLAY_H;++y) for(int x=0;x<width;++x) if(o->cover[y][x]) {
    const uint8_t *c=o->rgb[y][x]; pixels[y*width+x]=0xff000000u|c[0]|(c[1]<<8)|(c[2]<<16);
  }
}
void mods_apply_writes(GB *gb) {
  ModWrite w[MODS_WRITES_MAX]; int n=mods_take_writes(w,MODS_WRITES_MAX);
  for(int i=0;i<n;++i) gb->wram[(w[i].addr>>12)&1][w[i].addr&0xfff]=w[i].value;
}
