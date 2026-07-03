@echo off
setlocal
set "SCRIPT_DIR=%~dp0"
set "WORKSPACE_ROOT=%SCRIPT_DIR%..\.."
cd /d "%WORKSPACE_ROOT%"

set "GAME_ROOT=%SCRIPT_DIR%"
if "%GAME_ROOT:~-1%"=="\" set "GAME_ROOT=%GAME_ROOT:~0,-1%"

set "SCRIPTS=%CD%\Scripts"
set "TARGET_EXE="
call "%SCRIPTS%\Resolve-RawIronBinary.cmd" "Games\RawIronMultiplayerSandbox\App" "RawIron.MultiplayerSandboxGame.exe" TARGET_EXE "%CD%"

if not defined TARGET_EXE (
  echo [RawIron Multiplayer Sandbox] Executable not found under build\dev-msvc.
  echo Build target first:
  echo   cmake --preset dev-msvc
  echo   cmake --build build\dev-msvc --config RelWithDebInfo --target RawIron.MultiplayerSandboxGame
  exit /b 1
)

echo [RawIron Multiplayer Sandbox] Starting 3D play mode (RuntimeCore-mounted).
echo [RawIron Multiplayer Sandbox] Use --runtime-mode=net for headless net harness mode.
set "CORE_ARGS=--game=rawiron-multiplayer-sandbox --game-root=%GAME_ROOT% --workspace-root=%CD% --runtime-mode=play --boot-ui=gameplay --renderer=vulkan"
set "DEFAULT_ARGS="
if "%~1"=="" set "DEFAULT_ARGS=--width=1600 --height=900 --net-mode=listen --issue-join-code --bots=32"
call "%SCRIPTS%\Launch-WithMsvcRuntime.cmd" "%TARGET_EXE%" %CORE_ARGS% %DEFAULT_ARGS% %*
set "RC=%errorlevel%"
if not "%RC%"=="0" (
  echo.
  echo [RawIron Multiplayer Sandbox] Process exited with error code %RC%.
  pause
)
exit /b %RC%
