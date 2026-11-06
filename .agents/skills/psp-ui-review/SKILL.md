---
name: psp-ui-review
description: Review and implement PSP Library UI behavior and layout for 480x272 PSP GU constraints, memory, typography, screens, themes, and PPSSPP validation; use for interface work rather than build-only or data-only tasks.
---

# PSP UI Review

Use this skill when reviewing or changing the PSP Library interface. Inspect the current `src/psp/ui.c`, `gfx.c`, `text.c`, `image.c`, related headers, assets, and project documentation before proposing UI changes.

## Hardware and renderer constraints

- Design and review against the PSP's 480x272 display, limited RAM, and roughly 2 MiB VRAM budget. Account for the framebuffer pair, depth buffer, command lists, cover textures, font atlas, and allocator overhead from the current implementation rather than relying on a generic desktop assumption.
- Preserve the PSP GU frame lifecycle: double buffering, VBlank synchronization, active display-list lifetime, texture lifetime, cache writeback/alignment, and synchronization before replacing or reusing resources. A visually plausible static layout can still be wrong if it races the GU.
- Check costly work such as PNG decode, texture upload, and allocation against frame timing and navigation behavior. Prefer bounded, incremental work and existing cache/pinning rules.

## Screens, typography, and visual checks

- Review GRID, LIST, and GAME DETAILS as a connected navigation flow, including selection, paging, platform switching, sorting, input repeat, empty/missing-cover states, and return paths.
- Check card geometry, safe margins, row/column spacing, clipping, word wrapping, ellipsis, contrast, and readability with realistic long Latin and Cyrillic names. Preserve UTF-8 codepoint boundaries; verify the SDF/T8 atlas, CLUT, metrics, fallback glyphs, and resource paths whenever text or fonts change.
- Keep the theme model ready for Dark/Light and seven color accents (the current code may expose the combinations programmatically); do not introduce a settings/persistence layer unless requested.
- Validate resource lifetime and screen transitions, not just pixel placement. Do not treat a successful compile or host test as proof of PPSSPP rendering correctness.

## Evidence

- Use PPSSPP when it is available and record what was actually exercised, including resolution, input, text, covers, scrolling, and screen transitions. If PPSSPP or a physical PSP is unavailable, clearly label the review as static/host-only and list the runtime checks still required.
- When making a change, keep the patch focused and preserve existing working GU and data paths unless the defect requires otherwise.
