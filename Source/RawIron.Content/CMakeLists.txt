add_library(RawIron.Content
  src/AssetDocument.cpp
  src/AssetPackageManifest.cpp
  src/DeclarativeModelDefinition.cpp
  src/EngineAssets.cpp
  src/ExtensionDescriptor.cpp
  src/GameManifest.cpp
  src/GameRuntimeSupport.cpp
  src/PipelineAssetInventory.cpp
  src/PluginProjectData.cpp
  src/PluginRuntime.cpp
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

if(RAWIRON_BUILD_TESTS)
  add_executable(PatchScriptScalarsSmoke ${CMAKE_SOURCE_DIR}/Tests/PatchScriptScalarsSmoke.cpp)
  target_link_libraries(PatchScriptScalarsSmoke PRIVATE RawIron::Content)
  add_test(NAME RawIron.Content.PatchScriptScalars COMMAND PatchScriptScalarsSmoke)

  add_executable(PluginProjectDataSmoke ${CMAKE_SOURCE_DIR}/Tests/PluginProjectDataSmoke.cpp)
  target_link_libraries(PluginProjectDataSmoke PRIVATE RawIron::Content)
  add_test(NAME RawIron.Content.PluginProjectDataSmoke COMMAND PluginProjectDataSmoke)

  add_executable(BundledPluginProjectDataSmoke ${CMAKE_SOURCE_DIR}/Tests/BundledPluginProjectDataSmoke.cpp)
  target_link_libraries(BundledPluginProjectDataSmoke PRIVATE RawIron::Content)
  add_test(NAME RawIron.Content.BundledPluginProjectDataSmoke COMMAND BundledPluginProjectDataSmoke)

  add_executable(ExtensionDescriptorSmoke ${CMAKE_SOURCE_DIR}/Tests/ExtensionDescriptorSmoke.cpp)
  target_link_libraries(ExtensionDescriptorSmoke PRIVATE RawIron::Content)
  add_test(NAME RawIron.Content.ExtensionDescriptorSmoke COMMAND ExtensionDescriptorSmoke)

  add_test(
    NAME RawIron.Content.PluginProjectData
    COMMAND ${CMAKE_COMMAND}
      -DRAWIRON_WORKSPACE=${CMAKE_SOURCE_DIR}
      -P ${CMAKE_SOURCE_DIR}/Tests/VerifyPluginProjectData.cmake
  )
endif()
