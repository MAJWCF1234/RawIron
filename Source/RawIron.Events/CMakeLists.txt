add_library(RawIron.Events
  src/EventEngine.cpp
)

add_library(RawIron::Events ALIAS RawIron.Events)

target_include_directories(RawIron.Events
  PUBLIC
    include
)

target_link_libraries(RawIron.Events
  PUBLIC
    RawIron::Runtime
)

target_compile_features(RawIron.Events PUBLIC cxx_std_20)
