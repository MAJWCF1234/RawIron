include_guard(GLOBAL)

# These are the stable foundation edges. Higher-level World, Content, rendering,
# editor, and game composition remains intentionally flexible while those APIs mature.
function(rawiron_architecture_dependency_allowed owner dependency result_out)
  # Accept both the public alias spelling and the concrete target spelling, then
  # compare one canonical name. This prevents a policy bypass via RawIron.Core.
  if(dependency MATCHES "^RawIron\\.")
    string(REGEX REPLACE "^RawIron\\." "RawIron::" dependency "${dependency}")
  endif()

  if(NOT dependency MATCHES "^RawIron::")
    set(${result_out} TRUE PARENT_SCOPE)
    return()
  endif()

  set(allowed)
  if(owner STREQUAL "RawIron.Core" OR owner STREQUAL "RawIron.Audio")
    set(allowed)
  elseif(owner STREQUAL "RawIron.Runtime"
      OR owner STREQUAL "RawIron.Logic"
      OR owner STREQUAL "RawIron.Spatial"
      OR owner STREQUAL "RawIron.Structural"
      OR owner STREQUAL "RawIron.UI")
    set(allowed RawIron::Core)
  elseif(owner STREQUAL "RawIron.Events")
    set(allowed RawIron::Runtime)
  elseif(owner STREQUAL "RawIron.Trace")
    set(allowed RawIron::Core RawIron::Runtime RawIron::Spatial)
  elseif(owner STREQUAL "RawIron.Validation")
    set(allowed RawIron::Events RawIron::Structural)
  else()
    # Unlisted composition modules are outside the frozen foundation policy.
    set(${result_out} TRUE PARENT_SCOPE)
    return()
  endif()

  list(FIND allowed "${dependency}" dependency_index)
  if(dependency_index EQUAL -1)
    set(${result_out} FALSE PARENT_SCOPE)
  else()
    set(${result_out} TRUE PARENT_SCOPE)
  endif()
endfunction()

function(rawiron_extract_engine_dependencies dependency_expression result_out)
  # LINK_LIBRARIES entries may be wrapped in LINK_ONLY or conditional generator
  # expressions. Extract target references instead of trusting the outer syntax.
  string(REGEX MATCHALL
    "RawIron(::|\\.)[A-Za-z0-9_.-]+"
    engine_dependencies
    "${dependency_expression}")

  set(normalized_dependencies)
  foreach(dependency IN LISTS engine_dependencies)
    string(REGEX REPLACE "^RawIron\\." "RawIron::" dependency "${dependency}")
    list(APPEND normalized_dependencies "${dependency}")
  endforeach()
  list(REMOVE_DUPLICATES normalized_dependencies)
  set(${result_out} "${normalized_dependencies}" PARENT_SCOPE)
endfunction()

function(rawiron_assert_foundation_target target_name)
  if(NOT TARGET "${target_name}")
    message(FATAL_ERROR "RawIron architecture policy references missing target ${target_name}")
  endif()

  get_target_property(direct_dependencies "${target_name}" LINK_LIBRARIES)
  if(NOT direct_dependencies)
    return()
  endif()

  foreach(dependency_expression IN LISTS direct_dependencies)
    rawiron_extract_engine_dependencies("${dependency_expression}" engine_dependencies)
    foreach(dependency IN LISTS engine_dependencies)
      rawiron_architecture_dependency_allowed("${target_name}" "${dependency}" allowed)
      if(NOT allowed)
        message(FATAL_ERROR
          "RawIron architecture violation: ${target_name} directly links ${dependency}. "
          "Move the dependency behind a lower-level interface or compose it in a higher layer.")
      endif()
    endforeach()
  endforeach()
endfunction()

function(rawiron_source_include_allowed owner include_path result_out)
  set(allowed_prefixes)
  if(owner STREQUAL "RawIron.Core")
    set(allowed_prefixes RawIron/Core/ RawIron/Math/ RawIron/Render/ RawIron/Scene/)
  elseif(owner STREQUAL "RawIron.Runtime")
    set(allowed_prefixes RawIron/Core/ RawIron/Math/ RawIron/Render/ RawIron/Runtime/ RawIron/Scene/)
  else()
    set(${result_out} TRUE PARENT_SCOPE)
    return()
  endif()

  set(allowed FALSE)
  foreach(prefix IN LISTS allowed_prefixes)
    string(FIND "${include_path}" "${prefix}" prefix_index)
    if(prefix_index EQUAL 0)
      set(allowed TRUE)
      break()
    endif()
  endforeach()
  set(${result_out} "${allowed}" PARENT_SCOPE)
endfunction()

function(rawiron_verify_foundation_source_boundaries workspace_root)
  foreach(layer IN ITEMS Core Runtime)
    set(module_root "${workspace_root}/Source/RawIron.${layer}")
    if(CMAKE_SCRIPT_MODE_FILE)
      file(GLOB_RECURSE source_files
        "${module_root}/*.cpp"
        "${module_root}/*.cxx"
        "${module_root}/*.h"
        "${module_root}/*.hpp")
    else()
      file(GLOB_RECURSE source_files CONFIGURE_DEPENDS
        "${module_root}/*.cpp"
        "${module_root}/*.cxx"
        "${module_root}/*.h"
        "${module_root}/*.hpp")
    endif()

    foreach(source_file IN LISTS source_files)
      file(STRINGS "${source_file}" include_lines
        REGEX [=[^[ \t]*#[ \t]*include[ \t]*[<"]RawIron/]=])
      foreach(include_line IN LISTS include_lines)
        string(REGEX MATCH [=[RawIron/[A-Za-z0-9_./-]+]=] include_path "${include_line}")
        if(include_path)
          rawiron_source_include_allowed("RawIron.${layer}" "${include_path}" allowed)
          if(NOT allowed)
            file(RELATIVE_PATH relative_source "${workspace_root}" "${source_file}")
            message(FATAL_ERROR
              "RawIron architecture violation: ${relative_source} reaches upward via ${include_path}")
          endif()
        endif()
      endforeach()
    endforeach()
  endforeach()
endfunction()

function(rawiron_enforce_foundation_architecture workspace_root)
  foreach(target_name IN ITEMS
      RawIron.Core
      RawIron.Audio
      RawIron.Runtime
      RawIron.Logic
      RawIron.Events
      RawIron.Spatial
      RawIron.Structural
      RawIron.Trace
      RawIron.Validation
      RawIron.UI)
    rawiron_assert_foundation_target("${target_name}")
  endforeach()
  rawiron_verify_foundation_source_boundaries("${workspace_root}")
endfunction()
