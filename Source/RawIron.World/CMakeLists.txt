add_library(RawIron.World
  src/AccessFeedbackState.cpp
  src/BuiltinEntityCommandRouting.cpp
  src/CheckpointPersistence.cpp
  src/DeveloperConsoleState.cpp
  src/EntityInputDispatchPipeline.cpp
  src/DialogueCueState.cpp
  src/HostileCharacterAi.cpp
  src/HudChannelTtlScheduler.cpp
  src/LevelFlowPresentationState.cpp
  src/MissionCompletionFlow.cpp
  src/NavMesh.cpp
  src/InteractionPromptState.cpp
  src/InventoryState.cpp
  src/InteractivePropField.cpp
  src/InteractivePropReplication.cpp
  src/NpcAgentState.cpp
  src/PickupFeedbackState.cpp
  src/PlayerVitality.cpp
  src/PortalTravel.cpp
  src/PresentationState.cpp
  src/RuntimeClipQuery.cpp
  src/RuntimeState.cpp
  src/SignalBroadcastState.cpp
  src/TextOverlayState.cpp
  src/TextOverlayEventBridge.cpp
  src/Instrumentation.cpp
  src/LogicEntityIoTelemetry.cpp
  src/SpatialDebugDraw.cpp
  src/VolumeDescriptors.cpp
)

add_library(RawIron::World ALIAS RawIron.World)

target_include_directories(RawIron.World
  PUBLIC
    include
)

target_link_libraries(RawIron.World
  PUBLIC
    RawIron::Audio
    RawIron::Logic
    RawIron::Runtime
    RawIron::Spatial
    RawIron::Structural
    RawIron::Validation
)

target_compile_features(RawIron.World PUBLIC cxx_std_20)

if (RAWIRON_BUILD_TESTS)
  add_executable(InteractivePropFieldSmoke ${CMAKE_SOURCE_DIR}/Tests/InteractivePropFieldSmoke.cpp)
  target_link_libraries(InteractivePropFieldSmoke PRIVATE RawIron::World)
  add_test(NAME RawIron.World.InteractivePropFieldSmoke COMMAND InteractivePropFieldSmoke)
  add_executable(WorldVolumeSafetySmoke ${CMAKE_SOURCE_DIR}/Tests/WorldVolumeSafetySmoke.cpp)
  target_link_libraries(WorldVolumeSafetySmoke PRIVATE RawIron::World)
  add_test(NAME RawIron.World.VolumeSafetySmoke COMMAND WorldVolumeSafetySmoke)

  add_executable(CheckpointPersistenceSmoke ${CMAKE_SOURCE_DIR}/Tests/CheckpointPersistenceSmoke.cpp)
  target_link_libraries(CheckpointPersistenceSmoke PRIVATE RawIron::World)
  add_test(NAME RawIron.World.CheckpointPersistenceSmoke COMMAND CheckpointPersistenceSmoke)

  add_executable(WorldStateSafetySmoke ${CMAKE_SOURCE_DIR}/Tests/WorldStateSafetySmoke.cpp)
  target_link_libraries(WorldStateSafetySmoke PRIVATE RawIron::World)
  add_test(NAME RawIron.World.StateSafetySmoke COMMAND WorldStateSafetySmoke)

  add_executable(NavMeshPathfindingSmoke ${CMAKE_SOURCE_DIR}/Tests/NavMeshPathfindingSmoke.cpp)
  target_link_libraries(NavMeshPathfindingSmoke PRIVATE RawIron::World)
  add_test(
    NAME RawIron.World.NavMeshPathfindingSmoke
    COMMAND NavMeshPathfindingSmoke ${CMAKE_SOURCE_DIR}
  )

  add_executable(PortalTravelSmoke ${CMAKE_SOURCE_DIR}/Tests/PortalTravelSmoke.cpp)
  target_link_libraries(PortalTravelSmoke PRIVATE RawIron::World)
  add_test(NAME RawIron.World.PortalTravelSmoke COMMAND PortalTravelSmoke)
endif()
