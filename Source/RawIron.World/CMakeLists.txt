add_library(RawIron.World
  src/AccessFeedbackState.cpp
  src/CheckpointPersistence.cpp
  src/DeveloperConsoleState.cpp
  src/DialogueCueState.cpp
  src/HostileCharacterAi.cpp
  src/LevelFlowPresentationState.cpp
  src/InteractionPromptState.cpp
  src/InventoryState.cpp
  src/NpcAgentState.cpp
  src/PickupFeedbackState.cpp
  src/PlayerVitality.cpp
  src/PresentationState.cpp
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
