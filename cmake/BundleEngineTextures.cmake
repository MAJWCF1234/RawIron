# Keep one canonical `Assets/Textures` tree in the workspace. Runtime lookup
# already walks from build outputs back to the repo root, so copying the full
# texture library into every build tree only bloats disk usage.
set(RAWIRON_PREVIEW_TEXTURE_LIBRARY_DIR "${CMAKE_SOURCE_DIR}/Assets/Textures")

function(rawiron_bundle_engine_textures target)
  if(NOT TARGET "${target}")
    message(FATAL_ERROR "rawiron_bundle_engine_textures: not a target: ${target}")
  endif()
  if(NOT EXISTS "${RAWIRON_PREVIEW_TEXTURE_LIBRARY_DIR}")
    message(WARNING "RawIron: missing ${RAWIRON_PREVIEW_TEXTURE_LIBRARY_DIR} — texture bundle skipped for ${target}")
    return()
  endif()
endfunction()

function(rawiron_stage_runtime_dlls target)
  if(NOT TARGET "${target}")
    message(FATAL_ERROR "rawiron_stage_runtime_dlls: not a target: ${target}")
  endif()
endfunction()

function(rawiron_runtime_dll_test_arg out_var target)
  if(NOT TARGET "${target}")
    message(FATAL_ERROR "rawiron_runtime_dll_test_arg: not a target: ${target}")
  endif()
  set(${out_var}
    "-DRUNTIME_DLLS_RAW=$<JOIN:$<TARGET_RUNTIME_DLLS:${target}>,|>"
    PARENT_SCOPE)
endfunction()
