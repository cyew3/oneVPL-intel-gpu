list( APPEND decoders
  h264 mpeg2 vc1 mjpeg vp8
  )

list( APPEND encoders
  h264 mpeg2 vc1 mjpeg mvc svc
  )

foreach( dec ${decoders} )
  list( APPEND dirs
    brc/${dec} bsd/${dec} dec/${dec}
    )

  list( APPEND dirs_impl
    decode/${dec}
    )
endforeach( )

foreach( enc ${encoders} )
  list( APPEND dirs 
    enc/${enc} encode/${enc} pak/${enc}          
    )

  list( APPEND dirs_hw
    enc_hw/${enc} encode_hw/${enc}
    )
endforeach( )

list( APPEND dirs scheduler )
list( APPEND dirs_impl vpp genx/h264_encode )

list( APPEND umc_dirs
  mpeg2_enc h264_enc jpeg_enc
  h264_dec mpeg2_dec vc1_dec jpeg_dec
  vc1_common vc1_spl jpeg_common
  brc scene_analyzer color_space_converter jpeg_common
  )

# =============================================================================

mfx_include_dirs( )

foreach( dir ${umc_dirs} )
  include_directories( ${CMAKE_HOME_DIRECTORY}/_studio/shared/umc/codec/${dir}/include )
endforeach( )

foreach( dir ${dirs} ${dirs_impl} ${dirs_hw} )
  include_directories( ${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/${dir}/include )
endforeach( )

# =============================================================================

set( sources "" )
set( sources.plus "" )
foreach( dir ${dirs} )
  file( GLOB_RECURSE srcs "${dir}/*.c" "${dir}/*.cpp" )
  list( APPEND sources ${srcs})
endforeach()

make_library( mfx_lib_merged none static )

# =============================================================================

set( sources "" )
set( sources.plus "" )
foreach( dir ${dirs_impl} )
  file( GLOB_RECURSE srcs "${dir}/*.c" "${dir}/*.cpp" )
  list( APPEND sources ${srcs}) 
endforeach()
foreach( prefix ${CMAKE_CURRENT_SOURCE_DIR}/shared/src )
  list( APPEND sources
    ${prefix}/mfx_brc_common.cpp
    ${prefix}/mfx_common_int.cpp
    ${prefix}/mfx_enc_common.cpp
    ${prefix}/mfx_h264_enc_common.cpp
    ${prefix}/mfx_mpeg2_dec_common.cpp
    ${prefix}/mfx_vc1_dec_common.cpp
    ${prefix}/mfx_vc1_enc_common.cpp
    ${prefix}/mfx_common_decode_int.cpp
    )
endforeach()

make_library( mfx_lib_merged sw static )

foreach( dir ${dirs_hw} )
  file( GLOB_RECURSE srcs "${dir}/*.c" "${dir}/*.cpp" )
  list( APPEND sources.plus ${srcs})  
endforeach()
foreach( prefix ${CMAKE_CURRENT_SOURCE_DIR}/shared/src )
  list( APPEND sources.plus
    ${prefix}/mfx_ddi_enc_dump.cpp
    ${prefix}/mfx_h264_enc_common_hw.cpp
    ${prefix}/mfx_h264_encode_vaapi.cpp
    ${prefix}/mfx_h264_encode_factory.cpp
    ${prefix}/mfx_mpeg2_enc_common_hw.cpp
    ${prefix}/mfx_mpeg2_encode_factory.cpp
    )
endforeach()

make_library( mfx_lib_merged hw static )

# =============================================================================
 
set( sources "" )
set( sources.plus "" )
foreach( prefix ${CMAKE_CURRENT_SOURCE_DIR}/shared/src )
  list( APPEND sources
    ${prefix}/libmfxsw.cpp
    ${prefix}/libmfxsw_async.cpp
    ${prefix}/libmfxsw_brc.cpp
    ${prefix}/libmfxsw_decode.cpp
    ${prefix}/libmfxsw_enc.cpp
    ${prefix}/libmfxsw_encode.cpp
    ${prefix}/libmfxsw_pak.cpp
    ${prefix}/libmfxsw_plugin.cpp
    ${prefix}/libmfxsw_query.cpp
    ${prefix}/libmfxsw_session.cpp
    ${prefix}/libmfxsw_vpp.cpp
    ${prefix}/mfx_check_hardware_support.cpp
    ${prefix}/mfx_session.cpp
    ${prefix}/mfx_user_plugin.cpp
  )
endforeach()
foreach( prefix ${CMAKE_HOME_DIRECTORY}/_studio/shared/src )
  list( APPEND sources
    ${prefix}/auxiliary_device.cpp
    ${prefix}/fast_copy.cpp
    ${prefix}/fast_compositing_ddi.cpp
    ${prefix}/mfx_vpp_vaapi.cpp
    ${prefix}/libmfx_allocator.cpp
    ${prefix}/libmfx_allocator_vaapi.cpp
    ${prefix}/libmfx_core.cpp
    ${prefix}/libmfx_core_factory.cpp
    ${prefix}/libmfx_core_vaapi.cpp
    ${prefix}/libmfx_core_vdaapi.cpp
    ${prefix}/mfx_umc_alloc_wrapper.cpp
    ${prefix}/mfx_dxva2_device.cpp
  )
endforeach()

set( USE_STRICT_NAME TRUE )
set(MFX_LDFLAGS "${MFX_LDFLAGS} -Wl,--version-script=${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/libmfx.map" )
    
foreach( variant sw hw )
  if( __ARCH MATCHES intel64 )
    set( mfxlibname mfx${variant}64 )
  else()
    set( mfxlibname mfx${variant}32 )
  endif()
  if( __CONFIG MATCHES debug )
    set( mfxlibname ${mfxlibname}_d )
  endif()

  set( LIBS "" )
  list( APPEND LIBS
    mfx_lib_merged
    mfx_lib_merged_${variant}
    umc_codecs_merged_${variant}
    umc_codecs_merged
    umc_io_merged_${variant}
    umc_core_merged 
    ${ITT_LIBS}
    ippmsdk_l ippj_l ippvc_l ippcc_l ippcv_l ippi_l ipps_l ippcore_l
    pthread
    )

  make_library( ${mfxlibname} ${variant} shared)
  make_library( mfx${variant}_static ${variant} static)
endforeach()
