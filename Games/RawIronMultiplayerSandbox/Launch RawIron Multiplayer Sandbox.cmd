@echo off
setlocal
set "SCRIPT_DIR=%~dp0"
set "WORKSPACE_ROOT=%SCRIPT_DIR%..\.."
set "EXE=%WORKSPACE_ROOT%\build\Games\RawIronMultiplayerSandbox\App\RawIron.MultiplayerSandboxGame.exe"

if not exist "%EXE%" (
  echo [RawIron Multiplayer Sandbox] Executable not found:
  echo   %EXE%
  echo Build target first: RawIron.MultiplayerSandboxGame
  exit /b 1
)

echo [RawIron Multiplayer Sandbox] Starting 3D play mode (RuntimeCore-mounted).
echo [RawIron Multiplayer Sandbox] Use --runtime-mode=net for headless net harness mode.
"%EXE%" --workspace-root="%WORKSPACE_ROOT%" --game=rawiron-multiplayer-sandbox --runtime-mode=play --width=1600 --height=900 %*
set "RC=%errorlevel%"
if not "%RC%"=="0" (
  echo.
  echo [RawIron Multiplayer Sandbox] Process exited with error code %RC%.
  pause
)
exit /b %RC%
