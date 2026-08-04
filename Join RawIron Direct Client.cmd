@echo off
setlocal
rem Fast LAN join (no EOS wait). Use when host printed RI1:... or you know host IP.
rem Usage: Join RawIron Direct Client.cmd 192.168.1.113 27015

set "HOST=%~1"
set "PORT=%~2"
if "%HOST%"=="" set "HOST=127.0.0.1"
if "%PORT%"=="" set "PORT=27015"

cd /d "%~dp0"
call "%~dp0Games\RawIronMultiplayerSandbox\Launch RawIron Multiplayer Sandbox.cmd" --net-mode=client --rendezvous=direct --join-code=RI1:%HOST%:%PORT%:0 --no-hybrid-hdr %3 %4 %5 %6 %7 %8 %9
exit /b %ERRORLEVEL%
