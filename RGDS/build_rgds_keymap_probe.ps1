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
  /work/RGDS/src/rgds_keymap_probe.cpp \
  /work/RGDS/src/rgds_drm_runtime.cpp \
  -o /out/rgds_keymap_probe \
  -L/work/H700/sysroot_device/usr/lib/aarch64-linux-gnu \
  -ldrm
file /out/rgds_keymap_probe
"@

docker run --rm `
    -v "${repoDocker}:/work" `
    -v "${outDocker}:/out" `
    $image `
    bash -lc $cmd

$Built = Join-Path $OutDirAbs "rgds_keymap_probe"
if (-not (Test-Path $Built)) {
    throw "Build did not produce: $Built"
}

if (Test-Path $SdAppsDir) {
    $DevicePayloadDir = Join-Path $SdAppsDir ".rgds_keymap_probe_files"
    New-Item -ItemType Directory -Force -Path $DevicePayloadDir | Out-Null
    Copy-Item -LiteralPath $Built -Destination (Join-Path $DevicePayloadDir "rgds_keymap_probe") -Force
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot "rgds_keymap_probe.sh") -Destination (Join-Path $SdAppsDir "rgds_keymap_probe.sh") -Force
    Write-Host "Copied RGDS keymap probe to $SdAppsDir"
}

Write-Host "RGDS keymap probe built: $Built"
