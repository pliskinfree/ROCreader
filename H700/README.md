# H700 packaging

This folder owns the H700 packaging workspace:

- `toolchain/`: H700 Docker build toolchain.
- `sysroot_device/`: device sysroot used by low-glibc builds.
- `dist_lowglibc/`: staging output, tarballs, and intermediate package files.
- `Downloads/`: final H700 release zip files for new builds.
- `logs/`: H700 build logs.

The repository-root `Downloads/` directory is intentionally kept as a legacy
online-update mirror for older H700 releases that still check that path.
H700 packaging copies each newly built release zip there automatically.

Packaging note:
- Online upgrade extraction must not replace `online_sources.ini`.
- Treat `online_sources.ini` as user/device configuration, not packaged content.
