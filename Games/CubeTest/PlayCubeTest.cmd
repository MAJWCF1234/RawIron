@echo off
setlocal
set "ROOT=%~dp0..\.."
set "GAME=%ROOT%\build\dev-msvc\Games\CubeTest\App\RelWithDebInfo\RawIron.CubeTestGame.exe"
if not exist "%GAME%" (
  echo Cube Test is not built yet. Build RawIron.CubeTestGame in RelWithDebInfo first.
  pause
  exit /b 1
)
pushd "%ROOT%"
"%GAME%" --workspace-root=. --game-root=Games\CubeTest --start-room=baseline
popd
