if(NOT DEFINED RAWIRON_RUNTIME_DESTINATION OR RAWIRON_RUNTIME_DESTINATION STREQUAL "")
  message(FATAL_ERROR "RAWIRON_RUNTIME_DESTINATION must be provided.")
endif()

if(NOT DEFINED RAWIRON_RUNTIME_DLLS OR RAWIRON_RUNTIME_DLLS STREQUAL "")
  return()
endif()

string(REPLACE "|" ";" rawiron_runtime_dlls "${RAWIRON_RUNTIME_DLLS}")

foreach(rawiron_runtime_dll IN LISTS rawiron_runtime_dlls)
  if(rawiron_runtime_dll STREQUAL "")
    continue()
  endif()
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
      "${rawiron_runtime_dll}"
      "${RAWIRON_RUNTIME_DESTINATION}"
    RESULT_VARIABLE rawiron_copy_result
  )
  if(NOT rawiron_copy_result EQUAL 0)
    message(FATAL_ERROR
      "Failed to copy runtime DLL '${rawiron_runtime_dll}' to '${RAWIRON_RUNTIME_DESTINATION}'.")
  endif()
endforeach()
