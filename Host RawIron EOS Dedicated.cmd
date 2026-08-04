@echo off
setlocal
rem Host a dedicated RawIron session with EOS join codes.
rem Share the printed "EOS:<lobbyId>" with clients.
rem Usage: Host RawIron EOS Dedicated.cmd [advertise-host] [extra server flags...]
rem Leave advertise-host empty to auto-detect the LAN address clients should dial.

cd /d "%~dp0"
set "EXE=%~dp0build\dev-msvc-network\Apps\RawIron.DedicatedServer\RelWithDebInfo\RawIron.DedicatedServer.exe"
if not exist "%EXE%" (
  echo Build missing. Run:
  echo   cmake --preset dev-msvc-network
  echo   cmake --build build/dev-msvc-network --config RelWithDebInfo --target RawIron.DedicatedServer
  exit /b 1
)

"%EXE%" --rendezvous=eos --issue-join-code --port 27015 --advertise-host=%~1 %2 %3 %4 %5 %6 %7 %8 %9
exit /b %ERRORLEVEL%
