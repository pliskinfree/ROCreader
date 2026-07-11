param(
  [string]$Distro = "Ubuntu",
  [int]$RequireMupdf = 1
)

$ErrorActionPreference = "Stop"
$scriptRoot = (Convert-Path $PSScriptRoot) -replace '\\', '/'
$wslDir = (wsl -d $Distro -- wslpath -a "$scriptRoot").Trim()
$cmd = "cd '$wslDir' && REQUIRE_MUPDF='$RequireMupdf' ./build_low_glibc.sh"
wsl -d $Distro -- bash -lc $cmd
