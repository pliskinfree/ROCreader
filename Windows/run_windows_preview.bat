@echo off
setlocal

echo [INFO] run_windows_preview.bat is kept as a compatibility entrypoint.
echo [INFO] Redirecting to run_windows_preview_720x480.bat
call "%~dp0run_windows_preview_720x480.bat"
exit /b %ERRORLEVEL%
