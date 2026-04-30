include(CMakeParseArguments)

function(rawiron_add_app)
  set(options BUNDLE_ENGINE_TEXTURES)
  set(oneValueArgs TARGET)
  set(multiValueArgs
    SOURCES
    LIBRARIES
    WIN32_LIBRARIES
    COMPILE_DEFINITIONS
    EXPECTED_STRINGS
    FORBIDDEN_STRINGS
    SMOKE_ARGS
    REQUIRED_RELATIVE_FILES)
  cmake_parse_arguments(RAWIRON_APP "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  if(NOT RAWIRON_APP_TARGET)
    message(FATAL_ERROR "rawiron_add_app: TARGET is required")
  endif()
  if(NOT RAWIRON_APP_SOURCES)
    message(FATAL_ERROR "rawiron_add_app: SOURCES are required for ${RAWIRON_APP_TARGET}")
  endif()

  add_executable("${RAWIRON_APP_TARGET}" ${RAWIRON_APP_SOURCES})

  if(RAWIRON_APP_LIBRARIES)
    target_link_libraries("${RAWIRON_APP_TARGET}" PRIVATE ${RAWIRON_APP_LIBRARIES})
  endif()

  if(WIN32 AND RAWIRON_APP_WIN32_LIBRARIES)
    target_link_libraries("${RAWIRON_APP_TARGET}" PRIVATE ${RAWIRON_APP_WIN32_LIBRARIES})
  endif()

  target_compile_features("${RAWIRON_APP_TARGET}" PRIVATE cxx_std_20)

  if(WIN32)
    target_compile_definitions("${RAWIRON_APP_TARGET}" PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
  endif()

  if(RAWIRON_APP_COMPILE_DEFINITIONS)
    target_compile_definitions("${RAWIRON_APP_TARGET}" PRIVATE ${RAWIRON_APP_COMPILE_DEFINITIONS})
  endif()

  if(WIN32 AND EXISTS "${RAWIRON_VULKAN_RUNTIME_DLL}")
    if(RAWIRON_APP_LIBRARIES MATCHES "RawIron::RenderVulkan")
      add_custom_command(TARGET "${RAWIRON_APP_TARGET}" POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${RAWIRON_VULKAN_RUNTIME_DLL}"
          "$<TARGET_FILE_DIR:${RAWIRON_APP_TARGET}>/vulkan-1.dll")
    endif()
  endif()

  rawiron_stage_runtime_dlls("${RAWIRON_APP_TARGET}")

  if(RAWIRON_APP_BUNDLE_ENGINE_TEXTURES)
    rawiron_bundle_engine_textures("${RAWIRON_APP_TARGET}")
  endif()

  if(RAWIRON_BUILD_TESTS AND RAWIRON_APP_EXPECTED_STRINGS)
    set(rawiron_verify_command_output "${PROJECT_SOURCE_DIR}/Tests/cmake/VerifyCommandOutput.cmake")
    set(rawiron_smoke_command_args
      "-DEXECUTABLE=$<TARGET_FILE:${RAWIRON_APP_TARGET}>"
      "-DCOMMAND_ARGS_RAW=$<JOIN:${RAWIRON_APP_SMOKE_ARGS},|>"
      "-DEXPECTED_STRINGS=${RAWIRON_APP_EXPECTED_STRINGS}"
      "-DFORBIDDEN_STRINGS=${RAWIRON_APP_FORBIDDEN_STRINGS}")
    if(RAWIRON_APP_REQUIRED_RELATIVE_FILES)
      set(rawiron_verify_files_root "${CMAKE_CURRENT_BINARY_DIR}/${RAWIRON_APP_TARGET}-smoke")
      list(APPEND rawiron_smoke_command_args
        "-DVERIFY_FILES_ROOT=${rawiron_verify_files_root}"
        "-DREQUIRED_RELATIVE_FILES=${RAWIRON_APP_REQUIRED_RELATIVE_FILES}")
    endif()

    add_test(
      NAME "${RAWIRON_APP_TARGET}.Smoke"
      COMMAND ${CMAKE_COMMAND}
        ${rawiron_smoke_command_args}
        -P "${rawiron_verify_command_output}"
    )
  endif()
endfunction()
