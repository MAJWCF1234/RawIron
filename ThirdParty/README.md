# Third-party dependencies

Vendored third-party code and libraries belong under `ThirdParty/<dependency>`.
Examples include `archive`, `backward-cpp`, `cgltf`, `EOS`, `meshoptimizer`, and `ufbx`.
`miniaudio` and `stb` were relocated here from engine-local vendor folders on
2026-09-04 with unchanged header hashes. Audio/texture adapters remain in their
owning engine modules; `ri_tool` shares the same stb copy for DEFLATE decoding.
Keep upstream licenses and version/provenance records beside the dependency. Point CMake targets
at that location; do not put new dependency implementations inside engine or game source folders.

Raw Iron adapters remain in the engine subsystem that owns the integration. Comparison models,
textures, and their licenses belong in the owning experience's `assets` tree, such as
`Games/CubeTest/assets/reference/threejs-r185`; those assets must not depend on a disposable upstream
checkout. Copying an asset does not require copying its upstream JavaScript runtime.

Existing dependency locations are not permission to add further copies. When updating or moving
one, update every build/include/tool reference and verify the affected targets. Never remove a
dependency or archive just because it is not needed by the current task.
