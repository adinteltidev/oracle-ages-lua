#include "platform/switch_mods.h"
#include "platform/switch_storage.h"
#include "game/features.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>
#include <time.h>

typedef struct { char id[64], name[32]; int value; } Choice;
static struct {
  char config[512], report[512], disabled[MODS_MAX][64]; int disabled_count;
  Choice choices[MODS_MAX_SETTINGS]; int choice_count;
  int staged[MODS_MAX_SETTINGS]; bool staged_known[MODS_MAX_SETTINGS],pending;
  UiFont font; const uint8_t *rom; size_t size; bool seasons;
  unsigned long long frames, late, underruns, io, cpu[2001], modcpu[2001];
  double cpu_sum, mod_sum, io_seconds; size_t heap_peak;
} state;
static double now(void) {
  struct timespec t;
  return clock_gettime(CLOCK_MONOTONIC,&t)==0 ? (double)t.tv_sec+t.tv_nsec/1e9 : -1;
}
// FAT has no symlinks; also reject them when this adapter is exercised on POSIX.
static bool plain(const char *path) {
  struct stat st;
  if(lstat(path,&st)) return errno==ENOENT;
  if(!S_ISREG(st.st_mode)) { errno=EINVAL; return false; }
  return true;
}
static bool safe_paths(const char *path) {
  char side[1024];
  if(!plain(path)) return false;
  if(snprintf(side,sizeof side,"%s.tmp",path)>=(int)sizeof side || !plain(side)) return false;
  if(snprintf(side,sizeof side,"%s.bak",path)>=(int)sizeof side || !plain(side)) return false;
  return true;
}
static bool recover(const char *path) { return safe_paths(path) && switch_storage_recover(path); }
static bool write_file(const char *path,const void *data,size_t size) {
  double begin=now(); state.io++;
  bool ok=safe_paths(path) && switch_storage_write(path,data,size);
  double end=now(); if(begin>=0 && end>=begin) state.io_seconds+=end-begin;
  return ok;
}
static bool wanted_id(const char *id,void *unused) {
  (void)unused;
  for(int i=0;i<state.disabled_count;++i) if(!strcmp(id,state.disabled[i])) return false;
  return true;
}
bool switch_mods_wanted(int i) { return wanted_id(mods_id(i),NULL); }
static void encode(char *out,const char *s) {
  static const char hex[]="0123456789abcdef";
  while(*s) { unsigned char c=*s++; *out++=hex[c>>4]; *out++=hex[c&15]; } *out=0;
}
static bool decode(char *out,const char *s) {
  size_t n=strlen(s); if(!n || n%2 || n>62) return false;
  for(size_t i=0;i<n;i+=2) {
    unsigned a,b;
    if(sscanf(s+i,"%1x%1x",&a,&b)!=2 || !(a*16+b)) return false;
    *out++=(char)(a*16+b);
  } *out=0; return true;
}
static bool read_config(void) {
  if(!recover(state.config)) { fprintf(stderr,"mods: config recovery failed: %s\n",strerror(errno)); return false; }
  FILE *f=fopen(state.config,"rb"); if(!f) return errno==ENOENT;
  struct stat st;
  if(fstat(fileno(f),&st) || st.st_size>32768) { fclose(f); errno=EINVAL; return false; }
  char line[256],id[64],hex[64],number[32],extra;
  bool ok=true;
  while(fgets(line,sizeof line,f)) {
    if(line[0]=='#' || line[0]=='\n' || line[0]=='\r') continue;
    if(sscanf(line,"disabled %63s %c",id,&extra)==1 && mods_id_valid(id)) {
      if(!wanted_id(id,NULL)) continue;
      if(state.disabled_count==MODS_MAX) { ok=false; break; }
      snprintf(state.disabled[state.disabled_count++],64,"%s",id);
    } else if(sscanf(line,"setting %63s %63s %31s %c",id,hex,number,&extra)==3 && mods_id_valid(id)) {
      char *end; errno=0; long value=strtol(number,&end,10);
      Choice c={0};
      if(errno || *end || value<INT_MIN || value>INT_MAX || !decode(c.name,hex)) { ok=false; break; }
      c.value=(int)value; snprintf(c.id,sizeof c.id,"%s",id);
      int j=0;
      for(;j<state.choice_count;++j) if(!strcmp(state.choices[j].id,c.id) && !strcmp(state.choices[j].name,c.name)) break;
      if(j==MODS_MAX_SETTINGS) { ok=false; break; }
      state.choices[j]=c; if(j==state.choice_count) ++state.choice_count;
    } else { ok=false; break; }
  }
  ok=ok && !ferror(f); fclose(f);
  if(!ok) { errno=EINVAL; fprintf(stderr,"mods: invalid mods.cfg; no external Lua executed\n"); }
  return ok;
}
static bool save_config(void) {
  char text[32768]; size_t used=0;
  for(int i=0;i<state.disabled_count;++i)
    used+=(size_t)snprintf(text+used,sizeof text-used,"disabled %s\n",state.disabled[i]);
  for(int i=0;i<state.choice_count;++i) {
    Choice *c=&state.choices[i]; char hex[64]; encode(hex,c->name);
    int n=snprintf(text+used,sizeof text-used,"setting %s %s %d\n",c->id,hex,c->value);
    if(n<0 || (size_t)n>=sizeof text-used) { errno=EOVERFLOW; return false; } used+=(size_t)n;
  }
  return write_file(state.config,text,used);
}
bool switch_mods_toggle(int i) {
  if(i<0 || i>=mods_count()) return false;
  char previous[MODS_MAX][64]; memcpy(previous,state.disabled,sizeof previous);
  int count=state.disabled_count; const char *id=mods_id(i);
  if(wanted_id(id,NULL)) {
    if(count==MODS_MAX) return false;
    snprintf(state.disabled[state.disabled_count++],64,"%s",id);
  } else for(int j=0;j<state.disabled_count;++j) if(!strcmp(id,state.disabled[j])) {
    memmove(state.disabled[j],state.disabled[j+1],(size_t)(state.disabled_count-j-1)*64); --state.disabled_count; break;
  }
  if(!save_config()) { memcpy(state.disabled,previous,sizeof previous); state.disabled_count=count; return false; }
  state.pending=true; return true;
}
static int restore_setting(const char *id,const char *name,int initial) {
  for(int j=0;j<state.choice_count;++j) if(!strcmp(state.choices[j].id,id) && !strcmp(state.choices[j].name,name)) return state.choices[j].value;
  return initial;
}
int switch_mods_setting(int i) {
  if(i<0 || i>=mods_setting_count()) return 0;
  if(!state.staged_known[i]) {
    state.staged[i]=mods_setting_kind(i)==MOD_SETTING_BOOL ? mods_setting_value(i) : mods_setting_number(i);
    state.staged_known[i]=true;
  }
  return state.staged[i];
}
bool switch_mods_change_setting(int i,int delta) {
  if(i<0 || i>=mods_setting_count()) return false;
  int value=switch_mods_setting(i);
  if(mods_setting_kind(i)==MOD_SETTING_BOOL) value=!value;
  else {
    long long v=(long long)value+delta;
    value=v<mods_setting_min(i) ? mods_setting_min(i) : v>mods_setting_max(i) ? mods_setting_max(i) : (int)v;
  }
  int j=0; for(;j<state.choice_count;++j) if(!strcmp(state.choices[j].id,mods_setting_mod_id(i)) && !strcmp(state.choices[j].name,mods_setting_name(i))) break;
  if(j==MODS_MAX_SETTINGS) return false;
  Choice old=state.choices[j]; int old_count=state.choice_count;
  snprintf(state.choices[j].id,64,"%s",mods_setting_mod_id(i));
  snprintf(state.choices[j].name,32,"%s",mods_setting_name(i)); state.choices[j].value=value;
  if(j==state.choice_count) ++state.choice_count;
  if(!save_config()) { state.choices[j]=old; state.choice_count=old_count; return false; }
  state.staged[i]=value; state.pending=true; return true;
}
bool switch_mods_pending(void) { return state.pending; }
bool switch_mods_start(const char *data,const uint8_t *rom,size_t size) {
  switch_mods_stop(); memset(&state,0,sizeof state);
  char dir[512];
  if(snprintf(dir,sizeof dir,"%smods",data)>=(int)sizeof dir ||
     snprintf(state.config,sizeof state.config,"%smods.cfg",data)>=(int)sizeof state.config ||
     snprintf(state.report,sizeof state.report,"%smods-performance.json",data)>=(int)sizeof state.report) return false;
  if(mkdir(dir,0777) && errno!=EEXIST) return false;
  if(!read_config()) return false;
  state.rom=rom; state.size=size;
  state.seasons=rom && size>0x150 && !memcmp(rom+0x134,"ZELDA DIN",9);
  ui_font_load(&state.font,rom,size);
  mods_store_backend(recover,write_file);
  mods_settings_policy(restore_setting);
  mods_heap_limit(16*1024*1024); // Experimental aggregate ceiling; hardware tuning remains pending.
  bool ok=mods_init_filtered(dir,state.seasons,wanted_id,NULL);
  features.treasure_swap=mods_treasure_swap;
  fprintf(stderr,"mods: Switch host API %d, %d packages, directory=%s\n",MODS_API_VERSION,mods_count(),dir);
  return ok;
}
void switch_mods_stop(void) {
  features.treasure_swap=NULL; mods_shutdown(); mods_reset_picture(); mods_store_backend(NULL,NULL); mods_heap_limit(0); mods_settings_policy(NULL);
}
bool switch_mods_present(GB *game,uint32_t *pixels) {
  if(!game->sample || !mods_count()) return false;
  bool failed=false;
  bool enabled[MODS_MAX]; for(int i=0;i<mods_count();++i) enabled[i]=mods_enabled(i);
  ModsFrame f=mods_snapshot(game->sample,state.seasons,160,state.rom,state.size,&state.font);
  mods_frame(&f); mods_compose_abgr(pixels,160); mods_apply_writes(game);
  for(int i=0;i<mods_count();++i) if(enabled[i] && !mods_enabled(i)) {
    failed=true; fprintf(stderr,"mods: disabled %s: %s\n",mods_id(i),mods_error(i));
  }
  size_t heap=mods_heap_peak(); if(heap>state.heap_peak) state.heap_peak=heap;
  return failed;
}
static unsigned bucket(double s) { return s<0 ? 0 : s>=.020 ? 2000 : (unsigned)(s*100000); }
void switch_mods_measure(double cpu,double mods,bool late,bool underrun) {
  ++state.frames; state.late+=late; state.underruns+=underrun;
  state.cpu[bucket(cpu)]++; state.modcpu[bucket(mods)]++;
  state.cpu_sum+=cpu; state.mod_sum+=mods;
}
static double percentile(const unsigned long long *bins,int pc) {
  unsigned long long n=0,target=(state.frames*(unsigned)pc+99)/100;
  for(int i=0;i<=2000;++i) { n+=bins[i]; if(n>=target) return (i+1)*.01; } return 20;
}
void switch_mods_report(void) {
  if(!state.frames) return;
  char out[2048];
  int n=snprintf(out,sizeof out,
    "{\"frames\":%llu,\"packages\":%d,\"cpu_ms\":{\"mean\":%.6f,\"median\":%.3f,\"p95\":%.3f,\"p99\":%.3f},"
    "\"mods_ms\":{\"mean\":%.6f,\"median\":%.3f,\"p95\":%.3f,\"p99\":%.3f},"
    "\"handler_calls\":%llu,\"histogram_ms_resolution\":0.01,\"histogram_overflow_ms\":20,\"heap_peak\":%zu,\"late\":%llu,\"underruns\":%llu,\"io_writes\":%llu,\"io_ms\":%.6f}\n",
    state.frames,mods_count(),state.cpu_sum/state.frames*1000,percentile(state.cpu,50),percentile(state.cpu,95),percentile(state.cpu,99),
    state.mod_sum/state.frames*1000,percentile(state.modcpu,50),percentile(state.modcpu,95),percentile(state.modcpu,99),
    (unsigned long long)mods_handler_calls(),state.heap_peak,state.late,state.underruns,state.io,state.io_seconds*1000);
  if(n>0 && (size_t)n<sizeof out && !write_file(state.report,out,(size_t)n)) fprintf(stderr,"mods: performance report write failed\n");
}
