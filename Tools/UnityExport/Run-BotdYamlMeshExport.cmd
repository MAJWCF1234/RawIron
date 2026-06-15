@echo off
setlocal
cd /d "%~dp0..\..\"
py -3 "%~dp0export_botd_unity_yaml_meshes.py"
exit /b %ERRORLEVEL%
