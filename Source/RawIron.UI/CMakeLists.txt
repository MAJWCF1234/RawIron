add_library(RawIron.UI
  src/UiFlowSession.cpp
  src/UiJsonIO.cpp
  src/UiCommandLine.cpp
)

add_library(RawIron::UI ALIAS RawIron.UI)

target_include_directories(RawIron.UI
  PUBLIC
    include
)

target_link_libraries(RawIron.UI
  PUBLIC
    RawIron::Core
)

target_compile_features(RawIron.UI PUBLIC cxx_std_20)

if (RAWIRON_BUILD_TESTS)
  add_executable(UiSystemsSafetySmoke ${CMAKE_SOURCE_DIR}/Tests/UiSystemsSafetySmoke.cpp)
  target_link_libraries(UiSystemsSafetySmoke PRIVATE RawIron::UI)
  add_test(NAME RawIron.UI.SystemsSafetySmoke COMMAND UiSystemsSafetySmoke)
endif()
