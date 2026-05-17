# ROCreader

ROCreader is a native SDL2/C++ reader for handheld Linux devices such as
H700-class systems and Trimui Brick. It focuses on local reading, fast shelf
navigation, device-friendly controls, and optional connections to user-provided
legal online catalogs.

This project does not provide, host, index, recommend, or bundle copyrighted
books, comics, manga, novels, third-party source lists, illegal download links,
or any content source that the user is not authorized to access.

## Legal and Content Policy

ROCreader is a general-purpose document and comic reader. It is intended for:

- Local files owned by the user or used with permission.
- Public-domain, open-license, or otherwise lawfully obtained works.
- Private or authorized OPDS/Kavita catalogs configured by the user.
- Personal libraries stored on the user's own SD card or device.

The official release package ships only a template `online_sources.ini`.
Maintainers must not include real third-party content sources, site-specific
illegal source lists, infringing download links, or copyrighted works in release
packages, README examples, screenshots, Issues, Discussions, Wiki pages, or
other official project materials.

Users are responsible for making sure every source they configure and every file
they read or download is lawful in their jurisdiction. The project maintainers
do not control user-configured sources and do not authorize using ROCreader to
access, copy, distribute, or download infringing content.

If you believe official project material contains infringing or otherwise
illegal content, please open an issue or contact the maintainer with enough
detail to identify the material. Valid reports will be reviewed and the relevant
official links, files, or references will be removed or disabled where
appropriate.

See [LEGAL.md](LEGAL.md) for the full legal disclaimer and contributor rules.

## Features

- Native SDL2 fullscreen reader optimized for low-power handheld devices.
- Bookshelf scanning from `books/` directories on supported storage roots.
- Cover scanning from sibling `book_covers/` directories.
- ZIP/CBZ comic reading with zoom, rotation, scrolling, page persistence, and
  cover caching.
- PDF reading when a real PDF backend such as MuPDF/Fitz or Poppler is present.
- EPUB reading, including flow/comic-oriented reader paths.
- TXT reading with encoding detection and UTF-8 conversion support.
- Reader progress, favorites, history, and settings persistence.
- Optional `SDL2_ttf` title rendering with focused-title marquee.
- Optional `SDL2_mixer` key sound effects.
- Device/system volume integration with fallback app sound volume.
- URL Entry panel for user-configured lawful OPDS/Kavita catalogs.
- Optional `manual_web` development interface for private, lawful site adapters;
  official releases must keep this unconfigured and source-free.
- Online downloads are stored under the app's `Downloads/` cache and can be
  moved into the local library only after user action.

## Supported Content

Local library files are expected under:

```text
ROCreader/books/
ROCreader/book_covers/
```

Supported reader paths include:

- `.zip` / `.cbz` image archives
- `.pdf` with a real PDF backend
- `.epub`
- `.txt`

Manual cover files should use the same base name as the book file when possible
and live under `book_covers/`.

## URL Entry

The URL Entry feature is an online shelf interface. It reads configuration from
`online_sources.ini` in the runtime directory, or from the path specified by
`ROCREADER_ONLINE_SOURCES`.

Official release packages must ship a template-only configuration:

```ini
[source.template]
name=Template only - copy this block and fill a real source
type=opds
url=
visible=0
enabled=0
category.0.name=All
category.0.url=
```

Supported source types:

- `opds`: standard OPDS-compatible feeds.
- `kavita`: Kavita OPDS-compatible feeds.
- `manual_web`: development hook for user-owned or otherwise lawful custom
  catalog adapters.

No official release should include real third-party source entries. Do not use
project infrastructure to share source lists for copyrighted works unless you
own the rights or have explicit permission.

## Install Layout

Typical device package layout:

```text
Roms/APPS/ROCreader.sh
Roms/APPS/ROCreader/
  rocreader_sdl
  online_sources.ini
  native_config.ini
  native_keymap.ini
  fonts/
  sounds/
  lib/
  lib_system_sdl/
  books/
  book_covers/
  cache/
  Downloads/
```

Online updates must preserve the user's local `online_sources.ini`. Treat it as
device/user configuration, not packaged content.

## Build on Device

