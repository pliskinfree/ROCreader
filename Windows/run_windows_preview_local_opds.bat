@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..") do set "PROJECT_WIN=%%~fI"
set "PORT=8765"

echo [INFO] Starting local OPDS test server at http://127.0.0.1:%PORT%/opds
powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%start_local_opds.ps1" -Port %PORT%
if not "%ERRORLEVEL%"=="0" (
  echo [ERROR] Local OPDS server did not respond.
  echo [INFO] stdout log: "%SCRIPT_DIR%local_opds_server.log"
  echo [INFO] stderr log: "%SCRIPT_DIR%local_opds_server.log.err"
  pause
  exit /b 2
)

echo [INFO] Use this source in online_sources.ini:
echo [source.local_opds]
echo name=Local Windows OPDS Test
echo type=opds
echo url=http://127.0.0.1:%PORT%/opds
echo enabled=1
echo category.0.name=All
echo category.0.url=http://127.0.0.1:%PORT%/opds
echo.

call "%SCRIPT_DIR%run_windows_preview_720x480.bat"
exit /b %ERRORLEVEL%
