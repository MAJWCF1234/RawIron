rawiron_add_app(
  TARGET RawIron.Forge
  SOURCES
    src/main.cpp
    src/ForgeCatalog.cpp
    ${CMAKE_SOURCE_DIR}/Apps/RawIron.Editor/src/EditorVulkanViewport.cpp
  LIBRARIES
    RawIron::Content
    RawIron::Core
    RawIron::RenderSoftware
    RawIron::RenderVulkan
    RawIron::SceneUtilities
  WIN32_LIBRARIES
    user32
    gdi32
    shell32
  EXPECTED_STRINGS
    "[Forge Ready]"
    "Model sources:"
    "Rig assets:"
  FORBIDDEN_STRINGS
    "failed"
    "exception"
  SMOKE_ARGS
    "--headless"
  BUNDLE_ENGINE_TEXTURES
)

target_include_directories(RawIron.Forge PRIVATE
  ${CMAKE_SOURCE_DIR}/Apps/RawIron.Editor/src
)

if (RAWIRON_BUILD_TESTS)
  add_executable(RawIron.Forge.CatalogSmoke
    Tests/ForgeCatalogSmoke.cpp
    src/ForgeCatalog.cpp
  )
  target_include_directories(RawIron.Forge.CatalogSmoke PRIVATE src)
  target_link_libraries(RawIron.Forge.CatalogSmoke PRIVATE RawIron::Content RawIron::SceneUtilities)
  target_compile_features(RawIron.Forge.CatalogSmoke PRIVATE cxx_std_20)
  add_test(NAME RawIron.Forge.Catalog COMMAND RawIron.Forge.CatalogSmoke)

  rawiron_runtime_dll_test_arg(rawiron_forge_runtime_dll_arg RawIron.Forge)
  add_test(
    NAME RawIron.Forge.HandoffProbe
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:RawIron.Forge>"
      "-DCOMMAND_ARGS=--handoff-probe;${PROJECT_SOURCE_DIR}/Assets/Source/models/scenekit_triangle.gltf;--workspace-root;${PROJECT_SOURCE_DIR}"
      "-DEXPECTED_STRINGS=Forge handoff: ready;Asset kind: model-source;--open-asset"
      "-DFORBIDDEN_STRINGS=rejected;failed;exception"
      "${rawiron_forge_runtime_dll_arg}"
      -P "${PROJECT_SOURCE_DIR}/Tests/cmake/VerifyCommandOutput.cmake"
  )
endif()
