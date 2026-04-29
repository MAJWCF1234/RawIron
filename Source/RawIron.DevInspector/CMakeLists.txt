add_library(RawIron.DevInspector
  src/DevelopmentInspector.cpp
)

add_library(RawIron::DevInspector ALIAS RawIron.DevInspector)

target_include_directories(RawIron.DevInspector
  PUBLIC
    include
)

target_link_libraries(RawIron.DevInspector
  PUBLIC
)

target_compile_features(RawIron.DevInspector PUBLIC cxx_std_20)

target_compile_definitions(RawIron.DevInspector PUBLIC RAWIRON_WITH_DEV_INSPECTOR=1)
