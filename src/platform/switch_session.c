#include "platform/switch_session.h"
#include "platform/switch_storage.h"
#include "platform/setup.h"
#include "assets/assets.h"
#include "rt/fibers.h"
#include "game/game.h"
#ifdef ORACLES_LUA_GAMEPLAY
#include "script/gameplay.h"
#endif
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
void switch_session_discard(SwitchPreparedGame *p) {
  if(p->gb) { free(p->gb->samples); free((void*)p->gb->cyctab); free((void*)p->gb->code_bits); free(p->gb); }
  free(p->rom); memset(p,0,sizeof *p);
}
bool switch_session_prepare(const char *root,SwitchGameId id,bool sram,SwitchPreparedGame *out) {
  SwitchPreparedGame p={0}; p.rom=switch_game_read_rom(root,id,&p.paths); if(!p.rom) return false;
  if(mkdir(p.paths.data,0777) && errno!=EEXIST) goto fail;
  struct stat st;
  if(stat(p.paths.data,&st) || !S_ISDIR(st.st_mode)) { errno=ENOTDIR; goto fail; }
  p.gb=calloc(1,sizeof *p.gb); if(!p.gb) goto fail;
  gb_init(p.gb); if(!p.gb->samples || !gb_load_rom(p.gb,p.rom,1048576)) goto fail;
  p.gb->cyctab=cyctab_alloc(p.rom,1048576); p.gb->code_bits=assets_code_bits(p.rom,1048576);
  if(!p.gb->cyctab || !p.gb->code_bits) goto fail;
  assets_zero_code(p.rom,1048576); gb_reset(p.gb);
  if(sram) {
    if(!switch_storage_recover(p.paths.sram)) goto fail;
    if(!stat(p.paths.sram,&st)) {
      size_t n=0; uint8_t *saved=oracles_read_file(p.paths.sram,&n);
      if(!saved || n!=p.gb->eram_size) { free(saved); errno=EINVAL; goto fail; }
      memcpy(p.gb->eram,saved,n); free(saved);
    } else if(errno!=ENOENT) goto fail;
  }
  *out=p; return true;
fail:;
  int error=errno ? errno : ENOMEM; switch_session_discard(&p); errno=error; return false;
}
void switch_session_commit(GB **game,uint8_t **rom,SwitchGamePaths *paths,SwitchPreparedGame *next) {
  if(*game) {
    fibers_reset(*game);
    next->gb->fib=(*game)->fib; (*game)->fib=NULL;
    next->gb->ring=(*game)->ring; (*game)->ring=NULL;
    SwitchPreparedGame old={.gb=*game,.rom=*rom}; switch_session_discard(&old);
  }
  *game=next->gb; *rom=next->rom; *paths=next->paths;
  syms_select(paths->id==SWITCH_GAME_SEASONS,paths->european);
#ifdef ORACLES_LUA_GAMEPLAY
  gameplay_enable(paths->id==SWITCH_GAME_AGES);
#endif
  memset(next,0,sizeof *next);
}
