#include "unit.h"
#include "platform/switch_session.h"
#include "platform/switch_storage.h"
#include "platform/switch_menu_boot.h"
#include "platform/setup.h"
#include "game/ram.h"
#ifdef ORACLES_LUA_GAMEPLAY
#include "script/gameplay.h"
#endif
#include <unistd.h>
static void advance(GB *g) {
  for(int i=0;i<360;++i) { gb_run_frame(g); ASSERT(!g->hung); g->apu.ring_r=g->apu.ring_w; }
}
int main(void) {
  const char *source=GAME_ROM_DIR "/Legend of Zelda, The - Oracle of Ages (USA, Australia).gbc";
  if(access(source,R_OK)) SKIP("Ages ROM missing for session-replacement regression");
  char root[]="/tmp/oracles-session-rom-XXXXXX"; ASSERT(mkdtemp(root)); char data[512],path[512];
  snprintf(data,sizeof data,"%s/",root); snprintf(path,sizeof path,"%sages.gbc",data); ASSERT_EQ(symlink(source,path),0);
  SwitchPreparedGame next={0}; GB *game=NULL; uint8_t *rom=NULL; SwitchGamePaths paths;
  ASSERT(switch_session_prepare(data,SWITCH_GAME_AGES,true,&next));
  switch_session_commit(&game,&rom,&paths,&next); advance(game);
  GB *old=game; uint8_t *oldrom=rom; uint64_t cycles=game->cycles; void *fib=game->fib;
  ASSERT(!switch_session_prepare(data,SWITCH_GAME_SEASONS,true,&next));
  ASSERT(game==old && rom==oldrom); ASSERT_EQ(game->cycles,cycles); ASSERT(game->fib==fib);
  ASSERT(switch_storage_write(paths.sram,game->eram,game->eram_size));
  ASSERT(switch_session_prepare(data,SWITCH_GAME_AGES,true,&next));
  ASSERT(!memcmp(game->eram,next.gb->eram,game->eram_size));
  switch_session_commit(&game,&rom,&paths,&next); ASSERT(game->fib==fib); advance(game);
  const char *season_source=GAME_ROM_DIR "/Legend of Zelda, The - Oracle of Seasons (USA, Australia).gbc";
  if(access(season_source,R_OK)) SKIP("Seasons ROM missing for both-direction switching");
  snprintf(path,sizeof path,"%sseasons.gbc",data); ASSERT_EQ(symlink(season_source,path),0);
  for(int round=0;round<4;++round) {
    ASSERT(switch_storage_write(paths.sram,game->eram,game->eram_size));
    SwitchGameId id=round%2==0 ? SWITCH_GAME_SEASONS : SWITCH_GAME_AGES;
    ASSERT(switch_session_prepare(data,id,true,&next));
    switch_session_commit(&game,&rom,&paths,&next); ASSERT(game->fib==fib);
    ASSERT_EQ(game_seasons,id==SWITCH_GAME_SEASONS);
#ifdef ORACLES_LUA_GAMEPLAY
    ASSERT_EQ(gameplay_is_enabled(),id==SWITCH_GAME_AGES);
#endif
    advance(game); ASSERT(!game->hung);
  }
  SwitchMenuBoot boot={.file=1,.phase=1};
  game->wram[0][wFileSelect_mode&0xfff]=8;
  ASSERT_EQ(switch_menu_boot_input(game,&boot,JOY_A),JOY_A); ASSERT_EQ(boot.file,1);
  SwitchPreparedGame last={.gb=game,.rom=rom}; switch_session_discard(&last);
  char cmd[640]; snprintf(cmd,sizeof cmd,"rm -rf '%s'",root); ASSERT_EQ(system(cmd),0);
  puts("ok live native session replacement, old session preserved on missing Seasons ROM, battery round-trip, reused fibers, EU language input"); return 0;
}
