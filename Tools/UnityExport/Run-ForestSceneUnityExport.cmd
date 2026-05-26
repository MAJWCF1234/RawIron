@echo off
setlocal
cd /d "%~dp0..\.."
powershell -NoProfile -ExecutionPolicy Bypass -File "Tools\UnityExport\Run-ForestSceneUnityExport.ps1"
exit /b %ERRORLEVEL%
