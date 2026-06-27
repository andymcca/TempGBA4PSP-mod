# TempGBA4PSP-mod (2025 Build)

Based on phoe-nix' Github repo.

- Added a 16:9 Full Screen option
- Fixed Menu text and Game -> Menu crashes
- Fixed a few games that have never worked previously -
  -   NBA Jam 2002
  -   Colin Mcrae Rally
  -   Banjo Kazooie
  -   Starsky and Hutch
  -   Balatro (https://github.com/cellos51/balatro-gba)
  -   ....and probably a few more!
- ~~Swapped X/O buttons around~~    (You can choose which button X/O to use for confirm)
- ~~Couple of minor fixes to make it work under PPSSPP (including w/ HW Rendering)~~    (on PPSSPP will only work with SW Rendering)

## Single-game mode (drop-in replacement)

This build auto-detects [GrabowskiDev single-game](https://github.com/GrabowskiDev/TempGBA4PSP-Single-game) folder layouts. If `roms/game.gba` exists, the emulator loads it on startup with no ROM browser — same behavior as that fork. Standard multi-ROM use is unchanged when `game.gba` is absent.

To upgrade an existing single-game install, copy these files from a fresh build into the game folder (keep your custom `PBOOT.PBP`, `roms/game.gba`, and save data):

- `EBOOT.PBP`
- `DATA.PSP`
- `TempGBA.prx`
- `exception.prx`
- `ku_bridge.prx` (if present)
- `gba_bios.bin`

Expected layout (unchanged from single-game releases):

| Path | Purpose |
|------|---------|
| `roms/game.gba` | ROM (unzipped) |
| `save/game.sav` | Battery save |
| `state/game_*.svs` | Save states |
| `cfg/game.cfg` | Per-game settings |
| `dir.ini` | Directory paths (`rom_directory = roms`, etc.) |

## Building

On Windows, use **Docker Desktop** and the PSPDev image; see **[BUILD-DOCKER.md](BUILD-DOCKER.md)** for the exact `docker run` command (mount `source/`, run `make`).
