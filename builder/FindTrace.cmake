if(__TRACE)
  message( STATUS "Enabled tracing: ${__TRACE}" )
endif()

# ITT instrumentation is enabled by default to make VTune working out of the box
set(__ITT TRUE)
include ($ENV{MFX_HOME}/mdp_msdk-lib/builder/FindVTune.cmake)
 
if(__TRACE MATCHES all)
  append("-DMFX_TRACE_ENABLE_TEXTLOG -DMFX_TRACE_ENABLE_STAT" CMAKE_C_FLAGS)
  append("-DMFX_TRACE_ENABLE_TEXTLOG -DMFX_TRACE_ENABLE_STAT" CMAKE_CXX_FLAGS)
endif()
