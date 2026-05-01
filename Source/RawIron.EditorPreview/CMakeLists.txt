add_library(RawIron.EditorPreview
  src/PreviewSceneRegistry.cpp
)

add_library(RawIron::EditorPreview ALIAS RawIron.EditorPreview)

target_include_directories(RawIron.EditorPreview
  PUBLIC
    include
)

target_link_libraries(RawIron.EditorPreview
  PUBLIC
    RawIron::RenderSoftware
    RawIron::SceneUtilities
)

target_compile_features(RawIron.EditorPreview PUBLIC cxx_std_20)
