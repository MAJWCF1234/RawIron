@echo off
setlocal EnableExtensions
cd /d "%~dp0"

REM Optional: set RAWIRON_PREVIEW_EXE=D:\full\path\RawIron.Preview.exe

set "SCRIPTS=%~dp0Scripts"
set "TARGET_EXE="
if defined RAWIRON_PREVIEW_EXE if exist "%RAWIRON_PREVIEW_EXE%" set "TARGET_EXE=%RAWIRON_PREVIEW_EXE%"

if not defined TARGET_EXE call "%SCRIPTS%\Resolve-RawIronBinary.cmd" "Apps\RawIron.Preview" "RawIron.Preview.exe" TARGET_EXE "%CD%"

if not defined TARGET_EXE (
  echo RawIron.Preview.exe not found.
  echo   cmake --preset dev-msvc
  echo   cmake --build build\dev-msvc --config RelWithDebInfo --target RawIron.Preview
  pause
  exit /b 1
)

echo Running: "%TARGET_EXE%" %*
"%TARGET_EXE%" %*
set "ERR=%ERRORLEVEL%"
if not "%ERR%"=="0" (
  echo RawIron.Preview exited with code %ERR%.
  pause
  exit /b %ERR%
)
exit /b 0
