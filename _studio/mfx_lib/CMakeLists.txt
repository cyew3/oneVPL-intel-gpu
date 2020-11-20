# =============================================================================

list( APPEND mdirs
  scheduler/$<IF:$<PLATFORM_ID:Linux>,linux,windows> vpp encode_hw/mjpeg
  decode/h264 decode/h265 decode/vp8 decode/vp9 decode/av1 decode/mpeg2/hw decode/mpeg2/sw decode/mjpeg cmrt_cross_platform encode_hw/h264 encode_hw/h265 encode_hw/mpeg2 encode_hw/vp9
  decode/vc1 encode/h264 enc/mpeg2 pak/mpeg2 enc_hw/mpeg2 encode/mpeg2 encode/mjpeg
  audio_decode/aac audio_decode/mp3 audio_encode/aac genx/copy_kernels
  mctf_package/mctf
)

list( APPEND cdirs
  brc h264_enc mpeg2_dec/hw mpeg2_dec/sw vp9_dec
  color_space_converter mpeg2_enc jpeg_enc vc1_dec vc1_common
  aac_dec common aac_common mp3_common mp3_dec aac_enc
  )

# =============================================================================

set(sources_common
    scheduler/$<IF:$<PLATFORM_ID:Linux>,linux,windows>/src/mfx_scheduler_core.cpp
    scheduler/$<IF:$<PLATFORM_ID:Linux>,linux,windows>/src/mfx_scheduler_core_ischeduler.cpp
    scheduler/$<IF:$<PLATFORM_ID:Linux>,linux,windows>/src/mfx_scheduler_core_iunknown.cpp
    scheduler/$<IF:$<PLATFORM_ID:Linux>,linux,windows>/src/mfx_scheduler_core_task.cpp
    scheduler/$<IF:$<PLATFORM_ID:Linux>,linux,windows>/src/mfx_scheduler_core_task_management.cpp
    scheduler/$<IF:$<PLATFORM_ID:Linux>,linux,windows>/src/mfx_scheduler_core_thread.cpp

    shared/src/libmfxsw.cpp
    shared/src/libmfxsw_async.cpp
    shared/src/libmfxsw_decode.cpp
    shared/src/libmfxsw_enc.cpp
    shared/src/libmfxsw_encode.cpp
    shared/src/libmfxsw_pak.cpp
    shared/src/libmfxsw_plugin.cpp
    shared/src/libmfxsw_query.cpp
    shared/src/libmfxsw_session.cpp
    shared/src/libmfxsw_vpp.cpp
    shared/src/mfx_check_hardware_support.cpp
    shared/src/mfx_session.cpp
    shared/src/mfx_user_plugin.cpp

    shared/include/feature_blocks/mfx_feature_blocks_base.h
    shared/include/feature_blocks/mfx_feature_blocks_decl_blocks.h
    shared/include/feature_blocks/mfx_feature_blocks_init_macros.h
    shared/include/feature_blocks/mfx_feature_blocks_utils.h
    shared/include/mfx_utils_extbuf.h
    ${CMAKE_BINARY_DIR}/mfxconfig.h  # Cmake-generated configuration derived from BuildOptions
    ${MSDK_STUDIO_ROOT}/shared/include/mfx_config.h

    ${MSDK_STUDIO_ROOT}/shared/include/mfx_common.h

    ${MSDK_STUDIO_ROOT}/shared/include/cm_mem_copy.h
    ${MSDK_STUDIO_ROOT}/shared/include/dispatch_session.h
    ${MSDK_STUDIO_ROOT}/shared/include/fast_copy.h
    ${MSDK_STUDIO_ROOT}/shared/include/libmfx_allocator.h
    ${MSDK_STUDIO_ROOT}/shared/include/libmfx_core.h
    ${MSDK_STUDIO_ROOT}/shared/include/libmfx_core_factory.h
    ${MSDK_STUDIO_ROOT}/shared/include/libmfx_core_interface.h
    ${MSDK_STUDIO_ROOT}/shared/include/libmfx_core_operation.h
    ${MSDK_STUDIO_ROOT}/shared/include/mfxaudio++int.h
    ${MSDK_STUDIO_ROOT}/shared/include/mfxenctools-int.h
    ${MSDK_STUDIO_ROOT}/shared/include/mfxstructures-int.h
    ${MSDK_STUDIO_ROOT}/shared/include/mfxstructurespro.h
    ${MSDK_STUDIO_ROOT}/shared/include/mfxvideo++int.h
    ${MSDK_STUDIO_ROOT}/shared/include/mfx_cenc.h
    ${MSDK_STUDIO_ROOT}/shared/include/mfx_common.h
    ${MSDK_STUDIO_ROOT}/shared/include/mfx_config.h
    ${MSDK_STUDIO_ROOT}/shared/include/mfx_ext_buffers.h
    ${MSDK_STUDIO_ROOT}/shared/include/mfx_reflect.h
    ${MSDK_STUDIO_ROOT}/shared/include/mfx_timing.h
    ${MSDK_STUDIO_ROOT}/shared/include/mfx_trace.h
    ${MSDK_STUDIO_ROOT}/shared/include/mfx_umc_alloc_wrapper.h
    ${MSDK_STUDIO_ROOT}/shared/include/mfx_utils.h
    ${MSDK_STUDIO_ROOT}/shared/include/mfx_vpp_interface.h

    ${MSDK_STUDIO_ROOT}/shared/src/libmfx_allocator.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/libmfx_core.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/libmfx_core_factory.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/mfx_umc_alloc_wrapper.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/mfx_umc_mjpeg_vpp.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/mfx_timing.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/mfx_static_assert_structs.cpp

    $<$<PLATFORM_ID:Windows>:libmfx.def>
  )

