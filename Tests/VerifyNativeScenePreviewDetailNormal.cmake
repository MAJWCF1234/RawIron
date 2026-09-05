if(NOT DEFINED SHADER_PATH)
  message(FATAL_ERROR "SHADER_PATH is required")
endif()

file(READ "${SHADER_PATH}" SHADER_TEXT)

# Both direct and hybrid presentation must bound contrast-produced negative RGB
# before the luma-ratio curve (otherwise dark colors reverse into white).
get_filename_component(SHADER_DIRECTORY "${SHADER_PATH}" DIRECTORY)
foreach(shader IN ITEMS NativeScenePreview.frag NativeComposite.frag)
  file(READ "${SHADER_DIRECTORY}/${shader}" CURVE_SHADER)
  if(NOT CURVE_SHADER MATCHES "rgb = clamp\\(rgb, 0\\.0, 1\\.0\\);[ \t\r\n]*float lum = dot\\(rgb")
    message(FATAL_ERROR "${shader}: bound curve input before dividing by luma")
  endif()
endforeach()
file(READ "${SHADER_DIRECTORY}/NativeHybridComposite.frag" LITE_CURVE_SHADER)
if(NOT LITE_CURVE_SHADER MATCHES "color = clamp\\(color, 0\\.0, 1\\.0\\);[ \t\r\n]*float luma = dot\\(color")
  message(FATAL_ERROR "NativeHybridComposite.frag: bound curve input before dividing by luma")
endif()

# A broadened roughness of 1.18 used to make the reflection lobe exponent negative;
# pow(0, negative) contaminated the output even when mixed with zero weight.
if(NOT SHADER_TEXT MATCHES "roughness = clamp\\(roughness, 0\\.0, 1\\.0\\)")
  message(FATAL_ERROR "Native reflection roughness must be bounded before evaluating its lobe")
endif()
if(NOT SHADER_TEXT MATCHES "tapReceiverDepth = receiverDepth \\+ dot\\(receiverDepthGradient, sampleUv - uv\\)"
    OR NOT SHADER_TEXT MATCHES "sampleDepth >= \\(tapReceiverDepth - bias\\)")
  message(FATAL_ERROR "Native PCF must compare the depth at each tap's receiver-plane position")
endif()
if(SHADER_TEXT MATCHES "float ComputeShadowFactor[^\r\n]*\\{[ \t\r\n]*return 1\\.0")
  message(FATAL_ERROR "Shadow sampling must not be disabled to hide receiver artifacts")
endif()

if(SHADER_TEXT MATCHES "vec3[ \t\r\n]+detailNormal[ \t\r\n]*=[ \t\r\n]*texture\\(detailTex,[ \t\r\n]*detailUv\\)")
  message(FATAL_ERROR "NativeScenePreview.frag must not decode detailTex as a tangent-space normal map")
endif()
