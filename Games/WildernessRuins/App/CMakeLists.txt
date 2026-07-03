add_executable(RawIron.ForestRuinsGame
  src/main.cpp
)

target_link_libraries(RawIron.ForestRuinsGame
  PRIVATE
    RawIron::Game.ForestRuins
)

target_compile_features(RawIron.ForestRuinsGame PRIVATE cxx_std_20)

rawiron_stage_runtime_dlls(RawIron.ForestRuinsGame)

rawiron_bundle_engine_textures(RawIron.ForestRuinsGame)

if(MSVC)
  # BuildForestRuinsWorld keeps many large scene-builder lambdas on one stack frame; the
  # default 1 MB Windows stack overflows during FBX hero imports after terrain/scatter.
  target_link_options(RawIron.ForestRuinsGame PRIVATE /STACK:67108864)
endif()
