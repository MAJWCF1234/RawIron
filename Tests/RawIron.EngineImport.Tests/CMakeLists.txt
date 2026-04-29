add_executable(RawIron.EngineImport.Tests
  src/main.cpp
  src/EngineImportTests.cpp
)

target_link_libraries(RawIron.EngineImport.Tests
  PRIVATE
    RawIron::Audio
    RawIron::Content
    RawIron::Core
    RawIron::Debug
    RawIron::Events
    RawIron::Logic
    RawIron::RenderVulkan
    RawIron::Runtime
    RawIron::Spatial
    RawIron::Trace
    RawIron::Structural
    RawIron::Validation
    RawIron::World
)

target_compile_features(RawIron.EngineImport.Tests PRIVATE cxx_std_20)

target_include_directories(RawIron.EngineImport.Tests
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../include")

if(MSVC)
  # Avoid C1041 when /MP compiles multiple TUs into one vc*.pdb.
  target_compile_options(RawIron.EngineImport.Tests PRIVATE /FS)
endif()

rawiron_stage_runtime_dlls(RawIron.EngineImport.Tests)

add_test(
  NAME RawIron.EngineImport.Tests
  COMMAND RawIron.EngineImport.Tests
)

set(rawiron_verify_command_output "${PROJECT_SOURCE_DIR}/Tests/cmake/VerifyCommandOutput.cmake")
add_test(
  NAME RawIron.EngineImport.StacktraceSmoke
  COMMAND ${CMAKE_COMMAND}
    "-DEXECUTABLE=$<TARGET_FILE:RawIron.EngineImport.Tests>"
    "-DCOMMAND_ARGS=--stacktrace-smoke"
    "-DEXPECTED_STRINGS=Stacktrace diagnostics"
    -P "${rawiron_verify_command_output}"
)
