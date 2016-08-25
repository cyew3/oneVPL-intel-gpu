set(MFX_ORIG_LDFLAGS "${MFX_LDFLAGS}" )

mfx_include_dirs( )

list( APPEND decdirs
  vc1 h264 mpeg2 mjpeg
  )
list( APPEND cdirs
  vc1_dec vc1_common h264 h264_enc brc mpeg2_dec common mp3_dec
  color_space_converter aac_enc aac_dec aac_common mp3_common
  )
foreach( dir ${decdirs})
  include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../decode/${dir}/include )
endforeach()
foreach( dir ${cdirs} )
  include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../../shared/umc/codec/${dir}/include )
endforeach( )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../audio_encode/aac/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../encode_hw/h264/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../audio_decode/aac/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../audio_decode/mp3/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../cmrt_cross_platform/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../vpp/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../shared/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../brc/mpeg2/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../end/mpeg2/include )

set( sources "" )
set( sources.plus "" )
foreach( prefix ${CMAKE_CURRENT_SOURCE_DIR}/src )
  list( APPEND sources
    ${prefix}/mfx_brc_common.cpp
    ${prefix}/mfx_common_decode_int.cpp
    ${prefix}/mfx_common_int.cpp
    ${prefix}/mfx_enc_common.cpp
    ${prefix}/mfx_h264_enc_common.cpp
    ${prefix}/mfx_mpeg2_dec_common.cpp
    ${prefix}/mfx_vc1_dec_common.cpp                ## open-source-cut-line
    ${prefix}/mfx_critical_error_handler.cpp
  )
endforeach()

set( defs "${WARNING_FLAGS}" )
make_library( mfx_common none static )
append_property( mfx_common COMPILE_FLAGS " -DMFX_RT" )
set( defs "" )

foreach( prefix ${CMAKE_CURRENT_SOURCE_DIR}/src )
  list( APPEND sources
    ${prefix}/mfx_ddi_enc_dump.cpp
    ${prefix}/mfx_h264_enc_common_hw.cpp
    ${prefix}/mfx_h264_encode_vaapi.cpp
    ${prefix}/mfx_h264_encode_factory.cpp
    ${prefix}/mfx_mpeg2_enc_common_hw.cpp
    ${prefix}/mfx_mpeg2_encode_vaapi.cpp
    ${prefix}/mfx_mpeg2_encode_factory.cpp
  )
endforeach()

set( defs "${WARNING_FLAGS}" )
make_library( mfx_common sw static )
append_property( mfx_common_sw COMPILE_FLAGS " -DMFX_RT" )
make_library( mfx_common hw static )
append_property( mfx_common_hw COMPILE_FLAGS " -DMFX_RT" )
set( defs "" )

## open-source-cut-start

set( sources "" )
set( sources.plus "" )
list( APPEND sources
  ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_vc1_enc_common.cpp
)

set( defs "${WARNING_FLAGS}" )
make_library( vc1_shared none static )
set( defs "" )

## open-source-cut-end