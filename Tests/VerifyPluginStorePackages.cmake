# Validates store plugin package descriptors under Plugins/Store/.
cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED RAWIRON_WORKSPACE)
  message(FATAL_ERROR "VerifyPluginStorePackages: RAWIRON_WORKSPACE is required.")
endif()

set(_store_root "${RAWIRON_WORKSPACE}/Plugins/Store")
if(NOT IS_DIRECTORY "${_store_root}")
  message(FATAL_ERROR "Plugin store root not found: ${_store_root}")
endif()

set(_required_keys id name version author category description loadOrder hookGroup manifestLine hooks badge extension)
set(_required_extension_keys id displayName version kind scope host entry capabilities tags)
set(_package_count 0)

file(GLOB _package_dirs LIST_DIRECTORIES true "${_store_root}/*")
foreach(_dir IN LISTS _package_dirs)
  if(NOT IS_DIRECTORY "${_dir}")
    continue()
  endif()

  set(_json "${_dir}/package.riplugin.json")
  if(NOT EXISTS "${_json}")
    message(FATAL_ERROR "Missing package.riplugin.json in ${_dir}")
  endif()

  file(READ "${_json}" _text)
  foreach(_key IN LISTS _required_keys)
    if(NOT "${_text}" MATCHES "\"${_key}\"")
      message(FATAL_ERROR "${_json}: missing required key '${_key}'")
    endif()
  endforeach()

  string(REGEX MATCH "\"extension\"[ \t\r\n]*:[ \t\r\n]*\\{([^}]*)\\}" _extension_match "${_text}")
  if(NOT _extension_match)
    message(FATAL_ERROR "${_json}: missing extension object")
  endif()

  foreach(_key IN LISTS _required_extension_keys)
    if(NOT "${_extension_match}" MATCHES "\"${_key}\"")
      message(FATAL_ERROR "${_json}: extension missing required key '${_key}'")
    endif()
  endforeach()

  math(EXPR _package_count "${_package_count} + 1")
endforeach()

if(_package_count LESS 1)
  message(FATAL_ERROR "No plugin store packages found under ${_store_root}")
endif()

message(STATUS "VerifyPluginStorePackages: ${_package_count} package(s) OK")
