param(
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path,
    [string]$OutputDir = (Join-Path $PSScriptRoot "dist"),
    [string]$SdAppsDir = "E:\Roms\APPS"
)

$ErrorActionPreference = "Stop"

$OutDirAbs = [System.IO.Path]::GetFullPath($OutputDir)
$Sysroot = Join-Path $RepoRoot "H700\sysroot_device"
$ToolchainDir = Join-Path $PSScriptRoot "toolchain"
$Dockerfile = Join-Path $ToolchainDir "Dockerfile.probe"

if (-not (Test-Path (Join-Path $Sysroot "usr\include\xf86drm.h"))) {
    throw "libdrm headers not found in sysroot: $Sysroot"
}
if (-not (Test-Path $Dockerfile)) {
    throw "Dockerfile missing. Run build_rgds_dualscreen_probe.ps1 once first: $Dockerfile"
}

New-Item -ItemType Directory -Force -Path $OutDirAbs | Out-Null

$image = "rocreader-rgds-probe:latest"
docker build -t $image -f $Dockerfile $ToolchainDir

$repoDocker = ($RepoRoot -replace "\\", "/")
$outDocker = ($OutDirAbs -replace "\\", "/")
$cmd = @"
set -eux
mkdir -p /out
aarch64-linux-gnu-g++ -O2 -std=c++17 -Wall -Wextra \
  --sysroot=/work/H700/sysroot_device \
  -I/work/H700/sysroot_device/usr/include \
  -I/work/H700/sysroot_device/usr/include/libdrm \
  -I/work/RGDS/src \
  /work/RGDS/src/rgds_reader_app.cpp \
  /work/RGDS/src/rgds_drm_runtime.cpp \
  /work/RGDS/src/rgds_evdev_input.cpp \
  -o /out/rgds_reader_app \
  -L/work/H700/sysroot_device/usr/lib/aarch64-linux-gnu \
  -ldrm
file /out/rgds_reader_app
aarch64-linux-gnu-readelf -d /out/rgds_reader_app | sed -n '1,120p'
"@

docker run --rm `
    -v "${repoDocker}:/work" `
    -v "${outDocker}:/out" `
    $image `
    bash -lc $cmd

$Built = Join-Path $OutDirAbs "rgds_reader_app"
if (-not (Test-Path $Built)) {
    throw "Build did not produce: $Built"
}

if (Test-Path $SdAppsDir) {
    $DevicePayloadDir = Join-Path $SdAppsDir ".rgds_reader_files"
    New-Item -ItemType Directory -Force -Path $DevicePayloadDir | Out-Null
    Copy-Item -LiteralPath $Built -Destination (Join-Path $DevicePayloadDir "rgds_reader_app") -Force
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot "rgds_reader.sh") -Destination (Join-Path $SdAppsDir "rgds_reader.sh") -Force
    Write-Host "Copied RGDS reader app to $SdAppsDir"
} else {
    Write-Host "SD apps dir not found, built reader left at $Built"
}

Write-Host "RGDS reader app built: $Built"
