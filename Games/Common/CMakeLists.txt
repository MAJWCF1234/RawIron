# Compatibility/test entry point. Shared host implementation lives in Source/RawIron.GameHost.
if(RAWIRON_BUILD_TESTS)
  add_executable(GameTuningContractSmoke ${CMAKE_SOURCE_DIR}/Tests/GameTuningContractSmoke.cpp)
  target_link_libraries(GameTuningContractSmoke PRIVATE RawIron::Game.RuntimeDiagnosticsDraw)
  add_test(NAME RawIron.Games.TuningContractSmoke COMMAND GameTuningContractSmoke ${CMAKE_SOURCE_DIR})
endif()
