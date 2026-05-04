@echo off
setlocal EnableExtensions
cd /d "%~dp0"

REM Optional override: set RAWIRON_EDITOR_EXE=D:\full\path\RawIron.Editor.exe

set "TARGET_EXE="
if defined RAWIRON_EDITOR_EXE if exist "%RAWIRON_EDITOR_EXE%" set "TARGET_EXE=%RAWIRON_EDITOR_EXE%"

REM Repo build\dev-msvc (multi-config)
if not defined TARGET_EXE if exist "build\dev-msvc\Apps\RawIron.Editor\RelWithDebInfo\RawIron.Editor.exe" set "TARGET_EXE=build\dev-msvc\Apps\RawIron.Editor\RelWithDebInfo\RawIron.Editor.exe"
if not defined TARGET_EXE if exist "build\dev-msvc\Apps\RawIron.Editor\Release\RawIron.Editor.exe" set "TARGET_EXE=build\dev-msvc\Apps\RawIron.Editor\Release\RawIron.Editor.exe"
if not defined TARGET_EXE if exist "build\dev-msvc\Apps\RawIron.Editor\Debug\RawIron.Editor.exe" set "TARGET_EXE=build\dev-msvc\Apps\RawIron.Editor\Debug\RawIron.Editor.exe"
if not defined TARGET_EXE if exist "build\dev-msvc\Apps\RawIron.Editor\MinSizeRel\RawIron.Editor.exe" set "TARGET_EXE=build\dev-msvc\Apps\RawIron.Editor\MinSizeRel\RawIron.Editor.exe"

REM Profile-drive cmake-build mirror (see README dev-msvc-localappdata preset)
if not defined TARGET_EXE if exist "%LOCALAPPDATA%\RawIron\cmake-build\dev-msvc\Apps\RawIron.Editor\RelWithDebInfo\RawIron.Editor.exe" set "TARGET_EXE=%LOCALAPPDATA%\RawIron\cmake-build\dev-msvc\Apps\RawIron.Editor\RelWithDebInfo\RawIron.Editor.exe"
if not defined TARGET_EXE if exist "%LOCALAPPDATA%\RawIron\cmake-build\dev-msvc\Apps\RawIron.Editor\Release\RawIron.Editor.exe" set "TARGET_EXE=%LOCALAPPDATA%\RawIron\cmake-build\dev-msvc\Apps\RawIron.Editor\Release\RawIron.Editor.exe"
if not defined TARGET_EXE if exist "%LOCALAPPDATA%\RawIron\cmake-build\dev-msvc\Apps\RawIron.Editor\Debug\RawIron.Editor.exe" set "TARGET_EXE=%LOCALAPPDATA%\RawIron\cmake-build\dev-msvc\Apps\RawIron.Editor\Debug\RawIron.Editor.exe"

if not defined TARGET_EXE (
  echo RawIron.Editor.exe not found.
  echo Configure with editor enabled, then build, for example:
  echo   cmake -U RAWIRON_BUILD_EDITOR --preset dev-msvc
  echo   cmake --build build\dev-msvc --config RelWithDebInfo --target RawIron.Editor
  pause
  exit /b 1
)

echo Running: "%TARGET_EXE%" --workspace="%CD%" %*
"%TARGET_EXE%" --workspace="%CD%" %*
set "ERR=%ERRORLEVEL%"
if not "%ERR%"=="0" (
  echo RawIron.Editor exited with code %ERR%.
  pause
  exit /b %ERR%
)
exit /b 0
