add_library(RawIron.Game.ForestRuins
  src/ForestRuinsEditorPreview.cpp
  src/ForestRuinsExperience.cpp
  src/ForestRuinsWorld.cpp
)

add_library(RawIron::Game.ForestRuins ALIAS RawIron.Game.ForestRuins)

target_include_directories(RawIron.Game.ForestRuins
  PUBLIC
    include
)

target_link_libraries(RawIron.Game.ForestRuins
  PUBLIC
    RawIron::Audio
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
  target_link_libraries(RawIron.Game.ForestRuins
    PUBLIC
      user32
  )
endif()

target_compile_features(RawIron.Game.ForestRuins PUBLIC cxx_std_20)

if (RAWIRON_BUILD_TESTS)
  add_test(
    NAME RawIron.ForestRuins.RenderingScriptPreview
    COMMAND ${CMAKE_COMMAND}
      -DRAWIRON_WORKSPACE=${CMAKE_SOURCE_DIR}
      -P ${CMAKE_SOURCE_DIR}/Tests/VerifyForestRuinsRenderingScript.cmake
  )
endif()
