cmake_minimum_required(VERSION 3.24)

project(RawIron VERSION 0.1.0 LANGUAGES CXX C)

include("${CMAKE_SOURCE_DIR}/cmake/BundleEngineTextures.cmake")
include(CMakeDependentOption)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(RAWIRON_BUILD_PLAYER "Build the player application" ON)
option(RAWIRON_BUILD_EDITOR "Build the editor application" ON)
option(RAWIRON_BUILD_PREVIEW "Build the preview application" ON)
option(RAWIRON_BUILD_LIMINAL_GAME "Build the liminal test game application" ON)
option(RAWIRON_BUILD_FOREST_RUINS_GAME "Build the forest ruins test game application" ON)
cmake_dependent_option(RAWIRON_EDITOR_LINK_LIMINAL
  "Link the Liminal Hall game module into the editor (registers editor preview id liminal-hall). Turn OFF for a shipping editor with no bundled games."
  ON
  "RAWIRON_BUILD_EDITOR"
  OFF)
option(RAWIRON_BUILD_VISUAL_SHELL "Build the retro visual shell application" ON)
option(RAWIRON_BUILD_TOOLS "Build command-line tools" ON)
option(RAWIRON_BUILD_TESTS "Build native test targets" ON)
option(RAWIRON_BUILD_DEV_INSPECTOR "Build RawIron.DevInspector (development observability channel)" ON)
set(RAWIRON_ENABLE_STACKTRACE_DIAGNOSTICS_DEFAULT ON)
if (NOT CMAKE_CONFIGURATION_TYPES AND CMAKE_BUILD_TYPE STREQUAL "Release")
  set(RAWIRON_ENABLE_STACKTRACE_DIAGNOSTICS_DEFAULT OFF)
endif()
option(RAWIRON_ENABLE_STACKTRACE_DIAGNOSTICS
  "Enable crash diagnostics stack traces (backward-cpp)"
  ${RAWIRON_ENABLE_STACKTRACE_DIAGNOSTICS_DEFAULT})

if (WIN32)
  set(RAWIRON_VULKAN_RUNTIME_DLL "$ENV{WINDIR}/System32/vulkan-1.dll")
endif()

if (RAWIRON_BUILD_TESTS)
  enable_testing()
endif()

if (MSVC)
  add_compile_options(/W4 /permissive-)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_subdirectory(Source/RawIron.Core)
add_subdirectory(Source/RawIron.Audio)
add_subdirectory(Source/RawIron.Content)
add_subdirectory(Source/RawIron.Debug)
if (RAWIRON_BUILD_DEV_INSPECTOR)
  add_subdirectory(Source/RawIron.DevInspector)
endif()
add_subdirectory(Source/RawIron.Runtime)
add_subdirectory(Source/RawIron.Logic)
add_subdirectory(Source/RawIron.World)
add_subdirectory(Source/RawIron.Events)
add_subdirectory(Source/RawIron.Spatial)
add_subdirectory(Source/RawIron.Trace)
add_subdirectory(Source/RawIron.Validation)
add_subdirectory(Source/RawIron.SceneUtilities)
add_subdirectory(Source/RawIron.EditorPreview)

set(RAWIRON_BUILD_LIMINAL_HALL_MODULE OFF)
if(RAWIRON_BUILD_LIMINAL_GAME)
  set(RAWIRON_BUILD_LIMINAL_HALL_MODULE ON)
elseif(RAWIRON_BUILD_EDITOR AND RAWIRON_EDITOR_LINK_LIMINAL)
  set(RAWIRON_BUILD_LIMINAL_HALL_MODULE ON)
endif()

if (RAWIRON_BUILD_FOREST_RUINS_GAME OR RAWIRON_BUILD_LIMINAL_HALL_MODULE)
  add_subdirectory(Games/Common)
endif()

if (RAWIRON_BUILD_FOREST_RUINS_GAME)
  add_subdirectory(Games/WildernessRuins)
endif()

if(RAWIRON_BUILD_LIMINAL_HALL_MODULE OR RAWIRON_BUILD_LIMINAL_GAME)
  add_subdirectory(Games/LiminalHall)
endif()

if(RAWIRON_BUILD_EDITOR)
  add_subdirectory(Source/RawIron.Editor.BundledGames)
endif()

add_subdirectory(Source/RawIron.Render.Vulkan)
add_subdirectory(Source/RawIron.Render.Software)
add_subdirectory(Source/RawIron.Structural)

if (RAWIRON_BUILD_PLAYER)
  add_subdirectory(Apps/RawIron.Player)
endif()

if (RAWIRON_BUILD_EDITOR)
  add_subdirectory(Apps/RawIron.Editor)
endif()

if (RAWIRON_BUILD_PREVIEW)
  add_subdirectory(Apps/RawIron.Preview)
endif()

if (RAWIRON_BUILD_VISUAL_SHELL)
  add_subdirectory(Apps/RawIron.VisualShell)
endif()

if (RAWIRON_BUILD_TOOLS)
  add_subdirectory(Tools/ri_tool)
endif()

if (RAWIRON_BUILD_TESTS)
  add_subdirectory(Tests)
endif()
