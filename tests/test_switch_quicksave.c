#include "unit.h"
#include "core/gb.h"
#include "assets/assets.h"
#include "platform/setup.h"
#include "platform/switch_storage.h"
#include "platform/switch_menu_boot.h"
#include "platform/tas.h"
#include "rt/fibers.h"
#include "game/game.h"
#include <unistd.h>
static uint8_t input(void *ctx, uint64_t frame) { return tas_input_at(ctx, frame); }
static uint8_t live_input(void *ctx, uint64_t frame) { (void)frame; return *(uint8_t *)ctx; }
static void advance(GB *gb, unsigned frames) {
  for (unsigned i = 0; i < frames; ++i) { gb_run_frame(gb); ASSERT(!gb->hung); gb->apu.ring_r = gb->apu.ring_w; }
}
static void park(GB *gb) {
  for (unsigned i = 0; i < 600 && !threads_parked(gb); ++i) advance(gb, 1);
  ASSERT(threads_parked(gb));
}
static void save(GB *gb, const char *tmp, const char *path) {
  park(gb); ASSERT(oracles_save_boot_state(gb, tmp));
  ASSERT(switch_storage_sync(tmp)); ASSERT(switch_storage_commit(tmp, path));
}
int main(void) {
  size_t n;
#ifdef ORACLES_TEST_SEASONS_ONLY
  const char *rom_path=GAME_ROM_DIR "/Legend of Zelda, The - Oracle of Seasons (USA, Australia).gbc";
  const char *input_path=TAS_DIR "/seasons-consoleverified.inputs",*boot_path=TAS_DIR "/seasons-consoleverified-boot.state";
#else
  const char *rom_path=GAME_ROM_DIR "/Legend of Zelda, The - Oracle of Ages (USA, Australia).gbc";
  const char *input_path=TAS_DIR "/ages-consoleverified.inputs",*boot_path=TAS_DIR "/ages-boot.state";
#endif
  uint8_t *rom=oracles_read_file(rom_path,&n); if(!rom) SKIP("selected game ROM missing");
  Tas tas; ASSERT(tas_load(&tas,input_path));
  GB *gb = calloc(1, sizeof *gb); ASSERT(gb); gb_init(gb); ASSERT(gb_load_rom(gb, rom, n));
  gb->cyctab = cyctab_alloc(rom, n); gb->code_bits = assets_code_bits(rom, n); assets_zero_code(rom, n);
  gb_reset(gb); ASSERT(oracles_load_boot_state(gb,boot_path));
  gb->input_at = input; gb->input_ctx = &tas;
  advance(gb, 20000);
  char dir[] = "/tmp/oracles-switch-quicksave-XXXXXX"; ASSERT(mkdtemp(dir));
  char path[512], tmp[512], backup[512];
  snprintf(path, sizeof path, "%s/quick.state", dir); snprintf(tmp, sizeof tmp, "%s.tmp", path);
  snprintf(backup, sizeof backup, "%s.bak", path);
  save(gb, tmp, path); advance(gb, 12); save(gb, tmp, path);
  GB *saved = malloc(sizeof *saved); ASSERT(saved); *saved = *gb;
  advance(gb, 120); uint64_t continued_hash = gb_state_hash(gb);
  ASSERT(oracles_load_boot_state(gb, path)); fibers_reset(gb);
  // gb_state_hash reads the rendered sample ring, deliberately reset by load.
  // Verify saved machine memory/clock now and rendered trajectory after stepping.
  ASSERT(!memcmp(gb->wram, saved->wram, sizeof gb->wram));
  ASSERT(!memcmp(gb->eram, saved->eram, sizeof gb->eram));
  ASSERT(!memcmp(gb->vram, saved->vram, sizeof gb->vram));
  ASSERT(!memcmp(gb->hram, saved->hram, sizeof gb->hram));
  ASSERT_EQ(gb->pc, saved->pc); ASSERT_EQ(gb->sp, saved->sp);
  ASSERT_EQ(gb->cycles, saved->cycles); ASSERT_EQ(gb->mcycles, saved->mcycles);
  free(saved);
  advance(gb, 120); ASSERT_EQ(gb_state_hash(gb), continued_hash);
  // Restore the previous generation after an interrupted installation.
  ASSERT_EQ(remove(path), 0); ASSERT(switch_storage_recover(path));
  ASSERT(oracles_load_boot_state(gb, path)); fibers_reset(gb); advance(gb, 120);
  ASSERT_EQ(remove(path), 0);
  for (int slot=1;slot<=4;++slot) {
    snprintf(path,sizeof path,"%s/slot%d.state",dir,slot);
    snprintf(tmp,sizeof tmp,"%s.tmp",path);
    save(gb,tmp,path); advance(gb,60);
    uint64_t expected_hash=gb_state_hash(gb);
    ASSERT(oracles_load_boot_state(gb,path)); fibers_reset(gb);
    advance(gb,60); ASSERT_EQ(gb_state_hash(gb),expected_hash);
    ASSERT_EQ(remove(path),0);
  }
  ASSERT_EQ(rmdir(dir), 0);
  // Restart from a running game preserves battery saves and selects all three
  // original file entries without carrying a suspended fiber into reset.
  uint8_t eram[sizeof gb->eram]; memcpy(eram,gb->eram,sizeof eram);
  SwitchMenuBoot boot={.file=-1};
  uint8_t live=0; gb->input_at=live_input; gb->input_ctx=&live;
  for (int file=0;file<3;++file) {
    memcpy(eram,gb->eram,sizeof eram);
    switch_menu_restart(gb,&boot,file);
    ASSERT(gb->input_at==live_input && gb->input_ctx==&live);
    ASSERT(!gb->init_ram_loaded);
    ASSERT(!memcmp(eram,gb->eram,sizeof eram));
    while (boot.file>=0 && boot.ticks<=3600) {
      live=switch_menu_boot_input(gb,&boot,0);
      advance(gb,1);
    }
    ASSERT(boot.ticks<=3600); ASSERT(!gb->hung);
    ASSERT_EQ(gb->wram[0][wFileSelect_mode&0xfff],1);
    ASSERT_EQ(gb->wram[0][wFileSelect_mode2&0xfff],1);
    ASSERT_EQ(gb->wram[0][wFileSelect_cursorPos&0xfff],file);
  }
  memcpy(eram,gb->eram,sizeof eram);
  switch_menu_restart(gb,&boot,-1); ASSERT_EQ(boot.file,-1);
  ASSERT(!memcmp(eram,gb->eram,sizeof eram));
  printf("ok switch_quicksave: repeated GB save, state restored, identical 120-frame continuation, backup reload, four independent slots, file-screen restart\n");
  return 0;
}
