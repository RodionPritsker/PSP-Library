---
name: pspdev-build
description: Build and verify PSP Library with CMake, Make, PSPDEV, GitHub Actions, EBOOT.PBP, and the PPSSPP ZIP; use for build, CI, packaging, or release-artifact tasks rather than UI-only or behavior-only reviews.
---

# PSPDEV Build

Use this skill for PSP Library build, CI, and release-artifact work. Keep source changes out of the task unless the user explicitly requests an implementation fix.

## Discover the current build contract

- Read `CMakeLists.txt`, the root `Makefile`, `.github/workflows/*.yml`, and `scripts/package_ppsspp.ps1` before judging a build. Treat CMake and the direct PSPSDK Makefile as separate build paths and compare their source lists, flags, PSP libraries, title/version, and output names.
- Inspect the repository status and the current workflow revision. Do not assume README claims or a previous run still describe the current build.
- Track runtime resources separately from compiled sources: `assets/`, `covers/`, font atlases, metadata, and test fixtures must be present in the path that creates the final package.

## Verification workflow

1. Run the repository's host-test command from the workflow (`make -C tests clean`, build with the workflow flags, then `make -C tests run`) and preserve the actual output.
2. Run the PSPDEV path when the official container/toolchain is available. Prefer the workflow's `pspdev/pspdev:latest` container and `psp-cmake`; otherwise report the exact unavailable prerequisite. A successful host compile is not a PSPDEV result.
3. Verify `build/EBOOT.PBP` exists and is non-empty after the PSPDEV build. Check the artifact path used by CI, not only an unrelated local output.
4. Reproduce or inspect PPSSPP packaging using the current workflow/script. Inspect ZIP entries and required paths: `PSP/GAME/PSPLIBRARY/EBOOT.PBP`, `covers/`, `assets/`, and `PSP/SAVEDATA/PSPLIBRARY/test100.flag.example`.
5. When a new font, atlas, image, cover, or fixture is involved, trace it from repository path to build/package command and verify it appears in the artifact. Do not infer inclusion from Git status alone.

## CI diagnosis and reporting

- Diagnose GitHub Actions from the real job log or artifact metadata. Separate checkout, host-test, PSPDEV/toolchain, EBOOT, packaging, and upload failures; do not guess from a red check alone.
- State which checks actually ran, which were unavailable, and which artifacts were inspected. Compilation, a non-empty EBOOT, or a valid ZIP does not prove correct behavior in PPSSPP or on real PSP hardware.
- Never claim PPSSPP or physical-PSP validation unless it was actually performed in the current task. If it was not available, mark runtime behavior as unverified and identify the next evidence needed.
