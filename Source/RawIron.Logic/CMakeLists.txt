add_library(RawIron.Logic
  src/LogicAuthoring.cpp
  src/LogicAuthoringEditorIO.cpp
  src/LogicAuthoringWireLayout.cpp
  src/LogicKitPortBridge.cpp
  src/LogicAuthoringSenseRuntime.cpp
  src/LogicAuthoringDetail.cpp
  src/LogicGraph.cpp
  src/LogicKitManifest.cpp
  src/LogicKitNodeFactory.cpp
  src/LogicPortSchema.cpp
  src/LogicVisualPrimitives.cpp
)

add_library(RawIron::Logic ALIAS RawIron.Logic)

target_include_directories(RawIron.Logic
  PUBLIC
    include
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(RawIron.Logic PUBLIC cxx_std_20)

# MinGW: very large TUs (e.g. `LogicAuthoring.cpp`) exceed the default COFF section limit.
if (MINGW)
  target_compile_options(RawIron.Logic PRIVATE "-Wa,-mbig-obj")
endif()

target_link_libraries(RawIron.Logic
  PRIVATE
    RawIron::Core
)

if (RAWIRON_BUILD_TESTS)
  add_executable(LogicSenseSmoke tests/LogicSenseSmoke.cpp)
  target_link_libraries(LogicSenseSmoke PRIVATE RawIron::Logic)
  add_test(NAME RawIron.Logic.SenseSmoke COMMAND LogicSenseSmoke)

  add_executable(LogicDispatchBudgetSmoke ${CMAKE_SOURCE_DIR}/Tests/LogicDispatchBudgetSmoke.cpp)
  target_link_libraries(LogicDispatchBudgetSmoke PRIVATE RawIron::Logic)
  add_test(NAME RawIron.Logic.DispatchBudgetSmoke COMMAND LogicDispatchBudgetSmoke)

  add_test(
    NAME RawIron.LogicKit.FactoryAudit
    COMMAND ${CMAKE_COMMAND}
      -DRAWIRON_WORKSPACE=${CMAKE_SOURCE_DIR}
      -P ${CMAKE_SOURCE_DIR}/Tests/VerifyLogicKitFactory.cmake
  )
endif()
