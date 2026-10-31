#include "unit.h"
#include "core/gb.h"
#include "core/bus.h"
#include "assets/assets.h"
#include "platform/setup.h"
#include "game/game.h"
#include "game/gen.h"
#include "script/gameplay.h"
int main(void) {
  size_t size; uint8_t *rom=oracles_read_file(GAME_ROM_DIR "/Legend of Zelda, The - Oracle of Ages (USA, Australia).gbc",&size);
  if (!rom) SKIP("Ages ROM missing");
  GB *g=calloc(1,sizeof *g), *seed=malloc(sizeof *seed), *expected=malloc(sizeof *expected);
  ASSERT(g && seed && expected); gb_init(g); ASSERT(gb_load_rom(g,rom,size));
  g->cyctab=cyctab_alloc(rom,size); g->code_bits=assets_code_bits(rom,size); assets_zero_code(rom,size); gb_reset(g);
  g->ime=false; g->ie=0; g->d=0xd0; g->sp=0xdff0; g->rom_bank=SYMBANK(interactionCode78); g->pc=SYM(interactionCode78); *seed=*g;
  GBSample *samples=malloc(16*sizeof *samples); ASSERT(samples);
  ASSERT(gameplay_init());
  for (int state=0;state<=1;++state) for (int mask=0;mask<16;++mask) for (int sw=0;sw<16;++sw) {
    for (int mode=0;mode<2;++mode) {
      *g=*seed; memset(g->samples,0,16*sizeof *g->samples);
      mem_wr(g,0xd040+OBJ_STATE,state); mem_wr(g,0xd040+OBJ_VAR03,3);
      mem_wr(g,0xd040+OBJ_SUBID,mask); mem_wr(g,0xd040+OBJ_XH,0); mem_wr(g,0xd040+OBJ_YH,0x44);
      mem_wr(g,wSwitchState,sw);
      gameplay_enable(mode!=0);
      hook_run(g,interactionCode78_hook,SYM(interactionCode78)); ASSERT(!g->hung);
      if (!mode) { *expected=*g; memcpy(samples,g->samples,16*sizeof *samples); }
      else { ASSERT(!memcmp(expected,g,sizeof *g)); ASSERT(!memcmp(samples,g->samples,16*sizeof *samples)); } // includes clocks, flags, APU, RAM, framebuffer
    }
  }
  ASSERT(gameplay_calls(0)>0 && gameplay_calls(1)>0);
  printf("ok lua_gameplay_rom: 512 real interaction cases, full GB equality, Lua calls %llu/%llu\n",
    (unsigned long long)gameplay_calls(0),(unsigned long long)gameplay_calls(1));
  gameplay_close(); free(samples); free(seed); free(expected); return 0;
}
