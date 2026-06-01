if(NOT DEFINED EXECUTABLE)
  message(FATAL_ERROR "EXECUTABLE must be provided.")
endif()

if(NOT DEFINED EXPECTED_STRINGS)
  message(FATAL_ERROR "EXPECTED_STRINGS must be provided.")
endif()

if(DEFINED COMMAND_ARGS_RAW)
  string(REPLACE "|" ";" COMMAND_ARGS "${COMMAND_ARGS_RAW}")
endif()

set(rawiron_command_prefix)
if(WIN32 AND DEFINED RUNTIME_DLLS_RAW AND NOT RUNTIME_DLLS_RAW STREQUAL "")
  string(REPLACE "|" ";" rawiron_runtime_dlls "${RUNTIME_DLLS_RAW}")
  set(rawiron_runtime_path_dirs)
  foreach(rawiron_runtime_dll IN LISTS rawiron_runtime_dlls)
    if(rawiron_runtime_dll STREQUAL "")
      continue()
    endif()
    get_filename_component(rawiron_runtime_dir "${rawiron_runtime_dll}" DIRECTORY)
    if(rawiron_runtime_dir STREQUAL "")
      continue()
    endif()
    list(FIND rawiron_runtime_path_dirs "${rawiron_runtime_dir}" rawiron_runtime_dir_index)
    if(rawiron_runtime_dir_index EQUAL -1)
      list(APPEND rawiron_runtime_path_dirs "${rawiron_runtime_dir}")
    endif()
  endforeach()
  if(rawiron_runtime_path_dirs)
    string(JOIN ";" rawiron_runtime_path_prefix ${rawiron_runtime_path_dirs})
    if(DEFINED ENV{PATH} AND NOT "$ENV{PATH}" STREQUAL "")
      set(rawiron_command_path "${rawiron_runtime_path_prefix};$ENV{PATH}")
    else()
      set(rawiron_command_path "${rawiron_runtime_path_prefix}")
    endif()
    string(REPLACE ";" "\\;" rawiron_path_assignment "PATH=${rawiron_command_path}")
    set(rawiron_command_prefix ${CMAKE_COMMAND} -E env "${rawiron_path_assignment}")
  endif()
endif()

if(rawiron_command_prefix)
  execute_process(
    COMMAND ${rawiron_command_prefix} "${EXECUTABLE}" ${COMMAND_ARGS}
    RESULT_VARIABLE command_result
    OUTPUT_VARIABLE command_stdout
    ERROR_VARIABLE command_stderr
  )
else()
  execute_process(
    COMMAND "${EXECUTABLE}" ${COMMAND_ARGS}
    RESULT_VARIABLE command_result
    OUTPUT_VARIABLE command_stdout
    ERROR_VARIABLE command_stderr
  )
endif()

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
