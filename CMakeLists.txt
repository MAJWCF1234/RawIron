cmake_minimum_required(VERSION 3.24)

project(RawIron VERSION 0.1.0 LANGUAGES CXX C)

include("${CMAKE_SOURCE_DIR}/cmake/BundleEngineTextures.cmake")
include("${CMAKE_SOURCE_DIR}/cmake/RawIronApp.cmake")
include("${CMAKE_SOURCE_DIR}/cmake/RawIronArchitecture.cmake")
include(CMakeDependentOption)
include(FetchContent)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(RAWIRON_BUILD_PLAYER "Build RawIron.Player (generic runtime host)" ON)
option(RAWIRON_BUILD_PREVIEW "Build RawIron.Preview (Scene Kit + snapshot host)" ON)
option(RAWIRON_BUILD_EDITOR "Build RawIron.Editor" ON)
option(RAWIRON_BUILD_FORGE "Build RawIron.Forge (modeling and rigging workbench)" ON)
option(RAWIRON_BUILD_PARTICLE_SHOWCASE "Build RawIron.ParticleShowcase" ON)
option(RAWIRON_BUILD_LIMINAL_GAME "Build the Liminal Hall game application" ON)
option(RAWIRON_BUILD_FOREST_RUINS_GAME "Build the Wilderness Ruins game application" ON)
option(RAWIRON_BUILD_MULTIPLAYER_SANDBOX_GAME "Build the RawIron Multiplayer Sandbox game application" ON)
option(RAWIRON_BUILD_CUBE_TEST_GAME "Build the Cube Test game application" ON)
option(RAWIRON_BUILD_VR_SHOWCASE "Build the OpenXR/SteamVR capability showcase" ON)
cmake_dependent_option(RAWIRON_EDITOR_LINK_LIMINAL
  "Link the Liminal Hall game module into the editor (registers editor preview id liminal-hall). Turn OFF for a shipping editor with no bundled games."
  ON
  "RAWIRON_BUILD_EDITOR"
  OFF)
option(RAWIRON_BUILD_VISUAL_SHELL "Build RawIron.VisualShell" ON)
option(RAWIRON_BUILD_BOT_CLIENT "Build RawIron.BotClient (headless bot swarm load client)" ON)
option(RAWIRON_BUILD_DEDICATED_SERVER "Build RawIron.DedicatedServer headless authoritative host" ON)
option(RAWIRON_BUILD_TOOLS "Build ri_tool and other CLI tools" OFF)
option(RAWIRON_BUILD_TESTS "Enable CTest and lightweight app smoke tests (no giant native suites)" OFF)
option(RAWIRON_BUILD_DEV_INSPECTOR "Build RawIron.DevInspector (development observability channel)" OFF)
option(RAWIRON_USE_ENET "Enable ENet UDP networking backend for RawIron runtime net transport" ON)
option(RAWIRON_USE_EOS "Enable Epic Online Services rendezvous/provider hooks" OFF)
option(RAWIRON_ENABLE_SANITIZERS "Enable supported memory/undefined-behavior sanitizers" OFF)
set(EOS_SDK_ROOT "${CMAKE_SOURCE_DIR}/ThirdParty/EOS/SDK" CACHE PATH "Path to Epic Online Services SDK root")
set(RAWIRON_ENABLE_STACKTRACE_DIAGNOSTICS_DEFAULT ON)
if (NOT CMAKE_CONFIGURATION_TYPES AND CMAKE_BUILD_TYPE STREQUAL "Release")
  set(RAWIRON_ENABLE_STACKTRACE_DIAGNOSTICS_DEFAULT OFF)
endif()
option(RAWIRON_ENABLE_STACKTRACE_DIAGNOSTICS
  "Enable crash diagnostics stack traces (backward-cpp)"
  ${RAWIRON_ENABLE_STACKTRACE_DIAGNOSTICS_DEFAULT})

if (RAWIRON_BUILD_TESTS)
  enable_testing()
endif()

if (RAWIRON_USE_ENET)
  FetchContent_Declare(
    enet
    GIT_REPOSITORY https://github.com/lsalzman/enet.git
    GIT_TAG v1.3.18
  )
  FetchContent_MakeAvailable(enet)
endif()

