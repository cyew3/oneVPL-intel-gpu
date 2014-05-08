mfx_include_dirs( )

include_directories (
  ${CMAKE_CURRENT_SOURCE_DIR}/../spy_test_component/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../codec/h264_dec/include
)

list( APPEND LIBS
  umc_codecs_merged
  umc_codecs_merged_sw
  umc_io_merged_sw
  umc_core_merged
  ippvc_l ippcc_l ippi_l ipps_l ippcore_l
  pthread
  dl
  )
make_executable( shortname none static )
