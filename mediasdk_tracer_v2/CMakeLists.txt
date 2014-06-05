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
  "loggers/ilog.h"
  "loggers/log.h"
  "loggers/log_console.h"
  "loggers/log_file.h"
  "loggers/log_syslog.h"
  "loggers/log_etw_events.h"
  "loggers/timer.h"
  "loggers/thread_info.h"
  "config/config.h"
  "dumps/dump.h"
  "tracer/functions_table.h"
  "tracer/bits/mfxfunctions.h"
  "wrappers/mfx_core.h"
  "wrappers/mfx_video_core.h"
  "wrappers/mfx_video_decode.h"
  "wrappers/mfx_video_encode.h"
  "wrappers/mfx_video_vpp.h"
  "wrappers/proxymfx.h"
  )

set(SOURCES
  "loggers/log.cpp"
  "loggers/log_console.cpp"
  "loggers/log_file.cpp"
  "loggers/log_syslog.cpp"
  "loggers/log_etw_events.cpp"
  "config/config.cpp"
  "dumps/dump_mfxcommon.cpp"
  "dumps/dump_mfxdefs.cpp"
  "dumps/dump_mfxstructures.cpp"
  "tracer/tracer_linux.cpp"
  "tracer/tracer_windows.cpp"
  "wrappers/mfx_core.cpp"
  "wrappers/mfx_video_core.cpp"
  "wrappers/mfx_video_decode.cpp"
  "wrappers/mfx_video_encode.cpp"
  "wrappers/mfx_video_vpp.cpp"
  )


add_library(tracer_lin SHARED ${SOURCES} ${HEADERS})
set_target_properties( tracer_lin PROPERTIES COMPILE_FLAGS "-g -O0" )
