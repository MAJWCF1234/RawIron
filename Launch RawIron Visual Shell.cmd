@echo off
setlocal EnableExtensions
cd /d "%~dp0"

REM Optional override: set RAWIRON_VISUAL_SHELL_EXE=D:\full\path\RawIron.VisualShell.exe

set "TARGET_EXE="
if defined RAWIRON_VISUAL_SHELL_EXE if exist "%RAWIRON_VISUAL_SHELL_EXE%" set "TARGET_EXE=%RAWIRON_VISUAL_SHELL_EXE%"

if not defined TARGET_EXE if exist "build\Apps\RawIron.VisualShell\RawIron.VisualShell.exe" set "TARGET_EXE=build\Apps\RawIron.VisualShell\RawIron.VisualShell.exe"
if not defined TARGET_EXE if exist "build\Apps\RawIron.VisualShell\RelWithDebInfo\RawIron.VisualShell.exe" set "TARGET_EXE=build\Apps\RawIron.VisualShell\RelWithDebInfo\RawIron.VisualShell.exe"
if not defined TARGET_EXE if exist "build\Apps\RawIron.VisualShell\Release\RawIron.VisualShell.exe" set "TARGET_EXE=build\Apps\RawIron.VisualShell\Release\RawIron.VisualShell.exe"
if not defined TARGET_EXE if exist "build\Apps\RawIron.VisualShell\Debug\RawIron.VisualShell.exe" set "TARGET_EXE=build\Apps\RawIron.VisualShell\Debug\RawIron.VisualShell.exe"
if not defined TARGET_EXE if exist "build\Apps\RawIron.VisualShell\MinSizeRel\RawIron.VisualShell.exe" set "TARGET_EXE=build\Apps\RawIron.VisualShell\MinSizeRel\RawIron.VisualShell.exe"
if not defined TARGET_EXE if exist "build\dev-msvc\Apps\RawIron.VisualShell\RelWithDebInfo\RawIron.VisualShell.exe" set "TARGET_EXE=build\dev-msvc\Apps\RawIron.VisualShell\RelWithDebInfo\RawIron.VisualShell.exe"
if not defined TARGET_EXE if exist "build\dev-msvc\Apps\RawIron.VisualShell\Release\RawIron.VisualShell.exe" set "TARGET_EXE=build\dev-msvc\Apps\RawIron.VisualShell\Release\RawIron.VisualShell.exe"
if not defined TARGET_EXE if exist "build\dev-msvc\Apps\RawIron.VisualShell\Debug\RawIron.VisualShell.exe" set "TARGET_EXE=build\dev-msvc\Apps\RawIron.VisualShell\Debug\RawIron.VisualShell.exe"
if not defined TARGET_EXE if exist "build\dev-msvc\Apps\RawIron.VisualShell\MinSizeRel\RawIron.VisualShell.exe" set "TARGET_EXE=build\dev-msvc\Apps\RawIron.VisualShell\MinSizeRel\RawIron.VisualShell.exe"

if not defined TARGET_EXE if exist "%LOCALAPPDATA%\RawIron\cmake-build\Apps\RawIron.VisualShell\RawIron.VisualShell.exe" set "TARGET_EXE=%LOCALAPPDATA%\RawIron\cmake-build\Apps\RawIron.VisualShell\RawIron.VisualShell.exe"
if not defined TARGET_EXE if exist "%LOCALAPPDATA%\RawIron\cmake-build\Apps\RawIron.VisualShell\RelWithDebInfo\RawIron.VisualShell.exe" set "TARGET_EXE=%LOCALAPPDATA%\RawIron\cmake-build\Apps\RawIron.VisualShell\RelWithDebInfo\RawIron.VisualShell.exe"
if not defined TARGET_EXE if exist "%LOCALAPPDATA%\RawIron\cmake-build\Apps\RawIron.VisualShell\Release\RawIron.VisualShell.exe" set "TARGET_EXE=%LOCALAPPDATA%\RawIron\cmake-build\Apps\RawIron.VisualShell\Release\RawIron.VisualShell.exe"
if not defined TARGET_EXE if exist "%LOCALAPPDATA%\RawIron\cmake-build\Apps\RawIron.VisualShell\Debug\RawIron.VisualShell.exe" set "TARGET_EXE=%LOCALAPPDATA%\RawIron\cmake-build\Apps\RawIron.VisualShell\Debug\RawIron.VisualShell.exe"
if not defined TARGET_EXE if exist "%LOCALAPPDATA%\RawIron\cmake-build\dev-msvc\Apps\RawIron.VisualShell\RelWithDebInfo\RawIron.VisualShell.exe" set "TARGET_EXE=%LOCALAPPDATA%\RawIron\cmake-build\dev-msvc\Apps\RawIron.VisualShell\RelWithDebInfo\RawIron.VisualShell.exe"
if not defined TARGET_EXE if exist "%LOCALAPPDATA%\RawIron\cmake-build\dev-msvc\Apps\RawIron.VisualShell\Release\RawIron.VisualShell.exe" set "TARGET_EXE=%LOCALAPPDATA%\RawIron\cmake-build\dev-msvc\Apps\RawIron.VisualShell\Release\RawIron.VisualShell.exe"
if not defined TARGET_EXE if exist "%LOCALAPPDATA%\RawIron\cmake-build\dev-msvc\Apps\RawIron.VisualShell\Debug\RawIron.VisualShell.exe" set "TARGET_EXE=%LOCALAPPDATA%\RawIron\cmake-build\dev-msvc\Apps\RawIron.VisualShell\Debug\RawIron.VisualShell.exe"

if not defined TARGET_EXE (
  echo RawIron.VisualShell.exe not found.
  echo Configure with the visual shell enabled, then build, for example:
  echo   cmake -S . -B build -DRAWIRON_BUILD_VISUAL_SHELL=ON -DRAWIRON_BUILD_TOOLS=ON
  echo   cmake --build build --target RawIron.VisualShell ri_tool
  echo Console examples:
  echo   Launch RawIron Visual Shell.cmd --headless
  echo   Launch RawIron Visual Shell.cmd --list-actions
  echo   Launch RawIron Visual Shell.cmd --list-projects
  echo   Launch RawIron Visual Shell.cmd --describe-project rawiron-multiplayer-sandbox
  echo   Launch RawIron Visual Shell.cmd --doctor-project rawiron-multiplayer-sandbox
  echo   Launch RawIron Visual Shell.cmd --list-project-resources liminal-hall --resource-category levels
  echo   Launch RawIron Visual Shell.cmd --open-console
  echo   Launch RawIron Visual Shell.cmd --open-folder rawiron-multiplayer-sandbox
  echo   Launch RawIron Visual Shell.cmd --open-editor rawiron-multiplayer-sandbox
  echo   Launch RawIron Visual Shell.cmd --new-project tool-cli-sample-3 --name "Tool CLI Sample 3" --author "RawIron Tooling" --game-root D:\RawIron\Saved\Tooling\CliProjectSmoke\ToolCliSample3
  echo   Launch RawIron Visual Shell.cmd --run-action "Workspace Check"
  echo   Launch RawIron Visual Shell.cmd --ri-tool --list-projects --root D:\RawIron
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
