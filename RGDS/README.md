# ROCreader RGDS Platform Branch

RGDS is a dedicated RK3568 dual-screen target, not an H700 layout variant.

Hardware facts verified by `roc_matrix_probe.sh`:

- SoC: Rockchip RK3568, 4x Cortex-A55, aarch64.
- GPU path: Mali/OpenGL ES through SDL KMSDRM.
- Memory: about 3 GB.
- OS: Buildroot 2024.02, Linux 6.1, glibc 2.41.
- Display: two DRM/KMS displays exposed through SDL:
  - display 0: `640x480@60`, bounds `0,0 640x480`
  - display 1: `640x480@60`, bounds `640,0 640x480`
- SDL renderer: `opengles2`, accelerated, vsync, target textures supported on both screens.

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
- `rgds_platform_demo.sh`: click-friendly launcher for the RGDS prototype; writes `rgds_platform_demo_latest.log`.
- `src/rgds_dual_screen_runtime.*`: RGDS-specific SDL dual-screen runtime abstraction.
- `src/rgds_platform_demo.cpp`: standalone RGDS dual-screen platform prototype.

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
