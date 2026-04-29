@echo off
setlocal

cd /d "%~dp0"
title Proto Engine - Test Hall

echo ======================================
echo   Standalone Proto Engine - Test Hall
echo   (browser app window / kiosk mode)
echo ======================================
echo.

where node >nul 2>nul
if errorlevel 1 (
  echo Node.js is not on PATH. Install Node.js 20+ and retry.
  pause
  exit /b 1
)

if not exist "dist\index.html" (
  echo dist\index.html is missing.
  echo Build first with: npm run build:quick
  pause
  exit /b 1
)

echo Starting standalone Test Hall in app window mode...
call node ".\browser-launcher.mjs" --mechtest
if errorlevel 1 (
  echo.
  echo Launcher failed.
  pause
  exit /b 1
)

endlocal
