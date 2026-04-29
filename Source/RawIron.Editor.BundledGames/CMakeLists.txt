add_library(RawIron.Editor.BundledGames
  src/RegisterBundledGamePreviews.cpp
)

add_library(RawIron::Editor.BundledGames ALIAS RawIron.Editor.BundledGames)

target_include_directories(RawIron.Editor.BundledGames
  PUBLIC
    include
)

target_link_libraries(RawIron.Editor.BundledGames
  PUBLIC
    RawIron::EditorPreview
)

if(RAWIRON_EDITOR_LINK_LIMINAL)
  target_compile_definitions(RawIron.Editor.BundledGames PRIVATE RAWIRON_EDITOR_BUNDLE_LIMINAL=1)
  target_link_libraries(RawIron.Editor.BundledGames PRIVATE RawIron::Game.LiminalHall)
endif()

if(RAWIRON_BUILD_FOREST_RUINS_GAME)
  target_compile_definitions(RawIron.Editor.BundledGames PRIVATE RAWIRON_EDITOR_BUNDLE_WILDERNESS=1)
  target_link_libraries(RawIron.Editor.BundledGames PRIVATE RawIron::Game.ForestRuins)
endif()

target_compile_features(RawIron.Editor.BundledGames PUBLIC cxx_std_20)
