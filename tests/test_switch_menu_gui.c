#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
// Optional offscreen SDL2 check of the same GUI compiled into the NRO.
#include "platform/switch_menu.h"
#include "gui/widgets.h"
#ifdef ORACLES_SWITCH_MODS
#include "platform/switch_mods.h"
#include "game/features.h"
#include <sys/stat.h>
Features features;
static char fixture[512];
static void mods_fixture(void) {
  char tmp[]="/tmp/oracles-mods-gui-XXXXXX"; char *root=mkdtemp(tmp); assert(root);
  snprintf(fixture,sizeof fixture,"%s/",root);
  char path[1024]; snprintf(path,sizeof path,"%smods",fixture); assert(!mkdir(path,0700));
  for(int i=0;i<12;++i) {
    char id[64]; snprintf(id,sizeof id,"example-%02d%s",i,i==0 ? "-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa" : "");
    snprintf(path,sizeof path,"%smods/%s",fixture,id); assert(!mkdir(path,0700));
    snprintf(path,sizeof path,"%smods/%s/mod.ini",fixture,id); FILE *f=fopen(path,"wb"); assert(f);
    fprintf(f,"id=%s\nname=Example package %d\napi=1\ngames=ages\n",id,i); assert(!fclose(f));
    snprintf(path,sizeof path,"%smods/%s/main.lua",fixture,id); f=fopen(path,"wb"); assert(f);
    fputs(i==0 ? "error('A deliberately invalid mod: inspect this message in the list')" : "oracles.setting('Show overlay',true)",f); assert(!fclose(f));
  }
  assert(switch_mods_start(fixture,NULL,0));
}
#endif
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void capture(SDL_Renderer *renderer, SwitchMenu *menu, int w, int h, const char *dir, const char *name) {
  for (int i=0;i<4;++i) switch_menu_draw(menu,w,h);
  SDL_Surface *surface=SDL_CreateSurface(w,h,SDL_PIXELFORMAT_RGBA32); assert(surface);
  assert(SDL_RenderReadPixels(renderer,NULL,surface->format->format,surface->pixels,surface->pitch)==0);
  char path[1024]; snprintf(path,sizeof path,"%s/%s.bmp",dir,name);
  assert(SDL_SaveBMP(surface,path)==0); SDL_DestroySurface(surface);
  SDL_RenderPresent(renderer);
}
int main(int argc,char **argv) {
  assert(argc==2);
  SDL_SetHint(SDL_HINT_VIDEODRIVER,"dummy"); assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)==0);
  SDL_Window *window=SDL_CreateWindow("Switch menu preview",0,0,1280,720,0); assert(window);
  SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE); assert(renderer);
  SDL_SetRenderDrawColor(renderer,0,0,0,255); SDL_RenderClear(renderer);
  SDL_SetRenderDrawColor(renderer,255,255,255,255);
  SDL_FRect empty={5,5,10,0}; SDL_RenderFillRect(renderer,&empty);
  empty=(SDL_FRect){5,5,0,10}; SDL_RenderFillRect(renderer,&empty);
  SDL_Rect point={5,5,1,1}; Uint32 pixel=0;
  assert(SDL_RenderReadPixels(renderer,&point,SDL_PIXELFORMAT_ABGR8888,&pixel,4)==0);
  assert(pixel==0xff000000u); // no phantom borders from zero-sized Clay sides
  assert(gui_init(renderer));
  SwitchMenu *menu=calloc(1,sizeof *menu); assert(menu);
  menu->volume=10; menu->integer_scale=true; switch_menu_open(menu,true);
  menu->states[0].used=menu->states[1].used=true;
  menu->states[0].time=menu->states[1].time=now_time();
  for (int y=0;y<UI_H;++y) for (int x=0;x<UI_W;++x) {
    menu->states[0].thumb[y][x][0]=(x/8%2)*70;
    menu->states[0].thumb[y][x][1]=100+y;
    menu->states[0].thumb[y][x][2]=(y/8%2)*50;
  }
  memcpy(menu->states[1].thumb,menu->states[0].thumb,sizeof menu->states[1].thumb);
  capture(renderer,menu,1280,720,argv[1],"home-720p");
  Clay_ElementData tab=Clay_GetElementData(CLAY_IDI("SwitchTab",0));
  Clay_ElementData resume=Clay_GetElementData(CLAY_IDI("SwitchRow",0));
  assert(tab.found && resume.found && resume.boundingBox.y>tab.boundingBox.y+tab.boundingBox.height);
  menu->row=6; capture(renderer,menu,1280,720,argv[1],"home-scrolled-720p");
  Clay_ElementData list=Clay_GetElementData(CLAY_ID("SwitchList"));
  Clay_ElementData title=Clay_GetElementData(CLAY_IDI("SwitchRow",6));
  assert(list.found && title.found);
  assert(title.boundingBox.y>=list.boundingBox.y-1);
  assert(title.boundingBox.y+title.boundingBox.height<=list.boundingBox.y+list.boundingBox.height+1);
  menu->tab=2; capture(renderer,menu,1280,720,argv[1],"settings-720p");
#ifdef ORACLES_SWITCH_MODS
  mods_fixture(); menu->mods_page=true; menu->mod_count=mods_count()+mods_setting_count();
  menu->mod_row=0; capture(renderer,menu,1280,720,argv[1],"mods-720p");
  menu->mod_row=menu->mod_count-1; capture(renderer,menu,1280,720,argv[1],"mods-scrolled-720p");
  Clay_ElementData mr=Clay_GetElementData(CLAY_IDI("SwitchRow",menu->mod_row));
  list=Clay_GetElementData(CLAY_ID("SwitchList")); assert(mr.found && list.found);
  assert(mr.boundingBox.y>=list.boundingBox.y-1);
  assert(mr.boundingBox.y+mr.boundingBox.height<=list.boundingBox.y+list.boundingBox.height+1);
  menu->mods_page=false;
#endif
  menu->tab=1; capture(renderer,menu,1280,720,argv[1],"seasons-720p");
  menu->tab=0; menu->files_page=true; menu->files[0].valid=true; menu->files[0].max_health=12;
  strcpy(menu->files[0].name,"LINK"); capture(renderer,menu,1280,720,argv[1],"files-720p");
  menu->files_page=false; menu->row=0; SDL_SetWindowSize(window,1920,1080);
  capture(renderer,menu,1920,1080,argv[1],"home-1080p");
#ifdef ORACLES_SWITCH_MODS
  menu->mods_page=true; menu->mod_row=0; capture(renderer,menu,1920,1080,argv[1],"mods-1080p");
  switch_mods_stop(); char cmd[1024]; snprintf(cmd,sizeof cmd,"rm -rf '%s'",fixture); assert(!system(cmd));
#endif
  free(menu); SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); SDL_Quit();
  puts("ok switch_menu_gui: shared SDL2 renderer, 720p/1080p, tabs, file list, selected-row scrolling");
  return 0;
}
