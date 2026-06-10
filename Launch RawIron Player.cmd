@echo off

setlocal EnableExtensions

cd /d "%~dp0"



REM Optional: set RAWIRON_PLAYER_EXE=D:\full\path\RawIron.Player.exe



set "SCRIPTS=%~dp0Scripts"

set "TARGET_EXE="

if defined RAWIRON_PLAYER_EXE if exist "%RAWIRON_PLAYER_EXE%" set "TARGET_EXE=%RAWIRON_PLAYER_EXE%"



if not defined TARGET_EXE call "%SCRIPTS%\Resolve-RawIronBinary.cmd" "Apps\RawIron.Player" "RawIron.Player.exe" TARGET_EXE "%CD%"



if not defined TARGET_EXE (

  echo RawIron.Player.exe not found.

  echo   cmake --preset dev-msvc

  echo   cmake --build build\dev-msvc --config RelWithDebInfo --target RawIron.Player

  pause

  exit /b 1

)



echo Running: "%TARGET_EXE%" %*

"%TARGET_EXE%" %*

set "ERR=%ERRORLEVEL%"

if not "%ERR%"=="0" (

  echo RawIron.Player exited with code %ERR%.

  pause

  exit /b %ERR%

)

exit /b 0

