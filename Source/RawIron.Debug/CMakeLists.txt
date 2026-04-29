add_library(RawIron.Debug
  src/RuntimeSnapshots.cpp
)

add_library(RawIron::Debug ALIAS RawIron.Debug)

target_include_directories(RawIron.Debug
  PUBLIC
    include
)

target_link_libraries(RawIron.Debug
  PUBLIC
    RawIron::Audio
    RawIron::Core
    RawIron::Events
    RawIron::Runtime
    RawIron::Spatial
    RawIron::Structural
    RawIron::Trace
    RawIron::Validation
)

target_compile_features(RawIron.Debug PUBLIC cxx_std_20)
