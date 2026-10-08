add_library(RawIron.GameHost STATIC
  src/GameConfigContracts.cpp
  src/GamePhysicsTuning.cpp
  src/GameNetworkTuning.cpp
  src/GamePluginRuntimeBridge.cpp
  src/GameRuntimeCore.cpp
  src/GameProjectBoot.cpp
  src/GameTextOverlayHost.cpp
  src/RuntimeDiagnosticsStandaloneDraw.cpp
  src/TextOverlayStandaloneDraw.cpp
)

add_library(RawIron::GameHost ALIAS RawIron.GameHost)
# Compatibility target for existing games and editor callers.
add_library(RawIron.Game.RuntimeDiagnosticsDraw ALIAS RawIron.GameHost)
add_library(RawIron::Game.RuntimeDiagnosticsDraw ALIAS RawIron.GameHost)

target_include_directories(RawIron.GameHost
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(RawIron.GameHost
  PUBLIC
    RawIron::Content
    RawIron::Core
    RawIron::Runtime
    RawIron::SceneUtilities
    RawIron::Spatial
    RawIron::Trace
    RawIron::World
)

target_compile_features(RawIron.GameHost PUBLIC cxx_std_20)

if(RAWIRON_BUILD_TESTS)
  add_executable(GameProjectBootSmoke ${CMAKE_SOURCE_DIR}/Tests/GameProjectBootSmoke.cpp)
  target_link_libraries(GameProjectBootSmoke PRIVATE RawIron::GameHost)
  add_test(NAME RawIron.GameHost.ProjectBootSmoke COMMAND GameProjectBootSmoke ${CMAKE_SOURCE_DIR})
endif()
