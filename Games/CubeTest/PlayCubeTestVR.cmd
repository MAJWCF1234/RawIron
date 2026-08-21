@echo off
setlocal
set "ROOT=%~dp0..\.."
set "VR=%ROOT%\build\dev-msvc\Apps\RawIron.VRShowcase\RelWithDebInfo\RawIron.VRShowcase.exe"
if not exist "%VR%" (
  echo Raw Iron VR Showcase is not built yet. Build RawIron.VRShowcase in RelWithDebInfo first.
  pause
  exit /b 1
)
pushd "%ROOT%"
"%VR%" --steamvr --start-room=interaction --frames=36000 --net-mode=offline
popd
