set(MFX_ORIG_LDFLAGS "${MFX_LDFLAGS}" )

# =============================================================================

mfx_include_dirs( )

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/ptir_plugin/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/pa )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/pa/deinterlacer )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/pa/pfc )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/pa/telecine )

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/pacm )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/pacm/cmut )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/pacm/kernels )

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../mfx_lib/cmrt_cross_platform/include )

# =============================================================================

#PA (originally PA_cmd system reference application) static lib
set( sources "" )
set( sources.plus "" )
foreach( dir ${CMAKE_CURRENT_SOURCE_DIR}/pa/deinterlacer )
  file( GLOB_RECURSE srcs "${dir}/*.c" "${dir}/*.cpp" )
  list( APPEND sources ${srcs})
endforeach()

foreach( dir ${CMAKE_CURRENT_SOURCE_DIR}/pa/pfc )
  file( GLOB_RECURSE srcs "${dir}/*.c" "${dir}/*.cpp" )
  list( APPEND sources ${srcs})
endforeach()

foreach( dir ${CMAKE_CURRENT_SOURCE_DIR}/pa/telecine )
  file( GLOB_RECURSE srcs "${dir}/*.c" "${dir}/*.cpp" )
  list( APPEND sources ${srcs})
endforeach()

foreach( dir ${CMAKE_CURRENT_SOURCE_DIR}/pa )
  file( GLOB_RECURSE srcs "${dir}/*.c" "${dir}/*.cpp" )
  list( APPEND sources ${srcs})
endforeach()

make_library( pa none static )

####################################################################

#PACM (originally CM-ported reference application) static lib
set( sources "" )
set( sources.plus "" )


list( APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/pacm/kernels/deinterlace_genx_hsw.cpp )

list( APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/pacm/deinterlacer.cpp )
list( APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/pacm/utilities.cpp )

list( APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/../mfx_lib/cmrt_cross_platform/src/cmrt_cross_platform.cpp )

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")

make_library( pacm none static )

####################################################################

#ptir_plugin shared lib
set( sources "" )
set( sources.plus "" )


foreach( dir ${CMAKE_CURRENT_SOURCE_DIR}/ptir_plugin/src )
  file( GLOB_RECURSE srcs "${dir}/*.c" "${dir}/*.cpp" )
  list( APPEND sources ${srcs})
endforeach()

  set( LIBS "" )
  list( APPEND LIBS
    pa
    pacm
    umc_core_merged
    pthread
    dl
    ippmsdk_l ippj_l ippvc_l ippcc_l ippcv_l ippi_l ipps_l ippcore_l
  )

make_library( ptir_plugin none shared )