set(sources_hw
    ${MSDK_STUDIO_ROOT}/shared/include/libmfx_allocator_vaapi.h
    ${MSDK_STUDIO_ROOT}/shared/include/libmfx_core_hw.h
    ${MSDK_STUDIO_ROOT}/shared/include/libmfx_core_vaapi.h
    ${MSDK_STUDIO_ROOT}/shared/include/mfx_ext_ddi.h
    ${MSDK_STUDIO_ROOT}/shared/include/mfx_mfe_adapter.h
    ${MSDK_STUDIO_ROOT}/shared/include/mfx_vpp_vaapi.h

    ${MSDK_STUDIO_ROOT}/shared/src/auxiliary_device.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/cm_mem_copy.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/fast_compositing_ddi.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/mfx_vpp_vaapi.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/libmfx_allocator_vaapi.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/libmfx_allocator_d3d9.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/libmfx_allocator_d3d11.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/libmfx_core_hw.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/libmfx_core_vaapi.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/libmfx_core_d3d9.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/libmfx_core_d3d11.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/d3d11_decode_accelerator.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/d3d11_video_processor.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/mfx_dxva2_device.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/mfx_mfe_adapter.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/mfx_mfe_adapter_dxva.cpp
  )

if(NOT MFX_DISABLE_SW_FALLBACK)
  list(APPEND sources_hw
    ${MSDK_STUDIO_ROOT}/shared/src/fast_copy.cpp
  )
endif()

get_mfx_version(mfx_version_major mfx_version_minor)

if(MFX_DISABLE_SW_FALLBACK)
  set(VARIANTS hw)
else()
  set(VARIANTS sw hw)
endif()

set(IPP_LIBS "")
if (MFX_BUNDLED_IPP)
  set(IPP_LIBS ipp)
else()
  set(IPP_LIBS IPP::msdk IPP::j IPP::vc IPP::cc IPP::cv IPP::i IPP::s IPP::core)
endif()

if(CMAKE_SYSTEM_NAME MATCHES Windows)
  set(lib_prefix "lib")
else()
  set(lib_prefix "")
endif()

foreach( variant ${VARIANTS} )
  if( DEFINED MFX_LIBNAME )
    set( mfxlibname "${MFX_LIBNAME}")
  else()
    if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
      set( mfxlibname ${lib_prefix}mfx${variant}64 )
    else()
      set( mfxlibname ${lib_prefix}mfx${variant}32 )
    endif()
  endif()

  add_library(${mfxlibname} SHARED)

  target_sources(${mfxlibname} PRIVATE ${sources_common})
 
  if (variant MATCHES hw)
    target_sources(${mfxlibname} PRIVATE ${sources_hw})
  endif()

  set (JPEG_SW_LIBS "")
  if (MFX_ENABLE_JPEG_SW_FALLBACK)
    set(JPEG_SW_LIBS encode encode_sw)
  endif ()

  set (AVC_FEI_LIBS "")
  if (MFX_ENABLE_H264_VIDEO_FEI_ENCODE)
    set(AVC_FEI_LIBS h264_preenc h264_enc h264_pak)
  endif ()

  set (SW_FALBACK_LIBS "")
  if (NOT MFX_DISABLE_SW_FALLBACK)
    set(SW_FALBACK_LIBS
      enc_hw
      h264_enc_sw
      scene_analyzer
      color_space_converter
      media_buffers
      encode
      vc1_enc
      me
      mpeg2_enc
      enc
      h264_dispatcher
      h264_px
      h264_sse3
      pak
      umc
      umc_io)
  endif ()

  set(HW_ONLY_LIBS "")
  if (variant MATCHES hw)
    set(HW_ONLY_LIBS umc_va_hw cmrt_cross_platform_hw enctools_hw mctf_hw asc genx)
  endif()

  set(LIBS
    bitrate_control
    ${JPEG_SW_LIBS}
    ${SW_FALBACK_LIBS}
    ${HW_ONLY_LIBS}
    decode_${variant}
    encode_${variant}
    ${AVC_FEI_LIBS}
    mfx_common_${variant} 
    vpp_${variant}
    mfx_trace
    mfx_shared_lib
    ${ITT_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    ${CMAKE_DL_LIBS}
    mfx_require_sse4_properties
    )


  if (variant MATCHES hw)
    target_compile_definitions(${mfxlibname}
      PRIVATE
        MFX_VA
    )
  endif()

  target_include_directories(${mfxlibname}
    PRIVATE
      scheduler/$<IF:$<PLATFORM_ID:Linux>,linux,windows>/include
      fei/include
    )

  target_link_options(${mfxlibname}
    PRIVATE
      $<$<PLATFORM_ID:Windows>:LINKER:/DEF:${MSDK_LIB_ROOT}/libmfx.def>
      $<$<PLATFORM_ID:Linux>:LINKER:--version-script=${MSDK_LIB_ROOT}/libmfxhw.map>
    )
  target_link_libraries(${mfxlibname} PRIVATE ${LIBS})

  set_target_properties(${mfxlibname} PROPERTIES   VERSION ${mfx_version_major}.${mfx_version_minor})
  set_target_properties(${mfxlibname} PROPERTIES SOVERSION ${mfx_version_major})

  install(TARGETS ${mfxlibname} LIBRARY DESTINATION ${MFX_MODULES_DIR})

  set( PKG_CONFIG_FNAME "${CMAKE_LIB_DIR}/${CMAKE_BUILD_TYPE}/lib${mfxlibname}.pc")
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/pkg-config.pc.cmake" ${PKG_CONFIG_FNAME} @ONLY)
  install( FILES ${PKG_CONFIG_FNAME} DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig )

endforeach()

include(sources_ext.cmake OPTIONAL)
