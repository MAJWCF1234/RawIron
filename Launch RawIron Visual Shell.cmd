@echo off
setlocal
cd /d "%~dp0"

REM Prefer Debug over Release so iterative builds (CMake Debug) beat a stale Release binary.
REM Override: set RAWIRON_VISUAL_SHELL_EXE=D:\path\RawIron.VisualShell.exe before running this script.

set "TARGET_EXE="
if defined RAWIRON_VISUAL_SHELL_EXE if exist "%RAWIRON_VISUAL_SHELL_EXE%" set "TARGET_EXE=%RAWIRON_VISUAL_SHELL_EXE%"

if not defined TARGET_EXE if exist "build\dev-msvc\Apps\RawIron.VisualShell\Debug\RawIron.VisualShell.exe" set "TARGET_EXE=build\dev-msvc\Apps\RawIron.VisualShell\Debug\RawIron.VisualShell.exe"
if not defined TARGET_EXE if exist "build\dev-msvc\Apps\RawIron.VisualShell\RelWithDebInfo\RawIron.VisualShell.exe" set "TARGET_EXE=build\dev-msvc\Apps\RawIron.VisualShell\RelWithDebInfo\RawIron.VisualShell.exe"
if not defined TARGET_EXE if exist "build\dev-msvc\Apps\RawIron.VisualShell\Release\RawIron.VisualShell.exe" set "TARGET_EXE=build\dev-msvc\Apps\RawIron.VisualShell\Release\RawIron.VisualShell.exe"
if not defined TARGET_EXE if exist "build\dev-msvc\Apps\RawIron.VisualShell\MinSizeRel\RawIron.VisualShell.exe" set "TARGET_EXE=build\dev-msvc\Apps\RawIron.VisualShell\MinSizeRel\RawIron.VisualShell.exe"
if not defined TARGET_EXE if exist "build\dev-msvc\Apps\RawIron.VisualShell\RawIron.VisualShell.exe" set "TARGET_EXE=build\dev-msvc\Apps\RawIron.VisualShell\RawIron.VisualShell.exe"

if not defined TARGET_EXE if exist "build\dev-vs-community\Apps\RawIron.VisualShell\Debug\RawIron.VisualShell.exe" set "TARGET_EXE=build\dev-vs-community\Apps\RawIron.VisualShell\Debug\RawIron.VisualShell.exe"
if not defined TARGET_EXE if exist "build\dev-vs-community\Apps\RawIron.VisualShell\RelWithDebInfo\RawIron.VisualShell.exe" set "TARGET_EXE=build\dev-vs-community\Apps\RawIron.VisualShell\RelWithDebInfo\RawIron.VisualShell.exe"
if not defined TARGET_EXE if exist "build\dev-vs-community\Apps\RawIron.VisualShell\Release\RawIron.VisualShell.exe" set "TARGET_EXE=build\dev-vs-community\Apps\RawIron.VisualShell\Release\RawIron.VisualShell.exe"
if not defined TARGET_EXE if exist "build\dev-vs-community\Apps\RawIron.VisualShell\MinSizeRel\RawIron.VisualShell.exe" set "TARGET_EXE=build\dev-vs-community\Apps\RawIron.VisualShell\MinSizeRel\RawIron.VisualShell.exe"
if not defined TARGET_EXE if exist "build\dev-vs-community\Apps\RawIron.VisualShell\RawIron.VisualShell.exe" set "TARGET_EXE=build\dev-vs-community\Apps\RawIron.VisualShell\RawIron.VisualShell.exe"

if not defined TARGET_EXE if exist "build\dev-clang\Apps\RawIron.VisualShell\Debug\RawIron.VisualShell.exe" set "TARGET_EXE=build\dev-clang\Apps\RawIron.VisualShell\Debug\RawIron.VisualShell.exe"
if not defined TARGET_EXE if exist "build\dev-clang\Apps\RawIron.VisualShell\RelWithDebInfo\RawIron.VisualShell.exe" set "TARGET_EXE=build\dev-clang\Apps\RawIron.VisualShell\RelWithDebInfo\RawIron.VisualShell.exe"
if not defined TARGET_EXE if exist "build\dev-clang\Apps\RawIron.VisualShell\Release\RawIron.VisualShell.exe" set "TARGET_EXE=build\dev-clang\Apps\RawIron.VisualShell\Release\RawIron.VisualShell.exe"
if not defined TARGET_EXE if exist "build\dev-clang\Apps\RawIron.VisualShell\MinSizeRel\RawIron.VisualShell.exe" set "TARGET_EXE=build\dev-clang\Apps\RawIron.VisualShell\MinSizeRel\RawIron.VisualShell.exe"
if not defined TARGET_EXE if exist "build\dev-clang\Apps\RawIron.VisualShell\RawIron.VisualShell.exe" set "TARGET_EXE=build\dev-clang\Apps\RawIron.VisualShell\RawIron.VisualShell.exe"

