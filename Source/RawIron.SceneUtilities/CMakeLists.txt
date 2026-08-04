set(CGLTF_SOURCE_DIR "${CMAKE_SOURCE_DIR}/ThirdParty/cgltf")
set(UFBX_SOURCE_DIR "${CMAKE_SOURCE_DIR}/ThirdParty/ufbx")

add_library(RawIron.cgltf STATIC
  ${CGLTF_SOURCE_DIR}/cgltf_impl.cpp
)
target_include_directories(RawIron.cgltf PUBLIC ${CGLTF_SOURCE_DIR})
target_compile_features(RawIron.cgltf PUBLIC cxx_std_20)
if (MSVC)
  target_compile_definitions(RawIron.cgltf PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

add_library(RawIron.ufbx STATIC
  ${UFBX_SOURCE_DIR}/ufbx.c
)
target_compile_features(RawIron.ufbx PUBLIC c_std_99)
if (MSVC)
  target_compile_definitions(RawIron.ufbx PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

add_library(RawIron.SceneUtilities
  src/Animation.cpp
  src/AnimationLibraryHydration.cpp
  src/FbxLoader.cpp
  src/UfbxMeshImport.cpp
  src/GltfLoader.cpp
  src/Helpers.cpp
  src/HumanoidAnimationProfiles.cpp
  src/HumanoidRigNames.cpp
  src/LevelObjectRegistry.cpp
  src/LoftPrimitiveStack.cpp
  src/ModelLoader.cpp
  src/PhysicsPropSpawn.cpp
  src/PrimitiveTypeCanonical.cpp
  src/StructuralAssemblyIO.cpp
  src/StructuralBrush.cpp
  src/StructuralPrimitiveBundle.cpp
  src/StructuralPrimitivePresets.cpp
  src/PrimitivesCsvIO.cpp
  src/PrimitiveModelBake.cpp
  src/Raycast.cpp
  src/RigAuthoring.cpp
  src/RuntimeMeshFactory.cpp
  src/SceneEntityPhysics.cpp
  src/ScriptedCameraReview.cpp
  src/SceneRenderSubmission.cpp
  src/ParticleSystem.cpp
  src/SceneKit.cpp
  src/SceneStateIO.cpp
  src/SceneUtils.cpp
  src/SemanticStructuralPartition.cpp
  src/SceneStructuralTraceFeed.cpp
  src/SceneSubtreeColliders.cpp
  src/SceneSubtreeHelpers.cpp
  src/TraceMeshRefinement.cpp
  src/UfbxImportSehWin32.cpp
  src/WorkspaceSandbox.cpp
)

add_library(RawIron::SceneUtilities ALIAS RawIron.SceneUtilities)
add_library(RawIron::SceneKit ALIAS RawIron.SceneUtilities)

target_include_directories(RawIron.SceneUtilities
  PUBLIC
    include
  PRIVATE
    ${UFBX_SOURCE_DIR}
)

target_link_libraries(RawIron.SceneUtilities
  PUBLIC
    RawIron::Core
    RawIron::Content
    RawIron::Structural
    RawIron::Trace
  PRIVATE
    RawIron.cgltf
    RawIron.ufbx
)

target_compile_features(RawIron.SceneUtilities PUBLIC cxx_std_20)

if (RAWIRON_BUILD_TESTS)
  add_executable(AnimationSafetySmoke ${CMAKE_SOURCE_DIR}/Tests/AnimationSafetySmoke.cpp)
  target_link_libraries(AnimationSafetySmoke PRIVATE RawIron::SceneUtilities)
  add_test(NAME RawIron.SceneUtilities.AnimationSafetySmoke COMMAND AnimationSafetySmoke)

  add_executable(PrimitivesCsvSafetySmoke ${CMAKE_SOURCE_DIR}/Tests/PrimitivesCsvSafetySmoke.cpp)
  target_link_libraries(PrimitivesCsvSafetySmoke PRIVATE RawIron::SceneUtilities)
  add_test(NAME RawIron.SceneUtilities.PrimitivesCsvSafetySmoke COMMAND PrimitivesCsvSafetySmoke)

  add_executable(StructuralBrushMetadataSmoke ${CMAKE_SOURCE_DIR}/Tests/StructuralBrushMetadataSmoke.cpp)
  target_link_libraries(StructuralBrushMetadataSmoke PRIVATE RawIron::SceneUtilities)
  add_test(NAME RawIron.SceneUtilities.StructuralBrushMetadataSmoke COMMAND StructuralBrushMetadataSmoke)

  add_executable(SemanticStructuralPartitionSmoke ${CMAKE_SOURCE_DIR}/Tests/SemanticStructuralPartitionSmoke.cpp)
  target_link_libraries(SemanticStructuralPartitionSmoke PRIVATE RawIron::SceneUtilities)
  add_test(NAME RawIron.SceneUtilities.SemanticStructuralPartitionSmoke COMMAND SemanticStructuralPartitionSmoke)

  add_executable(SemanticStructuralPartitionConcurrencySmoke
    ${CMAKE_SOURCE_DIR}/Tests/SemanticStructuralPartitionConcurrencySmoke.cpp)
  find_package(Threads REQUIRED)
  target_link_libraries(SemanticStructuralPartitionConcurrencySmoke
    PRIVATE RawIron::SceneUtilities Threads::Threads)
  add_test(NAME RawIron.SceneUtilities.SemanticStructuralPartitionConcurrencySmoke
    COMMAND SemanticStructuralPartitionConcurrencySmoke)

  add_executable(SceneSubtreeCollidersSmoke ${CMAKE_SOURCE_DIR}/Tests/SceneSubtreeCollidersSmoke.cpp)
  target_link_libraries(SceneSubtreeCollidersSmoke PRIVATE RawIron::SceneUtilities)
  add_test(NAME RawIron.SceneUtilities.SceneSubtreeCollidersSmoke COMMAND SceneSubtreeCollidersSmoke)

  add_executable(SceneStructuralTraceFeedSmoke ${CMAKE_SOURCE_DIR}/Tests/SceneStructuralTraceFeedSmoke.cpp)
  target_link_libraries(SceneStructuralTraceFeedSmoke PRIVATE RawIron::SceneUtilities)
  add_test(NAME RawIron.SceneUtilities.SceneStructuralTraceFeedSmoke COMMAND SceneStructuralTraceFeedSmoke)

  add_executable(RigAuthoringSmoke ${CMAKE_SOURCE_DIR}/Tests/RigAuthoringSmoke.cpp)
  target_link_libraries(RigAuthoringSmoke PRIVATE RawIron::SceneUtilities)
  add_test(NAME RawIron.SceneUtilities.RigAuthoringSmoke COMMAND RigAuthoringSmoke)

  add_executable(AuthoringHelpersSafetySmoke ${CMAKE_SOURCE_DIR}/Tests/AuthoringHelpersSafetySmoke.cpp)
  target_link_libraries(AuthoringHelpersSafetySmoke PRIVATE RawIron::SceneUtilities)
  add_test(NAME RawIron.SceneUtilities.AuthoringHelpersSafetySmoke COMMAND AuthoringHelpersSafetySmoke)

  add_executable(SceneEntityPhysicsSmoke ${CMAKE_SOURCE_DIR}/Tests/SceneEntityPhysicsSmoke.cpp)
  target_link_libraries(SceneEntityPhysicsSmoke PRIVATE RawIron::SceneUtilities)
  add_test(NAME RawIron.SceneUtilities.SceneEntityPhysicsSmoke COMMAND SceneEntityPhysicsSmoke)

  add_executable(SceneStateIOSmoke ${CMAKE_SOURCE_DIR}/Tests/SceneStateIOSmoke.cpp)
  target_link_libraries(SceneStateIOSmoke PRIVATE RawIron::SceneUtilities Threads::Threads)
  add_test(NAME RawIron.SceneUtilities.SceneStateIOSmoke COMMAND SceneStateIOSmoke)

  add_executable(SceneStateIOCommitRecoverySmoke
    ${CMAKE_SOURCE_DIR}/Tests/SceneStateIOCommitRecoverySmoke.cpp)
  target_include_directories(SceneStateIOCommitRecoverySmoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_compile_features(SceneStateIOCommitRecoverySmoke PRIVATE cxx_std_20)
  add_test(NAME RawIron.SceneUtilities.SceneStateIOCommitRecoverySmoke
    COMMAND SceneStateIOCommitRecoverySmoke)

  add_executable(PrimitiveModelBakeSmoke ${CMAKE_SOURCE_DIR}/Tests/PrimitiveModelBakeSmoke.cpp)
  target_link_libraries(PrimitiveModelBakeSmoke PRIVATE RawIron::SceneUtilities)
  add_test(NAME RawIron.SceneUtilities.PrimitiveModelBakeSmoke COMMAND PrimitiveModelBakeSmoke)
endif()