```sh
cd /Roms/APPS/ROCreader
chmod +x build_and_run.sh
./build_and_run.sh
```

The build defaults to requiring a real PDF backend:

```sh
REQUIRE_MUPDF=1 ./build_and_run.sh
```

Only use mock PDF rendering for explicit development tests:

```sh
REQUIRE_MUPDF=0 ./build_and_run.sh
```

## Preflight Check

```sh
cd /Roms/APPS/ROCreader
chmod +x preflight_check.sh
./preflight_check.sh
```

Cross preflight example:

```sh
cd /path/to/ROCreader
PRECHECK_MODE=cross CROSS_TOOL_PREFIX=arm-linux-gnueabihf ./preflight_check.sh
```

## Cross Compile and Package

```sh
cd /path/to/ROCreader
chmod +x cross_compile_and_package.sh
CROSS_TOOL_PREFIX=arm-linux-gnueabihf REQUIRE_MUPDF=1 ./cross_compile_and_package.sh
```

Typical outputs:

```text
dist_h700/APPS/ROCreader.sh
dist_h700/APPS/ROCreader/rocreader_sdl
dist_h700/ROCreader_APPS.tar.gz
```

Copy the generated `APPS` contents to the SD card's `/Roms/APPS` directory.

## Low-GLIBC H700 Build

When the device reports errors such as `GLIBC_2.38 not found`, build against a
sysroot synced from the target device:

```sh
cd /path/to/ROCreader
chmod +x sync_device_sysroot.sh cross_compile_low_glibc.sh
DEVICE_HOST=root@192.168.31.141 ./sync_device_sysroot.sh
SYSROOT=/path/to/ROCreader/H700/sysroot_device \
CROSS_TOOL_PREFIX=aarch64-linux-gnu \
REQUIRE_MUPDF=1 \
./cross_compile_low_glibc.sh
```

Release outputs are written under:

```text
H700/dist_lowglibc/
H700/Downloads/
```

The root `Downloads/` directory is retained only as a legacy online-update
mirror for older H700 builds.

## Trimui Brick Build

Trimui Brick packaging is maintained under `TrimuiBrick/`.

```powershell
.\TrimuiBrick\build_low_glibc_docker.ps1
```

See [TrimuiBrick/README.md](TrimuiBrick/README.md) for the dedicated build
workflow and PDF backend notes.

## Desktop Testing

Window mode on desktop builds:

```sh
ROCREADER_WINDOWED=1 ./build/rocreader_sdl
```

Fullscreen mode:

```sh
ROCREADER_FULLSCREEN=1 ./build/rocreader_sdl
```

## Controls

Shelf:

- D-pad: move focus.
- A: enter folder or open book.
- B: back to root shelf when inside a folder.
- Menu: open sidebar settings.

Reader:

- D-pad short press: page turn, rotation-aware.
- D-pad long press: smooth scroll, rotation-aware.
- L1/R1: zoom out/in.
- L2/R2: rotate -90/+90.
- A: reset to fit current orientation.
- B: exit reader and save progress.
- Menu: open sidebar settings.

Global:

- Start + Select: exit app.
- Volume +/-: adjust volume.

## Runtime Logs

The launcher writes diagnostics to:

```text
/Roms/APPS/ROCreader.log
```

Useful signs:

- `not found` in `ldd` output usually means a missing shared library.
- All video drivers failing usually points to an SDL video backend mismatch.
- URL Entry failures are logged with `online:` prefixes when runtime logging is
  enabled.

## Contributing

Contributions are welcome when they improve the reader, device support,
performance, accessibility, build reliability, or lawful source compatibility.

Do not submit:

- Copyrighted books, comics, manga, novels, fonts, music, or artwork without
  permission.
- Third-party source lists for unauthorized content.
- Site-specific adapters whose purpose is to bypass access controls or download
  infringing works.
- Screenshots or documentation that promote illegal content access.

Pull requests that add online-source functionality should keep configuration
template-only and document lawful use cases such as private OPDS/Kavita
libraries, public-domain catalogs, or authorized institutional catalogs.

## License

Add or update the project license before distributing binaries broadly. Any
third-party libraries, fonts, sounds, images, and bundled assets must be used in
compliance with their own licenses.
