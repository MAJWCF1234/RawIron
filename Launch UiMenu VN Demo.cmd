@echo off
setlocal EnableExtensions
cd /d "%~dp0"

set "TARGET_EXE="

REM MSVC multi-config under repo build\dev-msvc (default preset)
if not defined TARGET_EXE if exist "build\dev-msvc\Apps\RawIron.UiMenu\RelWithDebInfo\RawIron.UiMenu.exe" set "TARGET_EXE=build\dev-msvc\Apps\RawIron.UiMenu\RelWithDebInfo\RawIron.UiMenu.exe"
if not defined TARGET_EXE if exist "build\dev-msvc\Apps\RawIron.UiMenu\Release\RawIron.UiMenu.exe" set "TARGET_EXE=build\dev-msvc\Apps\RawIron.UiMenu\Release\RawIron.UiMenu.exe"
if not defined TARGET_EXE if exist "build\dev-msvc\Apps\RawIron.UiMenu\Debug\RawIron.UiMenu.exe" set "TARGET_EXE=build\dev-msvc\Apps\RawIron.UiMenu\Debug\RawIron.UiMenu.exe"

REM Profile-drive preset output
if not defined TARGET_EXE if exist "%LOCALAPPDATA%\RawIron\cmake-build\dev-msvc\Apps\RawIron.UiMenu\RelWithDebInfo\RawIron.UiMenu.exe" set "TARGET_EXE=%LOCALAPPDATA%\RawIron\cmake-build\dev-msvc\Apps\RawIron.UiMenu\RelWithDebInfo\RawIron.UiMenu.exe"
if not defined TARGET_EXE if exist "%LOCALAPPDATA%\RawIron\cmake-build\dev-msvc\Apps\RawIron.UiMenu\Release\RawIron.UiMenu.exe" set "TARGET_EXE=%LOCALAPPDATA%\RawIron\cmake-build\dev-msvc\Apps\RawIron.UiMenu\Release\RawIron.UiMenu.exe"

if not defined TARGET_EXE (
  echo RawIron.UiMenu.exe not found.
  echo Build first, for example:
  echo   cmake --preset dev-msvc
  echo   cmake --build build\dev-msvc --config RelWithDebInfo --target RawIron.UiMenu
  pause
  exit /b 1
)

echo Running: "%TARGET_EXE%" --workspace="%CD%" --demo-vn
"%TARGET_EXE%" --workspace="%CD%" --demo-vn
set "ERR=%ERRORLEVEL%"
if not "%ERR%"=="0" (
  echo UiMenu exited with code %ERR%.
  pause
  exit /b %ERR%
)
exit /b 0
