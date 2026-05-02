@echo off
setlocal
REM Double-click launcher for the full-workspace release installer (downloads GitHub release parts).
cd /d "%~dp0"
powershell.exe -NoProfile -STA -ExecutionPolicy Bypass -File "%~dp0RawIron.FullWorkspace.Installer.ps1" %*
if errorlevel 1 exit /b 1
endlocal
