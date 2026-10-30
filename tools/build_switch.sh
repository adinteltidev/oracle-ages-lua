#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
: "${DEVKITPRO:?Set DEVKITPRO to your devkitPro installation}"
export DEVKITA64="${DEVKITA64:-$DEVKITPRO/devkitA64}"
export PATH="$DEVKITPRO/tools/bin:$DEVKITA64/bin:$PATH"
cmake -S "$root" -B "$root/build-switch" \
  -DCMAKE_TOOLCHAIN_FILE="$DEVKITPRO/cmake/Switch.cmake" \
  -DORACLES_SWITCH=ON -DCMAKE_BUILD_TYPE=Release -DORACLES_LUA_GAMEPLAY="${ORACLES_LUA_GAMEPLAY:-OFF}" -DORACLES_LUA_BLOCK_PUZZLES="${ORACLES_LUA_BLOCK_PUZZLES:-OFF}" -DORACLES_LUA_COLORED_CUBE="${ORACLES_LUA_COLORED_CUBE:-OFF}" -DORACLES_LUA_LEVERS_LAVA="${ORACLES_LUA_LEVERS_LAVA:-OFF}" -DORACLES_LUA_DOORS="${ORACLES_LUA_DOORS:-OFF}" \
  -DORACLES_SWITCH_BUILD="${ORACLES_SWITCH_BUILD:-$(git -C "$root" rev-parse --short HEAD)-local}"
cmake --build "$root/build-switch" --parallel "${JOBS:-3}"
