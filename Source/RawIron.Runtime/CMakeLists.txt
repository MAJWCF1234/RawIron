add_library(RawIron.Runtime
  src/BotClients.cpp
  src/DemoStream.cpp
  src/EntityIoTelemetry.cpp
  src/EosRendezvousProvider.cpp
  src/ExperiencePresets.cpp
  src/HostChrome.cpp
  src/HostInputService.cpp
  src/JobSystemService.cpp
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
  src/SessionExtensionProtocol.cpp
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
  if (WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(rawiron_eos_lib "${EOS_SDK_ROOT}/Lib/EOSSDK-Win64-Shipping.lib")
    set(rawiron_eos_dll "${EOS_SDK_ROOT}/Bin/EOSSDK-Win64-Shipping.dll")
  else()
    set(rawiron_eos_lib "")
    set(rawiron_eos_dll "")
  endif()
  set(rawiron_eos_bin_dir "${EOS_SDK_ROOT}/Bin")

  # RAWIRON_HAS_EOS compiles the real SDK integration, so it may only be defined when the headers
  # and the import library are both present. Defining it off the RAWIRON_USE_EOS switch alone turns
  # "SDK not installed yet" into a compile/link failure instead of the documented stub fallback.
  set(rawiron_eos_available FALSE)
  if (EXISTS "${EOS_SDK_ROOT}/Include/eos_sdk.h" AND EXISTS "${rawiron_eos_lib}")
    set(rawiron_eos_available TRUE)
  endif()

  if (rawiron_eos_available)
    target_compile_definitions(RawIron.Runtime PUBLIC RAWIRON_HAS_EOS=1)
    target_include_directories(RawIron.Runtime PUBLIC "${EOS_SDK_ROOT}/Include")
    if (EXISTS "${rawiron_eos_bin_dir}")
      target_link_directories(RawIron.Runtime PUBLIC "${rawiron_eos_bin_dir}")
    endif()
    target_link_libraries(RawIron.Runtime PUBLIC "${rawiron_eos_lib}")
    message(STATUS "RawIron: linking EOS SDK ${rawiron_eos_lib}")
    if (EXISTS "${rawiron_eos_dll}")
      # Make the runtime DLL discoverable next to built tools/games when they link Runtime.
      set(RAWIRON_EOS_DLL "${rawiron_eos_dll}" CACHE FILEPATH "EOS runtime DLL to copy beside dependents" FORCE)
    endif()
  else()
    message(WARNING
      "RawIron: RAWIRON_USE_EOS=ON but a usable EOS SDK was not found under ${EOS_SDK_ROOT} "
      "(need Include/eos_sdk.h and Lib/EOSSDK-Win64-Shipping.lib). Building the EOS rendezvous "
      "stub instead; runtime falls back to the DirectToken provider.")
  endif()
endif()

target_compile_features(RawIron.Runtime PUBLIC cxx_std_20)

if(RAWIRON_BUILD_TESTS)
  add_executable(RuntimeNetcodeSmoke ${CMAKE_SOURCE_DIR}/Tests/RuntimeNetcodeSmoke.cpp)
  target_link_libraries(RuntimeNetcodeSmoke PRIVATE RawIron::Runtime)
  add_test(NAME RawIron.Runtime.NetcodeSmoke COMMAND RuntimeNetcodeSmoke)

  add_executable(SessionExtensionProtocolSmoke
    ${CMAKE_SOURCE_DIR}/Tests/SessionExtensionProtocolSmoke.cpp)
  target_include_directories(SessionExtensionProtocolSmoke PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_link_libraries(SessionExtensionProtocolSmoke PRIVATE RawIron::Runtime)
  add_test(NAME RawIron.Runtime.SessionExtensionProtocolSmoke COMMAND SessionExtensionProtocolSmoke)

  add_executable(DemoStreamSmoke ${CMAKE_SOURCE_DIR}/Tests/DemoStreamSmoke.cpp)
  target_link_libraries(DemoStreamSmoke PRIVATE RawIron::Runtime)
  add_test(NAME RawIron.Runtime.DemoStreamSmoke COMMAND DemoStreamSmoke)

  add_executable(RuntimeEventBusSafetySmoke ${CMAKE_SOURCE_DIR}/Tests/RuntimeEventBusSafetySmoke.cpp)
  target_link_libraries(RuntimeEventBusSafetySmoke PRIVATE RawIron::Runtime)
  add_test(NAME RawIron.Runtime.EventBusSafetySmoke COMMAND RuntimeEventBusSafetySmoke)

  add_executable(SnapshotReplicationSmoke ${CMAKE_SOURCE_DIR}/Tests/SnapshotReplicationSmoke.cpp)
  target_link_libraries(SnapshotReplicationSmoke PRIVATE RawIron::Runtime)
  add_test(NAME RawIron.Runtime.SnapshotReplicationSmoke COMMAND SnapshotReplicationSmoke)

  add_executable(NetHardeningSmoke ${CMAKE_SOURCE_DIR}/Tests/NetHardeningSmoke.cpp)
  target_link_libraries(NetHardeningSmoke PRIVATE RawIron::Runtime)
  add_test(NAME RawIron.Runtime.NetHardeningSmoke COMMAND NetHardeningSmoke)

  add_executable(NetPacketResourceBoundarySmoke
    ${CMAKE_SOURCE_DIR}/Tests/NetPacketResourceBoundarySmoke.cpp)
  target_link_libraries(NetPacketResourceBoundarySmoke PRIVATE RawIron::Runtime)
  if (RAWIRON_USE_ENET AND DEFINED enet_SOURCE_DIR)
    target_include_directories(NetPacketResourceBoundarySmoke PRIVATE "${enet_SOURCE_DIR}/include")
  endif()
  add_test(NAME RawIron.Runtime.NetPacketResourceBoundarySmoke COMMAND NetPacketResourceBoundarySmoke)

  add_executable(RuntimeCoreLifecycleSmoke ${CMAKE_SOURCE_DIR}/Tests/RuntimeCoreLifecycleSmoke.cpp)
  target_link_libraries(RuntimeCoreLifecycleSmoke PRIVATE RawIron::Runtime)
  add_test(NAME RawIron.Runtime.CoreLifecycleSmoke COMMAND RuntimeCoreLifecycleSmoke)

  add_executable(JobSystemServiceSmoke ${CMAKE_SOURCE_DIR}/Tests/JobSystemServiceSmoke.cpp)
  target_link_libraries(JobSystemServiceSmoke PRIVATE RawIron::Runtime)
  add_test(NAME RawIron.Runtime.JobSystemServiceSmoke COMMAND JobSystemServiceSmoke)

  # Runtime tests pull in the EOS rendezvous provider, so they need the same DLL
  # staging the apps get; otherwise they fail to load with 0xC0000135 on any
  # machine that actually has the EOS SDK installed.
  foreach(rawiron_runtime_test
      RuntimeNetcodeSmoke
      SessionExtensionProtocolSmoke
      DemoStreamSmoke
      RuntimeEventBusSafetySmoke
      SnapshotReplicationSmoke
      NetHardeningSmoke
      NetPacketResourceBoundarySmoke
      RuntimeCoreLifecycleSmoke
      JobSystemServiceSmoke)
    rawiron_stage_runtime_dlls(${rawiron_runtime_test})
  endforeach()
endif()
