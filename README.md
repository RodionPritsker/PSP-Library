# PSP Library v0.3

PSP Library is an offline game-activity diary for PSP and PPSSPP. It stores game metadata and sessions locally; it does not launch games, hook a system PRX, download covers, or provide online features.

The v0.3 UI is designed for the 480x272 PSP screen and uses the PSP GU for rendering. It has a dark card-based grid, a compact list mode, graphical game details, local PNG covers, a bounded image cache, and the existing PSPLIB 2 database/core APIs.

## Project structure

```text
include/psplibrary/
  core.h             Game model, filtering, sorting, and events
  storage.h          PSPLIB 2 database format and backup recovery
  navigation.h       Selection memory per platform filter
  demo.h             Five-game and isolated 100-game fixtures
  gfx.h              PSP GU 2D primitives and framebuffer setup
  text.h             Small graphical bitmap font and clipping
  image.h            PNG cover cache API
  ui.h               PSP screens and controller navigation
src/core/            Portable library, storage, navigation, and fixtures
src/psp/
  gfx.c              GU initialization, double buffering, primitives
  text.c             Lightweight graphical text renderer
  image.c            Streaming PNG decode, resize, cache, and fallback
  ui.c               Grid, list, details, input repeat, and navigation
src/tracker/         Existing PSP identity/session adapter
covers/              Original test covers used by the demo fixtures
tests/               Host-side core/storage/tracker tests
scripts/             PPSSPP ZIP packaging helper
```

## Build

The reproducible PSP build uses the official PSPDEV toolchain:

```sh
mkdir build
cd build
psp-cmake ..
make -j2
```

This creates `build/EBOOT.PBP`. The root `Makefile` is also available for a direct PSPSDK build. The PSP target links the GU, PNG, and zlib libraries supplied by PSPDEV.

The GitHub Actions workflow runs host tests first, then builds the EBOOT in `pspdev/pspdev:latest`, packages the EBOOT, cover resources, and the 100-game test-mode flag example, and uploads both artifacts.

## Installation and data

Copy the generated EBOOT to:

```text
ms0:/PSP/GAME/PSPLIBRARY/EBOOT.PBP
```

The user database is stored at:

```text
ms0:/PSP/SAVEDATA/PSPLIBRARY/library.db
```

The normal five-game demo is stored separately as `demo.db` and is never saved over user data. If the user database is missing, the app starts in demo mode.

### 100-game UI test mode

The 100-game fixture is isolated from `library.db`. Create an empty file at:

```text
ms0:/PSP/SAVEDATA/PSPLIBRARY/test100.flag
```

The packaged `PSP/SAVEDATA/PSPLIBRARY/test100.flag.example` can be renamed to `test100.flag`. On the next launch, the app creates/loads `test100.db` with 100 deterministic games across PSP, PS1, GBA, GB, GBC, NES, SNES, and OTHER. Remove the flag to return to normal mode; the test database is left untouched.

## Controls

- D-pad: move selection; holding a direction repeats movement
- X: open the selected game
- Circle: return from details
- SELECT: switch grid/list mode
- START: cycle title, total-time, and recently-played sorting
- L/R: switch platform filter
- Square: create a short test session for the selected game
- Triangle: exit

Selection is remembered independently for every platform filter, including when sorting, paging, or changing view modes.

## Covers

`pspl_game_t.cover_path` remains the source of truth and is stored in PSPLIB 2 without a format change. Relative paths are resolved below `ms0:/PSP/GAME/PSPLIBRARY/`; absolute `ms0:/` and `ef0:/` paths are also accepted.

PNG files are decoded only when first needed. The decoder streams source rows and scales into a maximum 128x176 RGBA texture, so large source images do not require a full-size decoded bitmap. Decoding happens between synchronized GU frames, one uncached path per loop, so a large cover cannot block or corrupt a partially submitted frame during startup or navigation. Eight cache slots are used with oldest-unused replacement; entries referenced by the current GU frame are pinned until that frame has been synchronized. Missing, corrupt, unsupported, or absent files draw a platform-colored placeholder and never terminate the app. Covers are used in the grid, list, and detail screen and survive EBOOT/database updates because they are independent files.

The included `covers/` files are original project test artwork, pre-scaled to fit the runtime's 128x176 decode limit. This keeps the first demo page fast on PSP; user-supplied larger PNGs are still streamed and reduced safely at runtime. No network downloading is performed.

## Rendering and memory

The UI no longer uses `pspDebugScreenPrintf`. It uses a compact PSP GU layer with two 512x272 RGBA framebuffers, VBlank synchronization, a depth-buffer reservation, alpha blending, textured sprites, geometric panels, and a small 5x7 graphical font. The framebuffer pair uses about 1.1 MiB of VRAM; the depth buffer reserves about 0.28 MiB. Cover textures are kept in regular RAM, with eight scaled textures requiring at most roughly 720 KiB plus decoder working memory for one PNG row. GU command lists are 1 MiB each, vertices are allocated from the active list, and texture buffers are 16-byte aligned before cache writeback. The first version intentionally avoids transitions and full-library cover preloading.

The font currently renders ASCII directly and maps other UTF-8 sequences to a safe fallback glyph. The data model already supports UTF-8 bytes in titles; a full Cyrillic glyph set can be added later without changing storage.

## Compatibility

The v0.3 changes preserve:

- PSPLIB 2 and migration from legacy databases
- dynamic game/session storage, CRC validation, and `.bak` recovery
- filtering, sorting, selection memory, event handling, and tracker API
- the existing canonical PSP identity behavior

The app remains an offline diary. Launching games, a real PRX tracker, achievements, online services, and richer analytics remain out of scope.

## Tests and verification

Host tests can be run with any C99 compiler:

```sh
cd tests
make clean
make CFLAGS="-std=c99 -Wall -Wextra -Werror -pedantic -O2 -I../include"
make run
```

The test suite covers deterministic IDs, navigation memory, event/session accounting, PSPLIB 2 round trips, escaping, corrupted/incomplete files, backup recovery, missing-file persistence, tracker identity, and scaling to 1000 games/10000 sessions. The CI workflow additionally checks the PSPDEV build and verifies that `EBOOT.PBP` is produced.

This workspace does not contain a PSPDEV compiler, PPSSPP runtime, or physical PSP, so those runtime checks must be completed from the GitHub Actions artifacts. In particular, verify PNG rendering, VRAM behavior, input feel, and the exact package layout in PPSSPP or on a PSP Street E1000 before treating the release as hardware-validated.
