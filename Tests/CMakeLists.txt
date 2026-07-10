# Lightweight test workspace: native `RawIron.Core.Tests` / `RawIron.EngineImport.Tests` trees were removed.
# Use game `*.riscript` flows and engine-level checks instead.
#
# When `RAWIRON_BUILD_TESTS` is ON, individual apps register small CTest entries (e.g.
# `RawIron.UiMenu.HeadlessParse` in `Apps/RawIron.UiMenu/CMakeLists.txt`).

add_executable(CommandLineSmoke CommandLineSmoke.cpp)
target_link_libraries(CommandLineSmoke PRIVATE RawIron::Core)
add_test(NAME RawIron.Core.CommandLineSmoke COMMAND CommandLineSmoke)

add_executable(FixedStepAccumulatorSmoke FixedStepAccumulatorSmoke.cpp)
target_link_libraries(FixedStepAccumulatorSmoke PRIVATE RawIron::Core)
add_test(NAME RawIron.Core.FixedStepAccumulatorSmoke COMMAND FixedStepAccumulatorSmoke)

add_executable(RenderCommandStreamSmoke RenderCommandStreamSmoke.cpp)
target_link_libraries(RenderCommandStreamSmoke PRIVATE RawIron::Core)
add_test(NAME RawIron.Core.RenderCommandStreamSmoke COMMAND RenderCommandStreamSmoke)

add_executable(FrameArenaSmoke FrameArenaSmoke.cpp)
target_link_libraries(FrameArenaSmoke PRIVATE RawIron::Core)
add_test(NAME RawIron.Core.FrameArenaSmoke COMMAND FrameArenaSmoke)

add_executable(BundledGameExperiencesSmoke BundledGameExperiencesSmoke.cpp)
target_link_libraries(BundledGameExperiencesSmoke
  PRIVATE
    RawIron::Content
    RawIron::EditorPreview
    RawIron::SceneUtilities
)
add_test(
  NAME RawIron.Games.BundledExperiencesSmoke
  COMMAND BundledGameExperiencesSmoke ${CMAKE_SOURCE_DIR}
)
