# Install layout: copy in-repo Assets/Textures next to the executable so
# ri::content::ResolveEngineTexturesDirectory finds `<exe>/Assets/Textures` without a fixed cwd.
set(RAWIRON_PREVIEW_TEXTURE_LIBRARY_DIR "${CMAKE_SOURCE_DIR}/Assets/Textures")
set(RAWIRON_COPY_RUNTIME_DLLS_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/CopyRuntimeDlls.cmake")

function(rawiron_bundle_engine_textures target)
  if(NOT TARGET "${target}")
    message(FATAL_ERROR "rawiron_bundle_engine_textures: not a target: ${target}")
  endif()
  if(NOT EXISTS "${RAWIRON_PREVIEW_TEXTURE_LIBRARY_DIR}")
    message(WARNING "RawIron: missing ${RAWIRON_PREVIEW_TEXTURE_LIBRARY_DIR} — texture bundle skipped for ${target}")
    return()
  endif()
  add_custom_command(
    TARGET "${target}"
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target}>/Assets/Textures"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${RAWIRON_PREVIEW_TEXTURE_LIBRARY_DIR}"
      "$<TARGET_FILE_DIR:${target}>/Assets/Textures"
    VERBATIM
    COMMENT "Bundle Assets/Textures for ${target}"
  )
endfunction()

function(rawiron_stage_runtime_dlls target)
  if(NOT TARGET "${target}")
    message(FATAL_ERROR "rawiron_stage_runtime_dlls: not a target: ${target}")
  endif()
  if(NOT MSVC)
    return()
  endif()
  add_custom_command(
    TARGET "${target}"
    POST_BUILD
    COMMAND ${CMAKE_COMMAND}
      "-DRAWIRON_RUNTIME_DLLS=$<JOIN:$<TARGET_RUNTIME_DLLS:${target}>,|>"
      "-DRAWIRON_RUNTIME_DESTINATION=$<TARGET_FILE_DIR:${target}>"
      -P "${RAWIRON_COPY_RUNTIME_DLLS_SCRIPT}"
    VERBATIM
    COMMENT "Stage runtime DLLs for ${target}"
  )
endfunction()
