@echo off
setlocal

set "MANIFEST=%~dp0manifest.json"
if not exist "%MANIFEST%" (
  echo Missing manifest.json in WildernessRuins folder.
  exit /b 1
)
findstr /c:"\"format\": \"rawiron-game-v1.3.5\"" "%MANIFEST%" >nul || (
  echo WildernessRuins manifest format is not rawiron-game-v1.3.5.
  exit /b 1
)
findstr /c:"\"editorProjectArg\": \"--game=wilderness-ruins\"" "%MANIFEST%" >nul || (
  echo WildernessRuins manifest editorProjectArg must be --game=wilderness-ruins.
  exit /b 1
)
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

call "%~dp0..\..\Launch RawIron Editor.cmd" --game=wilderness-ruins %*

