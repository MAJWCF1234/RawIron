add_subdirectory(RawIron.Core.Tests)
add_subdirectory(RawIron.EngineImport.Tests)

add_custom_target(rawiron-run-all-tests
  COMMAND ${CMAKE_CTEST_COMMAND}
          --output-on-failure
          --tests-regex "RawIron\\.Core\\.Tests|RawIron\\.EngineImport\\.Tests"
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  COMMENT "Run RawIron.Core.Tests and RawIron.EngineImport.Tests via CTest"
)

add_custom_target(rawiron-list-tests
  COMMAND ${CMAKE_CTEST_COMMAND} -N
          --tests-regex "RawIron\\.Core\\.Tests|RawIron\\.EngineImport\\.Tests"
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  COMMENT "List RawIron native CTest targets"
)
