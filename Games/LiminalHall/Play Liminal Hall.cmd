@echo off
setlocal

set "MANIFEST=%~dp0manifest.json"
if not exist "%MANIFEST%" (
  echo Missing manifest.json in LiminalHall folder.
  exit /b 1
)
findstr /c:"\"format\": \"rawiron-game-v1.3.5\"" "%MANIFEST%" >nul || (
  echo LiminalHall manifest format is not rawiron-game-v1.3.5.
  exit /b 1
)
if not exist "%~dp0scripts\audio.riscript" (
  echo Missing required scripts/audio.riscript in LiminalHall folder.
  exit /b 1
)
if not exist "%~dp0scripts\streaming.riscript" (
  echo Missing required scripts/streaming.riscript in LiminalHall folder.
  exit /b 1
)
if not exist "%~dp0config\game.cfg" (
  echo Missing required config/game.cfg in LiminalHall folder.
  exit /b 1
)
if not exist "%~dp0scripts\localization.riscript" (
  echo Missing required scripts/localization.riscript in LiminalHall folder.
  exit /b 1
)
if not exist "%~dp0scripts\physics.riscript" (
  echo Missing required scripts/physics.riscript in LiminalHall folder.
  exit /b 1
)
if not exist "%~dp0scripts\postprocess.riscript" (
  echo Missing required scripts/postprocess.riscript in LiminalHall folder.
  exit /b 1
)
if not exist "%~dp0scripts\init.riscript" (
  echo Missing required scripts/init.riscript in LiminalHall folder.
  exit /b 1
)
if not exist "%~dp0scripts\state.riscript" (
  echo Missing required scripts/state.riscript in LiminalHall folder.
  exit /b 1
)
if not exist "%~dp0scripts\network.riscript" (
  echo Missing required scripts/network.riscript in LiminalHall folder.
  exit /b 1
)
if not exist "%~dp0scripts\persistence.riscript" (
  echo Missing required scripts/persistence.riscript in LiminalHall folder.
  exit /b 1
)
if not exist "%~dp0scripts\ai.riscript" (
  echo Missing required scripts/ai.riscript in LiminalHall folder.
  exit /b 1
)
if not exist "%~dp0scripts\plugins.riscript" (
  echo Missing required scripts/plugins.riscript in LiminalHall folder.
  exit /b 1
)
if not exist "%~dp0scripts\animation.riscript" (
  echo Missing required scripts/animation.riscript in LiminalHall folder.
  exit /b 1
)
if not exist "%~dp0scripts\vfx.riscript" (
  echo Missing required scripts/vfx.riscript in LiminalHall folder.
  exit /b 1
)
if not exist "%~dp0config\input.map" (
  echo Missing required config/input.map in LiminalHall folder.
  exit /b 1
)
if not exist "%~dp0config\project.dev" (
  echo Missing required config/project.dev in LiminalHall folder.
  exit /b 1
)
if not exist "%~dp0config\network.cfg" (
  echo Missing required config/network.cfg in LiminalHall folder.
  exit /b 1
)
if not exist "%~dp0config\build.profile" (
  echo Missing required config/build.profile in LiminalHall folder.
  exit /b 1
)
if not exist "%~dp0config\security.policy" (
  echo Missing required config/security.policy in LiminalHall folder.
  exit /b 1
)
if not exist "%~dp0config\plugins.policy" (
  echo Missing required config/plugins.policy in LiminalHall folder.
  exit /b 1
)
if not exist "%~dp0levels\assembly.navmesh" (
  echo Missing required levels/assembly.navmesh in LiminalHall folder.
  exit /b 1
)
if not exist "%~dp0levels\assembly.zones.csv" (
  echo Missing required levels/assembly.zones.csv in LiminalHall folder.
  exit /b 1
)
if not exist "%~dp0levels\assembly.ai.nodes" (
  echo Missing required levels/assembly.ai.nodes in LiminalHall folder.
  exit /b 1
)
if not exist "%~dp0levels\assembly.lighting.csv" (
  echo Missing required levels/assembly.lighting.csv in LiminalHall folder.
  exit /b 1
)
if not exist "%~dp0levels\assembly.cinematics.csv" (
  echo Missing required levels/assembly.cinematics.csv in LiminalHall folder.
  exit /b 1
)
if not exist "%~dp0assets\manifest.assets" (
  echo Missing required assets/manifest.assets in LiminalHall folder.
  exit /b 1
)
if not exist "%~dp0assets\metadata.json" (
  echo Missing required assets/metadata.json in LiminalHall folder.
  exit /b 1
)
if not exist "%~dp0assets\layers.config" (
  echo Missing required assets/layers.config in LiminalHall folder.
  exit /b 1
)
if not exist "%~dp0assets\dependencies.json" (
  echo Missing required assets/dependencies.json in LiminalHall folder.
  exit /b 1
)
if not exist "%~dp0assets\streaming.manifest" (
  echo Missing required assets/streaming.manifest in LiminalHall folder.
  exit /b 1
)
if not exist "%~dp0assets\shaders.manifest" (
  echo Missing required assets/shaders.manifest in LiminalHall folder.
  exit /b 1
)
if not exist "%~dp0assets\animation.graph" (
  echo Missing required assets/animation.graph in LiminalHall folder.
  exit /b 1
)
if not exist "%~dp0assets\vfx.manifest" (
  echo Missing required assets/vfx.manifest in LiminalHall folder.
  exit /b 1
)
if not exist "%~dp0data\schema.db" (
  echo Missing required data/schema.db in LiminalHall folder.
  exit /b 1
)
if not exist "%~dp0data\lookup.index" (
  echo Missing required data/lookup.index in LiminalHall folder.
  exit /b 1
)
if not exist "%~dp0data\entity.registry" (
  echo Missing required data/entity.registry in LiminalHall folder.
  exit /b 1
)
if not exist "%~dp0data\telemetry.db" (
  echo Missing required data/telemetry.db in LiminalHall folder.
  exit /b 1
)
if not exist "%~dp0plugins\manifest.plugins" (
  echo Missing required plugins/manifest.plugins in LiminalHall folder.
  exit /b 1
)
if not exist "%~dp0plugins\load_order.cfg" (
  echo Missing required plugins/load_order.cfg in LiminalHall folder.
  exit /b 1
)
if not exist "%~dp0plugins\registry.json" (
  echo Missing required plugins/registry.json in LiminalHall folder.
  exit /b 1
)
if not exist "%~dp0plugins\hooks.riplugin" (
  echo Missing required plugins/hooks.riplugin in LiminalHall folder.
  exit /b 1
)
if not exist "%~dp0ai\behavior.tree" (
  echo Missing required ai/behavior.tree in LiminalHall folder.
  exit /b 1
)
if not exist "%~dp0ai\blackboard.json" (
  echo Missing required ai/blackboard.json in LiminalHall folder.
  exit /b 1
)
if not exist "%~dp0ai\factions.cfg" (
  echo Missing required ai/factions.cfg in LiminalHall folder.
  exit /b 1
)

