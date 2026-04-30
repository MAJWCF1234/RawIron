# Install layout: stage one shared `Assets/Textures` tree at the build root so
# ri::content::ResolveEngineTexturesDirectory finds it by walking upward from
# target output directories like `Apps/.../RelWithDebInfo`.
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

  get_property(rawiron_texture_bundle_target GLOBAL PROPERTY RAWIRON_TEXTURE_BUNDLE_TARGET)
  if(NOT rawiron_texture_bundle_target)
    set(rawiron_texture_bundle_target rawiron_engine_textures_bundle)
    set(rawiron_shared_texture_dir "${CMAKE_BINARY_DIR}/Assets/Textures")
    add_custom_target("${rawiron_texture_bundle_target}"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${rawiron_shared_texture_dir}"
      COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${RAWIRON_PREVIEW_TEXTURE_LIBRARY_DIR}"
        "${rawiron_shared_texture_dir}"
      VERBATIM
      COMMENT "Stage shared Assets/Textures library"
    )
    set_property(GLOBAL PROPERTY RAWIRON_TEXTURE_BUNDLE_TARGET "${rawiron_texture_bundle_target}")
  endif()

  add_dependencies("${target}" "${rawiron_texture_bundle_target}")
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
