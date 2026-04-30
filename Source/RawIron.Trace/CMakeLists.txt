add_library(RawIron.Trace
  src/AabbOverlapContact.cpp
  src/BvhLifetimeRegistry.cpp
  src/EntityPhysics.cpp
  src/ImpactDecalPlacement.cpp
  src/KinematicPhysics.cpp
  src/LocomotionRuntimeBridge.cpp
  src/LocomotionTuning.cpp
  src/MovementController.cpp
  src/ObjectPhysics.cpp
  src/SpatialQueryHelpers.cpp
  src/SweptAabbContact.cpp
  src/SweptVolumeContactSolver.cpp
  src/TraceScene.cpp
)

add_library(RawIron::Trace ALIAS RawIron.Trace)

target_include_directories(RawIron.Trace
  PUBLIC
    include
)

target_link_libraries(RawIron.Trace
  PUBLIC
    RawIron::Core
    RawIron::Spatial
  PRIVATE
    RawIron::Runtime
)

target_compile_features(RawIron.Trace PUBLIC cxx_std_20)
