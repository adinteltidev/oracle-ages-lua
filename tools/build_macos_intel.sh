#!/bin/sh
set -eu
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_dir=${1:-"$source_dir/build-native-x64"}
mkdir -p "$build_dir"
build_dir=$(CDPATH= cd -- "$build_dir" && pwd)
set -- cmake -S "$source_dir" -B "$build_dir" -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DORACLES_LUA_GAMEPLAY="${ORACLES_LUA_GAMEPLAY:-OFF}" -DORACLES_LUA_BLOCK_PUZZLES="${ORACLES_LUA_BLOCK_PUZZLES:-OFF}" -DORACLES_LUA_COLORED_CUBE="${ORACLES_LUA_COLORED_CUBE:-OFF}" -DORACLES_LUA_LEVERS_LAVA="${ORACLES_LUA_LEVERS_LAVA:-OFF}" -DORACLES_LUA_DOORS="${ORACLES_LUA_DOORS:-OFF}" -DORACLES_LUA_ITEMS_SHOPS="${ORACLES_LUA_ITEMS_SHOPS:-OFF}" -DCMAKE_OSX_ARCHITECTURES=x86_64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 -DORACLES_SDL=ON -DORACLES_SDL_VENDORED=ON
if [ -n "${ORACLES_SDL_SOURCE_DIR:-}" ]; then
  set -- "$@" "-DFETCHCONTENT_SOURCE_DIR_SDL3=$ORACLES_SDL_SOURCE_DIR"
fi
"$@"
cmake --build "$build_dir" --parallel 2
test_dir="$build_dir/local-test-artifacts"
mkdir -p "$test_dir"
ORACLES_TEST_ARTIFACTS="$test_dir" ctest --test-dir "$build_dir" --output-on-failure -R '^(native_binary|native_ages|native_movement|app_menus_ages)$'
rom="$source_dir/roms/Legend of Zelda, The - Oracle of Ages (USA, Australia).gbc"
if [ -f "$rom" ]; then
  # Each run needs its own cache; keep earlier results available for inspection.
  resume_dir=$(mktemp -d "$test_dir/app-resume.XXXXXX")
  python3 "$source_dir/tests/test_app_ages_resume.py" "$build_dir/oracles-native" "$rom" "$test_dir" "$resume_dir"
fi
printf 'App: %s/Oracles.app\n' "$build_dir"
