if (RAWIRON_BUILD_LIMINAL_HALL_MODULE)
  add_subdirectory(Runtime)
endif()

if (RAWIRON_BUILD_LIMINAL_GAME)
  add_subdirectory(App)
endif()
