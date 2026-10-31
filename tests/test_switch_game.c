#include "unit.h"
#include "platform/switch_game.h"
#include <errno.h>
#include <unistd.h>
int main(void) {
  bool eu=true;
  ASSERT(switch_game_identify(SWITCH_GAME_AGES,"880374fb978b18af4aa529e2e32f7ffb4d7dd2f4",1048576,&eu)); ASSERT(!eu);
  ASSERT(switch_game_identify(SWITCH_GAME_SEASONS,"ba1268290fb2b1b70505d2d7b5825fc8a4816a4b",1048576,&eu)); ASSERT(!eu);
  ASSERT(switch_game_identify(SWITCH_GAME_SEASONS,"360b9408e569ba826212eac1ebea1b09573cb48c",1048576,&eu)); ASSERT(eu);
  ASSERT(!switch_game_identify(SWITCH_GAME_AGES,"ba1268290fb2b1b70505d2d7b5825fc8a4816a4b",1048576,&eu));
  ASSERT(!switch_game_identify(SWITCH_GAME_SEASONS,"880374fb978b18af4aa529e2e32f7ffb4d7dd2f4",1048576,&eu));
  ASSERT(!switch_game_identify(SWITCH_GAME_SEASONS,"ba1268290fb2b1b70505d2d7b5825fc8a4816a4b",524288,&eu));
  SwitchGamePaths ages,s, europe;
  ASSERT(switch_game_paths("sdmc:/switch/oracle-ages-lua-phase7/",SWITCH_GAME_AGES,false,&ages));
  ASSERT(switch_game_paths("sdmc:/switch/oracle-ages-lua-phase7/",SWITCH_GAME_SEASONS,false,&s));
  ASSERT(switch_game_paths("sdmc:/switch/oracle-ages-lua-phase7/",SWITCH_GAME_SEASONS,true,&europe));
  ASSERT(!strcmp(ages.sram,"sdmc:/switch/oracle-ages-lua-phase7/sram.sav"));
  ASSERT(!strcmp(s.sram,"sdmc:/switch/oracle-ages-lua-phase7/seasons/sram.sav"));
  ASSERT(!strcmp(europe.sram,"sdmc:/switch/oracle-ages-lua-phase7/seasons-eu/sram.sav"));
  ASSERT(!strcmp(s.rom,europe.rom)); ASSERT(strstr(s.rom,"/seasons.gbc"));
  for(int i=0;i<4;++i) { ASSERT(strcmp(s.slots[i],ages.slots[i])); ASSERT(strcmp(s.slots[i],europe.slots[i])); }
  ASSERT(strstr(s.boot,"seasons-consoleverified-boot.state")); ASSERT_EQ(s.replay_checks,5361); ASSERT_EQ(s.replay_frames,321712);
  ASSERT(!switch_game_paths("missing-slash",SWITCH_GAME_AGES,false,&ages));
  char long_root[600]; memset(long_root,'a',598); long_root[598]='/'; long_root[599]=0;
  ASSERT(!switch_game_paths(long_root,SWITCH_GAME_SEASONS,false,&ages)); ASSERT_EQ(errno,ENAMETOOLONG);
  char root[]="/tmp/oracles-rom-check-XXXXXX"; ASSERT(mkdtemp(root)); char path[512],data[512];
  snprintf(data,sizeof data,"%s/",root); snprintf(path,sizeof path,"%s/seasons.gbc",root);
  ASSERT(!switch_game_read_rom(data,SWITCH_GAME_SEASONS,&ages)); ASSERT_EQ(errno,ENOENT);
  FILE *f=fopen(path,"wb"); ASSERT(f); fputs("bad rom",f); ASSERT_EQ(fclose(f),0);
  ASSERT(!switch_game_read_rom(data,SWITCH_GAME_SEASONS,&ages)); ASSERT_EQ(errno,EINVAL);
  f=fopen(path,"wb"); ASSERT(f); ASSERT_EQ(ftruncate(fileno(f),1048576),0); ASSERT_EQ(fclose(f),0);
  ASSERT(!switch_game_read_rom(data,SWITCH_GAME_SEASONS,&ages)); ASSERT_EQ(errno,EINVAL);
  ASSERT_EQ(remove(path),0); ASSERT_EQ(rmdir(root),0);
  puts("ok Switch game profiles: ROM whitelist, region selection, legacy Ages paths and isolated Seasons paths"); return 0;
}
