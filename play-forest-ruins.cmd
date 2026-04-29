@echo off
setlocal
cd /d "%~dp0"

set "TARGET_EXE="

if not defined TARGET_EXE if exist "build\dev-msvc\Games\WildernessRuins\App\Release\RawIron.ForestRuinsGame.exe" set "TARGET_EXE=build\dev-msvc\Games\WildernessRuins\App\Release\RawIron.ForestRuinsGame.exe"
if not defined TARGET_EXE if exist "build\dev-msvc\Games\WildernessRuins\App\RelWithDebInfo\RawIron.ForestRuinsGame.exe" set "TARGET_EXE=build\dev-msvc\Games\WildernessRuins\App\RelWithDebInfo\RawIron.ForestRuinsGame.exe"
if not defined TARGET_EXE if exist "build\dev-msvc\Games\WildernessRuins\App\Debug\RawIron.ForestRuinsGame.exe" set "TARGET_EXE=build\dev-msvc\Games\WildernessRuins\App\Debug\RawIron.ForestRuinsGame.exe"
if not defined TARGET_EXE if exist "build\dev-msvc\Games\WildernessRuins\App\MinSizeRel\RawIron.ForestRuinsGame.exe" set "TARGET_EXE=build\dev-msvc\Games\WildernessRuins\App\MinSizeRel\RawIron.ForestRuinsGame.exe"

if not defined TARGET_EXE if exist "build\dev-clang\Games\WildernessRuins\App\Release\RawIron.ForestRuinsGame.exe" set "TARGET_EXE=build\dev-clang\Games\WildernessRuins\App\Release\RawIron.ForestRuinsGame.exe"
if not defined TARGET_EXE if exist "build\dev-clang\Games\WildernessRuins\App\RelWithDebInfo\RawIron.ForestRuinsGame.exe" set "TARGET_EXE=build\dev-clang\Games\WildernessRuins\App\RelWithDebInfo\RawIron.ForestRuinsGame.exe"
if not defined TARGET_EXE if exist "build\dev-clang\Games\WildernessRuins\App\Debug\RawIron.ForestRuinsGame.exe" set "TARGET_EXE=build\dev-clang\Games\WildernessRuins\App\Debug\RawIron.ForestRuinsGame.exe"
if not defined TARGET_EXE if exist "build\dev-clang\Games\WildernessRuins\App\MinSizeRel\RawIron.ForestRuinsGame.exe" set "TARGET_EXE=build\dev-clang\Games\WildernessRuins\App\MinSizeRel\RawIron.ForestRuinsGame.exe"

if not defined TARGET_EXE if exist "build\dev-mingw\Games\WildernessRuins\App\Release\RawIron.ForestRuinsGame.exe" set "TARGET_EXE=build\dev-mingw\Games\WildernessRuins\App\Release\RawIron.ForestRuinsGame.exe"
if not defined TARGET_EXE if exist "build\dev-mingw\Games\WildernessRuins\App\RelWithDebInfo\RawIron.ForestRuinsGame.exe" set "TARGET_EXE=build\dev-mingw\Games\WildernessRuins\App\RelWithDebInfo\RawIron.ForestRuinsGame.exe"
if not defined TARGET_EXE if exist "build\dev-mingw\Games\WildernessRuins\App\Debug\RawIron.ForestRuinsGame.exe" set "TARGET_EXE=build\dev-mingw\Games\WildernessRuins\App\Debug\RawIron.ForestRuinsGame.exe"
if not defined TARGET_EXE if exist "build\dev-mingw\Games\WildernessRuins\App\MinSizeRel\RawIron.ForestRuinsGame.exe" set "TARGET_EXE=build\dev-mingw\Games\WildernessRuins\App\MinSizeRel\RawIron.ForestRuinsGame.exe"

