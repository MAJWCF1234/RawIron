rawiron_add_app(
  TARGET RawIron.Forge
  SOURCES
    src/main.cpp
    src/ForgeCatalog.cpp
  LIBRARIES
    RawIron::Core
    RawIron::SceneUtilities
  WIN32_LIBRARIES
    user32
    gdi32
    shell32
  EXPECTED_STRINGS
    "[Forge Ready]"
    "Model sources:"
    "Rig assets:"
  FORBIDDEN_STRINGS
    "failed"
    "exception"
  SMOKE_ARGS
    "--headless"
)

if (RAWIRON_BUILD_TESTS)
  add_executable(RawIron.Forge.CatalogSmoke
    Tests/ForgeCatalogSmoke.cpp
    src/ForgeCatalog.cpp
  )
  target_include_directories(RawIron.Forge.CatalogSmoke PRIVATE src)
  target_link_libraries(RawIron.Forge.CatalogSmoke PRIVATE RawIron::SceneUtilities)
  target_compile_features(RawIron.Forge.CatalogSmoke PRIVATE cxx_std_20)
  add_test(NAME RawIron.Forge.Catalog COMMAND RawIron.Forge.CatalogSmoke)
endif()