if not defined TARGET_EXE if exist "build\dev-mingw\Apps\RawIron.VisualShell\Debug\RawIron.VisualShell.exe" set "TARGET_EXE=build\dev-mingw\Apps\RawIron.VisualShell\Debug\RawIron.VisualShell.exe"
if not defined TARGET_EXE if exist "build\dev-mingw\Apps\RawIron.VisualShell\RelWithDebInfo\RawIron.VisualShell.exe" set "TARGET_EXE=build\dev-mingw\Apps\RawIron.VisualShell\RelWithDebInfo\RawIron.VisualShell.exe"
if not defined TARGET_EXE if exist "build\dev-mingw\Apps\RawIron.VisualShell\Release\RawIron.VisualShell.exe" set "TARGET_EXE=build\dev-mingw\Apps\RawIron.VisualShell\Release\RawIron.VisualShell.exe"
if not defined TARGET_EXE if exist "build\dev-mingw\Apps\RawIron.VisualShell\MinSizeRel\RawIron.VisualShell.exe" set "TARGET_EXE=build\dev-mingw\Apps\RawIron.VisualShell\MinSizeRel\RawIron.VisualShell.exe"
if not defined TARGET_EXE if exist "build\dev-mingw\Apps\RawIron.VisualShell\RawIron.VisualShell.exe" set "TARGET_EXE=build\dev-mingw\Apps\RawIron.VisualShell\RawIron.VisualShell.exe"

if not defined TARGET_EXE if exist "build\hygiene\Apps\RawIron.VisualShell\Debug\RawIron.VisualShell.exe" set "TARGET_EXE=build\hygiene\Apps\RawIron.VisualShell\Debug\RawIron.VisualShell.exe"
if not defined TARGET_EXE if exist "build\hygiene\Apps\RawIron.VisualShell\RelWithDebInfo\RawIron.VisualShell.exe" set "TARGET_EXE=build\hygiene\Apps\RawIron.VisualShell\RelWithDebInfo\RawIron.VisualShell.exe"
if not defined TARGET_EXE if exist "build\hygiene\Apps\RawIron.VisualShell\Release\RawIron.VisualShell.exe" set "TARGET_EXE=build\hygiene\Apps\RawIron.VisualShell\Release\RawIron.VisualShell.exe"
if not defined TARGET_EXE if exist "build\hygiene\Apps\RawIron.VisualShell\MinSizeRel\RawIron.VisualShell.exe" set "TARGET_EXE=build\hygiene\Apps\RawIron.VisualShell\MinSizeRel\RawIron.VisualShell.exe"
if not defined TARGET_EXE if exist "build\hygiene\Apps\RawIron.VisualShell\RawIron.VisualShell.exe" set "TARGET_EXE=build\hygiene\Apps\RawIron.VisualShell\RawIron.VisualShell.exe"

if not defined TARGET_EXE if exist "build\Apps\RawIron.VisualShell\Debug\RawIron.VisualShell.exe" set "TARGET_EXE=build\Apps\RawIron.VisualShell\Debug\RawIron.VisualShell.exe"
if not defined TARGET_EXE if exist "build\Apps\RawIron.VisualShell\RelWithDebInfo\RawIron.VisualShell.exe" set "TARGET_EXE=build\Apps\RawIron.VisualShell\RelWithDebInfo\RawIron.VisualShell.exe"
if not defined TARGET_EXE if exist "build\Apps\RawIron.VisualShell\Release\RawIron.VisualShell.exe" set "TARGET_EXE=build\Apps\RawIron.VisualShell\Release\RawIron.VisualShell.exe"
if not defined TARGET_EXE if exist "build\Apps\RawIron.VisualShell\MinSizeRel\RawIron.VisualShell.exe" set "TARGET_EXE=build\Apps\RawIron.VisualShell\MinSizeRel\RawIron.VisualShell.exe"
if not defined TARGET_EXE if exist "build\Apps\RawIron.VisualShell\RawIron.VisualShell.exe" set "TARGET_EXE=build\Apps\RawIron.VisualShell\RawIron.VisualShell.exe"

if not defined TARGET_EXE if exist "build\codex-gcc\Apps\RawIron.VisualShell\RawIron.VisualShell.exe" set "TARGET_EXE=build\codex-gcc\Apps\RawIron.VisualShell\RawIron.VisualShell.exe"

if not defined TARGET_EXE goto :missing_shell

call :stage_runtime "%TARGET_EXE%"
call :launch_with_msvc_runtime "%TARGET_EXE%"
if %errorlevel% equ 0 exit /b 0

echo Could not launch RawIron.VisualShell with Visual C++ runtime environment.
echo If this binary is a Debug build, it may require VCRUNTIME140D.dll and ucrtbased.dll.
echo Rebuild with a non-Debug preset (RelWithDebInfo/Release) for standalone launching.
echo Install the Visual C++ Redistributable and try again:
echo https://aka.ms/vs/17/release/vc_redist.x64.exe
pause
exit /b 1

:stage_runtime
set "EXE_PATH=%~1"
for %%I in ("%EXE_PATH%") do set "EXE_DIR=%%~dpI"
for %%D in (vcruntime140.dll vcruntime140_1.dll msvcp140.dll concrt140.dll) do (
  if exist "C:\Windows\System32\%%D" (
    copy /y "C:\Windows\System32\%%D" "%EXE_DIR%%%D" >nul
  )
)
exit /b 0

:launch_with_msvc_runtime
set "EXE_PATH=%~1"
shift
set "VCVARS="
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
if not defined VCVARS if exist "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if not defined VCVARS if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
if not defined VCVARS if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"

if defined VCVARS (
  call "%VCVARS%" >nul 2>nul
)

start "" "%EXE_PATH%" %*
exit /b %errorlevel%

:missing_shell

echo RawIron.VisualShell.exe was not found.
echo Build the project first with CMake, then run this launcher again.
pause
