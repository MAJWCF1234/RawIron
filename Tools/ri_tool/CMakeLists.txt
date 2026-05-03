add_executable(ri_tool
  src/main.cpp
)

target_link_libraries(ri_tool
  PRIVATE
    RawIron::Content
    RawIron::Core
    RawIron::RenderVulkan
    RawIron::RenderSoftware
    RawIron::SceneUtilities
)

if (UNIX AND NOT APPLE AND CMAKE_DL_LIBS)
  target_link_libraries(ri_tool
    PRIVATE
      ${CMAKE_DL_LIBS}
  )
endif()

target_compile_features(ri_tool PRIVATE cxx_std_20)

if (WIN32 AND EXISTS "${RAWIRON_VULKAN_RUNTIME_DLL}")
  add_custom_command(TARGET ri_tool POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${RAWIRON_VULKAN_RUNTIME_DLL}"
      "$<TARGET_FILE_DIR:ri_tool>/vulkan-1.dll"
  )
endif()

rawiron_stage_runtime_dlls(ri_tool)

rawiron_bundle_engine_textures(ri_tool)

if (RAWIRON_BUILD_TESTS)
  set(rawiron_verify_command_output "${PROJECT_SOURCE_DIR}/Tests/cmake/VerifyCommandOutput.cmake")
  set(rawiron_tool_sample_expected
    "Scene: ToolPreview"
    "Nodes=13 Meshes=6 Materials=6 Cameras=1 Lights=2"
    "Renderable nodes: 6"
    "Orbit camera path: World/OrbitRig/OrbitSwivel/MainCamera"
  )
  set(rawiron_tool_format_expected
    "RawIron standard asset format:"
    ".ri_asset.json"
    "Legacy/experimental aliases:"
    ".ri_model"
    ".ri_meshc"
  )
  file(TO_NATIVE_PATH "${PROJECT_SOURCE_DIR}" RAWIRON_TOOL_WORKSPACE_ROOT_NATIVE)
  file(TO_NATIVE_PATH "${PROJECT_SOURCE_DIR}/Assets/Source" RAWIRON_TOOL_WORKSPACE_ASSETS_NATIVE)
  file(TO_NATIVE_PATH "${PROJECT_SOURCE_DIR}/Projects/Sandbox/Scenes" RAWIRON_TOOL_WORKSPACE_SCENES_NATIVE)
  set(rawiron_tool_workspace_expected
    "Workspace root: ${RAWIRON_TOOL_WORKSPACE_ROOT_NATIVE}"
    "Assets/Source: ${RAWIRON_TOOL_WORKSPACE_ASSETS_NATIVE}"
    "Projects/Sandbox/Scenes: ${RAWIRON_TOOL_WORKSPACE_SCENES_NATIVE}"
  )
  set(rawiron_tool_reference_expected
    "Scene Kit parity gate: 11 tracked references"
    "scene_controls_orbit - Orbit controls"
    "scene_loader_gltf - GLTF loader"
    "scene_audio_orientation - Positional audio orientation"
    "scene_particles - Particle fountain"
  )
  set(rawiron_scenekit_checks_root "${CMAKE_CURRENT_BINARY_DIR}/scenekit-checks")
  set(rawiron_scenekit_checks_expected
    "Scene Kit milestone checks:"
    "[PASS] Orbit controls"
    "[PASS] Geometry cube"
    "[PASS] Interactive cubes"
    "[PASS] Terrain raycasting"
    "[PASS] Spot lights"
    "[PASS] glTF loader"
    "[PASS] Animation keyframes"
    "[PASS] Instancing performance"
    "[PASS] Environment maps"
    "[PASS] Positional audio orientation"
    "[PASS] Particle fountain"
    "Scene Kit checks summary: 11/11 passed"
  )
  set(rawiron_scenekit_required_files
    "scene_controls_orbit.bmp"
    "scene_geometry_cube.bmp"
    "scene_interactive_cubes.bmp"
    "scene_terrain_raycast.bmp"
    "scene_lighting_spotlights.bmp"
    "scene_loader_gltf.bmp"
    "scene_animation_keyframes.bmp"
    "scene_instancing_performance.bmp"
    "scene_materials_envmaps.bmp"
    "scene_audio_orientation.bmp"
    "scene_particles.bmp"
  )
  if (WIN32)
    set(rawiron_tool_vulkan_expected
      "Vulkan platform: Windows"
      "Vulkan SDK root:"
      "Vulkan runtime library:"
      "Vulkan instance API version:"
    )
    set(rawiron_tool_vulkan_forbidden
      "unavailable"
      "could not be loaded"
      "failed"
      "exception"
    )
  elseif(UNIX AND NOT APPLE)
    set(rawiron_tool_vulkan_expected
      "Vulkan platform: Linux"
      "Vulkan SDK root:"
      "Vulkan runtime library:"
    )
    set(rawiron_tool_vulkan_forbidden
      "failed"
      "exception"
    )
  else()
    set(rawiron_tool_vulkan_expected
      "Vulkan diagnostics unavailable."
    )
    set(rawiron_tool_vulkan_forbidden
      "failed"
      "exception"
    )
  endif()
  set(rawiron_workspace_smoke_root "${CMAKE_CURRENT_BINARY_DIR}/workspace-smoke")
  set(rawiron_workspace_create_expected
    "Workspace ensured."
    "Workspace root:"
    "Assets/Cooked:"
    "Projects/Sandbox/Saved:"
  )
  set(rawiron_render_cube_root "${CMAKE_CURRENT_BINARY_DIR}/render-smoke")
  set(rawiron_render_cube_expected
    "Rendered shaded cube preview."
    "Output: ${rawiron_render_cube_root}/cube.bmp"
    "Size: 256x256"
  )
  set(rawiron_scene_state_root "${CMAKE_CURRENT_BINARY_DIR}/scene-state-smoke")
  set(rawiron_scene_state_path "${rawiron_scene_state_root}/tool_scene_state.ri_state")
  set(rawiron_scene_state_save_expected
    "Saved scene state."
    "Output: ${rawiron_scene_state_path}"
    "Nodes: 13"
  )
  set(rawiron_scene_state_load_expected
    "Loaded scene state."
    "Input: ${rawiron_scene_state_path}"
    "Scene: ToolStateScene"
    "Nodes: 13"
  )
  set(rawiron_workspace_required
    "Documentation"
    "Source"
    "Apps"
    "Tools"
    "Config"
    "Assets/Source"
    "Assets/Cooked"
    "Projects"
    "Projects/Sandbox"
    "Projects/Sandbox/Content"
    "Projects/Sandbox/Scenes"
    "Projects/Sandbox/Saved"
    "Saved"
    "Scripts"
    "ThirdParty"
  )

  add_test(
    NAME RawIron.Tool.SampleScene
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:ri_tool>"
      "-DCOMMAND_ARGS=--sample-scene"
      "-DEXPECTED_STRINGS=${rawiron_tool_sample_expected}"
      "-DFORBIDDEN_STRINGS=failed;exception"
      -P "${rawiron_verify_command_output}"
  )

  add_test(
    NAME RawIron.Tool.Formats
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:ri_tool>"
      "-DCOMMAND_ARGS=--formats"
      "-DEXPECTED_STRINGS=${rawiron_tool_format_expected}"
      "-DFORBIDDEN_STRINGS=failed;exception"
      -P "${rawiron_verify_command_output}"
  )

  add_test(
    NAME RawIron.Tool.Workspace
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:ri_tool>"
      "-DCOMMAND_ARGS=--workspace;--root;${PROJECT_SOURCE_DIR}"
      "-DEXPECTED_STRINGS=${rawiron_tool_workspace_expected}"
      "-DFORBIDDEN_STRINGS=failed;exception"
      -P "${rawiron_verify_command_output}"
  )

  add_test(
    NAME RawIron.Tool.SceneKitTargets
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:ri_tool>"
      "-DCOMMAND_ARGS=--scenekit-targets"
      "-DEXPECTED_STRINGS=${rawiron_tool_reference_expected}"
      "-DFORBIDDEN_STRINGS=failed;exception"
      -P "${rawiron_verify_command_output}"
  )

  add_test(
    NAME RawIron.Tool.SceneKitChecks
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:ri_tool>"
      "-DCOMMAND_ARGS=--scenekit-checks;--root;${PROJECT_SOURCE_DIR};--output-dir;${rawiron_scenekit_checks_root};--width;320;--height;256"
      "-DEXPECTED_STRINGS=${rawiron_scenekit_checks_expected}"
      "-DFORBIDDEN_STRINGS=failed;exception;[FAIL]"
      "-DVERIFY_FILES_ROOT=${rawiron_scenekit_checks_root}"
      "-DREQUIRED_RELATIVE_FILES=${rawiron_scenekit_required_files}"
      -P "${rawiron_verify_command_output}"
  )

  add_test(
    NAME RawIron.Tool.VulkanDiagnostics
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:ri_tool>"
      "-DCOMMAND_ARGS=--vulkan-diagnostics"
      "-DEXPECTED_STRINGS=${rawiron_tool_vulkan_expected}"
      "-DFORBIDDEN_STRINGS=${rawiron_tool_vulkan_forbidden}"
      -P "${rawiron_verify_command_output}"
  )

  add_test(
    NAME RawIron.Tool.EnsureWorkspace
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:ri_tool>"
      "-DCOMMAND_ARGS=--ensure-workspace;--root;${rawiron_workspace_smoke_root}"
      "-DEXPECTED_STRINGS=${rawiron_workspace_create_expected}"
      "-DFORBIDDEN_STRINGS=failed;exception"
      "-DVERIFY_DIRECTORIES_ROOT=${rawiron_workspace_smoke_root}"
      "-DREQUIRED_RELATIVE_DIRECTORIES=${rawiron_workspace_required}"
      -P "${rawiron_verify_command_output}"
  )

  add_test(
    NAME RawIron.Tool.RenderCube
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:ri_tool>"
      "-DCOMMAND_ARGS=--render-cube;--output;${rawiron_render_cube_root}/cube.bmp;--width;256;--height;256"
      "-DEXPECTED_STRINGS=${rawiron_render_cube_expected}"
      "-DFORBIDDEN_STRINGS=failed;exception"
      "-DVERIFY_FILES_ROOT=${rawiron_render_cube_root}"
      "-DREQUIRED_RELATIVE_FILES=cube.bmp"
      -P "${rawiron_verify_command_output}"
  )

  add_test(
    NAME RawIron.Tool.SaveSceneState
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:ri_tool>"
      "-DCOMMAND_ARGS=--save-scene-state;--state-file;${rawiron_scene_state_path}"
      "-DEXPECTED_STRINGS=${rawiron_scene_state_save_expected}"
      "-DFORBIDDEN_STRINGS=failed;exception"
      "-DVERIFY_FILES_ROOT=${rawiron_scene_state_root}"
      "-DREQUIRED_RELATIVE_FILES=tool_scene_state.ri_state"
      -P "${rawiron_verify_command_output}"
  )

  add_test(
    NAME RawIron.Tool.LoadSceneState
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:ri_tool>"
      "-DCOMMAND_ARGS=--load-scene-state;--state-file;${rawiron_scene_state_path}"
      "-DEXPECTED_STRINGS=${rawiron_scene_state_load_expected}"
      "-DFORBIDDEN_STRINGS=failed;exception"
      -P "${rawiron_verify_command_output}"
  )
endif()