if not defined TARGET_EXE if exist "build\hygiene\Games\WildernessRuins\App\Release\RawIron.ForestRuinsGame.exe" set "TARGET_EXE=build\hygiene\Games\WildernessRuins\App\Release\RawIron.ForestRuinsGame.exe"
if not defined TARGET_EXE if exist "build\hygiene\Games\WildernessRuins\App\RelWithDebInfo\RawIron.ForestRuinsGame.exe" set "TARGET_EXE=build\hygiene\Games\WildernessRuins\App\RelWithDebInfo\RawIron.ForestRuinsGame.exe"
if not defined TARGET_EXE if exist "build\hygiene\Games\WildernessRuins\App\Debug\RawIron.ForestRuinsGame.exe" set "TARGET_EXE=build\hygiene\Games\WildernessRuins\App\Debug\RawIron.ForestRuinsGame.exe"
if not defined TARGET_EXE if exist "build\hygiene\Games\WildernessRuins\App\MinSizeRel\RawIron.ForestRuinsGame.exe" set "TARGET_EXE=build\hygiene\Games\WildernessRuins\App\MinSizeRel\RawIron.ForestRuinsGame.exe"

if not defined TARGET_EXE if exist "build\Games\WildernessRuins\App\Release\RawIron.ForestRuinsGame.exe" set "TARGET_EXE=build\Games\WildernessRuins\App\Release\RawIron.ForestRuinsGame.exe"
if not defined TARGET_EXE if exist "build\Games\WildernessRuins\App\RelWithDebInfo\RawIron.ForestRuinsGame.exe" set "TARGET_EXE=build\Games\WildernessRuins\App\RelWithDebInfo\RawIron.ForestRuinsGame.exe"
if not defined TARGET_EXE if exist "build\Games\WildernessRuins\App\Debug\RawIron.ForestRuinsGame.exe" set "TARGET_EXE=build\Games\WildernessRuins\App\Debug\RawIron.ForestRuinsGame.exe"
if not defined TARGET_EXE if exist "build\Games\WildernessRuins\App\MinSizeRel\RawIron.ForestRuinsGame.exe" set "TARGET_EXE=build\Games\WildernessRuins\App\MinSizeRel\RawIron.ForestRuinsGame.exe"

if not defined TARGET_EXE goto :missing_forest_ruins

echo Launching %TARGET_EXE%
set "DEFAULT_ARGS="
if "%~1"=="" set "DEFAULT_ARGS=--renderer=vulkan --width=2560 --height=1440"
call :stage_runtime "%TARGET_EXE%"
call :launch_with_msvc_runtime "%TARGET_EXE%" %DEFAULT_ARGS% %*
set "GAME_EXIT_CODE=%errorlevel%"
if %GAME_EXIT_CODE% equ 0 exit /b 0

echo RawIron.ForestRuinsGame exited with code %GAME_EXIT_CODE%.
echo If it closes immediately, run this launcher from a terminal to read the failure text.
echo For runtime DLL issues on debug builds, prefer RelWithDebInfo/Release or install:
echo https://aka.ms/vs/17/release/vc_redist.x64.exe
pause
exit /b %GAME_EXIT_CODE%

:stage_runtime
set "EXE_PATH=%~1"
for %%I in ("%EXE_PATH%") do set "EXE_DIR=%%~dpI"
for %%D in (vcruntime140.dll vcruntime140_1.dll msvcp140.dll concrt140.dll) do (
  if exist "C:\Windows\System32\%%D" (
    copy /y "C:\Windows\System32\%%D" "%EXE_DIR%%%D" >nul
  )
)
exit /b 0

:launch_with_msvc_runtime
set "EXE_PATH=%~1"
shift
set "VCVARS="
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if not defined VCVARS if exist "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not defined VCVARS if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
if not defined VCVARS if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"

if defined VCVARS (
  call "%VCVARS%" >nul 2>nul
)

"%EXE_PATH%" %*
exit /b %errorlevel%

:missing_forest_ruins
echo RawIron.ForestRuinsGame.exe was not found.
echo Build the project first with CMake, then run this launcher again.
pause
exit /b 1
