@echo off
REM Resolves a built RawIron executable to a single canonical layout.
REM
REM Usage: call "%~dp0Resolve-RawIronBinary.cmd" RELATIVE_DIR EXE_NAME OUT_VAR [REPO_ROOT]
REM
REM Search order (first hit wins):
REM   1. <repo>\build\dev-msvc\<RELATIVE_DIR>\<Config>\<EXE_NAME>
REM   2. %LOCALAPPDATA%\RawIron\cmake-build\dev-msvc\...  (dev-msvc-localappdata preset)
REM
REM Config subdirs tried: RelWithDebInfo, Release, Debug, MinSizeRel, then flat (Ninja).
REM Sets OUT_VAR to the full path, or clears it. ERRORLEVEL 0 = found, 1 = not found.

setlocal EnableExtensions EnableDelayedExpansion

if "%~3"=="" (
  echo Usage: Resolve-RawIronBinary.cmd RELATIVE_DIR EXE_NAME OUT_VAR [REPO_ROOT]
  exit /b 1
)

set "REL_DIR=%~1"
set "EXE_NAME=%~2"
set "OUT_VAR=%~3"
set "REPO_ROOT=%~4"
if not defined REPO_ROOT set "REPO_ROOT=%CD%"

set "FOUND="
call :scan_root "%REPO_ROOT%\build\dev-msvc"
if not defined FOUND call :scan_root "%LOCALAPPDATA%\RawIron\cmake-build\dev-msvc"

if defined FOUND (
  endlocal & set "%OUT_VAR%=%FOUND%" & exit /b 0
)
endlocal & set "%OUT_VAR%=" & exit /b 1

:scan_root
set "ROOT=%~1"
if not exist "%ROOT%\CMakeCache.txt" exit /b 0
for %%C in (RelWithDebInfo Release Debug MinSizeRel) do (
  if not defined FOUND if exist "%ROOT%\%REL_DIR%\%%C\%EXE_NAME%" set "FOUND=%ROOT%\%REL_DIR%\%%C\%EXE_NAME%"
)
if not defined FOUND if exist "%ROOT%\%REL_DIR%\%EXE_NAME%" set "FOUND=%ROOT%\%REL_DIR%\%EXE_NAME%"
exit /b 0
