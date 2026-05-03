include(FetchContent)

set(RAWIRON_IMGUI_TAG "v1.91.9" CACHE STRING "Dear ImGui git tag for RawIron.UiMenu")

FetchContent_Declare(
  dear_imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG ${RAWIRON_IMGUI_TAG}
)

FetchContent_GetProperties(dear_imgui)
if(NOT dear_imgui_POPULATED)
  FetchContent_Populate(dear_imgui)
endif()

set(IMGUI_DIR "${dear_imgui_SOURCE_DIR}")

add_library(rawiron_imgui STATIC
  ${IMGUI_DIR}/imgui.cpp
  ${IMGUI_DIR}/imgui_draw.cpp
  ${IMGUI_DIR}/imgui_tables.cpp
  ${IMGUI_DIR}/imgui_widgets.cpp
)

target_include_directories(rawiron_imgui PUBLIC "${IMGUI_DIR}")
target_compile_features(rawiron_imgui PUBLIC cxx_std_11)

if(MSVC)
  target_compile_definitions(rawiron_imgui PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

add_library(rawiron_imgui_win32_dx11 STATIC
  ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
  ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
)

target_include_directories(rawiron_imgui_win32_dx11
  PUBLIC
    "${IMGUI_DIR}"
    "${IMGUI_DIR}/backends"
)

target_link_libraries(rawiron_imgui_win32_dx11
  PUBLIC
    rawiron_imgui
)

target_link_libraries(rawiron_imgui_win32_dx11 PRIVATE d3d11 dxgi)

target_compile_features(rawiron_imgui_win32_dx11 PUBLIC cxx_std_11)

add_executable(RawIron.UiMenu
  src/main.cpp
)

target_link_libraries(RawIron.UiMenu
  PRIVATE
    RawIron::Core
    RawIron::UI
    RawIron::RenderSoftware
    rawiron_imgui_win32_dx11
    user32
    gdi32
    imm32
)

target_compile_features(RawIron.UiMenu PRIVATE cxx_std_20)

if(MSVC)
  target_compile_definitions(RawIron.UiMenu PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

if(WIN32 AND RAWIRON_BUILD_TESTS)
  set(rawiron_verify_command_output "${PROJECT_SOURCE_DIR}/Tests/cmake/VerifyCommandOutput.cmake")
  set(rawiron_uimenu_expected
    "[UiMenu] manifest loaded"
    "screens="
  )
  set(rawiron_uimenu_args "--workspace|${CMAKE_SOURCE_DIR}|--headless")
  add_test(
    NAME RawIron.UiMenu.HeadlessParse
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:RawIron.UiMenu>"
      "-DCOMMAND_ARGS_RAW=${rawiron_uimenu_args}"
      "-DEXPECTED_STRINGS=${rawiron_uimenu_expected}"
      "-DFORBIDDEN_STRINGS=failed;FAILED;exception"
      -P "${rawiron_verify_command_output}"
  )
endif()
