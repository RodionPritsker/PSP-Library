# PSP Library

PSP Library is an offline game-activity library for PSP and PPSSPP. Version 0.2 contains a portable C core, a text-based PSP UI suitable for the 480x272 screen, a versioned local database, dynamic collection storage, demo data, and host-side tests.

The project intentionally does **not** launch games and does not include the future system PRX, `sceIoOpen` interception, network features, cover downloading, or emulator modifications.

## Project structure

```text
include/psplibrary/
  core.h             Game model, filtering, sorting, and event processing
  storage.h          Versioned database format and atomic save/load
  demo.h             Isolated demonstration database
  ui.h               PSP UI entry point
  tracker.h          Launch descriptor and PSP tracker adapter API
src/core/
  core.c             Platform-independent library and session logic
  storage.c          Portable text database implementation
  demo.c             Five demonstration games
src/psp/
  main.c             PSP module and input loop
  ui.c               PSP Library screens and controller navigation
src/tracker/
  psp_tracker.c      PSP identity/session adapter
  psp_fs.c           PSP/host file-presence abstraction
tests/
  test_core.c        Host-side core, storage, and tracker tests
CMakeLists.txt       PSPDEV CMake build to EBOOT.PBP
Makefile             Direct PSPSDK build to EBOOT.PBP
tests/Makefile       Host test build
.github/workflows/   GitHub Actions host tests and PSPDEV build
scripts/              Windows PPSSPP ZIP packaging helper
```

## PSP build

