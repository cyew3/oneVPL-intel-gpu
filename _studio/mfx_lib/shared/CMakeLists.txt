set(MFX_ORIG_LDFLAGS "${MFX_LDFLAGS}" )

mfx_include_dirs( )

list( APPEND decdirs
  vc1 h264 mpeg2/hw mpeg2/sw mjpeg
  )
list( APPEND cdirs
  vc1_dec vc1_common h264 h264_enc brc mpeg2_dec/hw mpeg2_dec/sw common mp3_dec
  color_space_converter aac_enc aac_dec aac_common mp3_common
  )
foreach( dir ${decdirs})
  include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../decode/${dir}/include )
endforeach()
foreach( dir ${cdirs} )
  include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../../shared/umc/codec/${dir}/include )
endforeach( )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../../shared/asc/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../audio_encode/aac/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../encode_hw/h264/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../encode_hw/h265/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../audio_decode/aac/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../audio_decode/mp3/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../cmrt_cross_platform/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../vpp/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../shared/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../brc/mpeg2/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../end/mpeg2/include )

set(sources
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_brc_common.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_common_decode_int.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_common_int.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_enc_common.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_mpeg2_dec_common.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_vc1_dec_common.cpp                ## open-source-cut-line
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_critical_error_handler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_ddi_enc_dump.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_h264_enc_common_hw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_h264_encode_vaapi.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_h264_encode_factory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_mpeg2_enc_common_hw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_mpeg2_encode_vaapi.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_mpeg2_encode_factory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_vpx_dec_common.cpp                ## open-source-cut-line
    )
set( sources.plus "" )

set( defs "${API_FLAGS} ${WARNING_FLAGS}" )
if(NOT OPEN_SOURCE)
  list(APPEND sources
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_h264_enc_common.cpp
    )
  make_library( mfx_common sw static )
  append_property( mfx_common_sw COMPILE_FLAGS " -DMFX_RT" )
endif()

make_library( mfx_common hw static )
set( defs "" )
