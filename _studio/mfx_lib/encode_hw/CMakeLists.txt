set(MFX_ORIG_LDFLAGS "${MFX_LDFLAGS}" )

list( APPEND vdirs
  h265 h264 mjpeg mpeg2 mvc svc vp9 hevc
  )
list( APPEND cdirs
  mpeg2_enc jpeg_common brc color_space_converter h264_enc
  )
list( APPEND hevcdir
  hevc/agnostic
  hevc/agnostic/base
  hevc/agnostic/g12
  hevc/linux
  hevc/linux/base
  hevc/linux/g12
  )
if( NOT STRIP_EMABRGO )
    list( APPEND hevcdir
        hevc/embargo/agnostic
        hevc/embargo/agnostic/base
        hevc/embargo/agnostic/g11lkf
        hevc/embargo/agnostic/g12
        hevc/embargo/agnostic/g12ats
        hevc/embargo/agnostic/g12dg2
        hevc/embargo/linux
        hevc/embargo/linux/base
        hevc/embargo/linux/g11lkf
        hevc/embargo/linux/g12
        hevc/embargo/linux/g12ats
        hevc/embargo/linux/g12dg2
        )
endif()
mfx_include_dirs( )
foreach( dir ${vdirs} )
  include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/${dir}/include )
endforeach()
foreach( dir ${hevcdir} )
  include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../encode_hw/${dir} )
endforeach()
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../encode_hw/shared )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../shared/include)
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../../shared/asc/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../cmrt_cross_platform/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../enc_hw/mpeg2/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../pak/mpeg2/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../genx/h264_encode/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../fei/h264_preenc )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../../hevce_hw/h265/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../../hevc_fei/h265_fei/include )
foreach( dir ${cdirs} )
  include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../../shared/umc/codec/${dir}/include )
endforeach( )

set( sources "" )
set( sources.plus "" )

foreach( dir ${hevcdir} )
  file( GLOB_RECURSE srcs "${dir}/*.cpp" )
  list( APPEND sources.plus ${srcs})
endforeach()
list( APPEND sources.plus
    hevc/hevcehw_disp.cpp
    shared/ehw_resources_pool.cpp
    shared/ehw_task_manager.cpp
    shared/ehw_device_vaapi.cpp
    shared/ehw_utils_vaapi.cpp
    )

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

set( LIBS "" )
  list( APPEND LIBS
  asc
  )

set( defs "${API_FLAGS} ${WARNING_FLAGS} ${defs}" )
make_library( encode hw static )
set( defs "" )
