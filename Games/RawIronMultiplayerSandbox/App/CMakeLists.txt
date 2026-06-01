add_executable(RawIron.MultiplayerSandboxGame
  src/main.cpp
)

target_link_libraries(RawIron.MultiplayerSandboxGame
  PRIVATE
    RawIron::Game.MultiplayerSandbox
)

target_compile_features(RawIron.MultiplayerSandboxGame PRIVATE cxx_std_20)

rawiron_stage_runtime_dlls(RawIron.MultiplayerSandboxGame)
rawiron_bundle_engine_textures(RawIron.MultiplayerSandboxGame)
