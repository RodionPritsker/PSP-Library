# PSP Library v0.3.1 — Visual Polish implementation report

## Scope

The v0.3.1 implementation follows `docs/GameDiary-audit-v0.3.1.md` and uses the GameDiary checkout only as a reference for a narrow offline SDF/T8 pipeline, semantic themes, and compact screen composition. The PSP Library renderer, image cache, PSPLIB 2 model, tracker API, navigation state, and GU lifetime fixes remain the project’s own implementation.

No GameDiary runtime renderer, CJK assets, intraFont, PRX Tracker/POPS/session logic, or GameDiary images were copied.

## Implemented

- `src/psp/text.c` now loads a build-time-generated Noto Sans Medium SDF atlas as an 8-bit T8 texture and draws it through a GU CLUT. It supports proportional advances, Latin/Cyrillic lookup, invalid/missing glyph fallback, UTF-8 codepoint boundaries, clipping with ellipsis, word wrapping, and multiple logical sizes. Heap work is limited to one-time resource loading; drawing uses the existing GU command list memory.
- `src/psp/gfx.c` adds a focused `pspl_gfx_draw_t8` primitive with cache-safe texture use, CLUT loading, linear filtering, blending, and an alpha test suited to SDF edges. Existing RGBA texture drawing and framebuffer setup were left intact.
- `src/core/theme.c` and `include/psplibrary/theme.h` define all 14 Dark/Light × Amber/Ocean/Mint/Forest/Violet/Rose/Crimson combinations using the semantic fields `background`, `surface`, `surface_secondary`, `primary_text`, `secondary_text`, `accent`, `accent_text`, `selected_background`, `selected_border`, and `border`. The default is Dark Amber. There are no temporary theme controls; the API is programmatic and ready for a future settings screen.
- `src/core/text_layout.c` provides portable UTF-8 boundary helpers for host tests.
- Grid remains 3×2 with stronger focus coverage, larger covers, two-line titles, platform and total time only. Session counts stay on details.
- List rows prioritize the title and retain platform/total time. Detail view makes total time primary and retains sessions, dates, cover status, and recent sessions.
- Header/footer retain title, section, count/page, and compact PSP hints while reducing visual noise.
- `scripts/package_ppsspp.ps1` and GitHub Actions now include `assets/fonts` in PPSSPP packages. Version metadata is `v0.3.1` / PBP `03.01`.

## Runtime resources and memory

The checked-in atlas was generated from `NotoSans-Medium.ttf` with the audited GameDiary `msdf-atlas-gen` tool using SDF mode, 24 px base size, 4 px distance range, and a 512×512 atlas. The resource sizes are:

| Resource | Size |
| --- | ---: |
| T8 atlas pixels | 262,144 bytes |
| Metadata + 65,536-entry lookup + 191 glyph records | 137,212 bytes |

These are file sizes, not measured PSP heap usage. The runtime additionally allocates aligned copies of the same resources during text initialization. The existing measured/planned framebuffer footprint remains two 512×272 RGBA buffers (~1.1 MiB) plus the reserved depth buffer (~0.28 MiB); the existing eight-slot cover cache and 1 MiB GU command lists were not expanded.

If the package assets are missing, initialization is non-fatal and the text module uses a safe outline fallback rather than terminating the UI.

## Verification

- Atlas generation completed locally and produced the expected 512×512 T8 file and metadata.
- `git diff --check` passes.
- Host test sources now cover mixed Latin/Cyrillic UTF-8 boundaries, truncation at codepoint boundaries, all 14 themes, contrast checks, and atlas file sizes.
- The local Windows environment has neither `make` nor a C compiler/PSPDEV toolchain, so host and PSP compilation must be validated by the repository GitHub Actions workflow. PPSSPP/PSP Street visual and performance checks require the resulting artifact and are not claimed here as completed.

## Licensing

The runtime font resource is derived from Noto Sans Medium by Google LLC under SIL Open Font License 1.1. The project keeps `assets/fonts/LICENSE-OFL-1.1.txt` and `assets/fonts/NOTICE.txt`. The build-time generator is referenced in `NOTICE.txt` and is not part of the runtime. Existing project cover artwork remains unchanged and no GameDiary image assets were imported.