cd /d "%~dp0..\.."

set "TARGET_EXE="

REM Prefer game-local build output first (Games\LiminalHall\App), then legacy Apps path.
if not defined TARGET_EXE if exist "build\dev-msvc\Games\LiminalHall\App\Release\RawIron.LiminalGame.exe" set "TARGET_EXE=build\dev-msvc\Games\LiminalHall\App\Release\RawIron.LiminalGame.exe"
if not defined TARGET_EXE if exist "build\dev-msvc\Games\LiminalHall\App\RelWithDebInfo\RawIron.LiminalGame.exe" set "TARGET_EXE=build\dev-msvc\Games\LiminalHall\App\RelWithDebInfo\RawIron.LiminalGame.exe"
if not defined TARGET_EXE if exist "build\dev-msvc\Games\LiminalHall\App\Debug\RawIron.LiminalGame.exe" set "TARGET_EXE=build\dev-msvc\Games\LiminalHall\App\Debug\RawIron.LiminalGame.exe"
if not defined TARGET_EXE if exist "build\dev-msvc\Games\LiminalHall\App\MinSizeRel\RawIron.LiminalGame.exe" set "TARGET_EXE=build\dev-msvc\Games\LiminalHall\App\MinSizeRel\RawIron.LiminalGame.exe"
if not defined TARGET_EXE if exist "build\dev-msvc\Games\LiminalHall\App\RawIron.LiminalGame.exe" set "TARGET_EXE=build\dev-msvc\Games\LiminalHall\App\RawIron.LiminalGame.exe"

if not defined TARGET_EXE if exist "build\dev-clang\Games\LiminalHall\App\Release\RawIron.LiminalGame.exe" set "TARGET_EXE=build\dev-clang\Games\LiminalHall\App\Release\RawIron.LiminalGame.exe"
if not defined TARGET_EXE if exist "build\dev-clang\Games\LiminalHall\App\RelWithDebInfo\RawIron.LiminalGame.exe" set "TARGET_EXE=build\dev-clang\Games\LiminalHall\App\RelWithDebInfo\RawIron.LiminalGame.exe"
if not defined TARGET_EXE if exist "build\dev-clang\Games\LiminalHall\App\Debug\RawIron.LiminalGame.exe" set "TARGET_EXE=build\dev-clang\Games\LiminalHall\App\Debug\RawIron.LiminalGame.exe"
if not defined TARGET_EXE if exist "build\dev-clang\Games\LiminalHall\App\MinSizeRel\RawIron.LiminalGame.exe" set "TARGET_EXE=build\dev-clang\Games\LiminalHall\App\MinSizeRel\RawIron.LiminalGame.exe"
if not defined TARGET_EXE if exist "build\dev-clang\Games\LiminalHall\App\RawIron.LiminalGame.exe" set "TARGET_EXE=build\dev-clang\Games\LiminalHall\App\RawIron.LiminalGame.exe"

