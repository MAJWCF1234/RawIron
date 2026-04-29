option(RAWIRON_AUDIO_MINIAUDIO "Build native miniaudio device backend (WASAPI/CoreAudio/ALSA/Pulse)" ON)

set(RAWIRON_AUDIO_SOURCES
  src/AudioEnvironment.cpp
  src/AudioManager.cpp
)

if (RAWIRON_AUDIO_MINIAUDIO)
  set(RAWIRON_MINIAUDIO_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniaudio/miniaudio.h")
  if (NOT EXISTS "${RAWIRON_MINIAUDIO_HEADER}")
    message(FATAL_ERROR
      "miniaudio.h missing at ${RAWIRON_MINIAUDIO_HEADER}. "
      "Download with:\n"
      "  curl -fsSL \"https://raw.githubusercontent.com/mackron/miniaudio/0.11.21/miniaudio.h\" -o \"${RAWIRON_MINIAUDIO_HEADER}\"")
  endif ()

  list(APPEND RAWIRON_AUDIO_SOURCES
    src/miniaudio_impl.c
    src/MiniaudioBackend.cpp
  )
endif ()

add_library(RawIron.Audio ${RAWIRON_AUDIO_SOURCES})

add_library(RawIron::Audio ALIAS RawIron.Audio)

target_include_directories(RawIron.Audio
  PUBLIC
    include
)

if (RAWIRON_AUDIO_MINIAUDIO)
  target_include_directories(RawIron.Audio
    PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniaudio"
  )
  target_compile_definitions(RawIron.Audio PUBLIC RAWIRON_AUDIO_HAS_MINIAUDIO=1)
else ()
  target_sources(RawIron.Audio PRIVATE src/MiniaudioBackendStub.cpp)
  target_compile_definitions(RawIron.Audio PUBLIC RAWIRON_AUDIO_HAS_MINIAUDIO=0)
endif ()

target_compile_features(RawIron.Audio PUBLIC cxx_std_20)

set_source_files_properties(src/miniaudio_impl.c PROPERTIES LANGUAGE C)

if (WIN32)
  target_link_libraries(RawIron.Audio PRIVATE ole32 user32 winmm uuid)
elseif (APPLE)
  find_library(RAWIRON_COREAUDIO_LIB CoreAudio REQUIRED)
  find_library(RAWIRON_AUDIOUNIT_LIB AudioUnit REQUIRED)
  find_library(RAWIRON_AUDIOTOOLBOX_LIB AudioToolbox REQUIRED)
  find_library(RAWIRON_COREFOUNDATION_LIB CoreFoundation REQUIRED)
  target_link_libraries(RawIron.Audio PRIVATE
    ${RAWIRON_COREAUDIO_LIB}
    ${RAWIRON_AUDIOUNIT_LIB}
    ${RAWIRON_AUDIOTOOLBOX_LIB}
    ${RAWIRON_COREFOUNDATION_LIB}
  )
elseif (UNIX)
  target_link_libraries(RawIron.Audio PRIVATE pthread dl m)
endif ()