if (RAWIRON_USE_EOS)
  if (EXISTS "${EOS_SDK_ROOT}/Include/eos_sdk.h")
    message(STATUS "RawIron: EOS SDK headers found at ${EOS_SDK_ROOT}")
  else()
    message(WARNING
      "RawIron: RAWIRON_USE_EOS=ON but EOS SDK was not found at ${EOS_SDK_ROOT}. "
      "Build keeps EOS provider stubs enabled; runtime will fall back to DirectToken until SDK is installed.")
  endif()
endif()

if (MSVC)
  add_compile_options(/W4 /permissive- /FS)
  if (RAWIRON_ENABLE_SANITIZERS)
    add_compile_options(/fsanitize=address)
    add_link_options(/INCREMENTAL:NO)
    message(STATUS "RawIron: MSVC AddressSanitizer enabled")
  endif()
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
  if (RAWIRON_ENABLE_SANITIZERS)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
    message(STATUS "RawIron: AddressSanitizer and UndefinedBehaviorSanitizer enabled")
  endif()
endif()

add_subdirectory(Source/RawIron.Core)
include("${CMAKE_SOURCE_DIR}/cmake/RawIronUIModule.cmake")
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
add_subdirectory(Source/RawIron.XR.OpenXR)

set(RAWIRON_BUILD_LIMINAL_HALL_MODULE OFF)
if(RAWIRON_BUILD_LIMINAL_GAME)
  set(RAWIRON_BUILD_LIMINAL_HALL_MODULE ON)
elseif(RAWIRON_BUILD_EDITOR AND RAWIRON_EDITOR_LINK_LIMINAL)
  set(RAWIRON_BUILD_LIMINAL_HALL_MODULE ON)
endif()

if (RAWIRON_BUILD_FOREST_RUINS_GAME
    OR RAWIRON_BUILD_LIMINAL_HALL_MODULE
    OR RAWIRON_BUILD_MULTIPLAYER_SANDBOX_GAME
    OR RAWIRON_BUILD_CUBE_TEST_GAME)
  add_subdirectory(Games/Common)
endif()

if (RAWIRON_BUILD_FOREST_RUINS_GAME)
  add_subdirectory(Games/WildernessRuins)
endif()

if (RAWIRON_BUILD_MULTIPLAYER_SANDBOX_GAME)
  add_subdirectory(Games/RawIronMultiplayerSandbox)
endif()

if (RAWIRON_BUILD_CUBE_TEST_GAME)
  add_subdirectory(Games/CubeTest)
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

if (RAWIRON_BUILD_PREVIEW)
  add_subdirectory(Apps/RawIron.Preview)
endif()

if (RAWIRON_BUILD_EDITOR)
  add_subdirectory(Apps/RawIron.Editor)
endif()

if (RAWIRON_BUILD_FORGE)
  add_subdirectory(Apps/RawIron.Forge)
endif()

if (RAWIRON_BUILD_PARTICLE_SHOWCASE)
  add_subdirectory(Apps/RawIron.ParticleShowcase)
endif()

if (RAWIRON_BUILD_VR_SHOWCASE)
  add_subdirectory(Apps/RawIron.VRShowcase)
endif()

if (RAWIRON_BUILD_VISUAL_SHELL)
  add_subdirectory(Apps/RawIron.VisualShell)
endif()

if (RAWIRON_BUILD_BOT_CLIENT)
  add_subdirectory(Apps/RawIron.BotClient)
endif()

if (RAWIRON_BUILD_DEDICATED_SERVER)
  add_subdirectory(Apps/RawIron.DedicatedServer)
endif()

if (RAWIRON_BUILD_TOOLS)
  add_subdirectory(Tools/ri_tool)
endif()

if (RAWIRON_BUILD_TESTS)
  add_subdirectory(Tests)
  add_test(
    NAME RawIron.Architecture.FoundationPolicy
    COMMAND ${CMAKE_COMMAND}
      -DRAWIRON_WORKSPACE=${CMAKE_SOURCE_DIR}
      -P ${CMAKE_SOURCE_DIR}/Tests/VerifyArchitecturePolicy.cmake
  )
endif()

rawiron_enforce_foundation_architecture("${CMAKE_SOURCE_DIR}")
