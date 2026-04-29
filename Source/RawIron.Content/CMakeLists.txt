add_library(RawIron.Content
  src/AssetDocument.cpp
  src/DeclarativeModelDefinition.cpp
  src/EngineAssets.cpp
  src/GameManifest.cpp
  src/GameRuntimeSupport.cpp
  src/PipelineAssetInventory.cpp
  src/PrefabExpansion.cpp
  src/ScriptScalars.cpp
  src/Value.cpp
  src/ValueSchema.cpp
  src/WorldVolumeContent.cpp
)

add_library(RawIron::Content ALIAS RawIron.Content)

target_include_directories(RawIron.Content
  PUBLIC
    include
)

target_link_libraries(RawIron.Content
  PUBLIC
    RawIron::Core
    RawIron::Validation
    RawIron::World
)

target_compile_features(RawIron.Content PUBLIC cxx_std_20)
