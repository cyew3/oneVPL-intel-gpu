add_subdirectory(tools/configure)

if(CMAKE_SYSTEM_NAME MATCHES Windows)
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(arch_postfix _64)
  else()
    set(arch_postfix _32)
  endif()

  set(tracerlibname mfx-tracer${arch_postfix})
else()
  set(tracerlibname mfx-tracer)
endif()

add_library(${tracerlibname} SHARED)

target_include_directories(${tracerlibname}
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_sources(${tracerlibname}
  PRIVATE
    config/config.h
    dumps/dump.h
    loggers/ilog.h
    loggers/log.h
    loggers/log_console.h
    loggers/log_etw_events.h
    loggers/log_file.h
    loggers/log_syslog.h
    loggers/timer.h
    loggers/thread_info.h
    tracer/tracer.h
    tracer/functions_table.h
    tracer/bits/mfxfunctions.h
    wrappers/mfx_structures.h
    config/config.cpp
    dumps/dump.cpp
    dumps/dump_mfxbrc.cpp
    dumps/dump_mfxcommon.cpp
    dumps/dump_mfxdefs.cpp
    dumps/dump_mfxenc.cpp
    dumps/dump_mfxplugin.cpp
    dumps/dump_mfxsession.cpp
    dumps/dump_mfxstructures.cpp
    dumps/dump_mfxvideo.cpp
    dumps/dump_mfxfei.cpp
    dumps/dump_mfxla.cpp
    dumps/dump_mfxvp8.cpp
    loggers/log.cpp
    loggers/log_console.cpp
    loggers/log_etw_events.cpp
    loggers/log_file.cpp
    loggers/log_syslog.cpp
    tracer/tracer.cpp
    tracer/tracer_linux.cpp
    tracer/tracer_windows.cpp
    wrappers/mfx_core.cpp
    wrappers/mfx_video_core.cpp
    wrappers/mfx_video_decode.cpp
    wrappers/mfx_video_enc.cpp
    wrappers/mfx_video_encode.cpp
    wrappers/mfx_video_user.cpp
    wrappers/mfx_video_vpp.cpp
    wrappers/mfx_video_fei.cpp
    $<$<PLATFORM_ID:Windows>:mfx-tracer.def>
)

if( NOT DEFINED MFX_MODULES_DIR )
  set( MFX_MODULES_DIR ${CMAKE_INSTALL_FULL_LIBDIR} )
endif( )

target_compile_definitions(${tracerlibname}
  PRIVATE
    MFX_MODULES_DIR="${MFX_MODULES_DIR}"
    $<$<PLATFORM_ID:Windows>:_MBCS>
)

if(CMAKE_SYSTEM_NAME MATCHES Windows)
  target_compile_options(${tracerlibname} PRIVATE /U_UNICODE /UUNICODE)
  target_link_libraries(${tracerlibname} PRIVATE ${CMAKE_DL_LIBS} mfx_dispatch_trace_mbcs)
endif()

target_link_options(${tracerlibname}
  PRIVATE
    $<$<PLATFORM_ID:Windows>:LINKER:/DEF:mfx-tracer.def>
    $<$<PLATFORM_ID:Linux>:LINKER:--version-script=${CMAKE_HOME_DIRECTORY}/api/mfx_dispatch/linux/libmfx.map>
)

get_mfx_version(mfx_version_major mfx_version_minor)
set_target_properties(${tracerlibname} PROPERTIES   VERSION ${mfx_version_major}.${mfx_version_minor})
set_target_properties(${tracerlibname} PROPERTIES SOVERSION ${mfx_version_major})

target_link_libraries(${tracerlibname} PRIVATE ${MFX_API_TARGET} ${CMAKE_DL_LIBS})

install(TARGETS ${tracerlibname} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

