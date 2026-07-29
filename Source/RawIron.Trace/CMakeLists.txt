add_library(RawIron.Trace
  src/AabbOverlapContact.cpp
  src/BvhLifetimeRegistry.cpp
  src/CompetitiveModel.cpp
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

if (RAWIRON_BUILD_TESTS)
  add_executable(CompetitiveWeaponSimulatorSmoke
    ${CMAKE_SOURCE_DIR}/Tests/CompetitiveWeaponSimulatorSmoke.cpp
  )
  target_link_libraries(CompetitiveWeaponSimulatorSmoke PRIVATE RawIron::Trace)
  add_test(NAME RawIron.Trace.CompetitiveWeaponSimulatorSmoke COMMAND CompetitiveWeaponSimulatorSmoke)

  add_executable(ObjectPhysicsDurationSmoke
    ${CMAKE_SOURCE_DIR}/Tests/ObjectPhysicsDurationSmoke.cpp
  )
  target_link_libraries(ObjectPhysicsDurationSmoke PRIVATE RawIron::Trace)
  add_test(NAME RawIron.Trace.ObjectPhysicsDurationSmoke COMMAND ObjectPhysicsDurationSmoke)

  add_executable(MovementControllerDurationSmoke
    ${CMAKE_SOURCE_DIR}/Tests/MovementControllerDurationSmoke.cpp
  )
  target_link_libraries(MovementControllerDurationSmoke PRIVATE RawIron::Trace)
  add_test(NAME RawIron.Trace.MovementControllerDurationSmoke COMMAND MovementControllerDurationSmoke)
endif()
