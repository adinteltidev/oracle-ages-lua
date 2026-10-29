#!/bin/sh
# Optional native-host LLVM path; the devkitPro GCC path is tools/build_switch.sh.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
: "${DEVKITPRO:?Set DEVKITPRO to the target SDK root}"
: "${ORACLES_LLVM:?Set ORACLES_LLVM to a native LLVM bin directory}"
: "${ORACLES_SWITCH_TOOLS:?Set ORACLES_SWITCH_TOOLS to native elf2nro/nacptool directory}"
export PATH="$ORACLES_SWITCH_TOOLS:$PATH"
cmake -S "$root" -B "$root/build-switch-llvm" \
  -DCMAKE_TOOLCHAIN_FILE="$root/platform/switch/llvm.cmake" \
  -DORACLES_SWITCH=ON -DCMAKE_BUILD_TYPE=Release -DORACLES_LUA_GAMEPLAY="${ORACLES_LUA_GAMEPLAY:-OFF}" -DORACLES_LUA_BLOCK_PUZZLES="${ORACLES_LUA_BLOCK_PUZZLES:-OFF}" -DORACLES_VERSION="${ORACLES_VERSION:-}" \
  -DORACLES_SWITCH_BUILD="${ORACLES_SWITCH_BUILD:-$(git -C "$root" rev-parse --short HEAD)-llvm-local}"
cmake --build "$root/build-switch-llvm" --parallel "${JOBS:-3}"
