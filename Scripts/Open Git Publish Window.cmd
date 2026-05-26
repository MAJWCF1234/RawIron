@echo off
setlocal EnableExtensions
cd /d "%~dp0.."

echo Opening a new PowerShell window for interactive git push...
echo Check your taskbar for that window.
echo.

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Open-PublishToGitHub-Window.ps1"
if errorlevel 1 (
  echo.
  echo Launcher reported an error. Try running from an elevated prompt only if you know you need it.
  pause
  exit /b 1
)

echo.
echo Done. The publish window should be open if PowerShell started OK.
pause
