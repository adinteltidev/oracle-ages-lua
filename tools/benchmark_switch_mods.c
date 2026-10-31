// Native host measurement of the actual Switch mod adapter plus engine.
// Audio/display/hardware cadence must still be checked on the Switch.
#include "unit.h"
#include "platform/switch_mods.h"
#include "platform/setup.h"
#include "assets/assets.h"
#include "rt/fibers.h"
#include "game/game.h"
#include <time.h>
static double now(void) { struct timespec t; ASSERT_EQ(clock_gettime(CLOCK_MONOTONIC,&t),0); return t.tv_sec+t.tv_nsec/1e9; }
static double cpu_now(void) { struct timespec t; ASSERT_EQ(clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&t),0); return t.tv_sec+t.tv_nsec/1e9; }
static int compare(const void *a,const void *b) { double x=*(const double*)a,y=*(const double*)b; return (x>y)-(x<y); }
static uint8_t input(void *unused,uint64_t frame) { (void)unused; (void)frame; return 0; }
int main(int argc,char **argv) {
  if(argc!=5) { fprintf(stderr,"Usage: benchmark-switch-mods ROM room.state DATA/ frames\n"); return 2; }
  size_t size; uint8_t *rom=oracles_read_file(argv[1],&size); ASSERT(rom);
  GB *g=calloc(1,sizeof *g); ASSERT(g); gb_init(g); ASSERT(gb_load_rom(g,rom,size));
  g->cyctab=cyctab_alloc(rom,size); g->code_bits=assets_code_bits(rom,size); ASSERT(g->cyctab && g->code_bits);
  assets_zero_code(rom,size); gb_reset(g); ASSERT(oracles_load_boot_state(g,argv[2]));
  g->input_at=input;
  ASSERT(switch_mods_start(argv[3],rom,size));
  for(int i=0;i<mods_count();++i) ASSERT(mods_enabled(i));
  static uint32_t pixels[160*144]; int frames=atoi(argv[4]); ASSERT(frames>0);
  double *cpu=calloc((size_t)frames,sizeof *cpu); ASSERT(cpu); double sum=0;
  for(int i=-120;i<frames;++i) {
    double process_begin=cpu_now(),begin=now(); gb_run_frame(g); ASSERT(!g->hung); g->apu.ring_r=g->apu.ring_w;
    const uint16_t *fb=g->sample ? g->sample->framebuffer : g->framebuffer;
    for(int j=0;j<160*144;++j) {
      unsigned c=fb[j]; pixels[j]=0xff000000u|(c&31)*255/31|((((c>>5)&31)*255/31)<<8)|((((c>>10)&31)*255/31)<<16);
    }
    double mod=now(); ASSERT(!switch_mods_present(g,pixels)); double end=now();
    if(i>=0) { cpu[i]=(cpu_now()-process_begin)*1000; sum+=cpu[i]; switch_mods_measure(end-begin,end-mod,false,false); }
  }
  qsort(cpu,(size_t)frames,sizeof *cpu,compare);
  char path[1024]; snprintf(path,sizeof path,"%smods-process-cpu.json",argv[3]); FILE *f=fopen(path,"wb"); ASSERT(f);
  fprintf(f,"{\"frames\":%d,\"mean_ms\":%.6f,\"median_ms\":%.6f,\"p95_ms\":%.6f,\"p99_ms\":%.6f}\n",frames,sum/frames,cpu[frames/2],cpu[(frames-1)*95/100],cpu[(frames-1)*99/100]); ASSERT_EQ(fclose(f),0); free(cpu);
  switch_mods_report(); switch_mods_stop(); free((void*)g->cyctab); free((void*)g->code_bits); free(g); free(rom); return 0;
}
