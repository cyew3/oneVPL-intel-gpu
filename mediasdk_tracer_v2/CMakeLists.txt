cmake_minimum_required( VERSION 2.6.2 FATAL_ERROR )
project(tracer)

include_directories(
  "$ENV{MFX_HOME}/include"
  "${CMAKE_HOME_DIRECTORY}/loggers"
  "${CMAKE_HOME_DIRECTORY}/config"
  "${CMAKE_HOME_DIRECTORY}/dumps"
  "${CMAKE_HOME_DIRECTORY}/tracer"
  "${CMAKE_HOME_DIRECTORY}/wrappers"
  )

set(HEADERS
  "config/config.h"
  "dumps/dump.h"
  "loggers/ilog.h"
  "loggers/log.h"
  "loggers/log_console.h"
  "loggers/log_etw_events.h"
  "loggers/log_file.h"
  "loggers/log_syslog.h"
  "loggers/timer.h"
  "loggers/thread_info.h"
  "tracer/functions_table.h"
  "tracer/bits/mfxfunctions.h"
  "wrappers/mfx_structures.h"
  "wrappers/proxymfx.h"
  )

set(SOURCES
  "loggers/log.cpp"
  "loggers/log_console.cpp"
  "loggers/log_etw_events.cpp"
  "loggers/log_file.cpp"
  "loggers/log_syslog.cpp"
  "config/config.cpp"
  "dumps/dump.cpp"
  "dumps/dump_mfxcommon.cpp"
  "dumps/dump_mfxdefs.cpp"
  "dumps/dump_mfxenc.cpp"
  "dumps/dump_mfxplugin.cpp"
  "dumps/dump_mfxsession.cpp"
  "dumps/dump_mfxstructures.cpp"
  "dumps/dump_mfxvideo.cpp"
  "tracer/tracer_linux.cpp"
  "tracer/tracer_windows.cpp"
  "wrappers/mfx_core.cpp"
  "wrappers/mfx_video_core.cpp"
  "wrappers/mfx_video_decode.cpp"
  "wrappers/mfx_video_enc.cpp"
  "wrappers/mfx_video_encode.cpp"
  "wrappers/mfx_video_user.cpp"
  "wrappers/mfx_video_vpp.cpp"
  )


set(no_warnings "-Wno-unknown-pragmas -Wno-unused")
set(debug_flags "-g -D_DEBUG -O0 -Wall ${no_warnings}")
set(release_flags "-DNDEBUG -O2 -D_FORTIFY_SOURCE=2 -fstack-protector -Wall ${no_warnings}")

set(CMAKE_C_FLAGS "-pipe -fPIC")
set(CMAKE_CXX_FLAGS "-pipe -fPIC")
set(CMAKE_C_FLAGS_DEBUG     "${debug_flags}" CACHE STRING "" FORCE)
set(CMAKE_C_FLAGS_RELEASE   "${release_flags}" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_DEBUG   "${debug_flags}" CACHE STRING "" FORCE)
set(CMAKE_CXX_FLAGS_RELEASE "${release_flags}" CACHE STRING "" FORCE)

add_library(mfx-tracer SHARED ${SOURCES} ${HEADERS})
set_target_properties( mfx-tracer PROPERTIES LINK_FLAGS
  "-Wl,--no-undefined,-z,relro,-z,now,-z,noexecstack,--version-script=${CMAKE_HOME_DIRECTORY}/libmfx.map -fstack-protector" )
target_link_libraries( mfx-tracer dl rt )
