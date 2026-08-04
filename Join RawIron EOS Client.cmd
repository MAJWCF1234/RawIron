@echo off
setlocal
rem Join a RawIron Multiplayer Sandbox session via EOS lobby id.
rem Usage: Join RawIron EOS Client.cmd EOS:<lobbyId>
rem Tip: same-LAN play is snappier with "Join RawIron Direct Client.cmd <ip> <port>"

if "%~1"=="" (
  echo Usage: %~nx0 EOS:^<lobbyId^>
  exit /b 1
)

cd /d "%~dp0"
call "%~dp0Games\RawIronMultiplayerSandbox\Launch RawIron Multiplayer Sandbox.cmd" --net-mode=client --rendezvous=eos --join-code=%~1 --no-hybrid-hdr %2 %3 %4 %5 %6 %7 %8 %9
exit /b %ERRORLEVEL%
