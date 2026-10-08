if(NOT DEFINED EXECUTABLE OR NOT DEFINED OUTPUT_ROOT)
  message(FATAL_ERROR "EXECUTABLE and OUTPUT_ROOT are required")
endif()

# All authored output is isolated in the build's test workspace.
file(MAKE_DIRECTORY "${OUTPUT_ROOT}")
set(character "${OUTPUT_ROOT}/pipeline.ri_blockchar.json")
function(run_tool expected_success)
  execute_process(COMMAND "${EXECUTABLE}" --root "${OUTPUT_ROOT}" ${ARGN}
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 20)
  if(expected_success AND NOT result STREQUAL "0")
    message(FATAL_ERROR "Authoring command failed: ${ARGN}\n${output}\n${error}")
  elseif(NOT expected_success AND result STREQUAL "0")
    message(FATAL_ERROR "Invalid authoring command unexpectedly succeeded: ${ARGN}")
  endif()
endfunction()

run_tool(TRUE --blockchar-create pipeline --output "${character}" --overwrite)
run_tool(TRUE --blockchar-add-part --blockchar "${character}" --bone pelvis
  --part torso --shape bevel --sx 0.3 --sy 0.4 --sz 0.2 --bevel 0.1)
file(READ "${character}" authored)
string(JSON count LENGTH "${authored}" parts)
string(JSON shape GET "${authored}" parts 0 shape)
string(JSON bone GET "${authored}" parts 0 boneName)
string(JSON extent GET "${authored}" parts 0 halfExtent y)
if(NOT count EQUAL 1 OR NOT shape STREQUAL "bevel" OR NOT bone STREQUAL "pelvis"
    OR extent LESS 0.399 OR extent GREATER 0.401)
  message(FATAL_ERROR "CLI-authored model lost its shape, bone, or dimensions")
endif()

# Failure must preserve the previously authored asset byte for byte.
file(SHA256 "${character}" before)
run_tool(FALSE --blockchar-add-part --blockchar "${character}" --part missing_bone)
run_tool(FALSE --blockchar-add-part --blockchar "${character}" --bone pelvis --part torso)
run_tool(FALSE --blockchar-add-part --blockchar "${character}" --bone pelvis
  --part invalid_extent --sx -1)
file(SHA256 "${character}" after)
if(NOT before STREQUAL after)
  message(FATAL_ERROR "Rejected authoring command modified the saved model")
endif()
run_tool(TRUE --blockchar-report "${character}")
message(STATUS "Block-character CLI create/add/reload and failure preservation passed")
