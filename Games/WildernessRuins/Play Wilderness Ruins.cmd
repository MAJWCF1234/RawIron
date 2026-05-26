@echo off
setlocal

set "MANIFEST=%~dp0manifest.json"
if not exist "%MANIFEST%" (
  echo Missing manifest.json in WildernessRuins folder.
  exit /b 1
)
findstr /c:"\"format\": \"rawiron-game-v1.3.7\"" "%MANIFEST%" >nul && goto wilderness_play_fmt_ok
findstr /c:"\"format\": \"rawiron-game-v1.3.6\"" "%MANIFEST%" >nul && goto wilderness_play_fmt_ok
findstr /c:"\"format\": \"rawiron-game-v1.3.5\"" "%MANIFEST%" >nul && goto wilderness_play_fmt_ok
echo WildernessRuins manifest format must be rawiron-game-v1.3.5, v1.3.6, or v1.3.7 ^(latest^).
exit /b 1
:wilderness_play_fmt_ok
if not exist "%~dp0scripts\audio.riscript" (
  echo Missing required scripts/audio.riscript in WildernessRuins folder.
  exit /b 1
)
if not exist "%~dp0scripts\streaming.riscript" (
  echo Missing required scripts/streaming.riscript in WildernessRuins folder.
  exit /b 1
)
if not exist "%~dp0config\game.cfg" (
  echo Missing required config/game.cfg in WildernessRuins folder.
  exit /b 1
)
if not exist "%~dp0scripts\localization.riscript" (
  echo Missing required scripts/localization.riscript in WildernessRuins folder.
  exit /b 1
)
if not exist "%~dp0scripts\physics.riscript" (
  echo Missing required scripts/physics.riscript in WildernessRuins folder.
  exit /b 1
)
if not exist "%~dp0scripts\postprocess.riscript" (
  echo Missing required scripts/postprocess.riscript in WildernessRuins folder.
  exit /b 1
)
if not exist "%~dp0scripts\init.riscript" (
  echo Missing required scripts/init.riscript in WildernessRuins folder.
  exit /b 1
)
if not exist "%~dp0scripts\state.riscript" (
  echo Missing required scripts/state.riscript in WildernessRuins folder.
  exit /b 1
)
if not exist "%~dp0scripts\network.riscript" (
  echo Missing required scripts/network.riscript in WildernessRuins folder.
  exit /b 1
)
if not exist "%~dp0scripts\persistence.riscript" (
  echo Missing required scripts/persistence.riscript in WildernessRuins folder.
  exit /b 1
)
if not exist "%~dp0scripts\ai.riscript" (
  echo Missing required scripts/ai.riscript in WildernessRuins folder.
  exit /b 1
)
if not exist "%~dp0scripts\plugins.riscript" (
  echo Missing required scripts/plugins.riscript in WildernessRuins folder.
  exit /b 1
)
if not exist "%~dp0scripts\animation.riscript" (
  echo Missing required scripts/animation.riscript in WildernessRuins folder.
  exit /b 1
)
if not exist "%~dp0scripts\vfx.riscript" (
  echo Missing required scripts/vfx.riscript in WildernessRuins folder.
  exit /b 1
)
if not exist "%~dp0config\input.map" (
  echo Missing required config/input.map in WildernessRuins folder.
  exit /b 1
)
if not exist "%~dp0config\project.dev" (
  echo Missing required config/project.dev in WildernessRuins folder.
  exit /b 1
)
if not exist "%~dp0config\network.cfg" (
  echo Missing required config/network.cfg in WildernessRuins folder.
  exit /b 1
)
if not exist "%~dp0config\build.profile" (
  echo Missing required config/build.profile in WildernessRuins folder.
  exit /b 1
)
if not exist "%~dp0config\security.policy" (
  echo Missing required config/security.policy in WildernessRuins folder.
  exit /b 1
)
if not exist "%~dp0config\plugins.policy" (
  echo Missing required config/plugins.policy in WildernessRuins folder.
  exit /b 1
)
if not exist "%~dp0levels\assembly.navmesh" (
  echo Missing required levels/assembly.navmesh in WildernessRuins folder.
  exit /b 1
)
if not exist "%~dp0levels\assembly.zones.csv" (
  echo Missing required levels/assembly.zones.csv in WildernessRuins folder.
  exit /b 1
)
if not exist "%~dp0levels\assembly.ai.nodes" (
  echo Missing required levels/assembly.ai.nodes in WildernessRuins folder.
  exit /b 1
)
if not exist "%~dp0levels\assembly.lighting.csv" (
  echo Missing required levels/assembly.lighting.csv in WildernessRuins folder.
  exit /b 1
)
if not exist "%~dp0levels\assembly.cinematics.csv" (
  echo Missing required levels/assembly.cinematics.csv in WildernessRuins folder.
  exit /b 1
)
if not exist "%~dp0assets\manifest.assets" (
  echo Missing required assets/manifest.assets in WildernessRuins folder.
  exit /b 1
)
if not exist "%~dp0assets\metadata.json" (
  echo Missing required assets/metadata.json in WildernessRuins folder.
  exit /b 1
)
if not exist "%~dp0assets\layers.config" (
  echo Missing required assets/layers.config in WildernessRuins folder.
  exit /b 1
)
if not exist "%~dp0assets\dependencies.json" (
  echo Missing required assets/dependencies.json in WildernessRuins folder.
  exit /b 1
)
if not exist "%~dp0assets\streaming.manifest" (
  echo Missing required assets/streaming.manifest in WildernessRuins folder.
  exit /b 1
)
if not exist "%~dp0assets\shaders.manifest" (
  echo Missing required assets/shaders.manifest in WildernessRuins folder.
  exit /b 1
)
if not exist "%~dp0assets\animation.graph" (
  echo Missing required assets/animation.graph in WildernessRuins folder.
  exit /b 1
)
if not exist "%~dp0assets\vfx.manifest" (
  echo Missing required assets/vfx.manifest in WildernessRuins folder.
  exit /b 1
)
if not exist "%~dp0data\schema.db" (
  echo Missing required data/schema.db in WildernessRuins folder.
  exit /b 1
)
if not exist "%~dp0data\lookup.index" (
  echo Missing required data/lookup.index in WildernessRuins folder.
  exit /b 1
)
if not exist "%~dp0data\entity.registry" (
  echo Missing required data/entity.registry in WildernessRuins folder.
  exit /b 1
)
if not exist "%~dp0data\telemetry.db" (
  echo Missing required data/telemetry.db in WildernessRuins folder.
  exit /b 1
)
if not exist "%~dp0plugins\manifest.plugins" (
  echo Missing required plugins/manifest.plugins in WildernessRuins folder.
  exit /b 1
)
if not exist "%~dp0plugins\load_order.cfg" (
  echo Missing required plugins/load_order.cfg in WildernessRuins folder.
  exit /b 1
)
if not exist "%~dp0plugins\registry.json" (
  echo Missing required plugins/registry.json in WildernessRuins folder.
  exit /b 1
)
if not exist "%~dp0plugins\hooks.riplugin" (
  echo Missing required plugins/hooks.riplugin in WildernessRuins folder.
  exit /b 1
)
if not exist "%~dp0ai\behavior.tree" (
  echo Missing required ai/behavior.tree in WildernessRuins folder.
  exit /b 1
)
if not exist "%~dp0ai\blackboard.json" (
  echo Missing required ai/blackboard.json in WildernessRuins folder.
  exit /b 1
)
if not exist "%~dp0ai\factions.cfg" (
  echo Missing required ai/factions.cfg in WildernessRuins folder.
  exit /b 1
)

