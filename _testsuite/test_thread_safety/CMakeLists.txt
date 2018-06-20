mfx_include_dirs( )

include_directories (
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../mfx_player/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../mfx_transcoder/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../shared/include
  ${MSDK_SAMPLES_ROOT}/sample_common/include
)

set( LIBS "" )
list( APPEND LIBS
    mfx_trans_pipeline
    mfx_pipeline
    shared_utils
    sample_common
    mfx
    umc_codecs_merged
    media_buffers
    umc_io
    umc
    vm
    vm_plus
    sample_spl_mux_dispatcher
    mfx_trace
    ${ITT_LIBRARIES}
    ippvc_l ippcc_l ippdc_l ippi_l ipps_l ippcp_l ippcore_l
    pthread dl
)
set( defs "-DMFX_VERSION_USE_LATEST ${WARNING_FLAGS}" )
make_executable( shortname universal )
