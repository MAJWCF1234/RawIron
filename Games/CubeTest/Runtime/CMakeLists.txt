add_library(RawIron.Game.CubeTest
  src/CubeTestAuthority.cpp
  src/CubeTestRuntime.cpp
  src/CubeTestWorld.cpp
  src/CubeTestEditorPreview.cpp
)

add_library(RawIron::Game.CubeTest ALIAS RawIron.Game.CubeTest)

target_include_directories(RawIron.Game.CubeTest
  PUBLIC
    include
)

target_link_libraries(RawIron.Game.CubeTest
  PUBLIC
    RawIron::Content
    RawIron::Core
    RawIron::Runtime
    RawIron::RenderSoftware
    RawIron::RenderVulkan
    RawIron::SceneUtilities
    RawIron::Trace
    RawIron::World
    RawIron::EditorPreview
    RawIron::Game.RuntimeDiagnosticsDraw
)

if (WIN32)
  target_link_libraries(RawIron.Game.CubeTest
    PUBLIC
      user32
  )
endif()

target_compile_features(RawIron.Game.CubeTest PUBLIC cxx_std_20)

if (RAWIRON_BUILD_TESTS)
  add_executable(RawIron.CubeTest.WorldSmoke
    tests/CubeTestWorldSmoke.cpp
  )

  target_link_libraries(RawIron.CubeTest.WorldSmoke
    PRIVATE
      RawIron::Game.CubeTest
  )

  target_compile_features(RawIron.CubeTest.WorldSmoke PRIVATE cxx_std_20)

  add_test(NAME RawIron.CubeTest.WorldSmoke COMMAND RawIron.CubeTest.WorldSmoke)
endif()