cd /d "%~dp0..\.."

set "GAME_ROOT=%~dp0"
if "%GAME_ROOT:~-1%"=="\" set "GAME_ROOT=%GAME_ROOT:~0,-1%"

set "TARGET_EXE="

REM Prefer game-local build output (Games\WildernessRuins\App), then generic build roots.
if not defined TARGET_EXE if exist "build\dev-msvc\Games\WildernessRuins\App\RelWithDebInfo\RawIron.ForestRuinsGame.exe" set "TARGET_EXE=build\dev-msvc\Games\WildernessRuins\App\RelWithDebInfo\RawIron.ForestRuinsGame.exe"
if not defined TARGET_EXE if exist "build\dev-msvc\Games\WildernessRuins\App\Release\RawIron.ForestRuinsGame.exe" set "TARGET_EXE=build\dev-msvc\Games\WildernessRuins\App\Release\RawIron.ForestRuinsGame.exe"
if not defined TARGET_EXE if exist "build\dev-msvc\Games\WildernessRuins\App\Debug\RawIron.ForestRuinsGame.exe" set "TARGET_EXE=build\dev-msvc\Games\WildernessRuins\App\Debug\RawIron.ForestRuinsGame.exe"
if not defined TARGET_EXE if exist "build\dev-msvc\Games\WildernessRuins\App\MinSizeRel\RawIron.ForestRuinsGame.exe" set "TARGET_EXE=build\dev-msvc\Games\WildernessRuins\App\MinSizeRel\RawIron.ForestRuinsGame.exe"
if not defined TARGET_EXE if exist "build\dev-msvc\Games\WildernessRuins\App\RawIron.ForestRuinsGame.exe" set "TARGET_EXE=build\dev-msvc\Games\WildernessRuins\App\RawIron.ForestRuinsGame.exe"

