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
