@echo off
setlocal EnableExtensions
cd /d "%~dp0"

REM Optional override: set RAWIRON_VISUAL_SHELL_EXE=D:\full\path\RawIron.VisualShell.exe

set "SCRIPTS=%~dp0Scripts"
set "TARGET_EXE="
if defined RAWIRON_VISUAL_SHELL_EXE if exist "%RAWIRON_VISUAL_SHELL_EXE%" set "TARGET_EXE=%RAWIRON_VISUAL_SHELL_EXE%"

if not defined TARGET_EXE call "%SCRIPTS%\Resolve-RawIronBinary.cmd" "Apps\RawIron.VisualShell" "RawIron.VisualShell.exe" TARGET_EXE "%CD%"

if not defined TARGET_EXE (
  echo RawIron.VisualShell.exe not found.
  echo   cmake --preset dev-msvc
  echo   cmake --build build\dev-msvc --config RelWithDebInfo --target RawIron.VisualShell ri_tool
  echo Console examples:
  echo   Launch RawIron Visual Shell.cmd --headless
  echo   Launch RawIron Visual Shell.cmd --list-actions
  echo   Launch RawIron Visual Shell.cmd --list-projects
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
