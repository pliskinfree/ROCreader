param(
  [int]$Port = 8765
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = Split-Path -Parent $scriptDir
$serverScript = Join-Path $scriptDir "local_opds_server.py"
$stdoutLog = Join-Path $scriptDir "local_opds_server.log"
$stderrLog = Join-Path $scriptDir "local_opds_server.log.err"

Get-CimInstance Win32_Process |
  Where-Object { $_.CommandLine -like "*local_opds_server.py*" } |
  ForEach-Object {
    try { Stop-Process -Id $_.ProcessId -Force -ErrorAction Stop } catch {}
  }

Start-Sleep -Milliseconds 500
Remove-Item -LiteralPath $stdoutLog,$stderrLog -Force -ErrorAction SilentlyContinue

$py = Get-Command py.exe -ErrorAction SilentlyContinue
if ($py) {
  $file = $py.Source
  $args = @("-X", "utf8", $serverScript)
} else {
  $python = Get-Command python.exe -ErrorAction SilentlyContinue
  if (-not $python) {
    throw "Python launcher not found. Install Python or make py.exe/python.exe available in PATH."
  }
  $file = $python.Source
  $args = @($serverScript)
}

$env:ROCREADER_LOCAL_OPDS_PORT = "$Port"
$process = Start-Process -FilePath $file `
  -ArgumentList $args `
  -WorkingDirectory $projectRoot `
  -RedirectStandardOutput $stdoutLog `
  -RedirectStandardError $stderrLog `
  -WindowStyle Hidden `
  -PassThru

$lastError = $null
for ($i = 0; $i -lt 30; ++$i) {
  Start-Sleep -Milliseconds 300
  if ($process.HasExited) {
    $err = if (Test-Path -LiteralPath $stderrLog) { Get-Content -LiteralPath $stderrLog -Raw } else { "" }
    $out = if (Test-Path -LiteralPath $stdoutLog) { Get-Content -LiteralPath $stdoutLog -Raw } else { "" }
    throw "Local OPDS server exited with code $($process.ExitCode). stdout: $out stderr: $err"
  }
  try {
    $response = Invoke-WebRequest -UseBasicParsing -Uri "http://127.0.0.1:$Port/opds" -TimeoutSec 2
    if ($response.Content.Length -ge 20) {
      Write-Host "[INFO] local OPDS ready, bytes=$($response.Content.Length), pid=$($process.Id)"
      exit 0
    }
    $lastError = "local OPDS returned empty feed"
  } catch {
    $lastError = $_.Exception.Message
  }
}

$stderrText = if (Test-Path -LiteralPath $stderrLog) { Get-Content -LiteralPath $stderrLog -Raw } else { "" }
throw "Local OPDS server did not respond: $lastError`n$stderrText"
