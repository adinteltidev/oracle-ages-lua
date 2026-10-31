#!/bin/bash
set -euo pipefail
platform=${1:?Usage: build.sh windows|macos|switch}
export ORACLES_VERSION=${ORACLES_VERSION:-}
export ORACLES_RELEASES=${GITHUB_REPOSITORY:-adinteltidev/oracle-ages-native}
export ORACLES_LUA_ITEMS_SHOPS=${ORACLES_LUA_ITEMS_SHOPS:-OFF}
export ORACLES_LUA_NPCS_EVENTS=${ORACLES_LUA_NPCS_EVENTS:-OFF}
export ORACLES_LUA_DOORS=${ORACLES_LUA_DOORS:-OFF}
export ORACLES_LUA_GAMEPLAY=${ORACLES_LUA_GAMEPLAY:-OFF}
export ORACLES_LUA_LEVERS_LAVA=${ORACLES_LUA_LEVERS_LAVA:-OFF}
export ORACLES_LUA_COLORED_CUBE=${ORACLES_LUA_COLORED_CUBE:-OFF}
export ORACLES_LUA_BLOCK_PUZZLES=${ORACLES_LUA_BLOCK_PUZZLES:-OFF}
export ORACLES_SWITCH_BUILD=${GITHUB_SHA:-$(git rev-parse HEAD)}
mkdir -p "dist/$platform" reports
case "$platform" in
  windows)
    export ORACLES_LLVM_MINGW
    ORACLES_LLVM_MINGW=$(python3 tools/fetch_windows_toolchain.py .tools/llvm)
    export ORACLES_JOBS=3
    sh tools/build_windows_llvm.sh build-win-ci
    if [ "$ORACLES_LUA_GAMEPLAY" = ON ]; then
      cmake --build build-win-ci --target test_lua_gameplay --parallel 3
      if [ "$ORACLES_LUA_BLOCK_PUZZLES" = ON ]; then cmake --build build-win-ci --target test_lua_block_rules --parallel 3; fi
      if [ "$ORACLES_LUA_COLORED_CUBE" = ON ]; then cmake --build build-win-ci --target test_lua_colored_cube --parallel 3; fi
      if [ "$ORACLES_LUA_DOORS" = ON ]; then cmake --build build-win-ci --target test_lua_doors --parallel 3; fi
      if [ "$ORACLES_LUA_NPCS_EVENTS" = ON ]; then cmake --build build-win-ci --target test_lua_npcs_events --parallel 3; fi
      if [ "$ORACLES_LUA_ITEMS_SHOPS" = ON ]; then cmake --build build-win-ci --target test_lua_items_shops --parallel 3; fi
      if [ "$ORACLES_LUA_LEVERS_LAVA" = ON ]; then cmake --build build-win-ci --target test_lua_levers_lava --parallel 3; fi
    fi
    python3 tools/audit_windows_binary.py build-win-ci/Oracles.exe --llvm-bin "$ORACLES_LLVM_MINGW/bin" --output reports/windows-audit.json
    python3 tools/ci/manifest.py --platform windows-x64 --binary build-win-ci/Oracles.exe \
      --audit reports/windows-audit.json --toolchain .tools/llvm/llvm-mingw.json \
      --compiler "$("$ORACLES_LLVM_MINGW/bin/x86_64-w64-mingw32-clang" --version | head -1)" --output dist/windows/windows-BUILD.json
    python3 tools/package_windows.py build-win-ci/Oracles.exe dist/windows/Oracles-Windows-x64.zip \
      --audit reports/windows-audit.json --verification dist/windows/windows-BUILD.json \
      --sdl-source build-win-ci/_deps/sdl3-src --llvm-root "$ORACLES_LLVM_MINGW"
    python3 - <<'PY'
import os,zipfile
with zipfile.ZipFile('reports/smoke-input.zip','w',zipfile.ZIP_DEFLATED) as z:
    for name in ['Oracles.exe','oracles-native-run.exe','test_ui.exe','test_platform_keys.exe']:
        z.write('build-win-ci/'+name,name)
    if os.environ.get('ORACLES_LUA_GAMEPLAY')=='ON':
        names=['test_lua_gameplay.exe']
        if os.environ.get('ORACLES_LUA_BLOCK_PUZZLES')=='ON':names.append('test_lua_block_rules.exe')
        if os.environ.get('ORACLES_LUA_COLORED_CUBE')=='ON':names.append('test_lua_colored_cube.exe')
        if os.environ.get('ORACLES_LUA_DOORS')=='ON':names.append('test_lua_doors.exe')
        if os.environ.get('ORACLES_LUA_ITEMS_SHOPS')=='ON':names.append('test_lua_items_shops.exe')
        if os.environ.get('ORACLES_LUA_NPCS_EVENTS')=='ON':names.append('test_lua_npcs_events.exe')
        if os.environ.get('ORACLES_LUA_LEVERS_LAVA')=='ON':names.append('test_lua_levers_lava.exe')
        for name in names:
            z.write('build-win-ci/'+name,name)
