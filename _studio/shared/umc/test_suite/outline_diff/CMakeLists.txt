mfx_include_dirs( )

include_directories (
  ${CMAKE_CURRENT_SOURCE_DIR}/../spy_test_component/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../codec/h264_dec/include
)

list( APPEND LIBS
  umc_codecs_merged
  color_space_converter_sw
  mpeg2_dec_sw
  h264_dec_sw
  vc1_dec_elk
  jpeg_dec_sw
  media_buffers
  umc_io
  umc
  vm
  vm_plus
  ippvc_l ippcc_l ippi_l ipps_l ippcore_l
  pthread
  dl
  )
make_executable( shortname none static )