if not defined TARGET_EXE if exist "build\dev-clang\Games\WildernessRuins\App\RelWithDebInfo\RawIron.ForestRuinsGame.exe" set "TARGET_EXE=build\dev-clang\Games\WildernessRuins\App\RelWithDebInfo\RawIron.ForestRuinsGame.exe"
if not defined TARGET_EXE if exist "build\dev-clang\Games\WildernessRuins\App\Release\RawIron.ForestRuinsGame.exe" set "TARGET_EXE=build\dev-clang\Games\WildernessRuins\App\Release\RawIron.ForestRuinsGame.exe"
if not defined TARGET_EXE if exist "build\dev-clang\Games\WildernessRuins\App\Debug\RawIron.ForestRuinsGame.exe" set "TARGET_EXE=build\dev-clang\Games\WildernessRuins\App\Debug\RawIron.ForestRuinsGame.exe"

if not defined TARGET_EXE if exist "build\dev-mingw\Games\WildernessRuins\App\RelWithDebInfo\RawIron.ForestRuinsGame.exe" set "TARGET_EXE=build\dev-mingw\Games\WildernessRuins\App\RelWithDebInfo\RawIron.ForestRuinsGame.exe"
if not defined TARGET_EXE if exist "build\dev-mingw\Games\WildernessRuins\App\Release\RawIron.ForestRuinsGame.exe" set "TARGET_EXE=build\dev-mingw\Games\WildernessRuins\App\Release\RawIron.ForestRuinsGame.exe"

if not defined TARGET_EXE if exist "build\Games\WildernessRuins\App\RelWithDebInfo\RawIron.ForestRuinsGame.exe" set "TARGET_EXE=build\Games\WildernessRuins\App\RelWithDebInfo\RawIron.ForestRuinsGame.exe"
if not defined TARGET_EXE if exist "build\Games\WildernessRuins\App\Release\RawIron.ForestRuinsGame.exe" set "TARGET_EXE=build\Games\WildernessRuins\App\Release\RawIron.ForestRuinsGame.exe"

if not defined TARGET_EXE goto :missing_forest_ruins

echo Launching %TARGET_EXE%
echo Game root: %GAME_ROOT%
echo Workspace: %CD%

set "CORE_ARGS=--game=wilderness-ruins --game-root="%GAME_ROOT%" --workspace-root="%CD%" --renderer=vulkan"
set "DEFAULT_ARGS=--render-quality=balanced --width=1920 --height=1080"
if not "%~1"=="" set "DEFAULT_ARGS="

call :stage_runtime "%TARGET_EXE%"
call :launch_with_msvc_runtime "%TARGET_EXE%" %CORE_ARGS% %DEFAULT_ARGS% %*
set "LAUNCH_ERR=%ERRORLEVEL%"
if %LAUNCH_ERR% equ 0 exit /b 0

echo Could not launch RawIron.ForestRuinsGame (exit %LAUNCH_ERR%).
echo Build first: cmake --preset dev-msvc
echo              cmake --build build\dev-msvc --config RelWithDebInfo --target RawIron.ForestRuinsGame
echo Install VC++ Redistributable if DLLs are missing: https://aka.ms/vs/17/release/vc_redist.x64.exe
pause
exit /b %LAUNCH_ERR%

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
if defined VCVARS call "%VCVARS%" >nul 2>nul
start "" /wait "%EXE_PATH%" %*
exit /b %ERRORLEVEL%

:missing_forest_ruins
echo RawIron.ForestRuinsGame.exe was not found.
echo Build the engine game target, then run this launcher again:
echo   cmake --preset dev-msvc
echo   cmake --build build\dev-msvc --config RelWithDebInfo --target RawIron.ForestRuinsGame
pause
exit /b 1

