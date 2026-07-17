foreach(required IN ITEMS RUNTIME_PATH OPTIONS_PATH SHADER_PATH LITE_SHADER_PATH)
  if(NOT DEFINED ${required})
    message(FATAL_ERROR "${required} is required")
  endif()
endforeach()

file(READ "${RUNTIME_PATH}" RUNTIME_TEXT)
file(READ "${OPTIONS_PATH}" OPTIONS_TEXT)
file(READ "${SHADER_PATH}" SHADER_TEXT)
file(READ "${LITE_SHADER_PATH}" LITE_SHADER_TEXT)

if(NOT OPTIONS_TEXT MATCHES "bool[ \t]+enableExtendedPostProcessShader[ \t]*=[ \t]*false")
  message(FATAL_ERROR "The expensive monolithic post-process port must remain an explicit opt-in")
endif()

if(NOT RUNTIME_TEXT MATCHES "enableExtendedPostProcessShader[\r\n \t]*\\?[\r\n \t]*\"NativeComposite\\.frag\\.spv\"")
  message(FATAL_ERROR "Hybrid HDR must select NativeComposite.frag.spv when extended shader ports are enabled")
endif()

foreach(required_port IN ITEMS
    ApplySweetFxFxaa
    ApplySweetFxSmaa
    ApplyReShadeDaltonize
    ApplyReShadeLut
    ApplyPd80HqBloom
    ApplyCreatorFilmGrain2)
  if(NOT SHADER_TEXT MATCHES "${required_port}\\(")
    message(FATAL_ERROR "NativeComposite.frag is missing required port ${required_port}")
  endif()
endforeach()

foreach(required_lite_port IN ITEMS
    ApplyLiteLumaCurve
    ApplyLiteLiftGammaGain
    ApplyLiteVibrance
    ApplyLitePresentationFx)
  if(NOT LITE_SHADER_TEXT MATCHES "${required_lite_port}\\(")
    message(FATAL_ERROR "NativeHybridComposite.frag is missing common fast-path port ${required_lite_port}")
  endif()
endforeach()
