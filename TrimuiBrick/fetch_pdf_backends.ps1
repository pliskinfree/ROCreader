param(
    [string]$ImageName = "ubuntu:22.04"
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$DebDir = Join-Path $ScriptDir "deb_cache"
$OverlayDir = Join-Path $ScriptDir "sysroot_overlay"
$FetchScript = Join-Path $ScriptDir "fetch_pdf_backends.sh"

New-Item -ItemType Directory -Force -Path $DebDir, $OverlayDir | Out-Null

$debMount = ($DebDir -replace "\\", "/")
$overlayMount = ($OverlayDir -replace "\\", "/")
$scriptMount = ($FetchScript -replace "\\", "/")

docker run --rm `
    -v "${debMount}:/out/debs" `
    -v "${overlayMount}:/out/overlay" `
    -v "${scriptMount}:/tmp/fetch_pdf_backends.sh:ro" `
    $ImageName `
    bash /tmp/fetch_pdf_backends.sh

if ($LASTEXITCODE -ne 0) {
    throw "Failed to fetch Trimui Brick PDF backend packages."
}

Write-Host "PDF backend sysroot overlay:"
Write-Host "  $OverlayDir"