PY
    ;;
  macos)
    cmake -S . -B build-macos-ci -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_OSX_ARCHITECTURES=x86_64 -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
      -DORACLES_SDL=ON -DORACLES_SDL_VENDORED=ON -DORACLES_VERSION="$ORACLES_VERSION" -DORACLES_RELEASES="$ORACLES_RELEASES" \
      -DORACLES_LUA_GAMEPLAY="$ORACLES_LUA_GAMEPLAY" -DORACLES_LUA_BLOCK_PUZZLES="$ORACLES_LUA_BLOCK_PUZZLES" -DORACLES_LUA_COLORED_CUBE="$ORACLES_LUA_COLORED_CUBE" -DORACLES_LUA_LEVERS_LAVA="$ORACLES_LUA_LEVERS_LAVA" -DORACLES_LUA_DOORS="$ORACLES_LUA_DOORS" -DORACLES_LUA_ITEMS_SHOPS="$ORACLES_LUA_ITEMS_SHOPS" -DORACLES_LUA_NPCS_EVENTS="$ORACLES_LUA_NPCS_EVENTS"
    cmake --build build-macos-ci --target oracles-native-app oracles-native-run test_ui test_platform_keys test_native_ages test_switch_storage --parallel 3
    ctest --test-dir build-macos-ci --output-on-failure --output-junit "$PWD/reports/macos.xml" -R '^(ui|platform_keys|native_ages|switch_storage)$'
    if [ "$ORACLES_LUA_GAMEPLAY" = ON ]; then
      cmake --build build-macos-ci --target test_lua_gameplay --parallel 3
      if [ "$ORACLES_LUA_BLOCK_PUZZLES" = ON ]; then cmake --build build-macos-ci --target test_lua_block_rules --parallel 3; fi
      if [ "$ORACLES_LUA_COLORED_CUBE" = ON ]; then cmake --build build-macos-ci --target test_lua_colored_cube --parallel 3; fi
      if [ "$ORACLES_LUA_DOORS" = ON ]; then cmake --build build-macos-ci --target test_lua_doors --parallel 3; fi
      if [ "$ORACLES_LUA_NPCS_EVENTS" = ON ]; then cmake --build build-macos-ci --target test_lua_npcs_events --parallel 3; fi
      if [ "$ORACLES_LUA_ITEMS_SHOPS" = ON ]; then cmake --build build-macos-ci --target test_lua_items_shops --parallel 3; fi
      if [ "$ORACLES_LUA_LEVERS_LAVA" = ON ]; then cmake --build build-macos-ci --target test_lua_levers_lava --parallel 3; fi
      ctest --test-dir build-macos-ci --output-on-failure --output-junit "$PWD/reports/macos-lua.xml" -R '^(lua_gameplay|lua_block_rules|lua_colored_cube|lua_levers_lava|lua_doors|lua_items_shops|lua_npcs_events)$'
      mkdir -p build-macos-ci/Oracles.app/Contents/Resources/Licenses
      cp third_party/lua/lua.h build-macos-ci/Oracles.app/Contents/Resources/Licenses/Lua-MIT-header.txt
    fi
    python3 tools/check_native_binary.py build-macos-ci/liboraclesnative.a build-macos-ci/Oracles.app/Contents/MacOS/Oracles build-macos-ci/oracles-native-run
    sh tools/make_dmg.sh build-macos-ci dist/macos/Oracles-macOS-Intel.dmg
    python3 tools/ci/macos_audit.py build-macos-ci/Oracles.app --output reports/macos-audit.json
    python3 tools/ci/manifest.py --platform macos-intel --binary build-macos-ci/Oracles.app/Contents/MacOS/Oracles \
      --audit reports/macos-audit.json --junit reports/macos.xml --compiler "$(clang --version | head -1)" --output dist/macos/macos-BUILD.json
    ;;
  switch)
    llvm_root=$(python3 tools/fetch_windows_toolchain.py .tools/llvm)
    export ORACLES_LLVM="$llvm_root/bin"
    python3 tools/fetch_switch_sdk.py --destination .tools/switch-sdk --downloads .tools/switch-downloads
    python3 tools/fetch_switch_host_tools.py --destination .tools/switch-host-tools
    export DEVKITPRO="$PWD/.tools/switch-sdk/opt/devkitpro"
    export ORACLES_SWITCH_TOOLS="$PWD/.tools/switch-host-tools"
    export JOBS=3
    sh tools/build_switch_llvm.sh
    python3 tools/audit_switch_binary.py build-switch-llvm --llvm-bin "$ORACLES_LLVM" --output reports/switch-audit.json
    python3 tools/ci/manifest.py --platform switch-arm64 --binary build-switch-llvm/oracles-switch.nro \
      --audit reports/switch-audit.json --toolchain platform/switch/sdk-lock.json \
      --compiler "$("$ORACLES_LLVM/clang" --version | head -1)" --output dist/switch/switch-BUILD.json
    python3 tools/package_switch.py build-switch-llvm dist/switch/Oracles-Switch.zip \
      --audit reports/switch-audit.json --verification dist/switch/switch-BUILD.json
    cp build-switch-llvm/oracles-switch.nro dist/switch/
    ;;
  *) echo "Unknown platform: $platform" >&2; exit 2 ;;
esac

python3 tools/ci/finalize.py "dist/$platform/$platform-BUILD.json"
