@echo off
setlocal
cd /d "%~dp0.."

echo Removes alternate CMake output trees that can shadow build\Apps\RawIron.Editor.exe
echo when using "Launch RawIron Editor.cmd". Does not delete build\Apps or build\Source.
echo.

for %%D in (dev-msvc dev-clang dev-mingw hygiene codex-gcc) do (
  if exist "build\%%D\" (
    echo Removing build\%%D\
    rd /s /q "build\%%D" 2>nul
    if exist "build\%%D\" echo   warning: could not fully remove build\%%D\  - close apps using files there and retry.
  )
)

echo.
echo Done. Rebuild with your usual preset if you still need one of those trees.
pause
