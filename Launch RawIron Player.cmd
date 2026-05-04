@echo off
setlocal EnableExtensions
cd /d "%~dp0"

REM Optional: set RAWIRON_PLAYER_EXE=D:\full\path\RawIron.Player.exe

set "TARGET_EXE="
if defined RAWIRON_PLAYER_EXE if exist "%RAWIRON_PLAYER_EXE%" set "TARGET_EXE=%RAWIRON_PLAYER_EXE%"

if not defined TARGET_EXE if exist "build\dev-msvc\Apps\RawIron.Player\RelWithDebInfo\RawIron.Player.exe" set "TARGET_EXE=build\dev-msvc\Apps\RawIron.Player\RelWithDebInfo\RawIron.Player.exe"
if not defined TARGET_EXE if exist "build\dev-msvc\Apps\RawIron.Player\Release\RawIron.Player.exe" set "TARGET_EXE=build\dev-msvc\Apps\RawIron.Player\Release\RawIron.Player.exe"
if not defined TARGET_EXE if exist "build\dev-msvc\Apps\RawIron.Player\Debug\RawIron.Player.exe" set "TARGET_EXE=build\dev-msvc\Apps\RawIron.Player\Debug\RawIron.Player.exe"
if not defined TARGET_EXE if exist "build\dev-msvc\Apps\RawIron.Player\MinSizeRel\RawIron.Player.exe" set "TARGET_EXE=build\dev-msvc\Apps\RawIron.Player\MinSizeRel\RawIron.Player.exe"

if not defined TARGET_EXE if exist "%LOCALAPPDATA%\RawIron\cmake-build\dev-msvc\Apps\RawIron.Player\RelWithDebInfo\RawIron.Player.exe" set "TARGET_EXE=%LOCALAPPDATA%\RawIron\cmake-build\dev-msvc\Apps\RawIron.Player\RelWithDebInfo\RawIron.Player.exe"
if not defined TARGET_EXE if exist "%LOCALAPPDATA%\RawIron\cmake-build\dev-msvc\Apps\RawIron.Player\Release\RawIron.Player.exe" set "TARGET_EXE=%LOCALAPPDATA%\RawIron\cmake-build\dev-msvc\Apps\RawIron.Player\Release\RawIron.Player.exe"

if not defined TARGET_EXE (
  echo RawIron.Player.exe not found.
  echo   cmake -U RAWIRON_BUILD_PLAYER --preset dev-msvc
  echo   cmake --build build\dev-msvc --config RelWithDebInfo --target RawIron.Player
  pause
  exit /b 1
)

echo Running: "%TARGET_EXE%" %*
"%TARGET_EXE%" %*
set "ERR=%ERRORLEVEL%"
if not "%ERR%"=="0" (
  echo RawIron.Player exited with code %ERR%.
  pause
  exit /b %ERR%
)
exit /b 0
