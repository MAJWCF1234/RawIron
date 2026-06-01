add_library(RawIron.Game.MultiplayerSandbox
  src/MultiplayerSandboxRuntime.cpp
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
    RawIron::Game.LiminalHall
    RawIron::Game.RuntimeDiagnosticsDraw
    RawIron::Runtime
)

target_compile_features(RawIron.Game.MultiplayerSandbox PUBLIC cxx_std_20)
