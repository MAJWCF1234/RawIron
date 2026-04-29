@echo off
setlocal
cd /d "%~dp0"
where node >nul 2>nul
if errorlevel 1 (
  echo [launcher] Node.js is not on PATH. Install Node.js 20+ and retry.
  pause
  exit /b 1
)
if not exist ".\dist\index.html" (
  echo [launcher] dist\index.html is missing.
  echo [launcher] Build first with: npm run build:quick
  pause
  exit /b 1
)
node ".\proto-tool.mjs" launch --mechtest
if errorlevel 1 (
  echo [launcher] Launch failed.
  pause
  exit /b 1
)
