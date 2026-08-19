# Lightweight test workspace: native `RawIron.Core.Tests` / `RawIron.EngineImport.Tests` trees were removed.
# Use game `*.riscript` flows and engine-level checks instead.
#
# When `RAWIRON_BUILD_TESTS` is ON, individual apps register small CTest entries (e.g.
# `RawIron.UiMenu.HeadlessParse` in `Apps/RawIron.UiMenu/CMakeLists.txt`).

add_executable(CommandLineSmoke CommandLineSmoke.cpp)
target_link_libraries(CommandLineSmoke PRIVATE RawIron::Core)
add_test(NAME RawIron.Core.CommandLineSmoke COMMAND CommandLineSmoke)

add_executable(CoreUtilitiesSafetySmoke CoreUtilitiesSafetySmoke.cpp)
target_link_libraries(CoreUtilitiesSafetySmoke PRIVATE RawIron::Core)
add_test(NAME RawIron.Core.UtilitiesSafetySmoke COMMAND CoreUtilitiesSafetySmoke)

add_executable(FixedStepAccumulatorSmoke FixedStepAccumulatorSmoke.cpp)
target_link_libraries(FixedStepAccumulatorSmoke PRIVATE RawIron::Core)
add_test(NAME RawIron.Core.FixedStepAccumulatorSmoke COMMAND FixedStepAccumulatorSmoke)

add_executable(RenderCommandStreamSmoke RenderCommandStreamSmoke.cpp)
target_link_libraries(RenderCommandStreamSmoke PRIVATE RawIron::Core)
add_test(NAME RawIron.Core.RenderCommandStreamSmoke COMMAND RenderCommandStreamSmoke)

add_executable(FrameArenaSmoke FrameArenaSmoke.cpp)
target_link_libraries(FrameArenaSmoke PRIVATE RawIron::Core)
add_test(NAME RawIron.Core.FrameArenaSmoke COMMAND FrameArenaSmoke)

add_executable(SceneSnapshotStorageSmoke SceneSnapshotStorageSmoke.cpp)
target_link_libraries(SceneSnapshotStorageSmoke PRIVATE RawIron::Core)
add_test(NAME RawIron.Core.SceneSnapshotStorageSmoke COMMAND SceneSnapshotStorageSmoke)

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

# Requires committed fixtures (or a local PsxPack Abandoned_House). Gate the
# CTest entry so a clean clone without those inputs does not fail the suite.
set(_rawiron_fbx_remap_fixture
  "${CMAKE_SOURCE_DIR}/Tests/fixtures/FbxTextureRemap/Abandoned_House/Models/Abandoned_House.fbx")
set(_rawiron_fbx_remap_psx
  "${CMAKE_SOURCE_DIR}/Games/WildernessRuins/assets/PsxPack/World/Abandoned_House/Models/Abandoned_House.fbx")
if(EXISTS "${CMAKE_SOURCE_DIR}/Tests/FbxTextureRemapSmoke.cpp"
   AND (EXISTS "${_rawiron_fbx_remap_fixture}" OR EXISTS "${_rawiron_fbx_remap_psx}"))
  add_executable(FbxTextureRemapSmoke FbxTextureRemapSmoke.cpp)
  target_link_libraries(FbxTextureRemapSmoke PRIVATE RawIron::SceneUtilities)
  add_test(
    NAME RawIron.Scene.FbxTextureRemapSmoke
    COMMAND FbxTextureRemapSmoke ${CMAKE_SOURCE_DIR}
  )
endif()
unset(_rawiron_fbx_remap_fixture)
unset(_rawiron_fbx_remap_psx)

add_executable(SessionExtensionSmoke SessionExtensionSmoke.cpp)
target_link_libraries(SessionExtensionSmoke PRIVATE RawIron::Core)
add_test(NAME RawIron.Core.SessionExtensionSmoke COMMAND SessionExtensionSmoke)
