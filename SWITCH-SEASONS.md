# Oracle of Seasons on Nintendo Switch

This build plays **Ages and Seasons** in one NRO. Ages keeps its existing data
locations. Seasons has separate battery saves, four state slots, logs, mods,
mod settings and stores. No ROM is included.

## Install

Copy the ZIP's `switch/` folder to the SD root. For the phase-7 package, add:

```text
sdmc:/switch/oracle-ages-lua-phase7/
  oracles-lua-phase7.nro
  ages.gbc                 # optional: Ages USA/Australia
  seasons.gbc              # Seasons USA/Australia OR Europe
  sram.sav                 # existing Ages battery save, kept in place
  quick.state              # existing Ages quicksave, kept in place
  mods/                    # Ages mods
  seasons/                 # created for Seasons USA/Australia
    sram.sav
    quick.state
    slot2.state
    slot3.state
    slot4.state
    mods/
    mods.cfg
    runtime.log
    mods-performance.json
  seasons-eu/              # separate data when seasons.gbc is European
```

Only install the ROMs you want to play. A Seasons-only setup works without an
Ages ROM: select Seasons before pressing A. Other builds use their existing
profile root (`oracle-ages-native`, `oracle-ages-lua-phaseN`, etc.) with the same
`seasons/` and `seasons-eu/` subdirectories.

Supported SHA1 checksums:

| Game / region | Filename | SHA1 |
| --- | --- | --- |
| Ages USA/Australia | ages.gbc | 880374fb978b18af4aa529e2e32f7ffb4d7dd2f4 |
| Seasons USA/Australia | seasons.gbc | ba1268290fb2b1b70505d2d7b5825fc8a4816a4b |
| Seasons Europe (En,Fr,De,Es,It) | seasons.gbc | 360b9408e569ba826212eac1ebea1b09573cb48c |

All three are 1,048,576 bytes. The checksum selects the region and save folder.
Keep states within their original game and region; their v1 file format is
unchanged. This implementation does not import or convert another game's states.

## Play and change games

- On startup, **Left/Right** chooses Ages or Seasons, then **A** plays.
- While playing, **L + R** opens the menu. Choose the other game's tab and
  press **A** on **Start Oracle of Seasons/Ages**.
- The next ROM and its battery save are validated before replacing the running
  session. If loading or saving fails, the current game stays available.
- The outgoing battery save is written before switching. The incoming game
  starts at its title screen; resume through its original save files or slots.
- **ZL** quicksaves and **ZR** quickloads the active game's state. The four slots
  and the file list in the menu belong to the active game.
- **Minus** in the external menu returns to the active game's tab. **B** resumes.
- European Seasons lets the player choose the original game's language; the
  file-screen helper passes controller input through on that screen.

## Lua and mods

Seasons gameplay uses the existing native **C** implementation. The migrated
Ages gameplay rules are disabled during Seasons and restored when returning to
Ages in a Lua build. There is no new Seasons gameplay-language migration.

With `ORACLES_SWITCH_MODS=ON`, Seasons runs external Lua mods declared with
`games=seasons` or `games=ages,seasons`. Put them in the active region's `mods/`
folder above. The API receives the Seasons game, room, camera and sprite data;
API 2 reward validation also uses Seasons. `overlay-info` and `room-reader`
work with either game. The Ages-specific `api2-demo` remains Ages-only.
See [SWITCH-MODS.md](SWITCH-MODS.md) and [MODDING.md](MODDING.md).

## Replay and validation

X/Y on startup replay the **selected USA/Australia game**, with external mods
off. Fixtures for both games are included. X runs 20,000 frames; Y runs the full
movie (Ages 289,518 frames, Seasons 321,712 frames). The Seasons full reference
has 5,361 checkpoints. European Seasons supports Play, not the USA reference.

Build and host tests do not prove Seasons gameplay on a Switch. The supplied
USA/Australia ROM was used for local gameplay tests. See
[SWITCH-SEASONS-IMPLEMENTATION.md](SWITCH-SEASONS-IMPLEMENTATION.md) for the local
validation scope; fresh package metadata is in `SWITCH-VERIFICATION.json`.
On the device, test normal play, ZL/ZR, all slots, file selection, both directions
of game switching, mods, audio, handheld/dock and suspend/resume with this NRO.
