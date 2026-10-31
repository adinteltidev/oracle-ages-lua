# Switch Ages + Seasons implementation — 2026-10-31

Local working-tree implementation on `feat/switch-mod-loader`, based on
`f9846b4`. These changes have not been committed or pushed. This report describes
this delivery, independently of the earlier mods-only NRO and its hardware reports.

## Result

One Switch NRO can launch Ages or Seasons and change games from the L + R menu.
`switch_game` validates each original ROM and selects paths by game and region.
`switch_session` prepares the replacement before changing the live game. The
frontend saves the outgoing SRAM, stops its mods, resets and reuses the native
fiber pool, and installs the prepared session. SDL rendering/audio stay alive.
Missing/invalid incoming files or outgoing-save errors leave the game available.

Existing Ages paths stay in place. Seasons USA/Australia uses `seasons/` and
Europe uses `seasons-eu/` under the same build profile. Each has independent
battery saves, four slots, logs, mods, mod preferences and stores. The original
state format is unchanged. Moving state files across games/regions is unsupported.

Seasons runs its existing C gameplay. Committing a Seasons session disables the
Ages gameplay Lua rules; returning to Ages enables them again in a Lua build.
External Lua mods receive the correct game and frame layout for either game.
Read-only examples support both; `api2-demo` remains Ages-only.

Supported original ROMs and installation instructions are in
[SWITCH-SEASONS.md](SWITCH-SEASONS.md). No game ROM is bundled.

## Local validation

Actual ROMs used:

- Ages USA/Australia, SHA1 `880374fb978b18af4aa529e2e32f7ffb4d7dd2f4`.
- Seasons USA/Australia supplied by the user, SHA1
  `ba1268290fb2b1b70505d2d7b5825fc8a4816a4b`.

The macOS test build enables all eight gameplay Lua options. Results:

- Eleven focused CTest cases passed without skips: `native_ages`,
  `native_seasons`, `switch_game`, `switch_session`, `switch_session_rom`,
  `switch_menu`, `switch_storage`, `switch_seasons_save`, `switch_quicksave`,
  `switch_mods` and `switch_mods_rom`.
- The additional `switch_seasons_mods_rom` CTest passed with the supplied ROM,
  checking font/text/item composition and the Seasons flag.
- Each game's short replay passed 20,000 frames in normal and reload modes,
  matching 333 checkpoints per mode with zero skipped checkpoints. Seasons
  reload mode completed 19 reloads.
- The full Seasons normal replay passed **321,712 frames / 5,361 checkpoints**,
  with zero skipped checkpoints and no mismatches. It ran the native engine on
  macOS; it did not run the Switch NRO. Full reload mode was not rerun here.
- Real-ROM session replacement exercised Ages → Seasons → Ages twice, advancing
  each game, preserving independent SRAM and reusing the native fiber allocation.
  A missing Seasons ROM preserved the running Ages session.
- Seasons quicksave testing checked reload equivalence for 120 following frames,
  backup recovery, all four slots and all three original file-select cursors.
- A separate C-only build passed the session test and the 20,000-frame Seasons
  normal replay (333 checkpoints, zero skips).
- The optional SDL2 menu preview passed. Seasons switch/active pages were
  rendered and inspected at 720p; the existing preview also covers 1080p.
- Shared mod-ROM and treasure-swap regressions passed. Seven CI Python tests,
  `actionlint` and `git diff --check` passed.

Commands for the focused host tests after configuring the original ROM directory:

```sh
cmake --build build-macos-lua --parallel 3
ctest --test-dir build-macos-lua \
  -R '^(native_ages|native_seasons|switch_game|switch_session|switch_session_rom|switch_menu|switch_storage|switch_seasons_save|switch_quicksave|switch_mods|switch_mods_rom|switch_seasons_mods_rom)$' \
  --output-on-failure
build-macos-lua/test_native_seasons --rom /absolute/path/to/seasons.gbc \
  --frames 321712 --run normal
```

ROM-dependent CTests return skip code 77 when the original files are unavailable;
a build with skipped gameplay tests is not gameplay validation.

## Switch artifact

`Oracles-Switch-Ages-Seasons-Phase7.zip` contains the all-eight-options Lua build
with external mods enabled, installation docs, both replay fixtures and matching
sources. The NRO installs at the existing phase-7 path. Version/build metadata
and capability flags are captured in the package manifest; checksums and the
ELF/NRO audit describe that exact binary.

The audit verifies AArch64 PIE/NRO structure, relative relocations, native engine
objects, no interpreter object, no unresolved strong symbols, and no embedded
original ROM header or RomFS. It is not a console gameplay test.

## Outstanding device validation

The new NRO has not been run on a physical Switch here. Earlier Ages/phase-7
hardware confirmations do not cover these Seasons changes. Test normal Seasons
play, ZL/ZR, all four slots, original file selection, switching in both directions,
mods, audio, handheld/dock, suspend/resume and missing-ROM handling on this NRO.

The European Seasons checksum, region paths and language-screen input are
covered by synthetic tests; its actual gameplay is untested because no European
ROM was supplied. Replay fixtures are USA/Australia only. Performance/overhead on
the console remains unmeasured for this build.

Local logs and JUnit reports are under ignored `reports/switch-seasons/`.
