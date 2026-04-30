param(
  [string]$MsysBash = "D:\Program Files\MSYS2\usr\bin\bash.exe"
)

$ErrorActionPreference = "Stop"

$repo = Split-Path -Parent $PSScriptRoot
Set-Location $repo

if (!(Test-Path -LiteralPath $MsysBash)) {
  throw "MSYS2 bash not found: $MsysBash"
}

Write-Host "[smoke] cleaning local object files"
Remove-Item -Force src\*.o -ErrorAction SilentlyContinue

Write-Host "[smoke] building with make"
& $MsysBash -lc 'export PATH=/ucrt64/bin:/usr/bin:$PATH; cd /d/Works/ROCreader; make -j4'
if ($LASTEXITCODE -ne 0) {
  throw "make failed with exit code $LASTEXITCODE"
}

if (!(Test-Path -LiteralPath "build\rocreader_sdl.exe") -and !(Test-Path -LiteralPath "build\rocreader_sdl")) {
  throw "smoke build output missing: build\rocreader_sdl"
}

$trackedGenerated = git ls-files log.txt src/*.o
if ($trackedGenerated) {
  throw "generated files are still tracked:`n$trackedGenerated"
}

git check-ignore -q log.txt
if ($LASTEXITCODE -ne 0) {
  throw "log.txt is not ignored"
}

git check-ignore -q src/main.o
if ($LASTEXITCODE -ne 0) {
  throw "src/*.o is not ignored"
}

Write-Host "[smoke] ok"
