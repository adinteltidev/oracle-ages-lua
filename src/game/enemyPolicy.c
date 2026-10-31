#include "game/game.h"
#include "script/enemies.h"
#ifdef ORACLES_LUA_GAMEPLAY
void gameplay_enemy_note(GB *gb){
  extern void gameplay_enemy_note_owner(uint8_t owner);
  uint8_t page=H8(hActiveObject);
  if(H8(hActiveObjectType)==ENEMY_BASE && page>=0xd0 && page<=0xdf)
    gameplay_enemy_note_owner(gb->wram[1][((page&15)<<8)+ENEMY_BASE+OBJ_ID]);
}
#endif
