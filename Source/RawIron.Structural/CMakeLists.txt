add_library(RawIron.Structural
  src/ConvexClipper.cpp
  src/StructuralCompiler.cpp
  src/StructuralDeferredOperations.cpp
  src/StructuralGraph.cpp
  src/ProceduralSurfaceMesh.cpp
  src/StructuralPrimitives.cpp
)

add_library(RawIron::Structural ALIAS RawIron.Structural)

target_include_directories(RawIron.Structural
  PUBLIC
    include
)

target_link_libraries(RawIron.Structural
  PUBLIC
    RawIron::Core
)

target_compile_features(RawIron.Structural PUBLIC cxx_std_20)

if (RAWIRON_BUILD_TESTS)
  add_executable(ProceduralGeometrySmoke ${CMAKE_SOURCE_DIR}/Tests/ProceduralGeometrySmoke.cpp)
  target_link_libraries(ProceduralGeometrySmoke PRIVATE RawIron::Structural)
  target_include_directories(ProceduralGeometrySmoke PRIVATE src)
  add_test(NAME RawIron.Structural.ProceduralGeometrySmoke COMMAND ProceduralGeometrySmoke)

  add_executable(ConvexClipperSmoke ${CMAKE_SOURCE_DIR}/Tests/ConvexClipperSmoke.cpp)
  target_link_libraries(ConvexClipperSmoke PRIVATE RawIron::Structural)
  add_test(NAME RawIron.Structural.ConvexClipperSmoke COMMAND ConvexClipperSmoke)

  add_executable(StructuralPrimitiveValidationSmoke
    ${CMAKE_SOURCE_DIR}/Tests/StructuralPrimitiveValidationSmoke.cpp)
  target_link_libraries(StructuralPrimitiveValidationSmoke PRIVATE RawIron::Structural)
  add_test(NAME RawIron.Structural.PrimitiveValidationSmoke COMMAND StructuralPrimitiveValidationSmoke)
endif()
