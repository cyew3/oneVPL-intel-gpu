mfx_include_dirs( )

include_directories (
  ${CMAKE_HOME_DIRECTORY}/mdp_msdk-api/opensource/mfx_dispatch/include
  ${CMAKE_HOME_DIRECTORY}/mdp_msdk-lib/_studio/shared/umc/codec/spl_common/include
  ${CMAKE_HOME_DIRECTORY}/mdp_msdk-lib/_studio/shared/umc/codec/avi_spl/include
  ${CMAKE_HOME_DIRECTORY}/mdp_msdk-lib/_studio/shared/umc/codec/demuxer/include
  ${CMAKE_HOME_DIRECTORY}/mdp_msdk-lib/_studio/shared/umc/test_suite/spy_test_component/include
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../shared/include
  ${CMAKE_HOME_DIRECTORY}/mdp_msdk-samples/samples/deprecated/sample_spl_mux/api
#  ${CMAKE_HOME_DIRECTORY}/samples/sample_common/include
)

# =============================================================================

set( defs "" )
set( sources "" )
set( sources.plus "" )
file( GLOB_RECURSE srcs "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp" )
list( APPEND sources ${srcs})

list( REMOVE_ITEM sources ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_player.cpp)

foreach( variant sw drm x11 )
  check_variant( ${variant} configured )
  if( configured )
    make_library( mfx_pipeline ${variant} static )
  endif( )
endforeach( )

set( defs "-DLUCAS_DLL" )
foreach( variant sw drm x11 )
  check_variant( ${variant} configured )
  if( configured )
    make_library( mfx_pipeline_lucas ${variant} static )
  endif( )
endforeach( )

# =============================================================================

set( defs "" )
set( sources "" )
set( sources.plus "" )
list( APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_player.cpp)

foreach( variant sw drm x11 )
  check_variant( ${variant} configured )
  if( configured )
    set( LIBS "" )
    list( APPEND LIBS
        mfx_pipeline_${variant}
#        vpp_plugin
        shared_utils_lucas
        umc_splmux_merged
        umc_io_merged_sw
        umc_core_merged
        ${ITT_LIBS}
        dispatch_trace
        sample_spl_mux_dispatcher
        ippvc_l ippcc_l ippdc_l ippi_l ipps_l ippcp_l ippcore_l
        pthread dl
    )
    make_executable( mfx_player ${variant} static )
  endif( )
endforeach( )

set( defs "-DLUCAS_DLL" )
foreach( variant sw drm x11 )
  check_variant( ${variant} configured )
  if( configured )
    set( LIBS "" )
    list( APPEND LIBS
        mfx_pipeline_lucas_${variant}
#        vpp_plugin
        shared_utils
        umc_splmux_merged
        umc_io_merged_sw
        umc_core_merged
        ${ITT_LIBS}
        dispatch_trace
        sample_spl_mux_dispatcher
        ippvc_l ippcc_l ippdc_l ippi_l ipps_l ippcp_l ippcore_l
        pthread dl
    )
    make_library( mfx_player_lucas ${variant} shared )
  endif( )
endforeach( )
