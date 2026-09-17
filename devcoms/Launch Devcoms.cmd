@echo off
cd /d "%~dp0"
where py >nul 2>nul
if %errorlevel%==0 (
  start "" "http://127.0.0.1:7420"
  py -3 server.py
  exit /b %errorlevel%
)
where python >nul 2>nul
if %errorlevel%==0 (
  start "" "http://127.0.0.1:7420"
  python server.py
  exit /b %errorlevel%
)
echo Python was not found. Install Python 3, then run server.py from this folder.
pause
exit /b 1
