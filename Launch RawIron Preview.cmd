@echo off
setlocal EnableExtensions
cd /d "%~dp0"

REM Optional: set RAWIRON_PREVIEW_EXE=D:\full\path\RawIron.Preview.exe

set "TARGET_EXE="
if defined RAWIRON_PREVIEW_EXE if exist "%RAWIRON_PREVIEW_EXE%" set "TARGET_EXE=%RAWIRON_PREVIEW_EXE%"

if not defined TARGET_EXE if exist "build\dev-msvc\Apps\RawIron.Preview\RelWithDebInfo\RawIron.Preview.exe" set "TARGET_EXE=build\dev-msvc\Apps\RawIron.Preview\RelWithDebInfo\RawIron.Preview.exe"
if not defined TARGET_EXE if exist "build\dev-msvc\Apps\RawIron.Preview\Release\RawIron.Preview.exe" set "TARGET_EXE=build\dev-msvc\Apps\RawIron.Preview\Release\RawIron.Preview.exe"
if not defined TARGET_EXE if exist "build\dev-msvc\Apps\RawIron.Preview\Debug\RawIron.Preview.exe" set "TARGET_EXE=build\dev-msvc\Apps\RawIron.Preview\Debug\RawIron.Preview.exe"
if not defined TARGET_EXE if exist "build\dev-msvc\Apps\RawIron.Preview\MinSizeRel\RawIron.Preview.exe" set "TARGET_EXE=build\dev-msvc\Apps\RawIron.Preview\MinSizeRel\RawIron.Preview.exe"

if not defined TARGET_EXE if exist "%LOCALAPPDATA%\RawIron\cmake-build\dev-msvc\Apps\RawIron.Preview\RelWithDebInfo\RawIron.Preview.exe" set "TARGET_EXE=%LOCALAPPDATA%\RawIron\cmake-build\dev-msvc\Apps\RawIron.Preview\RelWithDebInfo\RawIron.Preview.exe"
if not defined TARGET_EXE if exist "%LOCALAPPDATA%\RawIron\cmake-build\dev-msvc\Apps\RawIron.Preview\Release\RawIron.Preview.exe" set "TARGET_EXE=%LOCALAPPDATA%\RawIron\cmake-build\dev-msvc\Apps\RawIron.Preview\Release\RawIron.Preview.exe"

if not defined TARGET_EXE (
  echo RawIron.Preview.exe not found.
  echo   cmake -U RAWIRON_BUILD_PREVIEW --preset dev-msvc
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
