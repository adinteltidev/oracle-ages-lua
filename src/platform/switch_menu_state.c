#include "platform/switch_menu_state.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
void switch_menu_preview(const GB *gb, UiSlot *out) {
  out->used = true; out->time = (int64_t)time(NULL) * 1000000000LL;
  for (int y=0;y<UI_H;++y) for (int x=0;x<UI_W;++x) {
    unsigned c=gb->framebuffer[y*UI_W+x];
    out->thumb[y][x][0]=(c&31)*255/31;
    out->thumb[y][x][1]=((c>>5)&31)*255/31;
    out->thumb[y][x][2]=((c>>10)&31)*255/31;
  }
}
bool switch_menu_read_state(const char *path, UiSlot *out) {
  struct { char magic[8]; uint32_t version,size; } header;
  memset(out,0,sizeof *out);
  FILE *f=fopen(path,"rb"); if (!f) return false;
  GB *saved=NULL; bool ok=false; struct stat st;
  if (fread(&header,1,sizeof header,f)!=sizeof header || memcmp(header.magic,"ORCLBOOT",8) ||
      header.version!=1 || header.size!=sizeof(GB)) goto done;
  saved=malloc(sizeof *saved); if (!saved) goto done;
  if (fread(saved,1,sizeof *saved,f)!=sizeof *saved || fgetc(f)!=EOF || ferror(f)) goto done;
  switch_menu_preview(saved,out);
  if (!fstat(fileno(f),&st)) out->time=(int64_t)st.st_mtime*1000000000LL;
  ok=true;
done:
  free(saved); fclose(f); return ok;
}
