#include "platform/switch_game.h"
#include "core/sha1.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

const char switch_games_revision[]="switch-games-v1:ages,seasons-us,seasons-eu";
const char *switch_game_name(SwitchGameId id) {
  return id==SWITCH_GAME_SEASONS ? "Oracle of Seasons" : "Oracle of Ages";
}
bool switch_game_identify(SwitchGameId id,const char *sha,size_t size,bool *eu) {
  if(!sha || !eu || size!=1048576) return false;
  if(id==SWITCH_GAME_AGES && !strcmp(sha,"880374fb978b18af4aa529e2e32f7ffb4d7dd2f4")) { *eu=false; return true; }
  if(id==SWITCH_GAME_SEASONS) {
    if(!strcmp(sha,"ba1268290fb2b1b70505d2d7b5825fc8a4816a4b")) { *eu=false; return true; }
    if(!strcmp(sha,"360b9408e569ba826212eac1ebea1b09573cb48c")) { *eu=true; return true; }
  }
  return false;
}
static bool path(char *out,const char *root,const char *suffix) {
  int n=snprintf(out,512,"%s%s",root,suffix);
  if(n<0 || n>=512) { errno=ENAMETOOLONG; return false; } return true;
}
bool switch_game_paths(const char *root,SwitchGameId id,bool eu,SwitchGamePaths *out) {
  if(!root || !*root || root[strlen(root)-1]!='/' || !out ||
     (id!=SWITCH_GAME_AGES && id!=SWITCH_GAME_SEASONS) || (id==SWITCH_GAME_AGES && eu)) { errno=EINVAL; return false; }
  SwitchGamePaths p={.id=id,.european=eu};
  bool seasons=id==SWITCH_GAME_SEASONS;
  if(!path(p.data,root,seasons ? (eu ? "seasons-eu/" : "seasons/") : "") ||
     !path(p.rom,root,seasons ? "seasons.gbc" : "ages.gbc") ||
     !path(p.sram,p.data,"sram.sav") || !path(p.log,p.data,"runtime.log") ||
     !path(p.observed,p.data,"replay-observed.txt") || !path(p.reload,p.data,"replay-reload.state") ||
     !path(p.result,p.data,"replay-result.txt")) return false;
  const char *slots[]={"quick.state","slot2.state","slot3.state","slot4.state"};
  for(int i=0;i<4;++i) if(!path(p.slots[i],p.data,slots[i])) return false;
  if(!path(p.boot,root,seasons ? "tests/seasons-consoleverified-boot.state" : "tests/ages-boot.state") ||
     !path(p.reference,root,seasons ? "tests/seasons.ref" : "tests/ages.ref") ||
     !path(p.inputs,root,seasons ? "tests/seasons-consoleverified.inputs" : "tests/ages-consoleverified.inputs")) return false;
  p.replay_frames=seasons ? 321712 : 289518; p.replay_checks=seasons ? 5361 : 4825;
  *out=p; return true;
}
uint8_t *switch_game_read_rom(const char *root,SwitchGameId id,SwitchGamePaths *out) {
  SwitchGamePaths p; if(!switch_game_paths(root,id,false,&p)) return NULL;
  struct stat st;
  if(stat(p.rom,&st)) return NULL;
  if(!S_ISREG(st.st_mode) || st.st_size!=1048576) { errno=EINVAL; return NULL; }
  FILE *f=fopen(p.rom,"rb"); if(!f) return NULL;
  uint8_t *rom=malloc(1048576); if(!rom) { fclose(f); return NULL; }
  bool ok=fread(rom,1,1048576,f)==1048576 && fgetc(f)==EOF && !ferror(f);
  if(fclose(f)) ok=false;
  char sha[41]; bool eu=false;
  if(ok) { sha1_hex(rom,1048576,sha); ok=switch_game_identify(id,sha,1048576,&eu); }
  if(!ok || !switch_game_paths(root,id,eu,&p)) { free(rom); errno=EINVAL; return NULL; }
  *out=p; return rom;
}
