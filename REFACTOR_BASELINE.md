# ROCreader Refactor Baseline

This file records the safety checks for the gradual architecture cleanup.

## Current Scope

- Stage 0: baseline notes and smoke checklist.
- Stage 1: move status bar and volume overlay drawing out of `main.cpp`.
- Stage 2: move reader input routing out of `main.cpp`.
- Stage 3: thin `main.cpp` into `RunApp()` and establish app bootstrap/environment seams.
- Stage 4: group reader session dependencies without changing open/close behavior.
- Stage 5: move SDL/window/renderer/RGDS display bootstrap out of `RunApp()`.
- Stage 6: move SDL input device open/close lifecycle into `app_services`.
- Stage 7: move runtime path and local storage/cache path setup into `app_services`.
- Stage 8: add `app_composition` and move `AppContext` assembly out of `RunApp()`.
- Stage 9: move platform input/profile capability selection into `app_services`.
- Stage 10: move config/progress/favorites/history store initialization into `app_services`.
- Stage 11: move system service object creation into `app_services`.
- Stage 12: move system/TXT settings state initialization and startup sync into `app_services`.
- Stage 13: move app UI initial volume display setup and system-volume SFX env parsing into `app_runtime`.
- Stage 14: move contributor avatar panel initial focus setup into `contributor_avatar_runtime`.
- Stage 15: move version update state creation and current-version probing into `version_update_runtime`.
- Stage 16: move online shelf default config/download path resolution into `online_shelf_controller`.
- Stage 17: move lid-close screen-off startup sync into `app_services`.
- Stage 18: move RGDS startup shelf/menu state sync into `app_services`.
- Stage 19: factor RGDS menu open and reader-state sync helpers into `app_services`.
- Stage 20: factor online shelf input tick result application into `app_services`.
- Stage 21: factor menu scene input pre-sync into `app_services`.
- Stage 22: factor online shelf present tick result application into `app_services`.
- Stage 23: factor online shelf deferred-disconnect root refresh into `app_services`.
- Stage 24: factor online shelf deferred-connect result handling into `app_services`.
- Stage 25: factor menu scene input context construction into `app_services`.
- Stage 26: factor normal reader render dependency construction into the app loop helper layer.
- Stage 27: factor shelf scene render context construction into the app loop helper layer.
- Stage 28: factor menu scene render context construction into the app loop helper layer.
- Stage 29: factor volume overlay and status bar render dependency construction into the app loop helper layer.
- Stage 30: factor boot scene render dependency construction into the app loop helper layer.
- Stage 31: factor reader scene input dependency construction into the app loop helper layer.
- Stage 32: factor boot scene tick dependency construction into the app loop helper layer.
- Stage 33: factor shelf scene input context construction into the app loop helper layer.
- Stage 34: factor RGDS bottom render dependency construction into the app loop helper layer.
- Stage 35: factor reader progress controller dependency construction into a stable app loop helper.
- Stage 36: factor TXT session facade dependency construction into the app loop helper layer.
- Stage 37: factor shelf reader launch handler dependency construction into the app loop helper layer.
- Stage 38: factor online shelf action dependency construction into the app loop helper layer.
- Stage 39: cache online shelf render-status dependency construction per shelf render service build.
- Stage 40: add final architecture notes and update H700, Trimui Brick, and RGDS packaging README notes.

## Protected Entry Points

- Windows smoke: `scripts/smoke_windows.ps1`
- H700 low-glibc package: `H700/build_h700_low_glibc_docker.ps1`
- Trimui Brick package: `TrimuiBrick/build_low_glibc_docker.ps1`
- RGDS official package: `RGDS/build_rgds_official.ps1`

## Guardrails

- Keep user-visible behavior unchanged during these stages.
- Avoid changing ZIP image reader internals while the ZIP module is still stabilizing.
- Prefer moving existing logic into new modules before redesigning interfaces.
- Keep each stage buildable and easy to revert.
- Do not remove legacy online-source compatibility paths during cleanup.
- Do not replace a device-local `online_sources.ini` during packaging or update flows.
- Do not reopen the RGDS dual-window display route; preserve the Weston spanning route.
- Do not change key mappings, reader gestures, progress file semantics, or UI behavior while moving code.

## Cleanup Module Map

- App layer: process bootstrap, SDL lifetime, main loop, scene routing, cross-module service wiring.
- Platform layer: H700/Trimui/RGDS capabilities, input profile, system controls, RGDS layout/render helpers.
- Shelf layer: scanning, local library assembly, cover lookup/cache, shelf input/render runtime.
- Reader layer: reader module registry, launch/session lifecycle, per-format PDF/EPUB/TXT/ZIP runtimes.
- Online layer: transport, provider parsing, online state, online shelf bridge/controller, download jobs.
- UI/cache layer: assets, text textures, cover textures, status overlays, volume overlays.