if not defined TARGET_EXE if exist "build\dev-mingw\Games\LiminalHall\App\Release\RawIron.LiminalGame.exe" set "TARGET_EXE=build\dev-mingw\Games\LiminalHall\App\Release\RawIron.LiminalGame.exe"
if not defined TARGET_EXE if exist "build\dev-mingw\Games\LiminalHall\App\RelWithDebInfo\RawIron.LiminalGame.exe" set "TARGET_EXE=build\dev-mingw\Games\LiminalHall\App\RelWithDebInfo\RawIron.LiminalGame.exe"
if not defined TARGET_EXE if exist "build\dev-mingw\Games\LiminalHall\App\Debug\RawIron.LiminalGame.exe" set "TARGET_EXE=build\dev-mingw\Games\LiminalHall\App\Debug\RawIron.LiminalGame.exe"
if not defined TARGET_EXE if exist "build\dev-mingw\Games\LiminalHall\App\MinSizeRel\RawIron.LiminalGame.exe" set "TARGET_EXE=build\dev-mingw\Games\LiminalHall\App\MinSizeRel\RawIron.LiminalGame.exe"
if not defined TARGET_EXE if exist "build\dev-mingw\Games\LiminalHall\App\RawIron.LiminalGame.exe" set "TARGET_EXE=build\dev-mingw\Games\LiminalHall\App\RawIron.LiminalGame.exe"

if not defined TARGET_EXE if exist "build\Games\LiminalHall\App\Release\RawIron.LiminalGame.exe" set "TARGET_EXE=build\Games\LiminalHall\App\Release\RawIron.LiminalGame.exe"
if not defined TARGET_EXE if exist "build\Games\LiminalHall\App\RelWithDebInfo\RawIron.LiminalGame.exe" set "TARGET_EXE=build\Games\LiminalHall\App\RelWithDebInfo\RawIron.LiminalGame.exe"
if not defined TARGET_EXE if exist "build\Games\LiminalHall\App\Debug\RawIron.LiminalGame.exe" set "TARGET_EXE=build\Games\LiminalHall\App\Debug\RawIron.LiminalGame.exe"
if not defined TARGET_EXE if exist "build\Games\LiminalHall\App\MinSizeRel\RawIron.LiminalGame.exe" set "TARGET_EXE=build\Games\LiminalHall\App\MinSizeRel\RawIron.LiminalGame.exe"
if not defined TARGET_EXE if exist "build\Games\LiminalHall\App\RawIron.LiminalGame.exe" set "TARGET_EXE=build\Games\LiminalHall\App\RawIron.LiminalGame.exe"

if not defined TARGET_EXE if exist "build\Apps\RawIron.LiminalGame\Release\RawIron.LiminalGame.exe" set "TARGET_EXE=build\Apps\RawIron.LiminalGame\Release\RawIron.LiminalGame.exe"
if not defined TARGET_EXE if exist "build\Apps\RawIron.LiminalGame\RelWithDebInfo\RawIron.LiminalGame.exe" set "TARGET_EXE=build\Apps\RawIron.LiminalGame\RelWithDebInfo\RawIron.LiminalGame.exe"
if not defined TARGET_EXE if exist "build\Apps\RawIron.LiminalGame\Debug\RawIron.LiminalGame.exe" set "TARGET_EXE=build\Apps\RawIron.LiminalGame\Debug\RawIron.LiminalGame.exe"

if not defined TARGET_EXE goto :missing_liminal

echo Launching %TARGET_EXE%
set "DEFAULT_ARGS="
if "%~1"=="" set "DEFAULT_ARGS=--renderer=vulkan --width=2560 --height=1440"
call :stage_runtime "%TARGET_EXE%"
call :launch_with_msvc_runtime "%TARGET_EXE%" %DEFAULT_ARGS% %*
if %errorlevel% equ 0 exit /b 0

echo Could not launch RawIron.LiminalGame with Visual C++ runtime environment.
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

:missing_liminal
echo RawIron.LiminalGame.exe was not found.
echo Build the project first with CMake, then run this launcher again.
pause
exit /b 1

