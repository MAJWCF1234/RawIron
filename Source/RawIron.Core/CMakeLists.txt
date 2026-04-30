add_library(RawIron.Core
  src/ActionBindings.cpp
  src/CrashDiagnostics.cpp
  src/ContentPresentation.cpp
  src/CommandLine.cpp
  src/FixedStepAccumulator.cpp
  src/GameSimulationClock.cpp
  src/FrameArena.cpp
  src/InputLabelFormat.cpp
  src/InputPolling.cpp
  src/JsonScan.cpp
  src/Log.cpp
  src/MainLoop.cpp
  src/RenderCommandStream.cpp
  src/RenderRecorder.cpp
  src/RenderSubmissionPlan.cpp
  src/Scene.cpp
)

add_library(RawIron::Core ALIAS RawIron.Core)

target_include_directories(RawIron.Core
  PUBLIC
    include
    "${PROJECT_SOURCE_DIR}/ThirdParty/backward-cpp/include"
)

target_compile_features(RawIron.Core PUBLIC cxx_std_20)
target_compile_definitions(RawIron.Core
  PUBLIC
    RAWIRON_ENABLE_STACKTRACE_DIAGNOSTICS=$<BOOL:${RAWIRON_ENABLE_STACKTRACE_DIAGNOSTICS}>
)

if (RAWIRON_ENABLE_STACKTRACE_DIAGNOSTICS)
  target_compile_definitions(RawIron.Core
    PRIVATE
      BACKWARD_HAS_BFD=0
      BACKWARD_HAS_DW=0
      BACKWARD_HAS_BACKTRACE=0
  )
  if (WIN32)
    target_compile_definitions(RawIron.Core PRIVATE BACKWARD_HAS_DBGHELP=1)
    target_link_libraries(RawIron.Core PRIVATE dbghelp)
  endif()
endif()
