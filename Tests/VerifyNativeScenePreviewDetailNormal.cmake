if(NOT DEFINED SHADER_PATH)
  message(FATAL_ERROR "SHADER_PATH is required")
endif()

file(READ "${SHADER_PATH}" SHADER_TEXT)

if(SHADER_TEXT MATCHES "vec3[ \t\r\n]+detailNormal[ \t\r\n]*=[ \t\r\n]*texture\\(detailTex,[ \t\r\n]*detailUv\\)")
  message(FATAL_ERROR "NativeScenePreview.frag must not decode detailTex as a tangent-space normal map")
endif()
