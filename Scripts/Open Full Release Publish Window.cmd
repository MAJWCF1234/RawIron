@echo off
setlocal EnableExtensions
cd /d "%~dp0.."

echo Opens a new PowerShell window: optional MSVC build, full-workspace split ZIP,
echo release-notes draft, optional installer default patch, optional gh release.
echo.
echo For a quick re-zip without the build prompt, edit the script or run:
echo   pwsh -File "%~dp0Open-PublishFullRelease-Window.ps1" -SkipBuild
echo.

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Open-PublishFullRelease-Window.ps1"
if errorlevel 1 (
  echo.
  echo Launcher failed.
  pause
  exit /b 1
)

echo.
echo If the new window did not appear, run the same from x64 Native Tools or VS PowerShell.
pause
