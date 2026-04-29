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
findstr /c:"\"editorProjectArg\": \"--game=liminal-hall\"" "%MANIFEST%" >nul || (
  echo LiminalHall manifest editorProjectArg must be --game=liminal-hall.
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

call "%~dp0..\..\Launch RawIron Editor.cmd" --game=liminal-hall %*

