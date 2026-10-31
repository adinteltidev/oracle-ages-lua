#include <unistd.h>
#include "unit.h"
#include "platform/switch_mods.h"
#include "game/features.h"
#include "wide/item.h"
#include <sys/stat.h>
Features features;
int main(void) {
#ifdef ORACLES_TEST_SEASONS_ONLY
  const bool seasons=true;
  const char *source=GAME_ROM_DIR "/Legend of Zelda, The - Oracle of Seasons (USA, Australia).gbc";
#else
  const bool seasons=false;
  const char *source=GAME_ROM_DIR "/Legend of Zelda, The - Oracle of Ages (USA, Australia).gbc";
#endif
  size_t size; uint8_t *rom=read_file(source,&size);
  if(!rom) SKIP("Selected game ROM missing");
  char tmp[]="/tmp/oracles-mods-rom-XXXXXX"; char *root=mkdtemp(tmp); ASSERT(root);
  char path[1024],data[512]; snprintf(data,sizeof data,"%s/",root);
  snprintf(path,sizeof path,"%smods",data); ASSERT_EQ(mkdir(path,0700),0);
  snprintf(path,sizeof path,"%smods/graphics",data); ASSERT_EQ(mkdir(path,0700),0);
  snprintf(path,sizeof path,"%smods/graphics/mod.ini",data); FILE *f=fopen(path,"wb"); ASSERT(f);
  fprintf(f,"id=graphics\napi=1\ngames=%s\n",seasons ? "seasons" : "ages"); ASSERT_EQ(fclose(f),0);
  snprintf(path,sizeof path,"%smods/graphics/main.lua",data); f=fopen(path,"wb"); ASSERT(f);
  fputs("oracles.on_frame(function() oracles.draw_text(2,2,'MODS',32767) oracles.draw_icon(3,64,24) end)",f); ASSERT_EQ(fclose(f),0);
  ASSERT(switch_mods_start(data,rom,size));
  static GB game; static GBSample sample; static uint32_t pixels[160*144];
  game.sample=&sample; memset(sample.wram[0]+0x600,1,256);
  ASSERT(!switch_mods_present(&game,pixels)); ASSERT(mods_enabled(0)); ASSERT_EQ(mods_current_frame()->seasons,seasons); ASSERT(mods_current_frame()->font->loaded);
  int ink=0; for(int y=2;y<16;++y) for(int x=2;x<32;++x) ink+=mods_overlay()->cover[y][x]!=0; ASSERT(ink>0);
  WideItemIcon icon; ASSERT(wide_item_icon(rom,size,seasons,3,sample.wram[0]+0x600,&icon)); ink=0;
  for(int y=0;y<16;++y) for(int x=0;x<16;++x) {
    ASSERT_EQ(!!mods_overlay()->cover[24+y][64+x],!!icon.ci[y][x]); ink+=!!icon.ci[y][x];
  }
  ASSERT(ink>0); switch_mods_stop(); free(rom);
  snprintf(path,sizeof path,"rm -rf '%s'",root); ASSERT_EQ(system(path),0);
  printf("ok Switch adapter: %s ROM font, text, item icon and logical composition\n",seasons ? "Seasons" : "Ages"); return 0;
}
