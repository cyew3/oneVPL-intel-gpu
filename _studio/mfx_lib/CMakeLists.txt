set(MFX_ORIG_LDFLAGS "${MFX_LDFLAGS}" )

# =============================================================================

mfx_include_dirs( )

list( APPEND mdirs
  scheduler vpp encode_hw/mjpeg
  decode/h264 decode/h265 decode/vp8 decode/vp9 decode/av1 decode/mpeg2/hw decode/mpeg2/sw decode/mjpeg cmrt_cross_platform encode_hw/h264 encode_hw/h265 encode_hw/mpeg2 encode_hw/vp9
  decode/vc1 encode/h264 enc/mpeg2 pak/mpeg2 enc_hw/mpeg2 encode/mpeg2 encode/mjpeg
  audio_decode/aac audio_decode/mp3 audio_encode/aac genx/copy_kernels
  mctf_package/mctf
)

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/fei/h264_common )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/fei/h264_preenc )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/fei/h264_pak )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/fei/h264_enc )
foreach( dir ${mdirs} )
  include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/${dir}/include )
endforeach( )

list( APPEND cdirs
  brc h264_enc mpeg2_dec/hw mpeg2_dec/sw vp9_dec
  color_space_converter mpeg2_enc jpeg_enc vc1_dec vc1_common
  aac_dec common aac_common mp3_common mp3_dec aac_enc
  )
foreach( dir ${cdirs} )
  include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../shared/umc/codec/${dir}/include )
  include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../shared/asc/include )
endforeach( )

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../hevc_fei/h265_fei/include )

# =============================================================================

set(sources
    ${CMAKE_CURRENT_SOURCE_DIR}/scheduler/src/mfx_scheduler_core.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/scheduler/src/mfx_scheduler_core_ischeduler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/scheduler/src/mfx_scheduler_core_iunknown.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/scheduler/src/mfx_scheduler_core_task.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/scheduler/src/mfx_scheduler_core_task_management.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/scheduler/src/mfx_scheduler_core_thread.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/src/libmfxsw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/src/libmfxsw_async.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/src/libmfxsw_decode.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/src/libmfxsw_enc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/src/libmfxsw_encode.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/src/libmfxsw_pak.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/src/libmfxsw_plugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/src/libmfxsw_query.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/src/libmfxsw_session.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/src/libmfxsw_vpp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/src/mfx_check_hardware_support.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/src/mfx_session.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/src/mfx_user_plugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/src/mfx_brc_common.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/src/mfx_mpeg2_dec_common.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/src/mfx_vc1_dec_common.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/auxiliary_device.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/cm_mem_copy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/fast_compositing_ddi.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/mfx_vpp_vaapi.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/libmfx_allocator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/libmfx_allocator_vaapi.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/libmfx_allocator_vdaapi.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/libmfx_core.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/libmfx_core_factory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/libmfx_core_hw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/libmfx_core_vaapi.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/libmfx_core_vdaapi.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/mfx_umc_alloc_wrapper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/mfx_umc_mjpeg_vpp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/mfx_timing.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/mfx_dxva2_device.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/mfx_static_assert_structs.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/mfx_mfe_adapter.cpp
    )

if(NOT OPEN_SOURCE)
  list(APPEND sources
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/src/mfx_h264_enc_common.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/fast_copy.cpp
  )
endif()

set( sources.plus "" )

set( USE_STRICT_NAME TRUE )
set( MFX_LDFLAGS "${MFX_ORIG_LDFLAGS} -Wl,--version-script=${MSDK_LIB_ROOT}/libmfxhw.map" )

get_mfx_version(mfx_version_major mfx_version_minor)

if(OPEN_SOURCE)
  set(VARIANTS hw)
else()
  set(VARIANTS sw hw)
endif()

foreach( variant ${VARIANTS} )
  if( DEFINED MFX_LIBNAME )
    set( mfxlibname "${MFX_LIBNAME}")
  else()
    if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
      set( mfxlibname mfx${variant}64 )
    else()
      set( mfxlibname mfx${variant}32 )
    endif()
  endif()

  if(OPEN_SOURCE)
    set(LIBS
      bitrate_control
      umc
      vm
      vm_plus
      umc_va_hw
      cmrt_cross_platform_hw
      genx
      decode_${variant}
      encode_hw
      h264_la
      genx
      mfx_common_${variant}
      asc
      mctf_hw
      vpp_${variant}
      h264_common
      h264_preenc
      h264_enc
      h264_pak
      mfx_trace
      ${ITT_LIBRARIES}
      pthread
      dl
    )
  else()
    set(LIBS
      bitrate_control
      h264_enc_sw
      umc
      vm
      vm_plus
      umc_io
      umc_va_hw
      cmrt_cross_platform_hw
      genx
      decode_${variant}
      decode_ext_${variant}
      encode_hw
      enc_hw
      h264_la
      genx
      scene_analyzer
      color_space_converter_${variant}
      media_buffers
      encode
      vc1_enc
      jpeg_enc
      me
      mpeg2_enc
      enc
      h264_dispatcher
      h264_px
      h264_sse3
      pak
      mfx_common_${variant}
      asc
      mctf_hw
      vpp_${variant}
      h264_common
      h264_preenc
      h264_enc
      h264_pak
      mfx_trace
      ${ITT_LIBRARIES}
      ippmsdk_l ippj_l ippvc_l ippcc_l ippcv_l ippi_l ipps_l ippcore_l
      pthread
      dl
    )
  endif()

  set( defs "${API_FLAGS} ${WARNING_FLAGS} ${defs}" )
  make_library( ${mfxlibname} ${variant} shared)
  make_library( mfx${variant}_static ${variant} static)

  set_target_properties(${mfxlibname} PROPERTIES   VERSION ${mfx_version_major}.${mfx_version_minor})
  set_target_properties(${mfxlibname} PROPERTIES SOVERSION ${mfx_version_major})

  install(TARGETS ${mfxlibname} LIBRARY DESTINATION ${MFX_MODULES_DIR})

  set( PKG_CONFIG_FNAME "${CMAKE_LIB_DIR}/${CMAKE_BUILD_TYPE}/lib${mfxlibname}.pc")
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/pkg-config.pc.cmake" ${PKG_CONFIG_FNAME} @ONLY)
  install( FILES ${PKG_CONFIG_FNAME} DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig )


  set( defs "" )
endforeach()

include(sources_ext.cmake OPTIONAL)
