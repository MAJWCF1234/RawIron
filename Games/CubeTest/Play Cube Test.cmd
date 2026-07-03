@echo off
setlocal

cd /d "%~dp0..\.."

set "GAME_ROOT=%~dp0"
if "%GAME_ROOT:~-1%"=="\" set "GAME_ROOT=%GAME_ROOT:~0,-1%"

set "SCRIPTS=%CD%\Scripts"
set "TARGET_EXE="
call "%SCRIPTS%\Resolve-RawIronBinary.cmd" "Games\CubeTest\App" "RawIron.CubeTestGame.exe" TARGET_EXE "%CD%"
if not defined TARGET_EXE (
  echo Building RawIron.CubeTestGame ...
  call "%SCRIPTS%\Build-RawIronTarget.cmd" RawIron.CubeTestGame "%CD%"
  if errorlevel 1 goto :missing_cube_test
  call "%SCRIPTS%\Resolve-RawIronBinary.cmd" "Games\CubeTest\App" "RawIron.CubeTestGame.exe" TARGET_EXE "%CD%"
)
if not defined TARGET_EXE goto :missing_cube_test

echo Launching %TARGET_EXE%
set "CORE_ARGS=--game=cube-test --game-root=%GAME_ROOT% --workspace-root=%CD% --boot-ui=gameplay --renderer=vulkan"
set "DEFAULT_ARGS="
if "%~1"=="" set "DEFAULT_ARGS=--width=1280 --height=720"
call "%SCRIPTS%\Launch-WithMsvcRuntime.cmd" "%TARGET_EXE%" %CORE_ARGS% %DEFAULT_ARGS% %*
if %errorlevel% equ 0 exit /b 0

echo Could not launch RawIron.CubeTestGame.
pause
exit /b 1

:missing_cube_test
echo RawIron.CubeTestGame.exe was not found under build\dev-msvc.
echo   cmake --preset dev-msvc
echo   cmake --build build\dev-msvc --config RelWithDebInfo --target RawIron.CubeTestGame
pause
exit /b 1
