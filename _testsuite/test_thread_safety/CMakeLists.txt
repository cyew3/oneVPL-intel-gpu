mfx_include_dirs( )

include_directories (
  ${CMAKE_HOME_DIRECTORY}/_testsuite/test_thread_safety/include
  ${CMAKE_HOME_DIRECTORY}/_testsuite/mfx_player/include
  ${CMAKE_HOME_DIRECTORY}/_testsuite/mfx_transcoder/include
  ${CMAKE_HOME_DIRECTORY}/_testsuite/shared/include
  ${CMAKE_HOME_DIRECTORY}/samples/sample_common/include
)

foreach( variant x11 drm )
  check_variant( ${variant} configured )
  if( configured )
    set( LIBS "" )
    list( APPEND LIBS
      mfx_trans_pipeline_${variant}
      mfx_pipeline_${variant}
      shared_utils
      sample_common_${variant}
      dispatch_trace
      umc_codecs_merged
      umc_io_merged_sw
      umc_core_merged
      sample_spl_mux_dispatcher
      ${ITT_LIBS}
      ippvc_l ippcc_l ippdc_l ippi_l ipps_l ippcp_l ippcore_l
      pthread dl
    )

    make_executable( shortname ${variant} )
  endif()
endforeach()
