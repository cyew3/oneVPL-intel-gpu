mfx_include_dirs( )
###################################################################################################
# hevce_tests


set( ENABLE_HEVC FALSE )

if ( (CMAKE_C_COMPILER MATCHES icc) OR ENABLE_HEVC_ON_GCC )
  set(ENABLE_HEVC TRUE)
endif()

if ( ENABLE_HEVC )

###################################################################################################
# Includes
include_directories( ${CMAKE_CURRENT_SOURCE_DIR} )
include_directories( ${CMAKE_HOME_DIRECTORY}/mdp_msdk-lib/_studio/mfx_lib/encode/h265/include )
include_directories( ${CMAKE_HOME_DIRECTORY}/mdp_msdk-lib/_studio/mfx_lib/fs_det/include  )
include_directories( ${CMAKE_HOME_DIRECTORY}/mdp_msdk-lib/_studio/mfx_lib/fei/h265/include )
include_directories( ${CMAKE_HOME_DIRECTORY}/mdp_msdk-lib/_studio/mfx_lib/cmrt_cross_platform/include )
# gtest/gmock
include_directories( ${CMAKE_HOME_DIRECTORY}/mdp_msdk-contrib/gmock/gtest/include )
include_directories( ${CMAKE_HOME_DIRECTORY}/mdp_msdk-contrib/gmock/include )

###################################################################################################
# Plugin sources
foreach( dir ${CMAKE_HOME_DIRECTORY}/mdp_msdk-lib/_studio/mfx_lib/encode/h265/src )
  file( GLOB_RECURSE srcs "${dir}/*.c" "${dir}/*.cpp" )
  list( APPEND sources ${srcs})
endforeach()

file( GLOB_RECURSE srcs "${CMAKE_HOME_DIRECTORY}/mdp_msdk-lib/_studio/mfx_lib/fs_det/src/*.cpp" )
list( APPEND sources ${srcs} )

list( APPEND sources ${CMAKE_HOME_DIRECTORY}/mdp_msdk-lib/_studio/mfx_lib/shared/src/mfx_enc_common.cpp )
list( APPEND sources ${CMAKE_HOME_DIRECTORY}/mdp_msdk-lib/_studio/mfx_lib/shared/src/mfx_common_int.cpp )

###################################################################################################
# Tests sources
file( GLOB_RECURSE srcs "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp" )
list( APPEND sources ${srcs})

###################################################################################################
# Defines and Libs
set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x" )
set( defs "-DAS_HEVCE_PLUGIN" )

set( LIBS "" )
list( APPEND LIBS
    hevc_pp_dispatcher
    hevc_pp_atom
    hevc_pp_avx2
    hevc_pp_px
    hevc_pp_sse4
    hevc_pp_ssse3
    umc
    vm
    vm_plus
    ${ITT_LIBS}
    ippmsdk_l ippdc_l ippj_l ippvc_l ippcc_l ippcv_l ippi_l ipps_l ippcore_l
    pthread dl va
)

list( APPEND LIBS_SUFFIX gmock )

###################################################################################################
# Target
make_executable( hevce_tests none )

endif() # ENABLE_HEVC
