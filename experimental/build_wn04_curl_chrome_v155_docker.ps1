param(
    [string]$ImageName = "rocreader-wn04-curl-chrome",
    [switch]$NoBuildImage
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectRoot = Split-Path -Parent $ScriptDir
$LogsDir = Join-Path $ProjectRoot "H700\logs"
$OutDir = Join-Path $ScriptDir "wn04_transport_v155_out"
$Dockerfile = Join-Path $ScriptDir "Dockerfile.wn04-curl-chrome"

New-Item -ItemType Directory -Force -Path $LogsDir, $OutDir | Out-Null

docker version | Out-Host
if ($LASTEXITCODE -ne 0) {
    throw "Docker is not available."
}

if (-not $NoBuildImage) {
    docker build `
        -f $Dockerfile `
        --build-arg "BASE_IMAGE=ubuntu:22.04" `
        --build-arg "APT_MIRROR=http://mirrors.aliyun.com/ubuntu" `
        -t $ImageName `
        $ScriptDir
    if ($LASTEXITCODE -ne 0) {
        throw "Docker image build failed for $ImageName."
    }
}

$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$dockerLog = Join-Path $LogsDir "docker_wn04_curl_chrome_v155_$timestamp.log"
$projectMount = ($ProjectRoot -replace "\\", "/")
$dockerCmd = "docker run --rm " +
    "-e SYSROOT=/work/H700/sysroot_device " +
    "-e OUT_DIR=/work/experimental/wn04_transport_v155_out " +
    "-v `"${projectMount}:/work`" " +
    "-w /work " +
    "$ImageName " +
    "bash ./experimental/build_wn04_curl_chrome_v155_in_docker.sh"

cmd.exe /D /S /C "$dockerCmd 2>&1" | Tee-Object -FilePath $dockerLog
$runExitCode = $LASTEXITCODE
if ($runExitCode -ne 0) {
    throw "WN04 curl_chrome v1.5.5 Docker build failed."
}

Write-Host ""
Write-Host "WN04 curl_chrome v1.5.5 outputs:"
Write-Host "  Out:       $OutDir"
Write-Host "  DockerLog: $dockerLog"
