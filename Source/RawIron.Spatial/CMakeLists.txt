add_library(RawIron.Spatial
  src/SpatialIndex.cpp
)

add_library(RawIron::Spatial ALIAS RawIron.Spatial)

target_include_directories(RawIron.Spatial
  PUBLIC
    include
)

target_link_libraries(RawIron.Spatial
  PUBLIC
    RawIron::Core
)

target_compile_features(RawIron.Spatial PUBLIC cxx_std_20)

if (RAWIRON_BUILD_TESTS)
  add_executable(SpatialIndexSmoke ${CMAKE_SOURCE_DIR}/Tests/SpatialIndexSmoke.cpp)
  target_link_libraries(SpatialIndexSmoke PRIVATE RawIron::Spatial)
  add_test(NAME RawIron.Spatial.SpatialIndexSmoke COMMAND SpatialIndexSmoke)
endif()
