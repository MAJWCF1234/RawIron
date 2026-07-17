add_library(RawIron.Render.Software
  src/PreviewTexture.cpp
  src/RgbaImagePathCache.cpp
  src/ScenePreview.cpp
  src/ScenePreviewPlacement.cpp
  src/ScenePreviewRayTrace.cpp
  src/ScenePreviewRenderingScript.cpp
  src/SoftwarePreview.cpp
)

add_library(RawIron::RenderSoftware ALIAS RawIron.Render.Software)

target_include_directories(RawIron.Render.Software
  PUBLIC
    include
  PRIVATE
    third_party
)

target_link_libraries(RawIron.Render.Software
  PUBLIC
    RawIron::Content
    RawIron::Core
    RawIron::SceneUtilities
    RawIron::Spatial
)

target_compile_features(RawIron.Render.Software PUBLIC cxx_std_20)

if (WIN32)
  target_link_libraries(RawIron.Render.Software PRIVATE windowscodecs ole32)
endif()

if (RAWIRON_BUILD_TESTS)
  add_executable(ScenePreviewPlacementSmoke ${CMAKE_SOURCE_DIR}/Tests/ScenePreviewPlacementSmoke.cpp)
  target_link_libraries(ScenePreviewPlacementSmoke PRIVATE RawIron::RenderSoftware)
  add_test(NAME RawIron.Render.Software.ScenePreviewPlacementSmoke COMMAND ScenePreviewPlacementSmoke)

  add_executable(ScenePreviewRayTraceSafetySmoke ${CMAKE_SOURCE_DIR}/Tests/ScenePreviewRayTraceSafetySmoke.cpp)
  target_link_libraries(ScenePreviewRayTraceSafetySmoke PRIVATE RawIron::RenderSoftware)
  add_test(NAME RawIron.Render.Software.ScenePreviewRayTraceSafetySmoke COMMAND ScenePreviewRayTraceSafetySmoke)

  add_executable(ScenePreviewTransparencySmoke ${CMAKE_SOURCE_DIR}/Tests/ScenePreviewTransparencySmoke.cpp)
  target_link_libraries(ScenePreviewTransparencySmoke PRIVATE RawIron::RenderSoftware)
  add_test(NAME RawIron.Render.Software.ScenePreviewTransparencySmoke COMMAND ScenePreviewTransparencySmoke)

  add_executable(ScenePreviewScriptWatcherSmoke ${CMAKE_SOURCE_DIR}/Tests/ScenePreviewScriptWatcherSmoke.cpp)
  target_link_libraries(ScenePreviewScriptWatcherSmoke PRIVATE RawIron::RenderSoftware)
  add_test(NAME RawIron.Render.Software.ScenePreviewScriptWatcherSmoke COMMAND ScenePreviewScriptWatcherSmoke)
endif()
