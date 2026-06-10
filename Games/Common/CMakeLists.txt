add_library(RawIron.Game.RuntimeDiagnosticsDraw STATIC
  src/GameConfigContracts.cpp
  src/GamePluginRuntimeBridge.cpp
  src/GameRuntimeCore.cpp
  src/GameTextOverlayHost.cpp
  src/RuntimeDiagnosticsStandaloneDraw.cpp
  src/TextOverlayStandaloneDraw.cpp
)

add_library(RawIron::Game.RuntimeDiagnosticsDraw ALIAS RawIron.Game.RuntimeDiagnosticsDraw)

target_include_directories(RawIron.Game.RuntimeDiagnosticsDraw
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(RawIron.Game.RuntimeDiagnosticsDraw
  PUBLIC
    RawIron::Content
    RawIron::Core
    RawIron::Runtime
    RawIron::SceneUtilities
    RawIron::Spatial
    RawIron::Trace
    RawIron::World
)

target_compile_features(RawIron.Game.RuntimeDiagnosticsDraw PUBLIC cxx_std_20)
