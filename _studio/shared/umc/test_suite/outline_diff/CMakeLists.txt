if (CMAKE_SYSTEM_NAME MATCHES Windows OR MFX_BUNDLED_IPP OR NOT BUILD_EXTRA)
  return()
endif()

add_executable(outline_diff)

target_include_directories (outline_diff
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../spy_test_component/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../codec/h264_dec/include
  )

target_sources(outline_diff
  PRIVATE
    src/outline_diff.cpp
)

target_link_libraries( outline_diff
  PRIVATE
    umc_codecs_merged
    color_space_converter
    mpeg2_dec_hw
    h264_dec
    vc1_dec
    jpeg_dec_sw
    media_buffers
    umc_io
    umc
    vm
    vm_plus
    IPP::vc IPP::cc IPP::i IPP::s IPP::core
    pthread
    dl
    mfx_static_lib
    mfx_shared_lib
  )

