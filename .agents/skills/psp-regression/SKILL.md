---
name: psp-regression
description: Regression-test PSP Library after changes, covering navigation, platforms, sorting, pagination, TEST 100, sessions, PSPLIB 2, PNG caching, PSP GU safety, and Tracker API compatibility.
---

# PSP Regression

Use this skill after changes to existing PSP Library functionality or when investigating a suspected regression. Start from the current diff and public headers; do not alter source or database formats merely to make a test pass.

## Required regression surface

- Confirm selection is remembered by stable `game_id`, independently for platform filters, across sorting, paging, and GRID/LIST changes.
- Exercise platform switching, all current sorting modes, page boundaries, empty/short pages, input repeat, and detail-screen return behavior.
- Run the isolated TEST 100 fixture and verify it does not overwrite the user's `library.db`; use the repository's current flag/database paths and fixture behavior.
- Verify statistics and game-session accounting, including create/update/reload behavior and the existing Tracker API contract.
- Verify PSPLIB 2 round trips, migration/backup recovery, CRC or malformed-file handling, and that no unrequested storage-format change was introduced.
- Exercise PNG loading and cache behavior for first load, cache hit, missing/corrupt/unsupported files, bounded cache replacement, and navigation while a cover is decoded.
- Check for the previously observed PSP GU failure class: stale or reused textures/display lists, unsafe frame transitions, unsynchronized buffers, or resource lifetime violations. Use runtime logs or PPSSPP/PSP evidence where available; static review alone must be labeled as such.

## How to run and report

1. Run the host test target with the same strict flags used by CI, then add focused tests or manual checks only where the changed behavior is not covered.
2. Inspect current public API headers under `include/psplibrary/` and the existing tests before inventing a new assertion or compatibility rule.
3. If a PSPDEV build, PPSSPP run, or physical-PSP run is unavailable, say so explicitly. Do not convert host-test success into a claim of PSP runtime compatibility.
4. Report each area as passed, failed, or not run, with the command/log/screen evidence and the smallest reproducible failure. Distinguish a real regression from an environment limitation.
