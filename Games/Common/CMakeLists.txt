add_library(RawIron.Game.RuntimeDiagnosticsDraw STATIC
  src/RuntimeDiagnosticsStandaloneDraw.cpp
)

add_library(RawIron::Game.RuntimeDiagnosticsDraw ALIAS RawIron.Game.RuntimeDiagnosticsDraw)

target_include_directories(RawIron.Game.RuntimeDiagnosticsDraw
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(RawIron.Game.RuntimeDiagnosticsDraw
  PUBLIC
    RawIron::Core
    RawIron::SceneUtilities
    RawIron::Spatial
    RawIron::Trace
    RawIron::World
)

target_compile_features(RawIron.Game.RuntimeDiagnosticsDraw PUBLIC cxx_std_20)
