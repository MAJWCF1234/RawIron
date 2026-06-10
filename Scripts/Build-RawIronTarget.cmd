@echo off
REM Configure (dev-msvc preset) if needed, then build one CMake target.
REM
REM Usage: call "%~dp0Build-RawIronTarget.cmd" TARGET [REPO_ROOT]

setlocal EnableExtensions
set "TARGET=%~1"
set "REPO_ROOT=%~2"
if not defined REPO_ROOT set "REPO_ROOT=%CD%"

if "%TARGET%"=="" (
  echo Usage: Build-RawIronTarget.cmd TARGET [REPO_ROOT]
  exit /b 1
)

cd /d "%REPO_ROOT%"
if errorlevel 1 (
  echo ERROR: repo root not found: %REPO_ROOT%
  exit /b 1
)

if not exist "build\dev-msvc\CMakeCache.txt" (
  echo Configuring build\dev-msvc ^(cmake --preset dev-msvc^)...
  cmake --preset dev-msvc
  if errorlevel 1 exit /b 1
)

echo Building %TARGET% in build\dev-msvc ...
cmake --build build\dev-msvc --config RelWithDebInfo --target %TARGET%
exit /b %errorlevel%
