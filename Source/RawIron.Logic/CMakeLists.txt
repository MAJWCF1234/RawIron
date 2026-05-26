add_library(RawIron.Logic
  src/LogicAuthoring.cpp
  src/LogicAuthoringDetail.cpp
  src/LogicGraph.cpp
  src/LogicKitManifest.cpp
  src/LogicPortSchema.cpp
  src/LogicVisualPrimitives.cpp
)

add_library(RawIron::Logic ALIAS RawIron.Logic)

target_include_directories(RawIron.Logic
  PUBLIC
    include
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(RawIron.Logic PUBLIC cxx_std_20)

# MinGW: very large TUs (e.g. `LogicAuthoring.cpp`) exceed the default COFF section limit.
if (MINGW)
  target_compile_options(RawIron.Logic PRIVATE "-Wa,-mbig-obj")
endif()

target_link_libraries(RawIron.Logic
  PRIVATE
    RawIron::Core
)
