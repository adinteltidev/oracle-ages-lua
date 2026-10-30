#include "script/doors.h"
#include "script/items_shops.h"
// Headless full-engine frame benchmark. Correctness traces run separately from timing.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
// The game's register macros (B, H, ...) must not expand inside Windows headers.
#include <windows.h>
#endif
#include "core/gb.h"
#include "core/bus.h"
#include "core/hash.h"
#include "platform/setup.h"
#include "platform/tas.h"
#include "assets/assets.h"
#include "game/game.h"
#include "rt/fibers.h"
#ifdef ORACLES_LUA_GAMEPLAY
#include "script/gameplay.h"
#include "script/colored_cube.h"
#include "script/levers_lava.h"
#endif
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "benchmark: %s (%s:%d)\n", #c, __FILE__, __LINE__); exit(1); } } while (0)

static uint64_t wall_ns(void) {
#ifdef _WIN32
  LARGE_INTEGER t, f; QueryPerformanceCounter(&t); QueryPerformanceFrequency(&f);
  return (uint64_t)((long double)t.QuadPart * 1000000000.0L / f.QuadPart);
#else
  struct timespec t; CHECK(clock_gettime(CLOCK_MONOTONIC, &t) == 0);
  return (uint64_t)t.tv_sec * 1000000000u + t.tv_nsec;
#endif
}
static uint64_t cpu_ns(void) {
#ifdef _WIN32
  FILETIME created, exited, kernel, user;
  CHECK(GetThreadTimes(GetCurrentThread(), &created, &exited, &kernel, &user));
  ULARGE_INTEGER k = {.LowPart = kernel.dwLowDateTime, .HighPart = kernel.dwHighDateTime};
  ULARGE_INTEGER u = {.LowPart = user.dwLowDateTime, .HighPart = user.dwHighDateTime};
  return (k.QuadPart + u.QuadPart) * 100;
#else
  struct timespec t; CHECK(clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t) == 0);
  return (uint64_t)t.tv_sec * 1000000000u + t.tv_nsec;
#endif
}
typedef struct { Tas tas; bool live; } Input;
static uint8_t input_at(void *ctx, uint64_t frame) {
  Input *in = ctx; return in->live ? 0 : tas_input_at(&in->tas, frame);
}
static void tick(GB *g) { gb_run_frame(g); CHECK(!g->hung); g->apu.ring_r = g->apu.ring_w; }
static bool room_ready(const GB *g) {
  unsigned x = g->wram[1][0x0c] | g->wram[1][0x0d] << 8;
  unsigned y = g->wram[1][0x0a] | g->wram[1][0x0b] << 8;
  return g->wram[0][0xc2d] == 0 && g->wram[0][0xc30] == 0x8a &&
    g->wram[0][0x2ee] == 2 && g->wram[0][0x2ef] <= 1 && g->wram[1][4] == 1 &&
    !g->wram[0][0xba0] && !(g->wram[0][0xc8a] & 0x81) && !g->wram[0][0x4ab] &&
    !(g->wram[0][0xd00] & 0x0e) && !g->wram[0][0xc4f] &&
    y == 56 * 256 && x >= 70 * 256 && x <= 76 * 256;
}

// Hash the complete machine and all sample storage, excluding process-local addresses.
// Used only by --trace; timing runs never compute per-frame hashes.
static uint64_t digest(const GB *g) {
  GB normalized = *g;
  normalized.rom = NULL; normalized.boot = NULL;
  normalized.serial_out = NULL; normalized.serial_ctx = NULL;
  normalized.input_at = NULL; normalized.input_ctx = NULL;
  normalized.samples = NULL; normalized.sample = NULL;
  normalized.frame_cb = NULL; normalized.frame_ctx = NULL; normalized.step = NULL;
  normalized.ring = NULL; normalized.fib = NULL; normalized.cyctab = NULL; normalized.code_bits = NULL;
  uint64_t hash = fnv1a64_update(FNV1A64_INIT, (const unsigned char *)&normalized, sizeof normalized);
  return fnv1a64_update(hash, (const unsigned char *)g->samples, 16 * sizeof *g->samples);
}