Install the official PSP toolchain from [PSPDEV](https://pspdev.github.io/). On Windows, the supported workflow is WSL2 or the official `pspdev/pspdev` Docker image.

Inside the repository:

```sh
mkdir build
cd build
psp-cmake ..
make
```

This creates `build/EBOOT.PBP`. The included root `Makefile` is also available as a direct PSPSDK build:

```sh
make clean
make
```

Using the official Docker image from PowerShell instead:

```powershell
$projectPath = (Get-Location).Path.Replace('\', '/')
docker run --rm -v "${projectPath}:/source" -w /source pspdev/pspdev:latest sh -lc "mkdir -p build && cd build && psp-cmake .. && make"
```

Docker Desktop must be allowed to share the drive containing the project. The PSPDEV documentation also notes that a privileged container may be needed if `gmake` reports `Operation not permitted`.

The result is `EBOOT.PBP`. Copy the project to:

```text
ms0:/PSP/GAME/PSPLIBRARY/EBOOT.PBP
```

Then start **PSP Library** from the PSP Game menu, or add the same folder to PPSSPP's memory-stick directory and start it from the PSP menu.

The application stores its user database at:

```text
ms0:/PSP/SAVEDATA/PSPLIBRARY/library.db
```

The demonstration database is kept separately at `demo.db`. If no user database exists, the UI opens the demo database in read-only demo mode. The demo records are never silently saved over the user database.

## Host tests

With a C99 compiler available:

```sh
cd tests
make
make run
```

Or directly:

```sh
cc -std=c99 -Wall -Wextra -pedantic -I../include \
  ../src/core/core.c ../src/core/storage.c test_core.c \
  -o test_core
./test_core
```

The tests cover deterministic identifiers, filtering/sorting, repeated start/stop and switched sessions, time/session totals, save/load round trips, delimiter escaping, corrupted and incomplete files, backup recovery, and persistence of records when `file_present` becomes false. v0.2 also stress-tests 100, 500, and 1000 games, including 10000 sessions and large-file backup recovery.

The GitHub Actions workflow runs these host tests on Ubuntu and then builds the PSP target in the official `pspdev/pspdev:latest` container. A successful run publishes both `EBOOT.PBP` and an installation ZIP as workflow artifacts.

## GitHub Actions from Windows

This workspace currently has no `.git` directory and no remote repository was created. To connect it to a repository you create yourself:

```powershell
git init
git add .
git commit -m "Initial PSP Library v0.1"
git branch -M main
git remote add origin https://github.com/<your-user>/<your-repository>.git
git push -u origin main
```

Open the repository's **Actions** tab and select **PSP Library build**. It runs automatically on pushes and pull requests, or can be started with **Run workflow**. After a successful run, download the `psp-library-v0.2` artifact; it contains both the raw `EBOOT.PBP` and `psp-library-ppsspp.zip`.

## Windows PPSSPP package

After obtaining `build/EBOOT.PBP`, run from PowerShell:

```powershell
.\scripts\package_ppsspp.ps1
```

This creates `psp-library-ppsspp.zip` with the exact structure:

```text
PSP/GAME/PSPLIBRARY/EBOOT.PBP
```

Extract the ZIP into the PPSSPP memory-stick directory, preserving the `PSP` folder, or copy `EBOOT.PBP` directly to the corresponding `PSP\GAME\PSPLIBRARY` folder. Then launch it from the PSP Game menu.

## Controls

* D-pad: move through the library; grid and list views automatically scroll by page
* X: open the selected game's details
* Circle: return from details / clear platform filter
* L/R: previous/next platform filter
* Select: switch grid/list view; the current page is shown in the header
* Start: cycle sort mode
* Square: generate a short synthetic session for the selected game
* Triangle: exit

The PSP UI uses `pspDebugScreen` so it remains small, fast, and easy to build with a stock PSPSDK. Grid view uses cover placeholders and paths; it deliberately does not decode every cover into memory. Only the visible page is drawn: 21 grid slots or 28 list rows. The existing dirty redraw and PSP VRAM double buffering remain enabled.

## Architecture and limitations

The core only knows about a canonical game identity (`platform + stable content key`), not about a particular emulator. An adapter can therefore report the same content key from different emulators and merge its statistics, while region/release identity stays in the content key and executable version is stored separately as metadata.

The first PSP tracker adapter now accepts a launch descriptor (`content_id`, optional version, title, path, and timestamp), creates the game record when needed, emits start/switch/stop events, and preserves the canonical identity across path changes. The region-aware `content_id` is the stable identity input; executable version is stored as metadata and does not create a new record. A missing `content_id` is rejected instead of using a path as an unsafe identity fallback. On PSP, `pspl_psp_path_present()` uses `sceIoGetstat`; host tests use standard C file I/O. A future observer must feed descriptors into this API. The adapter is not a system PRX and does not hook `sceIoOpen`.

The database is a line-oriented versioned format with a CRC32 checksum. Version 2 stores the stable content key and executable version separately, and remains the on-disk format in v0.2. Version 1 files are still loadable: known legacy PSP keys are migrated to a version-independent identity, duplicate versions are merged, and session IDs are remapped. Games and sessions are kept in heap arrays that grow geometrically, so the library does not reserve 64 games or 256 sessions in every `pspl_library_t`. The storage reader and writer stream records instead of using a fixed 64-KiB database buffer. Safety ceilings for file loading are 4096 games and 65536 sessions; the tested target of 1000 games and 10000 sessions is well below them. Saves are written to a temporary file and then renamed, with the previous file retained as `.bak`. Reserved field characters are escaped, which keeps content keys intact. The application validates the primary file and falls back to `.bak` when the primary file is missing or invalid. This is best-effort crash recovery, not a claim that every PSP filesystem interruption is transactionally atomic.

The current build has not been tested on physical PSP hardware or PPSSPP in this workspace. The GitHub Actions build is the reproducible path to a real EBOOT when the workflow is run. PSPDEV documents `psp-cmake ..` followed by `make` as the normal build flow and documents the official Docker image for Windows/other hosts.

## Next small step

Implement one real tracker adapter, starting with a PSP game-launch/session source that reports canonical identities to `pspl_library_handle_event()`. Keep it behind the existing Tracking API so the UI and storage remain unchanged.
