if (NOT DEFINED RAWIRON_WORKSPACE)
  message(FATAL_ERROR "RAWIRON_WORKSPACE must point at the RawIron workspace root.")
endif()

set(factory_cpp "${RAWIRON_WORKSPACE}/Source/RawIron.Logic/src/LogicKitNodeFactory.cpp")
file(READ "${factory_cpp}" factory_text)

foreach(required_kit mem_flipflop mem_register mem_variable sense_prox sense_zone sense_tag sense_scalar sense_ray flow_relay gate_and)
  if (NOT factory_text MATCHES "canonicalId == \"${required_kit}\"")
    message(FATAL_ERROR "LogicKit factory missing executable mapping for ${required_kit}.")
  endif()
endforeach()

message(STATUS "LogicKit factory verification passed.")
