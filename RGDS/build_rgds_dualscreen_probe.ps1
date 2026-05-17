param(
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path,
    [string]$OutputDir = (Join-Path $PSScriptRoot "dist"),
    [string]$SdAppsDir = "E:\Roms\APPS"
)

$ErrorActionPreference = "Stop"

$ToolchainDir = Join-Path $PSScriptRoot "toolchain"
$Dockerfile = Join-Path $ToolchainDir "Dockerfile.probe"
$OutDirAbs = [System.IO.Path]::GetFullPath($OutputDir)
$Sysroot = Join-Path $RepoRoot "H700\sysroot_device"
$Source = Join-Path $RepoRoot "tools\rgds_sdl_dualscreen_probe.cpp"

if (-not (Test-Path $Source)) {
    throw "Probe source not found: $Source"
}
if (-not (Test-Path (Join-Path $Sysroot "usr\include\SDL2\SDL.h"))) {
    throw "SDL2 headers not found in sysroot: $Sysroot"
}

New-Item -ItemType Directory -Force -Path $ToolchainDir | Out-Null
New-Item -ItemType Directory -Force -Path $OutDirAbs | Out-Null

@'
FROM ubuntu:22.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get -o Acquire::Retries=5 update \
 && apt-get -o Acquire::Retries=5 install -y --no-install-recommends \
      ca-certificates \
      file \
      g++-aarch64-linux-gnu \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /work
'@ | Set-Content -Encoding ASCII -Path $Dockerfile

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
  -D_REENTRANT \
  /work/tools/rgds_sdl_dualscreen_probe.cpp \
  -o /out/rgds_sdl_dualscreen_probe \
  -L/work/H700/sysroot_device/usr/lib/aarch64-linux-gnu \
  -lSDL2 -ldl -lpthread
file /out/rgds_sdl_dualscreen_probe
aarch64-linux-gnu-readelf -d /out/rgds_sdl_dualscreen_probe | sed -n '1,120p'
"@

docker run --rm `
    -v "${repoDocker}:/work" `
    -v "${outDocker}:/out" `
    $image `
    bash -lc $cmd

$Built = Join-Path $OutDirAbs "rgds_sdl_dualscreen_probe"
if (-not (Test-Path $Built)) {
    throw "Build did not produce: $Built"
}

if (Test-Path $SdAppsDir) {
    Copy-Item -LiteralPath $Built -Destination (Join-Path $SdAppsDir "rgds_sdl_dualscreen_probe") -Force
    Copy-Item -LiteralPath (Join-Path $RepoRoot "tools\rgds_sdl_dualscreen_probe.sh") -Destination (Join-Path $SdAppsDir "rgds_sdl_dualscreen_probe.sh") -Force
    Copy-Item -LiteralPath (Join-Path $RepoRoot "tools\roc_matrix_probe.sh") -Destination (Join-Path $SdAppsDir "roc_matrix_probe.sh") -Force
    Write-Host "Copied probe to $SdAppsDir"
} else {
    Write-Host "SD apps dir not found, built probe left at $Built"
}

Write-Host "RGDS dual-screen probe built: $Built"
