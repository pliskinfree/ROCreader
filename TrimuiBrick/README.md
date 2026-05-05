# Trimui Brick packaging

This folder is the dedicated packaging workspace for the A133P Trimui Brick
target at 1024x768.

Run from PowerShell on the host:

```powershell
.\TrimuiBrick\build_low_glibc_docker.ps1
```

If Docker cannot pull `debian:bullseye-slim` but a compatible local base image
already exists, pass it explicitly. For this project CMake is not required:

```powershell
.\TrimuiBrick\build_low_glibc_docker.ps1 -BaseImage ubuntu:22.04 -SkipCMake
```

The A133P sysroot in the current toolchain does not include MuPDF or Poppler.
Fetch compatible Ubuntu 16.04 arm64 PDF backend packages into a sysroot overlay:

```powershell
.\TrimuiBrick\fetch_pdf_backends.ps1
```

Then build with the PDF backend requirement enabled:

```powershell
.\TrimuiBrick\build_low_glibc_docker.ps1 -NoBuildImage -RequireMupdf 1
```

To produce a package without a PDF backend, run:

```powershell
.\TrimuiBrick\build_low_glibc_docker.ps1 -NoBuildImage -RequireMupdf 0
```

The script builds the local `TrimuiBrick/toolchain` Docker image, mirrors
the project into `TrimuiBrick/workspace/source`, and runs the low-glibc package
flow there. Build intermediates, logs, staged APPS output, tarballs, and final
zip files are kept under this `TrimuiBrick` folder.
