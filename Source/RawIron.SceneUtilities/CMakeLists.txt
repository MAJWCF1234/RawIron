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
  src/GltfLoader.cpp
  src/Helpers.cpp
  src/LevelObjectRegistry.cpp
  src/LoftPrimitiveStack.cpp
  src/ModelLoader.cpp
  src/PhysicsPropSpawn.cpp
  src/PrimitiveTypeCanonical.cpp
  src/StructuralBrush.cpp
  src/StructuralPrimitiveBundle.cpp
  src/PrimitivesCsvIO.cpp
  src/Raycast.cpp
  src/RuntimeMeshFactory.cpp
  src/SceneEntityPhysics.cpp
  src/ScriptedCameraReview.cpp
  src/SceneRenderSubmission.cpp
  src/SceneKit.cpp
  src/SceneStateIO.cpp
  src/SceneUtils.cpp
  src/SceneStructuralTraceFeed.cpp
  src/SceneSubtreeColliders.cpp
  src/SceneSubtreeHelpers.cpp
  src/TraceMeshRefinement.cpp
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
    RawIron::Structural
    RawIron::Trace
  PRIVATE
    RawIron.cgltf
    RawIron.ufbx
)

target_compile_features(RawIron.SceneUtilities PUBLIC cxx_std_20)
