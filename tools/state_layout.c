// ABI evidence for the raw v1 boot-state format. Full replay/load tests remain required.
#include "core/gb.h"
#include <stddef.h>
#include <stdio.h>

int main(void) {
  printf("{\"GB\":%zu,\"Apu\":%zu,\"pointer\":%zu,\"rom\":%zu,\"wram\":%zu,"
         "\"hram\":%zu,\"vram\":%zu,\"io\":%zu,\"framebuffer\":%zu,"
         "\"apu\":%zu,\"cycles\":%zu,\"mcycles\":%zu}\n",
         sizeof(GB), sizeof(Apu), sizeof(void *), offsetof(GB, rom), offsetof(GB, wram),
         offsetof(GB, hram), offsetof(GB, vram), offsetof(GB, io), offsetof(GB, framebuffer),
         offsetof(GB, apu), offsetof(GB, cycles), offsetof(GB, mcycles));
  return 0;
}
