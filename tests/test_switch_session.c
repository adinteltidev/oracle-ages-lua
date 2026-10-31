#include "unit.h"
#include "platform/switch_session.h"
#include "rt/fibers.h"
#include <errno.h>
#include <unistd.h>
static GB *blank(void) { GB *g=calloc(1,sizeof *g); ASSERT(g); gb_init(g); g->cyctab=malloc(16); g->code_bits=malloc(16); return g; }
int main(void) {
  GB *live=blank(); uint8_t *rom=malloc(16); SwitchGamePaths paths={.id=SWITCH_GAME_AGES};
  live->eram[0]=11; fibers_reset(live); void *fib=live->fib;
  SwitchPreparedGame next={.gb=blank(),.rom=malloc(16),.paths={.id=SWITCH_GAME_SEASONS}};
  next.gb->eram[0]=27; switch_session_commit(&live,&rom,&paths,&next);
  ASSERT_EQ(paths.id,SWITCH_GAME_SEASONS); ASSERT_EQ(live->eram[0],27); ASSERT(live->fib==fib); ASSERT(!next.gb && !next.rom);
  char root[]="/tmp/oracles-session-XXXXXX"; ASSERT(mkdtemp(root)); char data[512]; snprintf(data,sizeof data,"%s/",root);
  GB *before=live; uint8_t *oldrom=rom;
  ASSERT(!switch_session_prepare(data,SWITCH_GAME_AGES,true,&next)); ASSERT_EQ(errno,ENOENT);
  ASSERT(live==before && rom==oldrom); ASSERT_EQ(live->eram[0],27); ASSERT_EQ(paths.id,SWITCH_GAME_SEASONS);
  ASSERT_EQ(rmdir(root),0);
  // The process owns the reused fiber pool; free all other session allocations.
  SwitchPreparedGame last={.gb=live,.rom=rom}; switch_session_discard(&last);
  puts("ok transactional game switch: candidate failure preserves the live game, SRAM stays separate, fiber storage reused"); return 0;
}
