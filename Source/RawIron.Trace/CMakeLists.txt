add_library(RawIron.Trace
  src/EntityPhysics.cpp
  src/KinematicPhysics.cpp
  src/ObjectPhysics.cpp
  src/LocomotionTuning.cpp
  src/MovementController.cpp
  src/SpatialQueryHelpers.cpp
  src/TraceScene.cpp
)

add_library(RawIron::Trace ALIAS RawIron.Trace)

target_include_directories(RawIron.Trace
  PUBLIC
    include
)

target_link_libraries(RawIron.Trace
  PUBLIC
    RawIron::Spatial
)

target_compile_features(RawIron.Trace PUBLIC cxx_std_20)
