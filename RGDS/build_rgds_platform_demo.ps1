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

if (-not (Test-Path (Join-Path $Sysroot "usr\include\SDL2\SDL.h"))) {
    throw "SDL2 headers not found in sysroot: $Sysroot"
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
  -I/work/H700/sysroot_device/usr/include/SDL2 \
  -I/work/RGDS/src \
  -D_REENTRANT \
  /work/RGDS/src/rgds_platform_demo.cpp \
  /work/RGDS/src/rgds_dual_screen_runtime.cpp \
  -o /out/rgds_platform_demo \
  -L/work/H700/sysroot_device/usr/lib/aarch64-linux-gnu \
  -lSDL2 -ldl -lpthread
file /out/rgds_platform_demo
aarch64-linux-gnu-readelf -d /out/rgds_platform_demo | sed -n '1,120p'
"@

docker run --rm `
    -v "${repoDocker}:/work" `
    -v "${outDocker}:/out" `
    $image `
    bash -lc $cmd

$Built = Join-Path $OutDirAbs "rgds_platform_demo"
if (-not (Test-Path $Built)) {
    throw "Build did not produce: $Built"
}

if (Test-Path $SdAppsDir) {
    $DevicePayloadDir = Join-Path $SdAppsDir ".rgds_platform_demo_files"
    New-Item -ItemType Directory -Force -Path $DevicePayloadDir | Out-Null
    Copy-Item -LiteralPath $Built -Destination (Join-Path $DevicePayloadDir "rgds_platform_demo") -Force
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot "rgds_platform_demo.sh") -Destination (Join-Path $SdAppsDir "rgds_platform_demo.sh") -Force
    $LegacyRootBinary = Join-Path $SdAppsDir "rgds_platform_demo"
    if (Test-Path $LegacyRootBinary) {
        Move-Item -LiteralPath $LegacyRootBinary -Destination (Join-Path $DevicePayloadDir "rgds_platform_demo.previous") -Force
    }
    Write-Host "Copied RGDS platform demo to $SdAppsDir"
} else {
    Write-Host "SD apps dir not found, built demo left at $Built"
}

Write-Host "RGDS platform demo built: $Built"
