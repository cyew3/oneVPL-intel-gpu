mfx_include_dirs( )

include_directories (
  ${CMAKE_CURRENT_SOURCE_DIR}/../mfx_player/include
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../shared/include
  ${MSDK_SAMPLES_ROOT}/deprecated/sample_spl_mux/api
#  ${CMAKE_HOME_DIRECTORY}/samples/sample_common/include
)

# =============================================================================

set( defs "" )
set( sources "" )
set( sources.plus "" )
file( GLOB_RECURSE srcs "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp" )
list( APPEND sources ${srcs})

list( REMOVE_ITEM sources ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_transcoder.cpp)

set( defs "${WARNING_FLAGS}" )
make_library( mfx_trans_pipeline universal static )

set( defs "-DLUCAS_DLL ${WARNING_FLAGS}" )
make_library( mfx_trans_pipeline_lucas universal static )

# =============================================================================

set( defs "" )
set( sources "" )
set( sources.plus "" )
list( APPEND sources
  ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_transcoder.cpp
)

set( LIBS "" )
list( APPEND LIBS
    mfx_trans_pipeline
    mfx_pipeline
#        vpp_plugin
    shared_utils
#        sample_common
    umc_splmux_merged
    media_buffers
    umc_io
    umc
    vm
    vm_plus
    ${ITT_LIBS}
    dispatch_trace
    sample_spl_mux_dispatcher
    ippvc_l ippcc_l ippdc_l ippi_l ipps_l ippcp_l ippcore_l
    pthread dl
)
set( defs "${WARNING_FLAGS}" )
make_executable( mfx_transcoder universal static )

set( defs "-DLUCAS_DLL ${WARNING_FLAGS}" )
set( LIBS "" )
list( APPEND LIBS
    mfx_trans_pipeline_lucas
    mfx_pipeline_lucas
#        vpp_plugin
    shared_utils_lucas
#        sample_common_${variant}
    umc_splmux_merged
    media_buffers
    umc_io
    umc
    vm
    vm_plus
    ${ITT_LIBS}
    dispatch_trace
    sample_spl_mux_dispatcher
    ippvc_l ippcc_l ippdc_l ippi_l ipps_l ippcp_l ippcore_l
    pthread dl
)
make_library( mfx_transcoder_lucas universal shared )
