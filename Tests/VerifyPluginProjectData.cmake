# Verifies bundled game plugin project surfaces exist.
cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED RAWIRON_WORKSPACE)
  message(FATAL_ERROR "VerifyPluginProjectData: RAWIRON_WORKSPACE is required.")
endif()

set(_games
  "Games/LiminalHall"
  "Games/WildernessRuins"
  "Games/RawIronMultiplayerSandbox"
)

set(_required_plugin_files
  plugins/manifest.plugins
  plugins/registry.json
  plugins/load_order.cfg
  plugins/hooks.riplugin
  scripts/plugins.riscript
  config/plugins.policy
)

set(_game_count 0)
foreach(_game IN LISTS _games)
  set(_root "${RAWIRON_WORKSPACE}/${_game}")
  if(NOT IS_DIRECTORY "${_root}")
    message(FATAL_ERROR "Missing game root: ${_root}")
  endif()
  foreach(_rel IN LISTS _required_plugin_files)
    if(NOT EXISTS "${_root}/${_rel}")
      message(FATAL_ERROR "${_game}: missing required ${_rel}")
    endif()
  endforeach()
  math(EXPR _game_count "${_game_count} + 1")
endforeach()

message(STATUS "VerifyPluginProjectData: ${_game_count} game plugin surfaces OK")
