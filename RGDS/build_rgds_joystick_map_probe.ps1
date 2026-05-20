param(
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path,
    [string]$OutputDir = (Join-Path $PSScriptRoot "dist"),
    [string]$SdAppsDir = "E:\Roms\APPS"
)

$ErrorActionPreference = "Stop"

$OutDirAbs = [System.IO.Path]::GetFullPath($OutputDir)
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
  /work/RGDS/src/rgds_joystick_map_probe.cpp \
  /work/RGDS/src/rgds_drm_runtime.cpp \
  -o /out/rgds_joystick_map_probe \
  -L/work/H700/sysroot_device/usr/lib/aarch64-linux-gnu \
  -ldrm
file /out/rgds_joystick_map_probe
"@

docker run --rm `
    -v "${repoDocker}:/work" `
    -v "${outDocker}:/out" `
    $image `
    bash -lc $cmd

$Built = Join-Path $OutDirAbs "rgds_joystick_map_probe"
if (-not (Test-Path $Built)) {
    throw "Build did not produce: $Built"
}

if (Test-Path $SdAppsDir) {
    $DevicePayloadDir = Join-Path $SdAppsDir ".rgds_joystick_map_probe_files"
    New-Item -ItemType Directory -Force -Path $DevicePayloadDir | Out-Null
    Copy-Item -LiteralPath $Built -Destination (Join-Path $DevicePayloadDir "rgds_joystick_map_probe") -Force
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot "rgds_joystick_map_probe.sh") -Destination (Join-Path $SdAppsDir "rgds_joystick_map_probe.sh") -Force
    Write-Host "Copied RGDS joystick map probe to $SdAppsDir"
}

Write-Host "RGDS joystick map probe built: $Built"