typedef struct { uint16_t slots[8]; unsigned count; } Blocks;
// The engine may be parked with a different SVBK selected. Fixture edits target
// the interaction bank explicitly without changing the emulated bank register.
static uint8_t object_read(const GB *g, uint16_t addr) { return g->wram[1][addr & 0xfff]; }
static void object_write(GB *g, uint16_t addr, uint8_t value) { g->wram[1][addr & 0xfff] = value; }
static void select_slots(GB *g, Blocks *b, unsigned count) {
  for (uint16_t page = 0xd2; page < 0xe0 && b->count < count; ++page) {
    uint16_t base = (page << 8) | INTERACTION_BASE;
    if (!object_read(g, base)) b->slots[b->count++] = base;
  }
  CHECK(b->count == count);
}
static void stimulus(GB *g, const Blocks *b, unsigned frame) {
  unsigned phase = frame % 80;
  if (!phase) {
    for (unsigned i = 0; i < b->count; ++i) {
      uint16_t base = b->slots[i]; CHECK(!object_read(g, base));
      for (unsigned j = 0; j < 0x40; ++j) object_write(g, base + j, 0);
      object_write(g, base + OBJ_ID, 0x13);
      object_write(g, base + OBJ_XH, 0x28 + i * 0x10); object_write(g, base + OBJ_YH, 0x68);
      object_write(g, base, 1);
    }
  } else if (phase == 3) {
    mem_wr(g, wNumEnemies, 0);
  } else if (phase == 6) {
    for (unsigned i = 0; i < b->count; ++i) {
      uint16_t base = b->slots[i]; CHECK(object_read(g, base + OBJ_STATE) == 2);
      uint16_t tile = wRoomLayout + object_read(g, base + OBJ_VAR18);
      mem_wr(g, tile, object_read(g, base + OBJ_VAR19) ^ 1);
    }
  } else if (phase == 50) {
    for (unsigned i = 0; i < b->count; ++i) CHECK(!object_read(g, b->slots[i]));
  }
}
static int compare_double(const void *a, const void *b) {
  double x = *(const double *)a, y = *(const double *)b; return (x > y) - (x < y);
}
static void flame_stimulus(GB *g,const Blocks *b,unsigned frame) {
  if(frame%80==0) {
    for(unsigned i=0;i<b->count;++i) {
      uint16_t base=b->slots[i];
      for(unsigned j=0;j<0x40;++j) object_write(g,base+j,0);
      object_write(g,base,1); object_write(g,base+OBJ_ID,0x1a);
      object_write(g,base+OBJ_XH,0x28+i*0x10); object_write(g,base+OBJ_YH,0x68);
      object_write(g,base+OBJ_OAM_FLAGS,0xa8);
    }
    mem_wr(g,wRotatingCubePos,0x44);
  }
  static const uint8_t colors[]={0x80,0x81,0x82,0,1,2};
  mem_wr(g,wRotatingCubeColor,colors[(frame/10)%6]);
}
static void lava_stimulus(GB *g,const Blocks *b,unsigned frame) {
  unsigned phase=frame%480;
  if(!phase) {
    uint16_t base=b->slots[0];
    for(unsigned j=0;j<0x40;++j)object_write(g,base+j,0);
    object_write(g,base,1);object_write(g,base+OBJ_ID,0xd8);object_write(g,base+OBJ_SUBID,(frame/480)%6);
    for(unsigned j=0;j<0xc0;++j) {mem_wr(g,wRoomLayout+j,0xa0);mem_wr(g,wRoomCollisions+j,0);}
    for(unsigned j=0;j<3;++j)mem_wr(g,wRoomLayout+0x13+j,0xc3+j);
    mem_wr(g,wActiveMusic,3);
  }
  mem_wr(g,wLever1PullDistance,phase<220?0x80:0);
}
// A private, deterministic room fixture. Real native actors execute every frame.
static void doors_stimulus(GB *g,const Blocks *b,unsigned frame) {
  unsigned phase=frame%480;
  if(!phase) {
    for(unsigned j=0;j<0xc0;++j){mem_wr(g,wRoomLayout+j,0xa0);mem_wr(g,wRoomCollisions+j,0);}
    static const uint8_t positions[]={0x44,0x46,0x64,0x66,0x54,0x56,0x74,0x76};
    for(unsigned i=0;i<b->count;++i) {
      uint16_t base=b->slots[i];for(unsigned j=0;j<0x40;++j)object_write(g,base+j,0);
      unsigned kind=i%5;object_write(g,base,1);object_write(g,base+OBJ_ID,kind==4?0x1b:0x1e);
      if(kind==4){object_write(g,base+OBJ_SUBID,((frame/480)&1)?0x20:0);object_write(g,base+OBJ_XH,0x48);object_write(g,base+OBJ_YH,0x58);}
      else {object_write(g,base+OBJ_SUBID,(const uint8_t[]){4,8,0x14,12}[kind]);object_write(g,base+OBJ_XH,0);object_write(g,base+OBJ_YH,positions[i]);}
    }
    mem_wr(g,w1Link_xh,0x28);mem_wr(g,w1Link_yh,0x28);mem_wr(g,wActiveMusic,3);
  }
  mem_wr(g,wActiveTriggers,phase>=160 && phase<320?1:0);mem_wr(g,wSwitchState,phase>=160 && phase<320?1:0);
  mem_wr(g,wNumTorchesLit,phase>=80?2:0);mem_wr(g,wNumEnemies,phase<60?1:0);
}
// Repeated real stock initialization, purchases and collectible delivery.
// Fixture edits occur outside the timed engine frame, just like the other scenes.
static void items_stimulus(GB *g,const Blocks *b,unsigned frame) {
  unsigned phase=frame%120;
  if(!phase){
    mem_wr(g,w1Link_xh,0x28);mem_wr(g,w1Link_yh,0x28);mem_wr(g,wActiveMusic,3);
    mem_wr(g,wNumRupees,0x99);mem_wr(g,wNumRupees+1,9);mem_wr(g,wMaxBombs,0x30);mem_wr(g,wNumBombs,0);
    for(unsigned i=0;i<b->count;++i){uint16_t base=b->slots[i];
      for(unsigned j=0;j<0x40;++j)object_write(g,base+j,0);
      object_write(g,base,1);object_write(g,base+OBJ_ID,i?0x60:0x47);object_write(g,base+OBJ_SUBID,i?0x28:1);
      object_write(g,base+OBJ_XH,0x48+i*0x10);object_write(g,base+OBJ_YH,0x68);
    }
  }
  if(phase==20){object_write(g,b->slots[0]+OBJ_STATE,3);
    for(unsigned i=1;i<b->count;++i){object_write(g,b->slots[i]+OBJ_XH,0x28);object_write(g,b->slots[i]+OBJ_YH,0x28);object_write(g,b->slots[i]+OBJ_Z,0);object_write(g,b->slots[i]+OBJ_ZH,0);}
  }
  if(phase>=24)mem_wr(g,wTextIsActive,0);
}
static void distribution(FILE *f, const char *name, double *values, unsigned count) {
  double total = 0; for (unsigned i = 0; i < count; ++i) total += values[i];
  qsort(values, count, sizeof *values, compare_double);
  fprintf(f, "\"%s\":{\"mean\":%.9f,\"median\":%.9f,\"p95\":%.9f,\"p99\":%.9f,\"max\":%.9f}",
    name, total/count, (values[(count-1)/2] + values[count/2])/2,
    values[(count*95u-1)/100], values[(count*99u-1)/100], values[count-1]);
}
static unsigned number(const char *s) {
  char *end; unsigned long n = strtoul(s, &end, 10); CHECK(*s && !*end && n <= 1000000); return (unsigned)n;
}
int main(int argc, char **argv) {
  const char *prepare = NULL, *state = NULL, *trace_path = NULL, *output = NULL, *scenario = "idle";
  const char *reload_state = NULL;
  bool lua_enabled=true; (void)lua_enabled;
  unsigned frames = 2000, warmup = 160, block_count = 4, reload_every = 0;
  for (int i = 1; i < argc; i += 2) {
    CHECK(i + 1 < argc);
    if (!strcmp(argv[i], "--prepare")) prepare = argv[i+1];
    else if (!strcmp(argv[i], "--state")) state = argv[i+1];
    else if (!strcmp(argv[i], "--trace")) trace_path = argv[i+1];
    else if (!strcmp(argv[i], "--output")) output = argv[i+1];
    else if (!strcmp(argv[i], "--lua")) { CHECK(!strcmp(argv[i+1],"on") || !strcmp(argv[i+1],"off")); lua_enabled=!strcmp(argv[i+1],"on"); }
    else if (!strcmp(argv[i], "--scenario")) scenario = argv[i+1];
    else if (!strcmp(argv[i], "--frames")) frames = number(argv[i+1]);
    else if (!strcmp(argv[i], "--warmup")) warmup = number(argv[i+1]);
    else if (!strcmp(argv[i], "--blocks")) block_count = number(argv[i+1]);
    else if (!strcmp(argv[i], "--reload-every")) reload_every = number(argv[i+1]);
    else if (!strcmp(argv[i], "--reload-state")) reload_state = argv[i+1];
    else { fprintf(stderr, "unknown argument: %s\n", argv[i]); return 2; }
  }
  bool stress = !strcmp(scenario, "blocks");
  bool flame=!strcmp(scenario,"colored_cube");
  bool lava=!strcmp(scenario,"levers_lava");
  bool doors=!strcmp(scenario,"doors");
  bool items=!strcmp(scenario,"items_shops");
  CHECK(stress || flame || lava || doors || items || !strcmp(scenario, "idle"));
  CHECK(!lava || (block_count==1 && warmup%480==0)); CHECK(frames && block_count && block_count <= 8);
  CHECK(!reload_every || (trace_path && reload_state)); // Disk I/O never enters timing runs.
  // Start the measured stimulus at an identical cycle boundary after warm-up.
  CHECK(!(stress || flame) || (frames >= 80 && warmup % 80 == 0));
  size_t size; uint8_t *rom = oracles_read_file(GAME_ROM_DIR "/Legend of Zelda, The - Oracle of Ages (USA, Australia).gbc", &size);
  if (!rom) { fprintf(stderr, "Ages ROM missing\n"); return 77; }
  Input in = {0}; GB *g = calloc(1, sizeof *g); CHECK(g);
  gb_init(g); CHECK(gb_load_rom(g, rom, size));
  g->cyctab = cyctab_alloc(rom, size); g->code_bits = assets_code_bits(rom, size);
  assets_zero_code(rom, size); gb_reset(g);
  if (prepare) {
    CHECK(!state); CHECK(tas_load(&in.tas, TAS_DIR "/ages-consoleverified.inputs"));
    CHECK(oracles_load_boot_state(g, TAS_DIR "/ages-boot.state"));
    g->input_at = input_at; g->input_ctx = &in;
    unsigned setup;
    for (setup = 0; setup < 20000 && !room_ready(g); ++setup) tick(g);
    CHECK(room_ready(g)); in.live = true;
    for (unsigned i = 0; i < 10; ++i) tick(g);
    for (unsigned i = 0; i < 120 && !threads_parked(g); ++i) tick(g);
    CHECK(threads_parked(g)); CHECK(oracles_save_boot_state(g, prepare));
    fprintf(stderr, "Prepared private room state after %u frames\n", setup);
    return 0;
  }
  CHECK(state); CHECK(oracles_load_boot_state(g, state));
  in.live = true; g->input_at = input_at; g->input_ctx = &in;
  Blocks blocks = {0}; if (stress || flame || lava || doors || items) select_slots(g, &blocks, block_count);
#ifdef ORACLES_LUA_GAMEPLAY
  CHECK(gameplay_init()); gameplay_enable(lua_enabled);
  if(flame && lua_enabled) CHECK(gameplay_colored_cube_enabled());
  if(lava && lua_enabled) CHECK(gameplay_levers_lava_enabled());
  if(doors && lua_enabled) CHECK(gameplay_doors_enabled());
  if(items && lua_enabled) CHECK(gameplay_items_shops_enabled());
#ifndef ORACLES_LUA_BLOCK_PUZZLES
  CHECK(!stress); // An unintegrated module must not be reported as covered.
#endif
#endif
  for (unsigned i = 0; i < warmup; ++i) { if (stress) stimulus(g, &blocks, i); if(flame) flame_stimulus(g,&blocks,i); if(lava) lava_stimulus(g,&blocks,i); if(doors) doors_stimulus(g,&blocks,i); if(items) items_stimulus(g,&blocks,i); tick(g); }
#ifdef ORACLES_LUA_GAMEPLAY
  uint64_t before[4]; for (int i = 0; i < 4; ++i) before[i] = gameplay_block_calls(i);
  uint64_t cube_before[3]; for(int i=0;i<3;++i) cube_before[i]=gameplay_colored_cube_calls(i);
  uint64_t lava_before[GAMEPLAY_LAVA_RULE_COUNT];for(int i=0;i<GAMEPLAY_LAVA_RULE_COUNT;++i)lava_before[i]=gameplay_lava_calls(i);
  uint64_t doors_before[GAMEPLAY_DOOR_RULE_COUNT];for(int i=0;i<GAMEPLAY_DOOR_RULE_COUNT;++i)doors_before[i]=gameplay_door_calls(i);
  uint64_t item_before[GAMEPLAY_ITEMS_COUNT],shop_before[GAMEPLAY_SHOPS_COUNT];
  for(int i=0;i<GAMEPLAY_ITEMS_COUNT;++i)item_before[i]=gameplay_item_calls(i);
  for(int i=0;i<GAMEPLAY_SHOPS_COUNT;++i)shop_before[i]=gameplay_shop_calls(i);
  GameplayMemoryStats start = gameplay_memory_stats();
#endif
  FILE *trace = trace_path ? fopen(trace_path, "w") : NULL; CHECK(!trace_path || trace);
  double *cpu = calloc(frames, sizeof *cpu), *wall = calloc(frames, sizeof *wall); CHECK(cpu && wall);
  bool pending = false;
  unsigned reloads = 0, reloaded_block_states = 0;
  for (unsigned i = 0; i < frames; ++i) {
    if (reload_every && i && i % reload_every == 0) pending = true;
    if (pending && !g->sample_count && threads_parked(g)) {
      for (unsigned j = 0; j < blocks.count; ++j) {
        unsigned s = object_read(g, blocks.slots[j] + OBJ_STATE);
        if (object_read(g, blocks.slots[j]) && s < 4) reloaded_block_states |= 1u << s;
      }
      CHECK(oracles_save_boot_state(g, reload_state));
      CHECK(oracles_load_boot_state(g, reload_state)); fibers_reset(g);
      ++reloads; pending = false;
    }
    if (stress) stimulus(g, &blocks, i); if(flame) flame_stimulus(g,&blocks,i); if(lava) lava_stimulus(g,&blocks,i); if(doors) doors_stimulus(g,&blocks,i); if(items) items_stimulus(g,&blocks,i);
    if (trace) { tick(g); fprintf(trace, "%u %016llx\n", i, (unsigned long long)digest(g)); }
    else {
      uint64_t w = wall_ns(), c = cpu_ns(); tick(g);
      cpu[i] = (cpu_ns() - c)/1000000.0; wall[i] = (wall_ns() - w)/1000000.0;
    }
  }
  if (trace) CHECK(fclose(trace) == 0);
  CHECK(!reload_every || reloads > 0);
  uint64_t coverage[4] = {0}; size_t heap = 0, peak_heap = 0; uint64_t allocations = 0;
  const char *mode = "c", *revision = "none";
#ifdef ORACLES_LUA_GAMEPLAY
  mode = lua_enabled?"lua":"c"; revision = gameplay_revision();
  GameplayMemoryStats end = gameplay_memory_stats();
  heap = end.current; peak_heap = end.peak; allocations = end.allocations - start.allocations;
  for (int i = 0; i < 4; ++i) { coverage[i] = gameplay_block_calls(i) - before[i]; if (stress && lua_enabled) CHECK(coverage[i]); }
#endif
  FILE *f = output ? fopen(output, "w") : stdout; CHECK(f);
  fprintf(f, "{\"mode\":\"%s\",\"scenario\":\"%s\",\"frames\":%u,\"warmup\":%u,\"blocks\":%u,\"timed\":%s,\"reloads\":%u,\"reloaded_block_states_mask\":%u,",
    mode, scenario, frames, warmup, (stress || flame || lava || doors || items) ? block_count : 0, trace ? "false" : "true", reloads, reloaded_block_states);
  distribution(f, "cpu_ms", cpu, frames); fputc(',', f); distribution(f, "wall_ms", wall, frames);
  fprintf(f, ",\"coverage\":[%llu,%llu,%llu,%llu],\"heap_bytes\":%zu,\"peak_heap_bytes\":%zu,\"measured_allocations\":%llu,\"scripts_sha256\":\"%s\"",
    (unsigned long long)coverage[0], (unsigned long long)coverage[1], (unsigned long long)coverage[2], (unsigned long long)coverage[3],
    heap, peak_heap, (unsigned long long)allocations, revision);
  uint64_t colored[3]={0};
#ifdef ORACLES_LUA_GAMEPLAY
  for(int i=0;i<3;++i) { colored[i]=gameplay_colored_cube_calls(i)-cube_before[i]; if(flame && lua_enabled) CHECK(colored[i]); }
#endif
  fprintf(f,",\"colored_cube_coverage\":[%llu,%llu,%llu]",(unsigned long long)colored[0],(unsigned long long)colored[1],(unsigned long long)colored[2]);
  uint64_t lava_counts[9]={0};
#ifdef ORACLES_LUA_GAMEPLAY
  for(int i=0;i<GAMEPLAY_LAVA_RULE_COUNT;++i) {lava_counts[i]=gameplay_lava_calls(i)-lava_before[i];if(lava && lua_enabled)CHECK(lava_counts[i]);}
#endif
  fputs(",\"levers_lava_coverage\":[",f);
  for(int i=0;i<9;++i)fprintf(f,"%s%llu",i?",":"",(unsigned long long)lava_counts[i]);
  fputs("],\"doors_coverage\":[",f);
  uint64_t door_counts[GAMEPLAY_DOOR_RULE_COUNT]={0};
#ifdef ORACLES_LUA_GAMEPLAY
  uint64_t door_total=0;
  for(int i=0;i<GAMEPLAY_DOOR_RULE_COUNT;++i){door_counts[i]=gameplay_door_calls(i)-doors_before[i];door_total+=door_counts[i];}
  if(doors && lua_enabled)CHECK(door_total>0);
#endif
  for(int i=0;i<GAMEPLAY_DOOR_RULE_COUNT;++i)fprintf(f,"%s%llu",i?",":"",(unsigned long long)door_counts[i]);
  fputs("],\"items_shops_coverage\":[",f);
  uint64_t trade_counts[GAMEPLAY_ITEMS_COUNT+GAMEPLAY_SHOPS_COUNT]={0};
#ifdef ORACLES_LUA_GAMEPLAY
  uint64_t trade_total=0;
  for(int i=0;i<GAMEPLAY_ITEMS_COUNT;++i){trade_counts[i]=gameplay_item_calls(i)-item_before[i];trade_total+=trade_counts[i];}
  for(int i=0;i<GAMEPLAY_SHOPS_COUNT;++i){trade_counts[GAMEPLAY_ITEMS_COUNT+i]=gameplay_shop_calls(i)-shop_before[i];trade_total+=trade_counts[GAMEPLAY_ITEMS_COUNT+i];}
  if(items && lua_enabled)CHECK(trade_total>0);
#endif
  for(int i=0;i<GAMEPLAY_ITEMS_COUNT+GAMEPLAY_SHOPS_COUNT;++i)fprintf(f,"%s%llu",i?",":"",(unsigned long long)trade_counts[i]);
  fputs("]}\n",f);
  if (output) CHECK(fclose(f) == 0);
  return 0;
}
