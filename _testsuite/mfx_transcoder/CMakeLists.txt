mfx_include_dirs( )

include_directories (
  ${CMAKE_HOME_DIRECTORY}/_testsuite/mfx_player/include
  ${CMAKE_HOME_DIRECTORY}/_testsuite/mfx_transcoder/include
  ${CMAKE_HOME_DIRECTORY}/_testsuite/shared/include
  ${CMAKE_HOME_DIRECTORY}/samples/sample_spl_mux/api
#  ${CMAKE_HOME_DIRECTORY}/samples/sample_common/include
)

# =============================================================================

set( defs "" )
set( sources "" )
set( sources.plus "" )
file( GLOB_RECURSE srcs "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp" )
list( APPEND sources ${srcs})

list( REMOVE_ITEM sources ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_transcoder.cpp)

foreach( variant sw drm x11 )
  check_variant( ${variant} configured )
  if( configured )
    make_library( mfx_trans_pipeline ${variant} static )
  endif( )
endforeach( )

set( defs "-DLUCAS_DLL" )
foreach( variant sw drm x11 )
  check_variant( ${variant} configured )
  if( configured )
    make_library( mfx_trans_pipeline_lucas ${variant} static )
  endif( )
endforeach( )

# =============================================================================

set( defs "" )
set( sources "" )
set( sources.plus "" )
list( APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_transcoder.cpp)

foreach( variant sw drm x11 )
  check_variant( ${variant} configured )
  if( configured )
    set( LIBS "" )
    list( APPEND LIBS
        mfx_trans_pipeline_${variant}
        mfx_pipeline_${variant}
#        vpp_plugin
        shared_utils
#        sample_common_${variant}
        umc_splmux_merged
        umc_io_merged_sw
        umc_core_merged
        ${ITT_LIBS}
        dispatch_trace
        sample_spl_mux_dispatcher
        ippvc_l ippcc_l ippdc_l ippi_l ipps_l ippcp_l ippcore_l
        pthread dl
    )
    make_executable( mfx_transcoder ${variant} static )
  endif( )
endforeach( )

set( defs "-DLUCAS_DLL" )
foreach( variant sw drm x11 )
  check_variant( ${variant} configured )
  if( configured )
    set( LIBS "" )
    list( APPEND LIBS
        mfx_trans_pipeline_lucas_${variant}
        mfx_pipeline_lucas_${variant}
#        vpp_plugin
        shared_utils_lucas
#        sample_common_${variant}
        umc_splmux_merged
        umc_io_merged_sw
        umc_core_merged
        ${ITT_LIBS}
        dispatch_trace
        sample_spl_mux_dispatcher
        ippvc_l ippcc_l ippdc_l ippi_l ipps_l ippcp_l ippcore_l
        pthread dl
    )
    make_library( mfx_transcoder_lucas ${variant} shared )
  endif( )
endforeach( )
