
add_library(mfx_trace STATIC)

target_sources(mfx_trace
  PRIVATE
    include/mfx_evntprov.h
    include/mfx_trace_etw.h
    include/mfx_trace_ftrace.h
    include/mfx_trace_itt.h
    include/mfx_trace_stat.h
    include/mfx_trace_tal.h
    include/mfx_trace_textlog.h
    include/mfx_trace_utils.h
    include/sys/mfx_trace_utils_linux.h
    include/sys/mfx_trace_utils_windows.h

    src/mfx_trace.cpp
    src/mfx_trace_etw.cpp
    src/mfx_trace_ftrace.cpp
    src/mfx_trace_itt.cpp
    src/mfx_trace_stat.cpp
    src/mfx_trace_tal.cpp
    src/mfx_trace_textlog.cpp
    src/mfx_trace_utils.cpp
    src/mfx_trace_utils_linux.cpp
    src/mfx_trace_utils_windows.cpp
    $<$<NOT:$<BOOL:${BUILD_VPL}>>:src/mfx_reflect.cpp>
  )

target_include_directories(mfx_trace
  PUBLIC
    include
    ${MFX_API_HOME}/mediasdk_structures
  )

target_link_libraries(mfx_trace PUBLIC mfx_static_lib vm)

include(sources_ext.cmake)