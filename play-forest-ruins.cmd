@echo off
setlocal
REM Repo-root shortcut documented in Games/WildernessRuins/README.md
call "%~dp0Games\WildernessRuins\Play Wilderness Ruins.cmd" %*
exit /b %ERRORLEVEL%
