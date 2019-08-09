set(MFX_ORIG_LDFLAGS "${MFX_LDFLAGS}" )

list( APPEND vdirs
  mjpeg
  )
list( APPEND cdirs
  jpeg_enc color_space_converter
  )

if( NOT OPEN_SOURCE )
  list( APPEND vdirs
    h264 ih265 mpeg2
    )
  list( APPEND cdirs
    mpeg2_enc h264_enc brc
    )
endif()

mfx_include_dirs( )
foreach( dir ${vdirs} )
  include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/${dir}/include )
endforeach()

foreach( dir ${cdirs} )
  include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../../shared/umc/codec/${dir}/include )
endforeach( )

set( defs "${API_FLAGS} ${WARNING_FLAGS}" )
set( sources "" )
set( sources.plus "" )
foreach( dir ${vdirs} )
  file( GLOB_RECURSE srcs "${dir}/src/*.c" "${dir}/src/*.cpp" )
  list( APPEND sources ${srcs})
endforeach()
make_library( encode none static )
set( defs "" )

if( MFX_ENABLE_SW_FALLBACK )
  ### JPEG Encoder
  include_directories( ${MSDK_UMC_ROOT}/codec/jpeg_common/include )
  include_directories( ${MSDK_UMC_ROOT}/codec/jpeg_enc/include )
  include_directories( ${MSDK_UMC_ROOT}/codec/color_space_converter/include )

  set( sources "" )
  set( defs "" )
  list( APPEND sources
      ${MSDK_UMC_ROOT}/codec/jpeg_enc/src/enchtbl.cpp
      ${MSDK_UMC_ROOT}/codec/jpeg_enc/src/encqtbl.cpp
      ${MSDK_UMC_ROOT}/codec/jpeg_enc/src/jpegenc.cpp
      ${MSDK_UMC_ROOT}/codec/jpeg_enc/src/jpegencrst.cpp
      ${MSDK_UMC_ROOT}/codec/jpeg_enc/src/umc_mjpeg_video_encoder.cpp
      )
  make_library( encode sw static )
endif()
