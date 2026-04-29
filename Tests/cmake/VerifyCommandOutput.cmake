if(NOT DEFINED EXECUTABLE)
  message(FATAL_ERROR "EXECUTABLE must be provided.")
endif()

if(NOT DEFINED EXPECTED_STRINGS)
  message(FATAL_ERROR "EXPECTED_STRINGS must be provided.")
endif()

execute_process(
  COMMAND "${EXECUTABLE}" ${COMMAND_ARGS}
  RESULT_VARIABLE command_result
  OUTPUT_VARIABLE command_stdout
  ERROR_VARIABLE command_stderr
)

set(command_output "${command_stdout}${command_stderr}")

if(NOT command_result EQUAL 0)
  message(FATAL_ERROR
    "Command failed with exit code ${command_result}\n"
    "Command: ${EXECUTABLE} ${COMMAND_ARGS}\n"
    "Output:\n${command_output}")
endif()

foreach(expected IN LISTS EXPECTED_STRINGS)
  string(FIND "${command_output}" "${expected}" expected_index)
  if(expected_index EQUAL -1)
    message(FATAL_ERROR
      "Missing expected output: ${expected}\n"
      "Command: ${EXECUTABLE} ${COMMAND_ARGS}\n"
      "Output:\n${command_output}")
  endif()
endforeach()

foreach(forbidden IN LISTS FORBIDDEN_STRINGS)
  string(FIND "${command_output}" "${forbidden}" forbidden_index)
  if(NOT forbidden_index EQUAL -1)
    message(FATAL_ERROR
      "Found forbidden output: ${forbidden}\n"
      "Command: ${EXECUTABLE} ${COMMAND_ARGS}\n"
      "Output:\n${command_output}")
  endif()
endforeach()

if(DEFINED VERIFY_DIRECTORIES_ROOT)
  foreach(relative_directory IN LISTS REQUIRED_RELATIVE_DIRECTORIES)
    if(NOT IS_DIRECTORY "${VERIFY_DIRECTORIES_ROOT}/${relative_directory}")
      message(FATAL_ERROR
        "Expected directory was not created: ${VERIFY_DIRECTORIES_ROOT}/${relative_directory}\n"
        "Command: ${EXECUTABLE} ${COMMAND_ARGS}\n"
      "Output:\n${command_output}")
    endif()
  endforeach()
endif()

if(DEFINED VERIFY_FILES_ROOT)
  foreach(relative_file IN LISTS REQUIRED_RELATIVE_FILES)
    if(NOT EXISTS "${VERIFY_FILES_ROOT}/${relative_file}")
      message(FATAL_ERROR
        "Expected file was not created: ${VERIFY_FILES_ROOT}/${relative_file}\n"
        "Command: ${EXECUTABLE} ${COMMAND_ARGS}\n"
        "Output:\n${command_output}")
    endif()
  endforeach()
endif()
