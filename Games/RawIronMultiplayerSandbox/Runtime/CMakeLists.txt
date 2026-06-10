add_library(RawIron.Game.MultiplayerSandbox
  src/MultiplayerSandboxEditorPreview.cpp
  src/MultiplayerSandboxExperience.cpp
  src/MultiplayerSandboxRuntime.cpp
  src/MultiplayerSandboxWorld.cpp
)

add_library(RawIron::Game.MultiplayerSandbox ALIAS RawIron.Game.MultiplayerSandbox)

target_include_directories(RawIron.Game.MultiplayerSandbox
  PUBLIC
    include
)

target_link_libraries(RawIron.Game.MultiplayerSandbox
  PUBLIC
    RawIron::Content
    RawIron::Core
    RawIron::EditorPreview
    RawIron::Game.RuntimeDiagnosticsDraw
    RawIron::RenderSoftware
    RawIron::RenderVulkan
    RawIron::Runtime
    RawIron::SceneUtilities
    RawIron::Trace
    RawIron::World
)

if (WIN32)
  target_link_libraries(RawIron.Game.MultiplayerSandbox
    PUBLIC
      user32
  )
endif()

target_compile_features(RawIron.Game.MultiplayerSandbox PUBLIC cxx_std_20)
