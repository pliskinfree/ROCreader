param(
    [string]$ImageName = "rocreader-h700-low-glibc",
    [string]$BaseImage = "ubuntu:22.04",
    [string]$AptMirror = "http://mirrors.aliyun.com/ubuntu",
    [ValidateSet("0", "1")]
    [string]$RequireMupdf = "1",
    [switch]$NoBuildImage
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptDir
$H700Root = $ScriptDir
$ToolchainDir = Join-Path $H700Root "toolchain"
$LogsDir = Join-Path $H700Root "logs"
$DistDir = Join-Path $H700Root "dist_lowglibc"
$DownloadsDir = Join-Path $H700Root "Downloads"
$SysrootDir = Join-Path $H700Root "sysroot_device"

if (-not (Test-Path (Join-Path $ToolchainDir "Dockerfile"))) {
    throw "H700 Dockerfile not found: $ToolchainDir"
}

New-Item -ItemType Directory -Force -Path $LogsDir, $DistDir, $DownloadsDir | Out-Null

docker version | Out-Host
if ($LASTEXITCODE -ne 0) {
    throw "Docker is not available. Start Docker Desktop before building H700 packages."
}

if (-not $NoBuildImage) {
    docker build `
        --build-arg "BASE_IMAGE=$BaseImage" `
        --build-arg "APT_MIRROR=$AptMirror" `
        -t $ImageName `
        $ToolchainDir
    if ($LASTEXITCODE -ne 0) {
        throw "Docker image build failed for $ImageName."
    }
}

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$dockerLog = Join-Path $LogsDir "docker_h700_low_glibc_$timestamp.log"
$projectMount = ($ProjectRoot -replace "\\", "/")

docker run --rm `
    -e "TRIMUI_BRICK_LAYOUT=0" `
    -e "REQUIRE_MUPDF=$RequireMupdf" `
    -e "H700_ROOT=/work/H700" `
    -e "SYSROOT=/work/H700/sysroot_device" `
    -e "ROC_NATIVE_LOG_DIR=/work/H700/logs" `
    -e "DIST_ROOT=/work/H700/dist_lowglibc" `
    -e "DOWNLOADS_ROOT=/work/H700/Downloads" `
    -v "${projectMount}:/work" `
    -w /work `
    $ImageName `
    bash ./cross_compile_low_glibc.sh 2>&1 | Tee-Object -FilePath $dockerLog
if ($LASTEXITCODE -ne 0) {
    throw "H700 Docker package build failed."
}

Write-Host ""
Write-Host "H700 package outputs:"
Write-Host "  Downloads: $DownloadsDir"
Write-Host "  Dist:      $DistDir"
Write-Host "  Logs:      $LogsDir"
Write-Host "  DockerLog: $dockerLog"
