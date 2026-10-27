# Local macOS Intel build

Base: kirby-letsgo/oracles-decomp at
`711fe31bd8510c281384ade60597c849e1f52954`.

Install Apple's Command Line Tools and create a Python 3.12 virtual environment.
Install `requirements-macos.txt` in it, activate it, then run:

```sh
sh tools/build_macos_intel.sh
```

The script builds the static SDL frontend and `build-native-x64/Oracles.app`.
It targets Intel, macOS 11 or later, with optimized code and debugging information.
SDL 3.4.16 is fetched from the hash-pinned upstream URL. For an already downloaded,
verified source tree, set `ORACLES_SDL_SOURCE_DIR` to its absolute path.

The supplied game ROM is kept in the ignored `roms/` directory with its upstream
filename, `Legend of Zelda, The - Oracle of Ages (USA, Australia).gbc`.
Its SHA-256 must be
`0b56b78a9e45452e98c33edd111234931f1e034dc097f6f23082eb8db6055474`.
Without it, the game-dependent tests report a skip. A skip is not a pass.
The app can import the ROM on first launch; no ROM is embedded in the app.

Local test additions keep Ages independent from the unavailable Seasons ROM:

- `native_ages`: upstream TAS checkpoints and save-state reloads for Ages alone.
- `native_movement`: reaches room 00:8A through natural TAS inputs, then checks
  normal rightward movement and collision without editing game RAM.
- `app_menus_ages`: upstream menu, save-state, settings, and reload tests with Ages.

The build script also runs `tests/test_app_ages_resume.py` when the Ages ROM is
present. It seeds an isolated app cache with the naturally reached native state,
then checks room, position, health and the SRAM checksum after two separate app
launches. Its observer mod only reads memory. No player cache is modified.

`ORACLES_DIAGNOSTICS=1` prints the selected video/audio backends for a desktop
smoke test. The local packaged app was exercised through Launch Services with
Cocoa, Metal, CoreAudio and 600 gameplay frames.

The extended headless replay matched every one of the upstream Ages reference's
4,825 checkpoints (frames 240 through 289680). This is a deterministic regression
against that reference, not an independent manual certification of every route.

`native_binary` checks that the native archive excludes the interpreter object.
The game's hardware timing, graphics, and sound models remain part of the port.
For a longer native replay, use `TAS_FRAMES` with `native_ages` and retain its log.

The separate validated assembly reference was not modified. All 13,350 Ages
bank/address references in `src/hooks/syms_used.txt` matched that reference.

To package locally, build the app, inspect its linked libraries with `otool -L`,
and use `tools/make_dmg.sh`. Keep the repository's license and third-party notices
with any source/binary delivery. The local signing identity is ad hoc.
