set(MFX_ORIG_LDFLAGS "${MFX_LDFLAGS}" )

# Plugin version info
set(ptir_version_defs  "-DMFX_PLUGIN_PRODUCT_VERSION=\"\\\"${MEDIA_VERSION_STR}\"\\\"")


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
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../mfx_lib/shared/include )

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

set( defs "${API_FLAGS} ${WARNING_FLAGS}" )
make_library( pa none static )
set( defs "" )

####################################################################

#PACM (originally CM-ported reference application) static lib
set( sources "" )
set( sources.plus "" )


list( APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/pacm/kernels/deinterlace_genx_hsw.cpp )
list( APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/pacm/kernels/deinterlace_genx_bdw.cpp )

list( APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/pacm/deinterlacer.cpp )
list( APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/pacm/utilities.cpp )

list( APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/../mfx_lib/cmrt_cross_platform/src/cmrt_cross_platform.cpp )

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")

set( defs "${API_FLAGS} ${WARNING_FLAGS}" )
make_library( pacm none static )
set( defs "" )

####################################################################

#ptir_plugin shared lib
set( sources "" )
set( sources.plus "" )

set( USE_STRICT_NAME TRUE )
set(MFX_LDFLAGS "${MFX_ORIG_LDFLAGS} -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/../mfx_lib/plugin/libmfxsw_plugin.map" )


foreach( dir ${CMAKE_CURRENT_SOURCE_DIR}/ptir_plugin/src )
  file( GLOB_RECURSE srcs "${dir}/*.c" "${dir}/*.cpp" )
  list( APPEND sources ${srcs})
endforeach()

  set( LIBS "" )
  list( APPEND LIBS
    pa
    pacm
    umc
    vm
    vm_plus
    pthread
    dl
    ippmsdk_l ippj_l ippvc_l ippcc_l ippcv_l ippi_l ipps_l ippcore_l
  )

set( defs "${API_FLAGS} ${WARNING_FLAGS} ${defs} ${ptir_version_defs}" )
make_library( mfx_ptir_hw64 hw shared )
set( defs "" )
