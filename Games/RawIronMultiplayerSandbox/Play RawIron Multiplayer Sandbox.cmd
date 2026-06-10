@echo off
setlocal

if "%~1"=="" (
  call "%~dp0Launch RawIron Multiplayer Sandbox.cmd" --net-mode=listen --issue-join-code --bots=32
) else (
  call "%~dp0Launch RawIron Multiplayer Sandbox.cmd" %*
)
exit /b %ERRORLEVEL%
