#include "unit.h"
#include "core/gb.h"
#include "game/game.h"
#include "game/features.h"
#include "assets/assets.h"
#include "platform/setup.h"
#include "platform/tas.h"
#include "rt/fibers.h"
#include <unistd.h>
static uint8_t held;
static uint8_t live(void *ctx,uint64_t f) { (void)ctx;(void)f;return held; }
static uint8_t movie(void *ctx,uint64_t f) { return tas_input_at(ctx,f); }
static void advance(GB *gb,int n) {
  for(int i=0;i<n;++i) { gb_run_frame(gb); ASSERT(!gb->hung); gb->apu.ring_r=gb->apu.ring_w; }
}
static void gates(bool seasons) {
  GB *gb=calloc(1,sizeof *gb); ASSERT(gb); syms_select(seasons,false);
  gb->d=0xd0; W8(w1Link_state)=1; W8(wLinkObjectIndex)=0xd0;
  features.run_requested=false; ASSERT_EQ(features_link_speed(gb,0x28,0),0x28);
  features.run_requested=true; ASSERT_EQ(features_link_speed(gb,0x28,0),0x50);
  W8(wMenuDisabled)=1; ASSERT_EQ(features_link_speed(gb,0x28,0),0x50);
  W8(wMenuDisabled)=0;
  ASSERT_EQ(features_link_speed(gb,0x3c,0),0x50); // Pegasus doesn't stack
  W8(wActiveTileType)=5; ASSERT_EQ(features_link_speed(gb,0x1e,0),0x3c);
  W8(wActiveTileType)=6; ASSERT_EQ(features_link_speed(gb,0x14,0),0x28);
  W8(wActiveTileType)=0;
  ASSERT_EQ(features_link_speed(gb,0x28,0x88),0x28);
#define BLOCK(n,v) gb->wram[n##_BANK][(n)&0xfff]=v; ASSERT_EQ(features_link_speed(gb,0x28,0),0x28); gb->wram[n##_BANK][(n)&0xfff]=0
  BLOCK(wDisabledObjects,1); BLOCK(wTextIsActive,1); BLOCK(wOpenedMenuType,1); BLOCK(wLinkInAir,1);
  BLOCK(wLinkSwimmingState,1); BLOCK(wLinkImmobilized,1); BLOCK(w1Link_knockbackCounter,1);
  BLOCK(wTilesetFlags,0x20); BLOCK(wTilesetFlags,0x40); BLOCK(wScrollMode,2);
  W8(w1Link_state)=3; ASSERT_EQ(features_link_speed(gb,0x28,0),0x28);
  features.run_requested=false; free(gb);
}
static int coord(GB *gb,bool horizontal) {
  int lo=horizontal ? 0x0d : 0x0b;
  return gb->wram[1][lo]*256+gb->wram[1][lo-1];
}
int main(int argc,char **argv) {
  bool seasons=argc>1 && !strcmp(argv[1],"seasons"); gates(seasons);
  char path[1024],inputs[1024],boot[1024];
  snprintf(path,sizeof path,"%s/Legend of Zelda, The - Oracle of %s (USA, Australia).gbc",GAME_ROM_DIR,seasons ? "Seasons" : "Ages");
  snprintf(inputs,sizeof inputs,"%s/%s-consoleverified.inputs",TAS_DIR,seasons ? "seasons" : "ages");
  snprintf(boot,sizeof boot,"%s/%s",TAS_DIR,seasons ? "seasons-consoleverified-boot.state" : "ages-boot.state");
  size_t n; uint8_t *rom=oracles_read_file(path,&n); if(!rom) SKIP("original ROM missing");
  GB *gb=calloc(1,sizeof *gb); ASSERT(gb); gb_init(gb); ASSERT(gb_load_rom(gb,rom,n));
  gb->cyctab=cyctab_alloc(rom,n); gb->code_bits=assets_code_bits(rom,n); assets_zero_code(rom,n); gb_reset(gb);
  ASSERT(oracles_load_boot_state(gb,argc>2 ? argv[2] : boot));
  Tas tas={0};
  if(argc<=2) {
    ASSERT(tas_load(&tas,inputs)); gb->input_at=movie; gb->input_ctx=&tas; advance(gb,20000);
  }
  gb->input_at=live; held=0; advance(gb,60);
  for(int i=0;i<600 && !threads_parked(gb);++i) advance(gb,1);
  ASSERT(threads_parked(gb));
  char tmp[]="/tmp/oracles-run-XXXXXX"; int fd=mkstemp(tmp); ASSERT(fd>=0); close(fd);
  ASSERT(oracles_save_boot_state(gb,tmp));
  uint8_t buttons[]={JOY_RIGHT,JOY_LEFT,JOY_UP,JOY_DOWN}; bool matched=false;
  for(int dir=0;dir<4;++dir) {
    int delta[2]={0}; uint8_t normal_speed=0; bool same_room=true;
    for(int run=0;run<2;++run) {
      ASSERT(oracles_load_boot_state(gb,tmp)); fibers_reset(gb); gb->input_at=live;
      held=buttons[dir]; features.run_requested=run;
      int before=coord(gb,dir<2),room=W8(wActiveRoom),group=W8(wActiveGroup);
      advance(gb,6); delta[run]=abs(coord(gb,dir<2)-before);
      if(!run) normal_speed=W8(w1Link_speed);
      same_room &= room==W8(wActiveRoom) && group==W8(wActiveGroup);
      if(run && delta[0]>=3*256 && delta[1]==2*delta[0] && same_room) {
        int at=coord(gb,dir<2); features.run_requested=false; advance(gb,2);
        ASSERT_EQ(W8(w1Link_speed),normal_speed);
        printf("released: %0.2fpx in two frames; speed tier restored to %u\n",abs(coord(gb,dir<2)-at)/256.0,normal_speed); matched=true;
      }
    }
    if(matched) { printf("ok %s: six frames walk=%.2fpx run=%.2fpx; normal speed restored on release, original collision movement\n",seasons ? "Seasons" : "Ages",delta[0]/256.0,delta[1]/256.0); break; }
    printf("direction %d: walk=%d run=%d room=%d\n",dir,delta[0],delta[1],same_room);
  }
  features.run_requested=false; remove(tmp); ASSERT(matched); return 0;
}
