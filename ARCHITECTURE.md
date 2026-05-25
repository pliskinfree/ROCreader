# ROCreader Architecture Notes

This document records the post-cleanup module boundaries. It is intentionally
practical: keep these boundaries stable when adding features.

## App Layer

- `main.cpp` is the thin process entry. It delegates to `RunApp()`.
- `app_bootstrap.*` owns process, SDL, font/image/audio bootstrap, window and
  renderer setup, and shutdown-adjacent runtime setup.
- `app_composition.*` builds app-wide context objects from already-created SDL
  and platform resources.
- `app_services.*` owns cross-module glue helpers that apply app-level results,
  such as online shelf tick results, menu input pre-sync, storage paths, input
  devices, stores, and system-service startup state.
- `app_loop.*` remains the runtime coordinator. It should assemble dependencies
  and route scenes, not absorb new business logic.

## Platform Layer

- H700, Trimui Brick, and RGDS differences are kept behind screen profile,
  input profile, system controls, storage-path detection, and RGDS runtime
  helpers.
- RGDS official runtime stays on the Weston/Wayland spanning-window path: one
  borderless `1280x480` SDL surface, not the historical two-window route.
- Shelf, menu, and readers should consume layout/input capability results
  instead of branching on package target whenever possible.

## Shelf Layer

- `book_scanner.*` scans files and describes book items.
- `book_library_service.*` assembles local shelf categories and scan caches.
- `cover_service.*` resolves manual and generated covers.
- `cover_cache_runtime.*` owns cover texture cache lifetime.
- `shelf_runtime.*` owns shelf state, input movement, paging, focus, and draw
  primitives.
- `shelf_scene.*` bridges app-level services into shelf input/render calls.

## Reader Layer

- `IReaderModule` and `ReaderManager` remain the common reader entry model.
- `reader_launch_service.*` maps a `BookItem` to the correct reader path.
- `reader_session_ops.*` owns session open/close and progress persistence
  helpers.
- Format runtimes stay format-specific: PDF, EPUB, TXT, and ZIP/CBZ should not
  share hidden state just because helper code looks similar.

## Online Layer

- Transport, provider parsing, runtime state, and shelf controller stay separate.
- `online_source_transport.*` owns HTTP/download/helper transport details.
- OPDS/Kavita/manual-web parsing belongs in provider/runtime code, not shelf
  rendering code.
- `online_shelf_controller.*` bridges online catalog entries into shelf items,
  remote cover loading, downloads, and local-save state.
- `online_sources.ini` is user/device configuration. Packaging and update flows
  must preserve it.

## UI And Cache Layer

- `ui_assets*`, `ui_text_cache.*`, and `texture_registry.*` own shared UI assets,
  text texture reuse, texture-size lookup, and title ellipsis caches.
- Cache keys should include enough semantic inputs to avoid stale data: path,
  size/mtime where appropriate, target dimensions, and source/origin.
- Do not merge caches with different ownership or invalidation rules.

## Protected Behaviors

- Do not change user config formats or progress file semantics during cleanup.
- Do not change key mappings, reader gestures, shelf navigation, menu behavior,
  TXT appearance settings, or online source compatibility as part of refactors.
- Do not delete legacy H700 `Downloads/` mirror behavior.
- Do not replace device-local `online_sources.ini`.
- Do not re-open RGDS dual-window display for the official package.
- Keep PDF/EPUB/TXT/ZIP reader launch and progress restore behavior compatible
  with existing libraries.
