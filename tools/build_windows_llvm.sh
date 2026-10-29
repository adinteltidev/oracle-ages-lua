#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build=${1:-"$root/build-win-x64"}
: "${ORACLES_LLVM_MINGW:?Set ORACLES_LLVM_MINGW to the pinned LLVM-MinGW directory}"
cmake_bin=${ORACLES_CMAKE:-cmake}
set -- -S "$root" -B "$build" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$root/cmake/llvm-mingw-x86_64.cmake" \
  -DORACLES_LLVM_MINGW="$ORACLES_LLVM_MINGW" \
  -DCMAKE_BUILD_TYPE=Release -DORACLES_LUA_GAMEPLAY="${ORACLES_LUA_GAMEPLAY:-OFF}" -DORACLES_LUA_BLOCK_PUZZLES="${ORACLES_LUA_BLOCK_PUZZLES:-OFF}" -DORACLES_SDL=ON -DORACLES_SDL_VENDORED=ON \
  -DORACLES_VERSION="${ORACLES_VERSION:-}" \
  -DORACLES_RELEASES="${ORACLES_RELEASES:-adinteltidev/oracle-ages-native}"
if [ -n "${ORACLES_NINJA:-}" ]; then set -- "$@" -DCMAKE_MAKE_PROGRAM="$ORACLES_NINJA"; fi
if [ -n "${ORACLES_SDL_SOURCE:-}" ]; then set -- "$@" -DFETCHCONTENT_SOURCE_DIR_SDL3="$ORACLES_SDL_SOURCE"; fi
if [ -n "${ORACLES_WINDOWS_SOURCE_ROOT:-}" ]; then
  set -- "$@" -DGAME_ROM_DIR="$ORACLES_WINDOWS_SOURCE_ROOT/roms" -DORACLES_TAS_DIR="$ORACLES_WINDOWS_SOURCE_ROOT/tas"
fi
if [ -n "${ORACLES_WINE:-}" ]; then set -- "$@" -DCMAKE_CROSSCOMPILING_EMULATOR="$ORACLES_WINE"; fi
"$cmake_bin" "$@"
"$cmake_bin" --build "$build" --target oracles-native oracles-native-run \
  test_native_ages test_native_tas test_native_movement test_platform_keys test_ui --parallel "${ORACLES_JOBS:-2}"
python3 "$root/tools/check_native_binary.py" --ar "$ORACLES_LLVM_MINGW/bin/llvm-ar" \
  "$build/liboraclesnative.a" "$build/Oracles.exe" "$build/oracles-native-run.exe" "$build/test_native_ages.exe"
