@echo off
setlocal
cd /d "%~dp0..\.."

REM Override: set RAWIRON_PARTICLE_SHOWCASE_EXE=D:\path\RawIron.ParticleShowcase.exe

set "SCRIPTS=%CD%\Scripts"
set "TARGET_EXE="
if defined RAWIRON_PARTICLE_SHOWCASE_EXE if exist "%RAWIRON_PARTICLE_SHOWCASE_EXE%" set "TARGET_EXE=%RAWIRON_PARTICLE_SHOWCASE_EXE%"

if not defined TARGET_EXE call "%SCRIPTS%\Resolve-RawIronBinary.cmd" "Apps\RawIron.ParticleShowcase" "RawIron.ParticleShowcase.exe" TARGET_EXE "%CD%"
if not defined TARGET_EXE goto :missing_showcase

for %%I in ("%TARGET_EXE%") do set "TARGET_EXE=%%~fI"

call "%SCRIPTS%\Launch-WithMsvcRuntime.cmd" "%TARGET_EXE%" --workspace "%CD%" %*
if %errorlevel% equ 0 exit /b 0

echo Could not launch RawIron.ParticleShowcase with Visual C++ runtime environment.
echo Rebuild with RelWithDebInfo or Release, or install the VC++ Redistributable:
echo https://aka.ms/vs/17/release/vc_redist.x64.exe
pause
exit /b 1

:missing_showcase
echo RawIron.ParticleShowcase.exe was not found under build\dev-msvc.
echo   cmake --preset dev-msvc
echo   cmake --build build\dev-msvc --config RelWithDebInfo --target RawIron.ParticleShowcase
pause
exit /b 1
