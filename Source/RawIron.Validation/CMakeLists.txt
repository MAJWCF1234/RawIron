add_library(RawIron.Validation
  src/ColorParse.cpp
  src/DataSchema.cpp
  src/DistinctStrings.cpp
  src/IdFormat.cpp
  src/Schemas.cpp
  src/StringPattern.cpp
)

add_library(RawIron::Validation ALIAS RawIron.Validation)

target_include_directories(RawIron.Validation
  PUBLIC
    include
)

target_link_libraries(RawIron.Validation
  PUBLIC
    RawIron::Events
    RawIron::Structural
)

target_compile_features(RawIron.Validation PUBLIC cxx_std_20)
