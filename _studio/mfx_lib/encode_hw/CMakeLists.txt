set(MFX_ORIG_LDFLAGS "${MFX_LDFLAGS}" )

list( APPEND vdirs
  h264 mjpeg mpeg2 mvc svc
  )
list( APPEND cdirs
  mpeg2_enc jpeg_common brc color_space_converter h264_enc
  )
mfx_include_dirs( )
foreach( dir ${vdirs} )
  include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/${dir}/include )
endforeach()
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../../shared/asc/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../cmrt_cross_platform/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../enc_hw/mpeg2/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../pak/mpeg2/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../genx/h264_encode/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../fei/h264_preenc )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../../hevce_hw/h265/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../../vp9e_hw/include )
foreach( dir ${cdirs} )
  include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../../shared/umc/codec/${dir}/include )
endforeach( )

set( sources "" )
set( sources.plus "" )
foreach( dir ${vdirs} )
  file( GLOB_RECURSE srcs "${dir}/src/*.c" "${dir}/src/*.cpp" )
  list( APPEND sources ${srcs})
endforeach()
foreach( prefix ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src )
  list( APPEND sources.plus
    ${prefix}/mfx_ddi_enc_dump.cpp
    ${prefix}/mfx_h264_enc_common_hw.cpp
    ${prefix}/mfx_h264_encode_vaapi.cpp
    ${prefix}/mfx_h264_fei_vaapi.cpp
    ${prefix}/mfx_h264_encode_factory.cpp
    ${prefix}/mfx_mpeg2_enc_common_hw.cpp
    ${prefix}/mfx_mpeg2_encode_factory.cpp
    ${prefix}/mfx_mpeg2_encode_vaapi.cpp
    )
endforeach()

list( APPEND sources.plus
    ${CMAKE_CURRENT_SOURCE_DIR}/../../hevce_hw/h265/src/export.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../hevce_hw/h265/src/mfx_h265_encode_hw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../hevce_hw/h265/src/mfx_h265_encode_hw_brc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../hevce_hw/h265/src/mfx_h265_encode_hw_bs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../hevce_hw/h265/src/mfx_h265_encode_hw_ddi.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../hevce_hw/h265/src/mfx_h265_encode_hw_ddi_trace.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../hevce_hw/h265/src/mfx_h265_encode_hw_par.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../hevce_hw/h265/src/mfx_h265_encode_hw_utils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../hevce_hw/h265/src/mfx_h265_encode_vaapi.cpp
    )

list( APPEND sources.plus
    ${CMAKE_CURRENT_SOURCE_DIR}/../../vp9e_hw/src/export.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../vp9e_hw/src/mfx_vp9_encode_hw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../vp9e_hw/src/mfx_vp9_encode_hw_ddi.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../vp9e_hw/src/mfx_vp9_encode_hw_par.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../vp9e_hw/src/mfx_vp9_encode_hw_utils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../vp9e_hw/src/mfx_vp9_encode_hw_vaapi.cpp
    )

set( LIBS "" )
  list( APPEND LIBS
  asc
  )

set( defs "${API_FLAGS} ${WARNING_FLAGS} ${defs}" )
make_library( encode hw static )
set( defs "" )
