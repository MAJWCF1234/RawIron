add_library(RawIron.Runtime
  src/BotClients.cpp
  src/DemoStream.cpp
  src/EntityIoTelemetry.cpp
  src/ExperiencePresets.cpp
  src/LagCompensation.cpp
  src/LatencyTools.cpp
  src/LevelSchedulerBundle.cpp
  src/LevelSchedulerRuntimeModule.cpp
  src/LevelScopedSchedulers.cpp
  src/NetTransport.cpp
  src/RendezvousProvider.cpp
  src/RuntimeCore.cpp
  src/RuntimeEventBus.cpp
  src/RuntimeId.cpp
  src/RuntimeNetcode.cpp
  src/SnapshotReplication.cpp
  src/RuntimeTuning.cpp
)

add_library(RawIron::Runtime ALIAS RawIron.Runtime)

target_include_directories(RawIron.Runtime
  PUBLIC
    include
)

target_link_libraries(RawIron.Runtime
  PUBLIC
    RawIron::Core
)

if (RAWIRON_USE_ENET)
  target_link_libraries(RawIron.Runtime PUBLIC enet)
  if (WIN32)
    # ENet is a static library on Windows and its system dependencies must be
    # visible to every executable that consumes RawIron.Runtime.
    target_link_libraries(RawIron.Runtime PUBLIC ws2_32 winmm)
  endif()
  target_compile_definitions(RawIron.Runtime PUBLIC RAWIRON_HAS_ENET=1)
  if (DEFINED enet_SOURCE_DIR)
    target_include_directories(RawIron.Runtime PRIVATE "${enet_SOURCE_DIR}/include")
  endif()
endif()

if (RAWIRON_USE_EOS)
  target_compile_definitions(RawIron.Runtime PUBLIC RAWIRON_HAS_EOS=1)
  if (EXISTS "${EOS_SDK_ROOT}/Include")
    target_include_directories(RawIron.Runtime PUBLIC "${EOS_SDK_ROOT}/Include")
  endif()
  if (WIN32)
    if (CMAKE_SIZEOF_VOID_P EQUAL 8)
      set(rawiron_eos_bin_dir "${EOS_SDK_ROOT}/Bin/Win64")
    else()
      set(rawiron_eos_bin_dir "${EOS_SDK_ROOT}/Bin/Win32")
    endif()
    if (EXISTS "${rawiron_eos_bin_dir}")
      target_link_directories(RawIron.Runtime PUBLIC "${rawiron_eos_bin_dir}")
    endif()
  endif()
endif()

target_compile_features(RawIron.Runtime PUBLIC cxx_std_20)

if(RAWIRON_BUILD_TESTS)
  add_executable(RuntimeNetcodeSmoke ${CMAKE_SOURCE_DIR}/Tests/RuntimeNetcodeSmoke.cpp)
  target_link_libraries(RuntimeNetcodeSmoke PRIVATE RawIron::Runtime)
  add_test(NAME RawIron.Runtime.NetcodeSmoke COMMAND RuntimeNetcodeSmoke)

  add_executable(DemoStreamSmoke ${CMAKE_SOURCE_DIR}/Tests/DemoStreamSmoke.cpp)
  target_link_libraries(DemoStreamSmoke PRIVATE RawIron::Runtime)
  add_test(NAME RawIron.Runtime.DemoStreamSmoke COMMAND DemoStreamSmoke)

  add_executable(RuntimeEventBusSafetySmoke ${CMAKE_SOURCE_DIR}/Tests/RuntimeEventBusSafetySmoke.cpp)
  target_link_libraries(RuntimeEventBusSafetySmoke PRIVATE RawIron::Runtime)
  add_test(NAME RawIron.Runtime.EventBusSafetySmoke COMMAND RuntimeEventBusSafetySmoke)

  add_executable(SnapshotReplicationSmoke ${CMAKE_SOURCE_DIR}/Tests/SnapshotReplicationSmoke.cpp)
  target_link_libraries(SnapshotReplicationSmoke PRIVATE RawIron::Runtime)
  add_test(NAME RawIron.Runtime.SnapshotReplicationSmoke COMMAND SnapshotReplicationSmoke)
endif()
