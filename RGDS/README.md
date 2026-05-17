# ROCreader RGDS Platform Branch

RGDS is a dedicated RK3568 dual-screen target, not an H700 layout variant.

Hardware facts verified by `roc_matrix_probe.sh`:

- SoC: Rockchip RK3568, 4x Cortex-A55, aarch64.
- GPU path: Mali/OpenGL ES is present. The preferred RGDS route is SDL2 on
  Wayland with Weston kept alive; direct DRM remains a diagnostic fallback.
- Memory: about 3 GB.
- OS: Buildroot 2024.02, Linux 6.1, glibc 2.41.
- Display: two DRM/KMS displays exposed through SDL:
  - display 0: `640x480@60`, bounds `0,0 640x480`
  - display 1: `640x480@60`, bounds `640,0 640x480`
- Weston owns `/dev/dri/card0` by default. The SDL2 route should not stop
  Weston; it should create RGDS windows through Wayland instead.
- Direct DRM/KMS dumb-buffer output is verified stable when Weston is stopped:
  - DSI-1: connector `177`, crtc `130`, `640x480@60`
  - DSI-2: connector `179`, crtc `90`, `640x480@60`

## Target UX

- Boot:
  - Top screen shows loading/progress.
  - Bottom screen shows a matching solid background.
- Shelf/Menu:
  - Top screen shows the 640x480 shelf.
  - Bottom screen shows the 640x480 menu/settings surface.
  - `Select` switches focus between top and bottom.
  - Focus change shows a short full-screen light-blue focus frame.
- Reader:
  - Reader modules render into a virtual `640x960` vertical canvas.
  - Top screen presents the upper half.
  - Bottom screen presents the lower half.
  - Rotation and zoom are disabled for RGDS reader mode.
  - `Menu` opens the reader menu on the bottom screen and locks focus there.
  - Progress and chapter overlays appear on the bottom screen only, and only outside the reader menu.

## Current Contents

- `build_rgds_dualscreen_probe.ps1`: builds the precompiled aarch64 SDL/KMSDRM dual-screen probe.
- `build_rgds_drm_probe.ps1`: builds the direct DRM/KMS dumb-buffer dual-screen probe.
- `rgds_drm_dualscreen_probe.sh`: click-friendly launcher for the direct DRM/KMS probe; writes `rgds_drm_dualscreen_probe_latest.log`.
- `rgds_display_owner_probe.sh`: click-friendly process/FD probe to find system components repainting the display.
- `rgds_stop_weston_drm_probe.sh`: stops Weston, runs the direct DRM probe, then attempts to restore Weston.
- `build_rgds_reader.ps1`: builds the first RGDS dedicated reader shell using DRM/KMS display and evdev input.
- `rgds_reader.sh`: click-friendly RGDS reader launcher. It stops Weston, runs `.rgds_reader_files/rgds_reader_app`, then attempts to restore Weston.
- `build_rgds_sdl_reader.ps1`: builds the SDL2/Wayland reader route test that
  runs on top of Weston.
- `rgds_sdl_reader.sh`: click-friendly SDL2 route launcher for comparing against the direct DRM renderer.
- `rgds_platform_demo.sh`: click-friendly launcher for the RGDS prototype; writes `rgds_platform_demo_latest.log`.
- `src/rgds_drm_runtime.*`: RGDS direct DRM/KMS display runtime.
- `src/rgds_evdev_input.*`: RGDS evdev input runtime.
- `src/rgds_reader_app.cpp`: first dedicated RGDS reader shell.
- `src/rgds_dual_screen_runtime.*`: RGDS-specific SDL dual-screen runtime abstraction.
- `src/rgds_platform_demo.cpp`: standalone RGDS dual-screen platform prototype.

## Current RGDS Runtime Direction

The raw DRM route proves that both DSI panels can be driven independently, but
the current dumb-buffer renderer flickers too visibly for a final reader. The
preferred route is now:

1. Keep Weston running.
2. Launch SDL2 with `SDL_VIDEODRIVER=wayland`.
3. Let Weston own page flips and composition.
4. Use either two 640x480 SDL windows or one compositor-spanning surface if
   Weston exposes the two panels as one usable desktop area.
5. Keep the RGDS reader model as a virtual 640x960 canvas split into top and
   bottom 640x480 surfaces.
6. Read keys through the collected RGDS mapping.

Build the current RGDS reader shell with:

```powershell
powershell -ExecutionPolicy Bypass -File RGDS\build_rgds_reader.ps1
```

Run `rgds_reader.sh` on the device.

## Build Probe

```powershell
powershell -ExecutionPolicy Bypass -File RGDS\build_rgds_dualscreen_probe.ps1
```

This copies `rgds_sdl_dualscreen_probe` to `E:\Roms\APPS` when the SD card is mounted there.

## Build Platform Demo

The platform demo is intentionally separate from the main reader while the RGDS architecture is being proven.

```powershell
powershell -ExecutionPolicy Bypass -File RGDS\build_rgds_platform_demo.ps1
```

Copy/run `rgds_platform_demo` on RGDS. It demonstrates:

- two SDL fullscreen windows,
- top/bottom focus switching with `Select`,
- top shelf mock surface and bottom menu mock surface,
- reader mode using a `640x960` target texture split across both displays.

When launching from the device UI, run `rgds_platform_demo.sh`. The build script
stores the raw binary in `.rgds_platform_demo_files/` so the launcher is the only
root-level entry the device UI should show.
