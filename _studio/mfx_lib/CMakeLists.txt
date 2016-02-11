set(MFX_ORIG_LDFLAGS "${MFX_LDFLAGS}" )

# =============================================================================

mfx_include_dirs( )

list( APPEND mdirs
  scheduler vpp brc/mpeg2 enc/mpeg2 pak/mpeg2 brc/h264 encode/h264 encode/mpeg2 encode/mjpeg decode/vc1
  decode/h264 decode/mpeg2 decode/mjpeg cmrt_cross_platform encode_hw/h264 encode_hw/mpeg2 enc_hw/mpeg2
  encode_hw/mjpeg audio_decode/aac audio_decode/mp3 audio_encode/aac
  )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/fei/h264_preenc ) 
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/fei/h264_pak )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/fei/h264_enc )
foreach( dir ${mdirs} )
  include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/${dir}/include )
endforeach( )

list( APPEND cdirs
  brc h264_enc vc1_dec mpeg2_enc vc1_common color_space_converter mpeg2_dec jpeg_enc
  aac_dec common aac_common mp3_common mp3_dec aac_enc
  )
foreach( dir ${cdirs} )
  include_directories( ${CMAKE_SOURCE_DIR}/shared/umc/codec/${dir}/include )
endforeach( )

# =============================================================================

set( sources "" )
set( sources.plus "" )
file( GLOB_RECURSE srcs "${CMAKE_CURRENT_SOURCE_DIR}/scheduler/src/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/scheduler/src/*.cpp" )
list( APPEND sources ${srcs})
# =============================================================================
 
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
    #=================
    ${prefix}/mfx_brc_common.cpp
    ${prefix}/mfx_h264_enc_common.cpp
    ${prefix}/mfx_mpeg2_dec_common.cpp
    ${prefix}/mfx_vc1_dec_common.cpp
    #=================
  )
endforeach()
foreach( prefix ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src )
  list( APPEND sources
    ${prefix}/auxiliary_device.cpp
    ${prefix}/cm_mem_copy.cpp
    ${prefix}/fast_copy.cpp
    ${prefix}/fast_compositing_ddi.cpp
    ${prefix}/mfx_vpp_vaapi.cpp
    ${prefix}/libmfx_allocator.cpp
    ${prefix}/libmfx_allocator_vaapi.cpp
    ${prefix}/libmfx_allocator_vdaapi.cpp
    ${prefix}/libmfx_core.cpp
    ${prefix}/libmfx_core_factory.cpp
    ${prefix}/libmfx_core_hw.cpp 
    ${prefix}/libmfx_core_vaapi.cpp
    ${prefix}/libmfx_core_vdaapi.cpp
    ${prefix}/mfx_umc_alloc_wrapper.cpp
    ${prefix}/mfx_timing.cpp
    ${prefix}/mfx_dxva2_device.cpp
    ${prefix}/mfx_static_assert_structs.cpp
  )
endforeach()

set( USE_STRICT_NAME TRUE )
set(MFX_LDFLAGS "${MFX_ORIG_LDFLAGS} -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/libmfx.map" )
    
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
    mfx_trace_hw
    bitrate_control
    color_space_converter_${variant}
    h264_dec_${variant}
    h264_enc_sw
    jpeg_common
    jpeg_dec_${variant}
    jpeg_enc
    me
    mpeg2_dec_${variant}
    mpeg2_enc
    scene_analyzer
    vc1_common
    vc1_dec_${variant}
    vc1_enc
    umc
    vm
    vm_plus
    media_buffers
    umc_io
    umc_va_elk_hw
    brc
    bsd
    cmrt_cross_platform_hw
    decode_${variant}
    dec
    encode
    encode_hw
    enc
    enc_hw
    h264_la
    genx_h264_encode_embeded
    h264_dispatcher
    h264_px
    h264_sse3
    pak
    mfx_common
    mfx_common_${variant} 
    vc1_shared
    vpp_${variant}
    h264_preenc
    h264_enc
    h264_pak
    ${ITT_LIBS}
    ippmsdk_l ippj_l ippvc_l ippcc_l ippcv_l ippi_l ipps_l ippcore_l
    pthread
    dl
  )

  set( defs "${defs} ${WARNING_FLAGS}")
  make_library( ${mfxlibname} ${variant} shared)
  make_library( mfx${variant}_static ${variant} static)
  set( defs "" )
  #message( STATUS "xxxxxxxxxxxxxxx __ARCH: "${__ARCH}, "mfxlibname: "${mfxlibname}, "variant: "${variant})
endforeach()

# =============================================================================

# libmfxrt reuse the same .map file as full library
set( sources "" )
set( sources.plus "" )

# mfxrt library itself
foreach( prefix ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src )
  list( APPEND sources
    ${prefix}/auxiliary_device.cpp
    ${prefix}/cm_mem_copy.cpp    
    ${prefix}/fast_compositing_ddi.cpp
    ${prefix}/fast_copy.cpp
    ${prefix}/libmfx_allocator.cpp
    ${prefix}/libmfx_allocator_vaapi.cpp
    ${prefix}/libmfx_core.cpp
    ${prefix}/libmfx_core_vaapi.cpp
    ${prefix}/libmfx_core_vdaapi.cpp    
    ${prefix}/libmfx_core_factory.cpp
    ${prefix}/libmfx_core_hw.cpp
    ${prefix}/mfx_dxva2_device.cpp    
    ${prefix}/mfx_search_dll.cpp
    ${prefix}/mfx_timing.cpp
    ${prefix}/mfx_umc_alloc_wrapper.cpp
    ${prefix}/mfx_vpp_vaapi.cpp
    ${prefix}/mfx_static_assert_structs.cpp
  )
