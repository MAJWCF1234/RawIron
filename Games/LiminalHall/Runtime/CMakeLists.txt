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
    RawIron::Runtime
    RawIron::SceneUtilities
    RawIron::Trace
    RawIron::UI
    RawIron::World
)

if (WIN32)
  target_link_libraries(RawIron.Game.LiminalHall
    PUBLIC
      user32
  )
endif()

target_compile_features(RawIron.Game.LiminalHall PUBLIC cxx_std_20)

if (WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_compile_options(RawIron.Game.LiminalHall PRIVATE "-Wa,-mbig-obj")
endif()

if (RAWIRON_BUILD_TESTS)
  add_test(
    NAME RawIron.LiminalHall.MaterialAudit
    COMMAND ${CMAKE_COMMAND}
      -DRAWIRON_WORKSPACE=${CMAKE_SOURCE_DIR}
      -P ${CMAKE_SOURCE_DIR}/Tests/VerifyLiminalHallMaterials.cmake
  )
  add_test(
    NAME RawIron.LiminalHall.RenderingScriptPreview
    COMMAND ${CMAKE_COMMAND}
      -DRAWIRON_WORKSPACE=${CMAKE_SOURCE_DIR}
      -P ${CMAKE_SOURCE_DIR}/Tests/VerifyRenderingScriptPreview.cmake
  )
endif()
