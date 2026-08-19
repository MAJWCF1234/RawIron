add_executable(ri_tool
  ../../Apps/RawIron.Editor/src/EditorProjectScaffolding.cpp
  ../../Apps/RawIron.Editor/src/EditorWorkspace.cpp
  src/SecureRipakArchive.cpp
  src/main.cpp
)

target_include_directories(ri_tool
  PRIVATE
    ${PROJECT_SOURCE_DIR}/Apps/RawIron.Editor/src
    ${PROJECT_SOURCE_DIR}/Source/RawIron.Render.Software/third_party
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

rawiron_stage_runtime_dlls(ri_tool)

rawiron_bundle_engine_textures(ri_tool)

if (RAWIRON_BUILD_TESTS)
  set(rawiron_verify_command_output "${PROJECT_SOURCE_DIR}/Tests/cmake/VerifyCommandOutput.cmake")
  rawiron_runtime_dll_test_arg(rawiron_tool_runtime_dll_arg ri_tool)
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
  set(rawiron_tool_rig_report_expected
    "RawIron modeling and rigging toolchain:"
    "Authoring source: .ri_rig.json"
    "Baseline: --rig-create-humanoid"
    "Mesh inputs: OBJ, glTF/GLB, and FBX"
    "Editor workflow: save rigs under Assets/Source/rigs"
  )
  set(rawiron_tool_help_expected
    "Usage: ri_tool <command> [options]"
    "Plugins, mods, and extensions:"
    "--plugins-doctor"
    "--extension-validate"
  )
  set(rawiron_tool_plugin_list_expected
    "Plugin project: liminal-hall"
    "Plugin summary: 3 active / 3 manifest"
    "[ACTIVE] liminal.telemetry"
    "[ACTIVE] liminal.ai.bridge"
    "[ACTIVE] liminal.cinematics"
    "Plugin validation: pass"
  )
  set(rawiron_tool_plugin_doctor_expected
    "Plugin doctor project: liminal-hall"
    "Plugin summary: 3 active / 3 manifest"
    "Plugin doctor result: healthy"
  )
  set(rawiron_tool_plugin_handlers_expected
    "Registered Raw Iron plugin hook handlers:"
    "bootstrap"
    "frame_sample"
    "zone_cutscene_trigger"
    "Handler count: 9"
  )
  set(rawiron_tool_extension_validate_expected
    "Extension id: rawiron.quest-beacons"
    "Extension kind: plugin"
    "Extension host: runtime"
    "Extension validation: pass"
  )
  file(TO_NATIVE_PATH "${PROJECT_SOURCE_DIR}" RAWIRON_TOOL_WORKSPACE_ROOT_NATIVE)
  file(TO_NATIVE_PATH "${PROJECT_SOURCE_DIR}/Assets/Source" RAWIRON_TOOL_WORKSPACE_ASSETS_NATIVE)
  file(TO_NATIVE_PATH "${PROJECT_SOURCE_DIR}/Projects/Sandbox/Scenes" RAWIRON_TOOL_WORKSPACE_SCENES_NATIVE)
  set(rawiron_tool_workspace_expected
    "Workspace root: ${RAWIRON_TOOL_WORKSPACE_ROOT_NATIVE}"
    "Assets/Source: ${RAWIRON_TOOL_WORKSPACE_ASSETS_NATIVE}"
    "Projects/Sandbox/Scenes: ${RAWIRON_TOOL_WORKSPACE_SCENES_NATIVE}"
  )
  file(TO_NATIVE_PATH "${PROJECT_SOURCE_DIR}/Games/RawIronMultiplayerSandbox" RAWIRON_TOOL_MPS_ROOT_NATIVE)
  set(rawiron_tool_projects_expected
    "Workspace root: ${RAWIRON_TOOL_WORKSPACE_ROOT_NATIVE}"
    "project id=cube-test"
    "project id=editor-ui-smoke"
    "project id=liminal-hall"
    "project id=rawiron-multiplayer-sandbox"
    "project id=wilderness-ruins"
    "Project count: 5"
  )
  set(rawiron_tool_describe_project_expected
    "Project id: rawiron-multiplayer-sandbox"
    "Project root: ${RAWIRON_TOOL_MPS_ROOT_NATIVE}"
    "Runtime module: RawIron.Game.MultiplayerSandbox"
    "Editor launch token: --game=rawiron-multiplayer-sandbox"
    "Format validation: pass"
  )
  set(rawiron_tool_doctor_project_expected
    "Doctor project id: rawiron-multiplayer-sandbox"
    "Runtime module: RawIron.Game.MultiplayerSandbox"
    "Doctor result: healthy"
  )
  set(rawiron_tool_list_resources_expected
    "Project id: liminal-hall"
    "Filter category: Level"
    "[Levels] levels/assembly.primitives.csv"
    "[Levels] levels/assembly.structural.csv"
    "Resource rows:"
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
      "Vulkan runtime status:"
    )
    set(rawiron_tool_vulkan_forbidden
      "[ri_tool Failure]"
      "Unhandled exception"
    )
  elseif(UNIX AND NOT APPLE)
    set(rawiron_tool_vulkan_expected
      "Vulkan platform: Linux"
      "Vulkan SDK root:"
      "Vulkan runtime status:"
    )
    set(rawiron_tool_vulkan_forbidden
      "[ri_tool Failure]"
      "Unhandled exception"
    )
  else()
    set(rawiron_tool_vulkan_expected
      "Vulkan platform:"
      "Vulkan runtime status:"
    )
    set(rawiron_tool_vulkan_forbidden
      "[ri_tool Failure]"
      "Unhandled exception"
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
  set(rawiron_project_create_root "${CMAKE_CURRENT_BINARY_DIR}/project-create-smoke/ToolCliSample")
  file(TO_NATIVE_PATH "${rawiron_project_create_root}" rawiron_project_create_root_native)
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
  set(rawiron_tool_create_project_expected
    "Created RawIron project."
    "Project id: tool-cli-sample"
    "Project name: Tool CLI Sample"
    "Project root: ${rawiron_project_create_root_native}"
    "Manifest path: ${rawiron_project_create_root_native}/manifest.json"
    "Editor launch token: --game=tool-cli-sample"
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
      "${rawiron_tool_runtime_dll_arg}"
      -P "${rawiron_verify_command_output}"
  )

  add_test(
    NAME RawIron.Tool.Formats
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:ri_tool>"
      "-DCOMMAND_ARGS=--formats"
      "-DEXPECTED_STRINGS=${rawiron_tool_format_expected}"
      "-DFORBIDDEN_STRINGS=failed;exception"
      "${rawiron_tool_runtime_dll_arg}"
      -P "${rawiron_verify_command_output}"
  )

  add_test(
    NAME RawIron.Tool.Help
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:ri_tool>"
      "-DCOMMAND_ARGS=--help"
      "-DEXPECTED_STRINGS=${rawiron_tool_help_expected}"
      "-DFORBIDDEN_STRINGS=failed;exception"
      "${rawiron_tool_runtime_dll_arg}"
      -P "${rawiron_verify_command_output}"
  )

  add_test(
    NAME RawIron.Tool.AmbiguousCommandRejected
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:ri_tool>"
      "-DCOMMAND_ARGS=--formats;--workspace"
      "-DEXPECTED_STRINGS=Multiple primary commands were provided:;--workspace;--formats;Run one command at a time."
      "-DEXPECT_NONZERO=ON"
      "${rawiron_tool_runtime_dll_arg}"
      -P "${rawiron_verify_command_output}"
  )

  add_test(
    NAME RawIron.Tool.MissingCommandValueRejected
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:ri_tool>"
      "-DCOMMAND_ARGS=--extension-validate"
      "-DEXPECTED_STRINGS=--extension-validate requires a descriptor or package JSON path."
      "-DEXPECT_NONZERO=ON"
      "${rawiron_tool_runtime_dll_arg}"
      -P "${rawiron_verify_command_output}"
  )

  add_test(
    NAME RawIron.Tool.PluginList
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:ri_tool>"
      "-DCOMMAND_ARGS=--plugins-list;--game;liminal-hall;--root;${PROJECT_SOURCE_DIR}"
      "-DEXPECTED_STRINGS=${rawiron_tool_plugin_list_expected}"
      "-DFORBIDDEN_STRINGS=failed;exception;Plugin validation: fail"
      "${rawiron_tool_runtime_dll_arg}"
      -P "${rawiron_verify_command_output}"
  )

  add_test(
    NAME RawIron.Tool.PluginHandlers
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:ri_tool>"
      "-DCOMMAND_ARGS=--plugin-handlers"
      "-DEXPECTED_STRINGS=${rawiron_tool_plugin_handlers_expected}"
      "-DFORBIDDEN_STRINGS=failed;exception"
      "${rawiron_tool_runtime_dll_arg}"
      -P "${rawiron_verify_command_output}"
  )

  add_test(
    NAME RawIron.Tool.PluginDoctor
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:ri_tool>"
      "-DCOMMAND_ARGS=--plugins-doctor;--game;liminal-hall;--root;${PROJECT_SOURCE_DIR}"
      "-DEXPECTED_STRINGS=${rawiron_tool_plugin_doctor_expected}"
      "-DFORBIDDEN_STRINGS=failed;exception;unhealthy"
      "${rawiron_tool_runtime_dll_arg}"
      -P "${rawiron_verify_command_output}"
  )

  add_test(
    NAME RawIron.Tool.ExtensionValidate
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:ri_tool>"
      "-DCOMMAND_ARGS=--extension-validate;${PROJECT_SOURCE_DIR}/Plugins/Store/rawiron.quest-beacons/package.riplugin.json;--root;${PROJECT_SOURCE_DIR}"
      "-DEXPECTED_STRINGS=${rawiron_tool_extension_validate_expected}"
      "-DFORBIDDEN_STRINGS=failed;exception;Extension validation: fail"
      "${rawiron_tool_runtime_dll_arg}"
      -P "${rawiron_verify_command_output}"
  )

  add_test(
    NAME RawIron.Tool.AssetPackageResolve
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:ri_tool>"
      "-DCOMMAND_ARGS=--asset-package-resolve;logickit;--root;${PROJECT_SOURCE_DIR}"
      "-DEXPECTED_STRINGS=RawIron package resolution:;Root: logickit *;Activation order:;logickit@0.1.0"
      "-DFORBIDDEN_STRINGS=failed;exception;Issue:"
      "${rawiron_tool_runtime_dll_arg}"
      -P "${rawiron_verify_command_output}"
  )

  add_test(
    NAME RawIron.Tool.AssetPackageMountCheck
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:ri_tool>"
      "-DCOMMAND_ARGS=--asset-package-mount-check;logickit;--root;${PROJECT_SOURCE_DIR}"
      "-DEXPECTED_STRINGS=RawIron package mount check:;Root: logickit *;Live mounts:;logickit@0.1.0 -> Packages/logickit"
      "-DFORBIDDEN_STRINGS=failed;exception;Issue:"
      "${rawiron_tool_runtime_dll_arg}"
      -P "${rawiron_verify_command_output}"
  )

  add_test(
    NAME RawIron.Tool.GamePackageMountCheck
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:ri_tool>"
      "-DCOMMAND_ARGS=--game-package-mount-check;--game;cube-test;--root;${PROJECT_SOURCE_DIR}"
      "-DEXPECTED_STRINGS=RawIron game package mount check:;Game: cube-test;Required packages: mounted;Live mounts: 0"
      "-DFORBIDDEN_STRINGS=failed;exception;Issue:"
      "${rawiron_tool_runtime_dll_arg}"
      -P "${rawiron_verify_command_output}"
  )

  add_test(
    NAME RawIron.Tool.RigToolchainReport
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:ri_tool>"
      "-DCOMMAND_ARGS=--rig-toolchain-report;--root;${PROJECT_SOURCE_DIR}"
      "-DEXPECTED_STRINGS=${rawiron_tool_rig_report_expected}"
      "-DFORBIDDEN_STRINGS=failed;exception"
      "${rawiron_tool_runtime_dll_arg}"
      -P "${rawiron_verify_command_output}"
  )

  add_test(
    NAME RawIron.Tool.Workspace
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:ri_tool>"
      "-DCOMMAND_ARGS=--workspace;--root;${PROJECT_SOURCE_DIR}"
      "-DEXPECTED_STRINGS=${rawiron_tool_workspace_expected}"
      "-DFORBIDDEN_STRINGS=failed;exception"
      "${rawiron_tool_runtime_dll_arg}"
      -P "${rawiron_verify_command_output}"
  )

  add_test(
    NAME RawIron.Tool.ListProjects
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:ri_tool>"
      "-DCOMMAND_ARGS=--list-projects;--root;${PROJECT_SOURCE_DIR}"
      "-DEXPECTED_STRINGS=${rawiron_tool_projects_expected}"
      "-DFORBIDDEN_STRINGS=failed;exception"
      "${rawiron_tool_runtime_dll_arg}"
      -P "${rawiron_verify_command_output}"
  )

  add_test(
    NAME RawIron.Tool.DescribeProject
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:ri_tool>"
      "-DCOMMAND_ARGS=--describe-project;--game;rawiron-multiplayer-sandbox;--root;${PROJECT_SOURCE_DIR}"
      "-DEXPECTED_STRINGS=${rawiron_tool_describe_project_expected}"
      "-DFORBIDDEN_STRINGS=failed;exception;Format validation: fail"
      "${rawiron_tool_runtime_dll_arg}"
      -P "${rawiron_verify_command_output}"
  )

  add_test(
    NAME RawIron.Tool.DoctorProject
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:ri_tool>"
      "-DCOMMAND_ARGS=--doctor-project=rawiron-multiplayer-sandbox;--game;rawiron-multiplayer-sandbox;--root;${PROJECT_SOURCE_DIR}"
      "-DEXPECTED_STRINGS=${rawiron_tool_doctor_project_expected}"
      "-DFORBIDDEN_STRINGS=failed;exception;Doctor result: unhealthy"
      "${rawiron_tool_runtime_dll_arg}"
      -P "${rawiron_verify_command_output}"
  )

  add_test(
    NAME RawIron.Tool.ListProjectResources
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:ri_tool>"
      "-DCOMMAND_ARGS=--list-project-resources;--game;liminal-hall;--resource-category;levels;--root;${PROJECT_SOURCE_DIR}"
      "-DEXPECTED_STRINGS=${rawiron_tool_list_resources_expected}"
      "-DFORBIDDEN_STRINGS=failed;exception"
      "${rawiron_tool_runtime_dll_arg}"
      -P "${rawiron_verify_command_output}"
  )

  add_test(
    NAME RawIron.Tool.CreateProject
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:ri_tool>"
      "-DCOMMAND_ARGS=--create-project;tool-cli-sample;--name;Tool CLI Sample;--author;RawIron Tooling;--type;game;--version;1.0.0;--description;Created by tool test.;--root;${PROJECT_SOURCE_DIR};--game-root;${rawiron_project_create_root}"
      "-DEXPECTED_STRINGS=${rawiron_tool_create_project_expected}"
      "-DFORBIDDEN_STRINGS=failed;exception;Format validation: fail"
      "${rawiron_tool_runtime_dll_arg}"
      "-DCLEAN_PATHS_RAW=${rawiron_project_create_root}"
      "-DVERIFY_FILES_ROOT=${rawiron_project_create_root}"
      "-DREQUIRED_RELATIVE_FILES=manifest.json;levels/assembly.primitives.csv;scripts/gameplay.riscript;config/game.cfg"
      -P "${rawiron_verify_command_output}"
  )

  add_test(
    NAME RawIron.Tool.SceneKitTargets
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:ri_tool>"
      "-DCOMMAND_ARGS=--scenekit-targets"
      "-DEXPECTED_STRINGS=${rawiron_tool_reference_expected}"
      "-DFORBIDDEN_STRINGS=failed;exception"
      "${rawiron_tool_runtime_dll_arg}"
      -P "${rawiron_verify_command_output}"
  )

  add_test(
    NAME RawIron.Tool.SceneKitChecks
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:ri_tool>"
      "-DCOMMAND_ARGS=--scenekit-checks;--root;${PROJECT_SOURCE_DIR};--output-dir;${rawiron_scenekit_checks_root};--width;320;--height;256"
      "-DEXPECTED_STRINGS=${rawiron_scenekit_checks_expected}"
      "-DFORBIDDEN_STRINGS=failed;exception;[FAIL]"
      "${rawiron_tool_runtime_dll_arg}"
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
      "${rawiron_tool_runtime_dll_arg}"
      -P "${rawiron_verify_command_output}"
  )

  add_test(
    NAME RawIron.Tool.EnsureWorkspace
    COMMAND ${CMAKE_COMMAND}
      "-DEXECUTABLE=$<TARGET_FILE:ri_tool>"
      "-DCOMMAND_ARGS=--ensure-workspace;--root;${rawiron_workspace_smoke_root}"
      "-DEXPECTED_STRINGS=${rawiron_workspace_create_expected}"
      "-DFORBIDDEN_STRINGS=failed;exception"
      "${rawiron_tool_runtime_dll_arg}"
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
      "${rawiron_tool_runtime_dll_arg}"
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
      "${rawiron_tool_runtime_dll_arg}"
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
      "${rawiron_tool_runtime_dll_arg}"
      -P "${rawiron_verify_command_output}"
  )

  add_executable(
    PackageInstallTrustBoundarySmoke
    ${PROJECT_SOURCE_DIR}/Tests/PackageInstallTrustBoundarySmoke.cpp
  )
  target_link_libraries(PackageInstallTrustBoundarySmoke PRIVATE RawIron::Content)
  target_compile_features(PackageInstallTrustBoundarySmoke PRIVATE cxx_std_20)
  add_dependencies(PackageInstallTrustBoundarySmoke ri_tool)
  add_test(
    NAME RawIron.Tool.PackageInstallTrustBoundarySmoke
    COMMAND PackageInstallTrustBoundarySmoke $<TARGET_FILE:ri_tool>
  )

  add_executable(
    PluginPackageBuildSmoke
    ${PROJECT_SOURCE_DIR}/Tests/PluginPackageBuildSmoke.cpp
  )
  target_link_libraries(PluginPackageBuildSmoke PRIVATE RawIron::Content)
  target_compile_features(PluginPackageBuildSmoke PRIVATE cxx_std_20)
  add_dependencies(PluginPackageBuildSmoke ri_tool)
  add_test(
    NAME RawIron.Tool.PluginPackageBuildSmoke
    COMMAND PluginPackageBuildSmoke $<TARGET_FILE:ri_tool> ${PROJECT_SOURCE_DIR}
  )

  add_executable(
    RipakArchiveSecuritySmoke
    ${PROJECT_SOURCE_DIR}/Tests/RipakArchiveSecuritySmoke.cpp
  )
  target_link_libraries(RipakArchiveSecuritySmoke PRIVATE RawIron::Content)
  target_compile_features(RipakArchiveSecuritySmoke PRIVATE cxx_std_20)
  add_dependencies(RipakArchiveSecuritySmoke ri_tool)
  add_test(
    NAME RawIron.Tool.RipakArchiveSecuritySmoke
    COMMAND RipakArchiveSecuritySmoke $<TARGET_FILE:ri_tool>
  )
endif()