endforeach()
list( APPEND sources
  ${CMAKE_CURRENT_SOURCE_DIR}/cmrt_cross_platform/src/cmrt_cross_platform.cpp
)

foreach( prefix ${CMAKE_CURRENT_SOURCE_DIR} )
  list( APPEND sources
    ${prefix}/shared/src/libmfxsw.cpp
    ${prefix}/shared/src/libmfxsw_async.cpp
    ${prefix}/shared/src/libmfxsw_decode.cpp
    ${prefix}/shared/src/libmfxsw_enc.cpp
    ${prefix}/shared/src/libmfxsw_encode.cpp
    ${prefix}/shared/src/libmfxsw_plugin.cpp
    ${prefix}/shared/src/libmfxsw_query.cpp
    ${prefix}/shared/src/libmfxsw_session.cpp
    ${prefix}/shared/src/libmfxsw_vpp.cpp
    ${prefix}/shared/src/mfx_session.cpp
    ${prefix}/shared/src/mfx_user_plugin.cpp    
    ${prefix}/shared/src/mfx_check_hardware_support.cpp    
    ${prefix}/scheduler/src/mfx_scheduler_core.cpp
    ${prefix}/scheduler/src/mfx_scheduler_core_ischeduler.cpp
    ${prefix}/scheduler/src/mfx_scheduler_core_iunknown.cpp
    ${prefix}/scheduler/src/mfx_scheduler_core_task.cpp
    ${prefix}/scheduler/src/mfx_scheduler_core_task_management.cpp
    ${prefix}/scheduler/src/mfx_scheduler_core_thread.cpp
    ${prefix}/vpp/src/mfx_vpp_factory.cpp
  )
endforeach()


if( __ARCH MATCHES intel64 )
  set( mfxrtlibname mfxrt64 )
else()
  set( mfxrtlibname mfxrt32 )
endif()
if( __CONFIG MATCHES debug )
  set( mfxrtlibname ${mfxrtlibname}_d )
endif()

set( LIBS "" )
list( APPEND LIBS
  media_buffers
  umc_io
  umc_va_elk_hw
  umc
  vm
  vm_plus
  mfx_common
  mfx_common_hw
  ${ITT_LIBS}
  ippmsdk_l ippj_l ippvc_l ippcc_l ippcv_l ippi_l ipps_l ippcore_l
  pthread
  dl )

set( defs "${defs} ${WARNING_FLAGS}")
make_library( ${mfxrtlibname} hw shared)
append_property( ${mfxrtlibname} COMPILE_FLAGS " -DMFX_RT" )  
set( defs "")

# =============================================================================
 
set( sources "" )
set( sources.plus "" )
foreach( prefix ${CMAKE_CURRENT_SOURCE_DIR}/shared/src )
  list( APPEND sources
    ${prefix}/libmfxsw.cpp
    ${prefix}/libmfxsw_async.cpp
    ${prefix}/libmfxsw_audio_decode.cpp
    ${prefix}/libmfxsw_audio_encode.cpp
    ${prefix}/libmfxsw_ext.cpp
    ${prefix}/libmfxsw_session.cpp
    ${prefix}/mfx_session.cpp
    #=====================
    ${prefix}/mfx_common_int.cpp
    #=====================
  )
endforeach()
foreach( prefix ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src )
  list( APPEND sources
    ${prefix}/fast_copy.cpp
    ${prefix}/libmfx_allocator.cpp
    ${prefix}/libmfx_core.cpp
    ${prefix}/libmfx_core_factory.cpp
    ${prefix}/mfx_umc_alloc_wrapper.cpp
    ${prefix}/mfx_search_dll.cpp
    ${prefix}/mfx_timing.cpp
    ${prefix}/mfx_static_assert_structs.cpp
  )
endforeach()
# =================================
file( GLOB_RECURSE srcs "${CMAKE_CURRENT_SOURCE_DIR}/scheduler/src/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/scheduler/src/*.cpp" )
list( APPEND sources ${srcs})
# =================================

set( USE_STRICT_NAME TRUE )
set(MFX_LDFLAGS "${MFX_ORIG_LDFLAGS} -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/libmfxaudio.map" )
    
foreach( variant sw )
  if( __ARCH MATCHES intel64 )
    set( mfxlibname mfxaudio${variant}64 )
  else()
    set( mfxlibname mfxaudio${variant}32 )
  endif()
  if( __CONFIG MATCHES debug )
    set( mfxlibname ${mfxlibname}_d )
  endif()

  set( LIBS "" )
  list( APPEND LIBS
    enc
    encode
    pak
    brc
    bsd
    dec
    decode_${variant}
    vpp_${variant}
    cmrt_cross_platform_hw
    genx_h264_encode_embeded
    audio_decode
    audio_encode
    umc_acodecs_merged
    umc_codecs_merged
    media_buffers
    umc_va_elk_hw
    umc_io
    umc
    vm
    vm_plus
    ${ITT_LIBS}
    ippcc_l ippdc_l ippac_l ippi_l ipps_l ippcore_l
    pthread
    dl
    )

  if(${variant} MATCHES hw)
    list( APPEND LIBS
    enc_hw
    encode_hw
    )
  endif()

  set( defs "${defs} ${WARNING_FLAGS}")
  make_library( ${mfxlibname} ${variant} shared)
  #make_library( mfxaudio${variant}_static ${variant} static)
  set( defs "")
endforeach()

# =============================================================================