See `ARCHITECTURE.md` for the maintained module-boundary notes and protected behavior list.

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
13. Connect an online source and confirm the online shelf renders.
14. Load online covers and confirm retry throttling still behaves normally.
15. Download an online item and confirm the local item can be opened.
16. Confirm H700 packaging still mirrors release zips to the legacy root `Downloads/`.
17. Confirm Trimui Brick packaging preserves device-local `online_sources.ini`.
18. Confirm RGDS uses the official Weston spanning path, not the historical dual-window route.

## Verification Notes

- Stage 3 smoke passed on Windows after thinning `main.cpp` into `RunApp()`.
- Stage 4 smoke passed on Windows after grouping reader session dependencies.
- Stage 5 smoke passed on Windows after extracting display bootstrap into `app_bootstrap`.
- Stage 6 smoke passed on Windows after moving SDL input device lifecycle into `app_services`.
- Stage 7 smoke passed on Windows after moving runtime file lookup and local storage/cache path setup into `app_services`.
- Stage 8 smoke passed on Windows after moving `AppContext` assembly into `app_composition`.
- Stage 9 smoke passed on Windows after moving H700/Trimui/RGDS input profile selection into `app_services`.
- Stage 10 smoke passed on Windows after moving config/progress/favorites/history store initialization into `app_services`.
- Stage 11 smoke passed on Windows after moving system service object creation into `app_services`.
- Stage 12 smoke passed on Windows after moving system/TXT settings state initialization into `app_services`.
- Stage 13 smoke passed on Windows after moving app UI initial volume setup into `app_runtime`.
- Stage 14 smoke passed on Windows after moving contributor avatar initial focus setup into `contributor_avatar_runtime`.
- Stage 15 smoke passed on Windows after moving version update state initialization and current-version probing into `version_update_runtime`.
- Stage 16 smoke passed on Windows after moving online shelf default `online_sources.ini` and `Downloads` path resolution into `online_shelf_controller`.
- Stage 17 smoke passed on Windows after moving lid-close screen-off startup sync into `app_services`.
- Stage 18 smoke passed on Windows after moving RGDS startup shelf/menu state sync into `app_services`.
- Stage 19 smoke passed on Windows after factoring RGDS menu open and reader-state sync helpers into `app_services`.
- Stage 20 smoke passed on Windows after factoring online shelf input tick result application into `app_services`.
- Stage 21 smoke passed on Windows after factoring menu scene input pre-sync into `app_services`.
- Stage 22 smoke passed on Windows after factoring online shelf present tick result application into `app_services`.
- Stage 23 smoke passed on Windows after factoring online shelf deferred-disconnect root refresh into `app_services`.
- Stage 24 smoke passed on Windows after factoring online shelf deferred-connect result handling into `app_services`.
- Stage 25 smoke passed on Windows after factoring menu scene input context construction into `app_services`.
- Stage 26 smoke passed on Windows after factoring normal reader render dependency construction into the app loop helper layer.
- Stage 27 smoke passed on Windows after factoring shelf scene render context construction into the app loop helper layer.
- Stage 28 smoke passed on Windows after factoring menu scene render context construction into the app loop helper layer.
- Stage 29 smoke passed on Windows after factoring volume overlay and status bar render dependency construction into the app loop helper layer.
- Stage 30 smoke passed on Windows after factoring boot scene render dependency construction into the app loop helper layer.
- Stage 31 smoke passed on Windows after factoring reader scene input dependency construction into the app loop helper layer.
- Stage 32 smoke passed on Windows after factoring boot scene tick dependency construction into the app loop helper layer.
- Stage 33 smoke passed on Windows after factoring shelf scene input context construction into the app loop helper layer.
- Stage 34 smoke passed on Windows after factoring RGDS bottom render dependency construction into the app loop helper layer.
- Stage 35 smoke passed on Windows after factoring reader progress controller dependency construction into a stable app loop helper.
- Stage 36 smoke passed on Windows after factoring TXT session facade dependency construction into the app loop helper layer.
- Stage 37 smoke passed on Windows after factoring shelf reader launch handler dependency construction into the app loop helper layer.
- Stage 38 smoke passed on Windows after factoring online shelf action dependency construction into the app loop helper layer.
- Stage 39 smoke passed on Windows after caching online shelf render-status dependency construction per shelf render service build.
- Stage 40 documentation-only cleanup updated `ARCHITECTURE.md`, `README.md`, `H700/README.md`, `TrimuiBrick/README.md`, and `RGDS/README.md`.
- Reader session behavior, legacy text fallback, progress persistence, online shelf actions, packaging entry points, and reader close paths were not changed in this stage.

## Completion Status

- Main architecture cleanup is complete for this pass.
- Remaining validation is device-side packaging and hands-on testing for H700,
  Trimui Brick, and RGDS.
- Future performance work should be handled as measured, isolated follow-up
  changes with before/after smoke or device evidence.
