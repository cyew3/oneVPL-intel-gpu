list( APPEND vdirs
  mjpeg
  )
list( APPEND cdirs
  jpeg_enc color_space_converter
  )

if(NOT MFX_DISABLE_SW_FALLBACK)
  list( APPEND vdirs
    h264 mpeg2
    )
  list( APPEND cdirs
    mpeg2_enc h264_enc brc
    )
endif()

set(include_dirs "")

foreach( dir ${vdirs} )
  list(APPEND include_dirs ${dir}/include)
endforeach()

foreach( dir ${cdirs} )
  list(APPEND include_dirs ${MSDK_UMC_ROOT}/codec/${dir}/include)
endforeach( )

set(sources
  mjpeg/include/mfx_mjpeg_encode.h
  mjpeg/src/mfx_mjpeg_encode.cpp
  )

if (NOT MFX_DISABLE_SW_FALLBACK)

  list(APPEND sources
    h264/include/mfx_h264_encode.h
    h264/include/mfx_mvc_encode.h
    h264/src/mfx_h264_encode.cpp
    h264/src/mfx_mvc_encode.cpp

    mpeg2/include/mfx_mpeg2_encode.h
    mpeg2/src/mfx_mpeg2_encode.cpp

  )
endif()


add_library(encode STATIC ${sources})

target_include_directories(encode PUBLIC ${include_dirs})

target_link_libraries(encode PUBLIC mfx_static_lib umc cmrt_cross_platform_hw)

set( defs "" )

if( MFX_ENABLE_JPEG_SW_FALLBACK )
  ### JPEG Encoder
  set( sources "" )
  set( defs "" )
  list( APPEND sources
      ${MSDK_UMC_ROOT}/codec/jpeg_enc/include/enchtbl.h
      ${MSDK_UMC_ROOT}/codec/jpeg_enc/include/encqtbl.h
      ${MSDK_UMC_ROOT}/codec/jpeg_enc/include/jpegenc.h
      ${MSDK_UMC_ROOT}/codec/jpeg_enc/include/umc_mjpeg_video_encoder.h
      ${MSDK_UMC_ROOT}/codec/jpeg_enc/src/enchtbl.cpp
      ${MSDK_UMC_ROOT}/codec/jpeg_enc/src/encqtbl.cpp
      ${MSDK_UMC_ROOT}/codec/jpeg_enc/src/jpegenc.cpp
      ${MSDK_UMC_ROOT}/codec/jpeg_enc/src/jpegencrst.cpp
      ${MSDK_UMC_ROOT}/codec/jpeg_enc/src/umc_mjpeg_video_encoder.cpp
      )

  add_library(encode_sw STATIC ${sources})
  set_property(TARGET encode_sw PROPERTY FOLDER "sw_fallback")

  target_include_directories(encode_sw PUBLIC
    ${MSDK_UMC_ROOT}/codec/jpeg_common/include
    ${MSDK_UMC_ROOT}/codec/jpeg_enc/include
    ${MSDK_UMC_ROOT}/codec/color_space_converter/include
    )

  target_link_libraries(encode_sw PUBLIC mfx_static_lib umc)
endif()

include(sources_ext.cmake OPTIONAL)