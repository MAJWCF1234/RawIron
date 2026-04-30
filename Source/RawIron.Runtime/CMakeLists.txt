add_library(RawIron.Runtime
  src/EntityIoTelemetry.cpp
  src/ExperiencePresets.cpp
  src/LevelScopedSchedulers.cpp
  src/RuntimeEventBus.cpp
  src/RuntimeId.cpp
  src/RuntimeTuning.cpp
)

add_library(RawIron::Runtime ALIAS RawIron.Runtime)

target_include_directories(RawIron.Runtime
  PUBLIC
    include
)

target_link_libraries(RawIron.Runtime
  PUBLIC
    RawIron::Core
)

target_compile_features(RawIron.Runtime PUBLIC cxx_std_20)
