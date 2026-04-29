add_library(RawIron.Render.Software
  src/PreviewTexture.cpp
  src/ScenePreview.cpp
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
)

target_compile_features(RawIron.Render.Software PUBLIC cxx_std_20)
