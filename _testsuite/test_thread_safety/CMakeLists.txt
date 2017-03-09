mfx_include_dirs( )

include_directories (
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../mfx_player/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../mfx_transcoder/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../shared/include
  ${CMAKE_HOME_DIRECTORY}/mdp_msdk-lib/samples/sample_common/include
)

set( LIBS "" )
list( APPEND LIBS
    mfx_trans_pipeline
    mfx_pipeline
    shared_utils
    sample_common
    dispatch_trace
    umc_codecs_merged
    media_buffers
    umc_io
    umc
    vm
    vm_plus
    sample_spl_mux_dispatcher
    ${ITT_LIBS}
    ippvc_l ippcc_l ippdc_l ippi_l ipps_l ippcp_l ippcore_l
    pthread dl
)
set( defs "${WARNING_FLAGS}" )
make_executable( shortname universal )
