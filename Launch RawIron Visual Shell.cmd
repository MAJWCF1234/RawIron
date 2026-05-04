@echo off
setlocal EnableExtensions
cd /d "%~dp0"

REM Optional override: set RAWIRON_VISUAL_SHELL_EXE=D:\full\path\RawIron.VisualShell.exe

set "TARGET_EXE="
if defined RAWIRON_VISUAL_SHELL_EXE if exist "%RAWIRON_VISUAL_SHELL_EXE%" set "TARGET_EXE=%RAWIRON_VISUAL_SHELL_EXE%"

if not defined TARGET_EXE if exist "build\dev-msvc\Apps\RawIron.VisualShell\RelWithDebInfo\RawIron.VisualShell.exe" set "TARGET_EXE=build\dev-msvc\Apps\RawIron.VisualShell\RelWithDebInfo\RawIron.VisualShell.exe"
if not defined TARGET_EXE if exist "build\dev-msvc\Apps\RawIron.VisualShell\Release\RawIron.VisualShell.exe" set "TARGET_EXE=build\dev-msvc\Apps\RawIron.VisualShell\Release\RawIron.VisualShell.exe"
if not defined TARGET_EXE if exist "build\dev-msvc\Apps\RawIron.VisualShell\Debug\RawIron.VisualShell.exe" set "TARGET_EXE=build\dev-msvc\Apps\RawIron.VisualShell\Debug\RawIron.VisualShell.exe"
if not defined TARGET_EXE if exist "build\dev-msvc\Apps\RawIron.VisualShell\MinSizeRel\RawIron.VisualShell.exe" set "TARGET_EXE=build\dev-msvc\Apps\RawIron.VisualShell\MinSizeRel\RawIron.VisualShell.exe"

if not defined TARGET_EXE if exist "%LOCALAPPDATA%\RawIron\cmake-build\dev-msvc\Apps\RawIron.VisualShell\RelWithDebInfo\RawIron.VisualShell.exe" set "TARGET_EXE=%LOCALAPPDATA%\RawIron\cmake-build\dev-msvc\Apps\RawIron.VisualShell\RelWithDebInfo\RawIron.VisualShell.exe"
if not defined TARGET_EXE if exist "%LOCALAPPDATA%\RawIron\cmake-build\dev-msvc\Apps\RawIron.VisualShell\Release\RawIron.VisualShell.exe" set "TARGET_EXE=%LOCALAPPDATA%\RawIron\cmake-build\dev-msvc\Apps\RawIron.VisualShell\Release\RawIron.VisualShell.exe"
if not defined TARGET_EXE if exist "%LOCALAPPDATA%\RawIron\cmake-build\dev-msvc\Apps\RawIron.VisualShell\Debug\RawIron.VisualShell.exe" set "TARGET_EXE=%LOCALAPPDATA%\RawIron\cmake-build\dev-msvc\Apps\RawIron.VisualShell\Debug\RawIron.VisualShell.exe"

if not defined TARGET_EXE (
  echo RawIron.VisualShell.exe not found.
  echo Configure with the visual shell enabled, then build, for example:
  echo   cmake -U RAWIRON_BUILD_VISUAL_SHELL --preset dev-msvc
  echo   cmake --build build\dev-msvc --config RelWithDebInfo --target RawIron.VisualShell
  pause
  exit /b 1
)

echo Running: "%TARGET_EXE%" %*
"%TARGET_EXE%" %*
set "ERR=%ERRORLEVEL%"
if not "%ERR%"=="0" (
  echo RawIron.VisualShell exited with code %ERR%.
  pause
  exit /b %ERR%
)
exit /b 0
