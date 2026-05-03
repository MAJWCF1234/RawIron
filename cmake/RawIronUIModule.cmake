# RawIron.UI is added from `Source/RawIron.Core/CMakeLists.txt`. Include this file **after** Core so
# `RawIron::UI` exists, then optionally add the Windows JSON + ImGui host:
#   include("${CMAKE_SOURCE_DIR}/cmake/RawIronUIModule.cmake")

if(DEFINED RAWIRON_UI_MODULE_INCLUDED)
  return()
endif()
set(RAWIRON_UI_MODULE_INCLUDED ON)

option(RAWIRON_BUILD_UI_MENU "Build RawIron.UiMenu (Windows JSON + ImGui menu host)" ON)

if(WIN32 AND RAWIRON_BUILD_UI_MENU)
  add_subdirectory("${CMAKE_SOURCE_DIR}/Apps/RawIron.UiMenu" "${CMAKE_BINARY_DIR}/Apps/RawIron.UiMenu")
endif()
