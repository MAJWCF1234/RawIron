@echo off

setlocal EnableExtensions EnableDelayedExpansion

cd /d "%~dp0"



REM RawIron level editor (Win32 UI). Builds on demand if missing.

REM Optional: set RAWIRON_EDITOR_EXE to a full path to skip discovery.



set "DEFAULT_GAME=liminal-hall"

set "SCRIPTS=%~dp0Scripts"

set "TARGET_EXE="



if defined RAWIRON_EDITOR_EXE if exist "%RAWIRON_EDITOR_EXE%" set "TARGET_EXE=%RAWIRON_EDITOR_EXE%"



if not defined TARGET_EXE call "%SCRIPTS%\Resolve-RawIronBinary.cmd" "Apps\RawIron.Editor" "RawIron.Editor.exe" TARGET_EXE "%CD%"



if not defined TARGET_EXE (

  echo RawIron.Editor.exe not found. Building ...

  call "%SCRIPTS%\Build-RawIronTarget.cmd" RawIron.Editor "%CD%"

  if errorlevel 1 goto :fail

  call "%SCRIPTS%\Resolve-RawIronBinary.cmd" "Apps\RawIron.Editor" "RawIron.Editor.exe" TARGET_EXE "%CD%"

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

echo Quick start: Scene tab ^| + Cube/Plane ^| T/R/U move ^| Ctrl+S save ^| F1 help ^| Playtest

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



:fail

echo.

echo Could not find or build RawIron.Editor.exe

echo.

echo Expected: %CD%\build\dev-msvc\Apps\RawIron.Editor\RelWithDebInfo\RawIron.Editor.exe

echo.

echo   cmake --preset dev-msvc

echo   cmake --build build\dev-msvc --config RelWithDebInfo --target RawIron.Editor

echo.

echo Or set: set RAWIRON_EDITOR_EXE=D:\path\to\RawIron.Editor.exe

echo.

pause

exit /b 1

