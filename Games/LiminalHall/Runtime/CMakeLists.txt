add_library(RawIron.Game.LiminalHall
  src/LiminalHallBenchmark.cpp
  src/LiminalHallEditorPreview.cpp
  src/LiminalHallExperience.cpp
  src/LiminalHallWorld.cpp
)

add_library(RawIron::Game.LiminalHall ALIAS RawIron.Game.LiminalHall)

target_include_directories(RawIron.Game.LiminalHall
  PUBLIC
    include
)

target_link_libraries(RawIron.Game.LiminalHall
  PUBLIC
    RawIron::Audio
    RawIron::Content
    RawIron::Core
    RawIron::EditorPreview
    RawIron::Game.RuntimeDiagnosticsDraw
    RawIron::Logic
    RawIron::RenderSoftware
    RawIron::RenderVulkan
    RawIron::SceneUtilities
    RawIron::Trace
    RawIron::World
)

if (WIN32)
  target_link_libraries(RawIron.Game.LiminalHall
    PUBLIC
      user32
  )
endif()

target_compile_features(RawIron.Game.LiminalHall PUBLIC cxx_std_20)
