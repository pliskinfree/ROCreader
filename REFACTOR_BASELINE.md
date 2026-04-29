# ROCreader Refactor Baseline

This file records the safety checks for the gradual architecture cleanup.

## Current Scope

- Stage 0: baseline notes and smoke checklist.
- Stage 1: move status bar and volume overlay drawing out of `main.cpp`.
- Stage 2: move reader input routing out of `main.cpp`.

## Guardrails

- Keep user-visible behavior unchanged during these stages.
- Avoid changing ZIP image reader internals while the ZIP module is still stabilizing.
- Prefer moving existing logic into new modules before redesigning interfaces.
- Keep each stage buildable and easy to revert.

## Smoke Checklist

Run these after each stage when a runnable build is available:

1. Start the app and reach the shelf.
2. Switch shelf categories.
3. Open TXT, scroll, exit, reopen, and confirm progress resumes.
4. Open PDF, page/scroll, zoom, rotate, exit, and reopen.
5. Open EPUB comic/image-only content.
6. Open EPUB flow/mixed content and confirm rotate/zoom remains blocked.
7. Open ZIP image content.
8. Open and close settings.
9. Change TXT settings and confirm reader colors/font size still apply.
10. Adjust volume and confirm the overlay appears.
11. Confirm battery/time/avatar status overlay still appears on shelf/settings.
12. Exit the app and confirm progress/config persists.

