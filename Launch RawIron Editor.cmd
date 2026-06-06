@echo off
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0"

REM RawIron level editor (Win32 UI). Builds on demand if missing.
REM Optional: set RAWIRON_EDITOR_EXE to a full path to skip discovery.

set "DEFAULT_GAME=liminal-hall"
set "CMAKE_BUILD_DIR=build"
set "TARGET_EXE="

if defined RAWIRON_EDITOR_EXE if exist "%RAWIRON_EDITOR_EXE%" set "TARGET_EXE=%RAWIRON_EDITOR_EXE%"

call :try_exe "%CD%\%CMAKE_BUILD_DIR%\Apps\RawIron.Editor\RawIron.Editor.exe"
call :try_exe "%CD%\%CMAKE_BUILD_DIR%\Apps\RawIron.Editor\Debug\RawIron.Editor.exe"
call :try_exe "%CD%\%CMAKE_BUILD_DIR%\Apps\RawIron.Editor\Release\RawIron.Editor.exe"
call :try_exe "%CD%\%CMAKE_BUILD_DIR%\Apps\RawIron.Editor\RelWithDebInfo\RawIron.Editor.exe"
call :try_exe "%CD%\build\dev-msvc\Apps\RawIron.Editor\RelWithDebInfo\RawIron.Editor.exe"
call :try_exe "%CD%\build\dev-msvc\Apps\RawIron.Editor\Release\RawIron.Editor.exe"
call :try_exe "%CD%\build\dev-msvc\Apps\RawIron.Editor\Debug\RawIron.Editor.exe"
call :try_exe "%LOCALAPPDATA%\RawIron\cmake-build\dev-msvc\Apps\RawIron.Editor\RelWithDebInfo\RawIron.Editor.exe"
call :try_exe "%LOCALAPPDATA%\RawIron\cmake-build\dev-msvc\Apps\RawIron.Editor\Release\RawIron.Editor.exe"

if not defined TARGET_EXE (
  echo RawIron.Editor.exe not found in the standard build outputs. Attempting in-place build...
  echo.
  where cmake >nul 2>&1
  if errorlevel 1 (
    echo ERROR: cmake is not on PATH. Install CMake or build the editor manually.
    goto :fail
  )
  if exist "%CD%\%CMAKE_BUILD_DIR%\CMakeCache.txt" (
    cmake --build "%CD%\%CMAKE_BUILD_DIR%" --target RawIron.Editor >nul 2>&1
  ) else (
    echo ERROR: build\ is not configured yet. Configure it first, then rerun this launcher.
    goto :fail
  )
  call :try_exe "%CD%\%CMAKE_BUILD_DIR%\Apps\RawIron.Editor\RawIron.Editor.exe"
  call :try_exe "%CD%\%CMAKE_BUILD_DIR%\Apps\RawIron.Editor\Debug\RawIron.Editor.exe"
  call :try_exe "%CD%\%CMAKE_BUILD_DIR%\Apps\RawIron.Editor\Release\RawIron.Editor.exe"
  call :try_exe "%CD%\%CMAKE_BUILD_DIR%\Apps\RawIron.Editor\RelWithDebInfo\RawIron.Editor.exe"
)

if not defined TARGET_EXE goto :fail

set "GAME_ARG="
set "HAS_GAME=0"
for %%A in (%*) do (
  set "ARG=%%~A"
  if /I "!ARG!"=="--game" set "HAS_GAME=1"
  if /I "!ARG:~0,6!"=="--game=" set "HAS_GAME=1"
  if /I "!ARG!"=="--project" set "HAS_GAME=1"
  if /I "!ARG!"=="--project-root" set "HAS_GAME=1"
  if /I "!ARG:~0,11!"=="--game-root=" set "HAS_GAME=1"
  if /I "!ARG:~0,14!"=="--project-root=" set "HAS_GAME=1"
)
if "!HAS_GAME!"=="0" set "GAME_ARG=--game=%DEFAULT_GAME%"

echo.
echo ========================================
echo   RawIron Editor
echo   %TARGET_EXE%
echo   workspace: %CD%
if not "!GAME_ARG!"=="" echo   project: !GAME_ARG!
echo ========================================
echo.
echo Quick start: Scene tab ^| + Cube/Plane ^| T/R/U move ^| Ctrl+S save ^| Ctrl+E export ^| Playtest
echo.

"%TARGET_EXE%" --editor-ui --workspace="%CD%" !GAME_ARG! %*
set "ERR=!ERRORLEVEL!"
if not "!ERR!"=="0" (
  echo.
  echo RawIron.Editor exited with code !ERR!.
  pause
  exit /b !ERR!
)
exit /b 0

:try_exe
if not exist "%~1" exit /b 0
if defined TARGET_EXE exit /b 0
set "TARGET_EXE=%~1"
exit /b 0

:fail
echo.
echo Could not find or build RawIron.Editor.exe
echo.
echo Expected locations include:
echo   %CD%\build\Apps\RawIron.Editor\RawIron.Editor.exe
echo   %CD%\build\dev-msvc\Apps\RawIron.Editor\RelWithDebInfo\RawIron.Editor.exe
echo.
echo In-place build:
echo   cmake --build build --target RawIron.Editor
echo.
echo Or set: set RAWIRON_EDITOR_EXE=D:\path\to\RawIron.Editor.exe
echo.
pause
exit /b 1
