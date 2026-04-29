add_library(RawIron.Logic
  src/LogicAuthoring.cpp
  src/LogicGraph.cpp
  src/LogicKitManifest.cpp
  src/LogicPortSchema.cpp
  src/LogicVisualPrimitives.cpp
)

add_library(RawIron::Logic ALIAS RawIron.Logic)

target_include_directories(RawIron.Logic
  PUBLIC
    include
)

target_compile_features(RawIron.Logic PUBLIC cxx_std_20)

target_link_libraries(RawIron.Logic
  PRIVATE
    RawIron::Core
)
