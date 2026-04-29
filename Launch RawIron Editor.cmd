@echo off
setlocal
cd /d "%~dp0"

set "TARGET_EXE="

rem Optional: full path to the editor exe you want (wins over all paths below).
if not "%RAWIRON_EDITOR_EXE%"=="" if exist "%RAWIRON_EDITOR_EXE%" set "TARGET_EXE=%RAWIRON_EDITOR_EXE%"

if not defined TARGET_EXE if exist "build\dev-msvc\Apps\RawIron.Editor\Release\RawIron.Editor.exe" set "TARGET_EXE=build\dev-msvc\Apps\RawIron.Editor\Release\RawIron.Editor.exe"
if not defined TARGET_EXE if exist "build\dev-msvc\Apps\RawIron.Editor\RelWithDebInfo\RawIron.Editor.exe" set "TARGET_EXE=build\dev-msvc\Apps\RawIron.Editor\RelWithDebInfo\RawIron.Editor.exe"
if not defined TARGET_EXE if exist "build\dev-msvc\Apps\RawIron.Editor\Debug\RawIron.Editor.exe" set "TARGET_EXE=build\dev-msvc\Apps\RawIron.Editor\Debug\RawIron.Editor.exe"
if not defined TARGET_EXE if exist "build\dev-msvc\Apps\RawIron.Editor\MinSizeRel\RawIron.Editor.exe" set "TARGET_EXE=build\dev-msvc\Apps\RawIron.Editor\MinSizeRel\RawIron.Editor.exe"
if not defined TARGET_EXE if exist "build\dev-msvc\Apps\RawIron.Editor\RawIron.Editor.exe" set "TARGET_EXE=build\dev-msvc\Apps\RawIron.Editor\RawIron.Editor.exe"

if not defined TARGET_EXE if exist "build\Apps\RawIron.Editor\Release\RawIron.Editor.exe" set "TARGET_EXE=build\Apps\RawIron.Editor\Release\RawIron.Editor.exe"
if not defined TARGET_EXE if exist "build\Apps\RawIron.Editor\RelWithDebInfo\RawIron.Editor.exe" set "TARGET_EXE=build\Apps\RawIron.Editor\RelWithDebInfo\RawIron.Editor.exe"
if not defined TARGET_EXE if exist "build\Apps\RawIron.Editor\Debug\RawIron.Editor.exe" set "TARGET_EXE=build\Apps\RawIron.Editor\Debug\RawIron.Editor.exe"
if not defined TARGET_EXE if exist "build\Apps\RawIron.Editor\MinSizeRel\RawIron.Editor.exe" set "TARGET_EXE=build\Apps\RawIron.Editor\MinSizeRel\RawIron.Editor.exe"
if not defined TARGET_EXE if exist "build\Apps\RawIron.Editor\RawIron.Editor.exe" set "TARGET_EXE=build\Apps\RawIron.Editor\RawIron.Editor.exe"

if not defined TARGET_EXE if exist "build\dev-clang\Apps\RawIron.Editor\Release\RawIron.Editor.exe" set "TARGET_EXE=build\dev-clang\Apps\RawIron.Editor\Release\RawIron.Editor.exe"
if not defined TARGET_EXE if exist "build\dev-clang\Apps\RawIron.Editor\RelWithDebInfo\RawIron.Editor.exe" set "TARGET_EXE=build\dev-clang\Apps\RawIron.Editor\RelWithDebInfo\RawIron.Editor.exe"
if not defined TARGET_EXE if exist "build\dev-clang\Apps\RawIron.Editor\Debug\RawIron.Editor.exe" set "TARGET_EXE=build\dev-clang\Apps\RawIron.Editor\Debug\RawIron.Editor.exe"
if not defined TARGET_EXE if exist "build\dev-clang\Apps\RawIron.Editor\MinSizeRel\RawIron.Editor.exe" set "TARGET_EXE=build\dev-clang\Apps\RawIron.Editor\MinSizeRel\RawIron.Editor.exe"
if not defined TARGET_EXE if exist "build\dev-clang\Apps\RawIron.Editor\RawIron.Editor.exe" set "TARGET_EXE=build\dev-clang\Apps\RawIron.Editor\RawIron.Editor.exe"

if not defined TARGET_EXE if exist "build\dev-mingw\Apps\RawIron.Editor\Release\RawIron.Editor.exe" set "TARGET_EXE=build\dev-mingw\Apps\RawIron.Editor\Release\RawIron.Editor.exe"
if not defined TARGET_EXE if exist "build\dev-mingw\Apps\RawIron.Editor\RelWithDebInfo\RawIron.Editor.exe" set "TARGET_EXE=build\dev-mingw\Apps\RawIron.Editor\RelWithDebInfo\RawIron.Editor.exe"
if not defined TARGET_EXE if exist "build\dev-mingw\Apps\RawIron.Editor\Debug\RawIron.Editor.exe" set "TARGET_EXE=build\dev-mingw\Apps\RawIron.Editor\Debug\RawIron.Editor.exe"
if not defined TARGET_EXE if exist "build\dev-mingw\Apps\RawIron.Editor\MinSizeRel\RawIron.Editor.exe" set "TARGET_EXE=build\dev-mingw\Apps\RawIron.Editor\MinSizeRel\RawIron.Editor.exe"
if not defined TARGET_EXE if exist "build\dev-mingw\Apps\RawIron.Editor\RawIron.Editor.exe" set "TARGET_EXE=build\dev-mingw\Apps\RawIron.Editor\RawIron.Editor.exe"

if not defined TARGET_EXE if exist "build\hygiene\Apps\RawIron.Editor\Release\RawIron.Editor.exe" set "TARGET_EXE=build\hygiene\Apps\RawIron.Editor\Release\RawIron.Editor.exe"
if not defined TARGET_EXE if exist "build\hygiene\Apps\RawIron.Editor\RelWithDebInfo\RawIron.Editor.exe" set "TARGET_EXE=build\hygiene\Apps\RawIron.Editor\RelWithDebInfo\RawIron.Editor.exe"
if not defined TARGET_EXE if exist "build\hygiene\Apps\RawIron.Editor\Debug\RawIron.Editor.exe" set "TARGET_EXE=build\hygiene\Apps\RawIron.Editor\Debug\RawIron.Editor.exe"
if not defined TARGET_EXE if exist "build\hygiene\Apps\RawIron.Editor\MinSizeRel\RawIron.Editor.exe" set "TARGET_EXE=build\hygiene\Apps\RawIron.Editor\MinSizeRel\RawIron.Editor.exe"
if not defined TARGET_EXE if exist "build\hygiene\Apps\RawIron.Editor\RawIron.Editor.exe" set "TARGET_EXE=build\hygiene\Apps\RawIron.Editor\RawIron.Editor.exe"

if not defined TARGET_EXE if exist "build\codex-gcc\Apps\RawIron.Editor\RawIron.Editor.exe" set "TARGET_EXE=build\codex-gcc\Apps\RawIron.Editor\RawIron.Editor.exe"

if not defined TARGET_EXE goto :missing_editor
for %%I in ("%TARGET_EXE%") do set "TARGET_EXE=%%~fI"

call :stage_runtime "%TARGET_EXE%"
call :launch_with_msvc_runtime "%TARGET_EXE%" --editor-ui %*
if %errorlevel% equ 0 exit /b 0

echo Could not launch RawIron.Editor with Visual C++ runtime environment.
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

:missing_editor
echo RawIron.Editor.exe was not found.
echo Build the project first with CMake, then run this launcher again.
pause
