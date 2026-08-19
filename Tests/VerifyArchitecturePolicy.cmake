if(NOT DEFINED RAWIRON_WORKSPACE)
  message(FATAL_ERROR "RAWIRON_WORKSPACE is required")
endif()

include("${RAWIRON_WORKSPACE}/cmake/RawIronArchitecture.cmake")

rawiron_architecture_dependency_allowed("RawIron.Runtime" "RawIron::Core" allowed)
if(NOT allowed)
  message(FATAL_ERROR "Runtime must be allowed to depend on Core")
endif()

rawiron_architecture_dependency_allowed("RawIron.Runtime" "RawIron.Core" allowed)
if(NOT allowed)
  message(FATAL_ERROR "Concrete engine target names must obey the alias policy")
endif()

rawiron_architecture_dependency_allowed("RawIron.Core" "RawIron::Runtime" allowed)
if(allowed)
  message(FATAL_ERROR "Core must not be allowed to depend on Runtime")
endif()

rawiron_architecture_dependency_allowed("RawIron.Validation" "RawIron::World" allowed)
if(allowed)
  message(FATAL_ERROR "Validation must not be allowed to depend on World")
endif()

rawiron_extract_engine_dependencies(
  "$<$<CONFIG:Debug>:RawIron::World>;$<LINK_ONLY:RawIron.Core>"
  extracted_dependencies)
list(FIND extracted_dependencies "RawIron::World" world_dependency_index)
list(FIND extracted_dependencies "RawIron::Core" core_dependency_index)
if(world_dependency_index EQUAL -1 OR core_dependency_index EQUAL -1)
  message(FATAL_ERROR "Engine dependencies must be found inside generator expressions")
endif()

rawiron_source_include_allowed("RawIron.Runtime" "RawIron/Scene/Scene.h" allowed)
if(NOT allowed)
  message(FATAL_ERROR "Runtime must be allowed to use Core-owned scene primitives")
endif()

rawiron_source_include_allowed("RawIron.Runtime" "RawIron/Content/GameManifest.h" allowed)
if(allowed)
  message(FATAL_ERROR "Runtime must not include Content directly")
endif()

rawiron_verify_foundation_source_boundaries("${RAWIRON_WORKSPACE}")
