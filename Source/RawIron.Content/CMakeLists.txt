add_library(RawIron.Content
  src/AuthoringHandoff.cpp
  src/AssetDocument.cpp
  src/AssetPackageManifest.cpp
  src/DeclarativeModelDefinition.cpp
  src/EngineAssets.cpp
  src/ExtensionDescriptor.cpp
  src/GameManifest.cpp
  src/GameRuntimeSupport.cpp
  src/NativePluginHost.cpp
  src/PackageMountRegistry.cpp
  src/PackageResolver.cpp
  src/PipelineAssetInventory.cpp
  src/PluginHookBindingParser.cpp
  src/PluginProjectData.cpp
  src/PluginRuntime.cpp
  src/PrimitiveModelDocument.cpp
  src/PrefabExpansion.cpp
  src/ScriptScalars.cpp
  src/ShaderAsset.cpp
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

  add_executable(ShaderAssetSmoke ${CMAKE_SOURCE_DIR}/Tests/ShaderAssetSmoke.cpp)
  target_link_libraries(ShaderAssetSmoke PRIVATE RawIron::Content)
  add_test(NAME RawIron.Content.ShaderAssetSmoke COMMAND ShaderAssetSmoke ${CMAKE_SOURCE_DIR})

  add_executable(ExtensionDescriptorSmoke ${CMAKE_SOURCE_DIR}/Tests/ExtensionDescriptorSmoke.cpp)
  target_link_libraries(ExtensionDescriptorSmoke PRIVATE RawIron::Content)
  add_test(NAME RawIron.Content.ExtensionDescriptorSmoke COMMAND ExtensionDescriptorSmoke)

  add_executable(AuthoringHandoffSmoke ${CMAKE_SOURCE_DIR}/Tests/AuthoringHandoffSmoke.cpp)
  target_link_libraries(AuthoringHandoffSmoke PRIVATE RawIron::Content)
  add_test(NAME RawIron.Content.AuthoringHandoffSmoke COMMAND AuthoringHandoffSmoke)

  add_executable(PrimitiveModelDocumentSmoke ${CMAKE_SOURCE_DIR}/Tests/PrimitiveModelDocumentSmoke.cpp)
  target_link_libraries(PrimitiveModelDocumentSmoke PRIVATE RawIron::Content)
  add_test(NAME RawIron.Content.PrimitiveModelDocumentSmoke COMMAND PrimitiveModelDocumentSmoke)

  add_executable(PluginRuntimeSmoke ${CMAKE_SOURCE_DIR}/Tests/PluginRuntimeSmoke.cpp)
  target_link_libraries(PluginRuntimeSmoke PRIVATE RawIron::Content)
  add_test(NAME RawIron.Content.PluginRuntimeSmoke COMMAND PluginRuntimeSmoke)

  add_executable(PackageResolverSmoke ${CMAKE_SOURCE_DIR}/Tests/PackageResolverSmoke.cpp)
  target_link_libraries(PackageResolverSmoke PRIVATE RawIron::Content)
  add_test(NAME RawIron.Content.PackageResolverSmoke COMMAND PackageResolverSmoke)

  add_executable(PackageMountRegistrySmoke ${CMAKE_SOURCE_DIR}/Tests/PackageMountRegistrySmoke.cpp)
  target_link_libraries(PackageMountRegistrySmoke PRIVATE RawIron::Content)
  add_test(NAME RawIron.Content.PackageMountRegistrySmoke COMMAND PackageMountRegistrySmoke)

  add_library(NativePluginFixture MODULE ${CMAKE_SOURCE_DIR}/Tests/NativePluginFixture.cpp)
  target_include_directories(NativePluginFixture PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_compile_features(NativePluginFixture PRIVATE cxx_std_20)
  set_target_properties(NativePluginFixture PROPERTIES PREFIX "")

  add_executable(NativePluginHostSmoke ${CMAKE_SOURCE_DIR}/Tests/NativePluginHostSmoke.cpp)
  target_link_libraries(NativePluginHostSmoke PRIVATE RawIron::Content)
  add_dependencies(NativePluginHostSmoke NativePluginFixture)
  add_test(
    NAME RawIron.Content.NativePluginHostSmoke
    COMMAND NativePluginHostSmoke $<TARGET_FILE:NativePluginFixture>
  )

  add_test(
    NAME RawIron.Content.PluginProjectData
    COMMAND ${CMAKE_COMMAND}
      -DRAWIRON_WORKSPACE=${CMAKE_SOURCE_DIR}
      -P ${CMAKE_SOURCE_DIR}/Tests/VerifyPluginProjectData.cmake
  )
endif()
