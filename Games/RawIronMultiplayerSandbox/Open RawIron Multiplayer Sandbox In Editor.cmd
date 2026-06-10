@echo off
setlocal

call "%~dp0..\..\Launch RawIron Editor.cmd" --game=rawiron-multiplayer-sandbox %*
exit /b %ERRORLEVEL%
