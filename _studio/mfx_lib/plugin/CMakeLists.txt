#
#

if(${API_USE_VPL})
  return()
endif()

set(HEVC_DECODER_HW_GUID   "33a61c0b4c27454ca8d85dde757c6f8e")
set(H264LA_ENCODER_GUID    "588f1185d47b42968dea377bb5d0dcb4")
set(VP8_DECODER_HW_GUID    "f622394d8d87452f878c51f2fc9b4131")
set(VP9_DECODER_HW_GUID    "a922394d8d87452f878c51f2fc9b4131")
# =============================================================================

set(version_defs "")

set_file_and_product_version( ${MEDIA_VERSION_STR} version_defs )

add_library(mfx_plugin_traits INTERFACE)

target_link_options(mfx_plugin_traits
  INTERFACE
    $<$<PLATFORM_ID:Linux>:LINKER:--version-script=${CMAKE_CURRENT_SOURCE_DIR}/libmfxsw_plugin.map>
  )

target_link_libraries(mfx_plugin_traits
  INTERFACE
    umc_va_hw
    ${CMAKE_THREAD_LIBS_INIT}
    ${CMAKE_DL_LIBS}
)

set(include_dirs
  include
  ${MSDK_STUDIO_ROOT}/shared/asc/include
  ${MSDK_STUDIO_ROOT}/shared/include
  ${MSDK_LIB_ROOT}/shared/include
  ${MSDK_LIB_ROOT}/decode/h265/include
  ${MSDK_LIB_ROOT}/encode/h265/include
  ${MSDK_LIB_ROOT}/fei/h264_preenc
  ${MSDK_LIB_ROOT}/fei/include
  ${MSDK_LIB_ROOT}/fei/h265/include
  ${MSDK_LIB_ROOT}/fs_det/include
  ${MSDK_LIB_ROOT}/scheduler/$<IF:$<PLATFORM_ID:Linux>,linux,windows>/include
)


list( APPEND umc_dirs
  common
  brc scene_analyzer
  mpeg2_dec/hw mpeg2_dec/sw h264_dec h264_enc
  vc1_common vc1_dec vc1_spl
  mpeg2_enc jpeg_enc color_space_converter

  h265_dec
  jpeg_common jpeg_dec )

foreach( dir ${umc_dirs} )
  list(APPEND include_dirs ${MSDK_UMC_ROOT}/codec/${dir}/include)
endforeach( )

list(APPEND vdecs h264 mpeg2/hw mpeg2/sw mjpeg vp8 vp9 vc1)

list( APPEND vencs h264 mpeg2 mjpeg mvc svc)

foreach( dec ${vdecs} )
  list(APPEND vdirs_impl decode/${dec})
endforeach( )

foreach( enc ${vencs} )
  list(APPEND vdirs  enc/${enc} encode/${enc} pak/${enc})
  list(APPEND vdirs_hw enc_hw/${enc} encode_hw/${enc})
endforeach( )


list( APPEND vdirs_impl vpp cmrt_cross_platform genx/h264_encode genx/h265_encode genx/copy_kernels)

foreach( dir  ${vdirs} ${vdirs_impl} ${vdirs_hw} )
  list(APPEND include_dirs  ${MSDK_LIB_ROOT}/${dir}/include )
endforeach( )

###################################################################################################################
# =============================================================================
# Plugins section
# =============================================================================
set(plugin_common_sources "")

set( prefix ${MSDK_LIB_ROOT}/shared/src )
  list( APPEND plugin_common_sources
    ${prefix}/mfx_check_hardware_support.cpp
    ${prefix}/mfx_common_int.cpp
    ${prefix}/mfx_common_decode_int.cpp
    ${prefix}/libmfxsw.cpp
    ${prefix}/libmfxsw_async.cpp
    ${prefix}/libmfxsw_query.cpp
    ${prefix}/libmfxsw_session.cpp
    ${prefix}/libmfxsw_encode.cpp
    ${prefix}/mfx_session.cpp
    ${prefix}/mfx_user_plugin.cpp
    ${prefix}/mfx_enc_common.cpp
    ${prefix}/mfx_critical_error_handler.cpp
    ${prefix}/mfx_h264_encode_d3d_common.cpp
    ${prefix}/mfx_win_event_cache.cpp
  )

set( prefix ${MSDK_STUDIO_ROOT}/shared/src )
  list( APPEND plugin_common_sources
    ${prefix}/cm_mem_copy.cpp
    ${prefix}/fast_compositing_ddi.cpp
    ${prefix}/libmfx_allocator.cpp
    ${prefix}/libmfx_allocator_vaapi.cpp
    ${prefix}/libmfx_allocator_d3d9.cpp
    ${prefix}/libmfx_allocator_d3d11.cpp
    ${prefix}/libmfx_core.cpp
    ${prefix}/libmfx_core_factory.cpp
    ${prefix}/libmfx_core_hw.cpp
    ${prefix}/libmfx_core_vaapi.cpp
    ${prefix}/libmfx_core_d3d9.cpp
    ${prefix}/libmfx_core_d3d11.cpp
    ${prefix}/mfx_dxva2_device.cpp
    ${prefix}/d3d11_decode_accelerator.cpp
    ${prefix}/d3d11_video_processor.cpp
    ${prefix}/auxiliary_device.cpp
    ${prefix}/mfx_umc_alloc_wrapper.cpp
  )
if(NOT MFX_DISABLE_SW_FALLBACK)
  list( APPEND plugin_common_sources
    ${prefix}/fast_copy.cpp
    )
endif()

list( APPEND plugin_common_sources
  ${MSDK_LIB_ROOT}/cmrt_cross_platform/src/cmrt_cross_platform.cpp
  ${MSDK_LIB_ROOT}/encode_hw/h264/src/mfx_h264_encode_hw.cpp
  ${MSDK_LIB_ROOT}/encode_hw/hevc/agnostic/base/hevcehw_base_impl.cpp
  ${MSDK_LIB_ROOT}/encode_hw/hevc/agnostic/base/hevcehw_base_recon_info.cpp
)

set( prefix ${MSDK_LIB_ROOT}/scheduler/$<IF:$<PLATFORM_ID:Linux>,linux,windows>/src )

list( APPEND plugin_common_sources
    ${prefix}/mfx_scheduler_core.cpp
    ${prefix}/mfx_scheduler_core_iunknown.cpp
    ${prefix}/mfx_scheduler_core_ischeduler.cpp
    ${prefix}/mfx_scheduler_core_task.cpp
    ${prefix}/mfx_scheduler_core_task_management.cpp
    ${prefix}/mfx_scheduler_core_thread.cpp
  )

###############################################################################
## 1toN Lookahead (H264 LA)                                                  ##
###############################################################################

if( DEFINED H264LA_ENCODER_GUID )
  ## cleanup
  set( sources "" )

  ## plugin info: name, description and version
  set (H264LA_PRODUCT_NAME "Intel(R) Media SDK")
  set( hw_H264LA_ENCODER_DESCRIPTION "Intel(R) Media SDK Advanced AVCe Plug-in" )

  if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
    set( plugin_name mfx_h264la_hw64 )
  else()
    set( plugin_name mfx_h264la_hw32 )
  endif()

  ## include, source and libs
  list( APPEND sources ${plugin_common_sources} )

  set( prefix ${MSDK_LIB_ROOT}/shared/src )
  list( APPEND sources
    ${prefix}/mfx_enc_common.cpp
    ${prefix}/mfx_brc_common.cpp
    ${prefix}/mfx_h264_enc_common_hw.cpp
    ${prefix}/mfx_h264_encode_factory.cpp
    ${prefix}/mfx_h264_encode_vaapi.cpp
    ${prefix}/mfx_h264_encode_d3d11.cpp
    ${prefix}/mfx_h264_encode_d3d9.cpp
    ${prefix}/libmfxsw_enc.cpp
  )

  set( prefix ${MSDK_LIB_ROOT}/encode_hw/h264/src )
  list( APPEND sources
    ${prefix}/mfx_h264_encode_cm.cpp
    ${prefix}/mfx_h264_encode_hw_utils.cpp
    ${prefix}/mfx_h264_encode_hw_utils_new.cpp
    ${prefix}/mfx_h264_encode_hw.cpp
  )

  set( prefix ${MSDK_STUDIO_ROOT}/shared/umc/codec/brc/src/ )
  list( APPEND sources
    ${prefix}/umc_video_brc.cpp
    ${prefix}/umc_h264_brc.cpp
    ${prefix}/umc_brc.cpp
  )

  list( APPEND sources ${MSDK_LIB_ROOT}/cmrt_cross_platform/src/cmrt_cross_platform.cpp )
  list( APPEND sources ${MSDK_LIB_ROOT}/encode_hw/mvc/src/mfx_mvc_encode_hw.cpp )
  list( APPEND sources ${MSDK_LIB_ROOT}/vpp/src/mfx_vpp_factory.cpp )
  list( APPEND sources ${MSDK_LIB_ROOT}/fei/h264_la/mfx_h264_la.cpp )
  list( APPEND sources ${MSDK_STUDIO_ROOT}/shared/umc/core/umc/src/umc_default_memory_allocator.cpp )
  list( APPEND sources ${MSDK_LIB_ROOT}/encode_hw/svc/src/mfx_svc_encode_hw.cpp )
  list( APPEND sources ${MSDK_LIB_ROOT}/encode_hw/svc/src/mfx_svc_encode_simulcast_over_avc.cpp )

  set( prefix ${MSDK_LIB_ROOT}/plugin/src )
  list( APPEND sources
    ${prefix}/mfx_h264la_plugin.cpp
  )

  add_library(${plugin_name} SHARED ${sources})

  gen_plugins_cfg("H264la_Encoder" ${H264LA_ENCODER_GUID} ${plugin_name} "04" "AVC")

  install( TARGETS ${plugin_name} LIBRARY DESTINATION ${MFX_PLUGINS_DIR} )

  target_include_directories(${plugin_name}
    PRIVATE
      ${include_dirs}
      ${MSDK_LIB_ROOT}/fei/h264_la
    )

  target_compile_definitions(${plugin_name}
    PRIVATE
      AS_H264LA_PLUGIN
      MFX_VA
      MFX_PLUGIN_PRODUCT_NAME=\"${H264LA_PRODUCT_NAME}\"
      MFX_FILE_DESCRIPTION=\"${hw_H264LA_ENCODER_DESCRIPTION}\"
      ${version_defs}
    )

  if(NOT MFX_DISABLE_SW_FALLBACK)
    if (MFX_BUNDLED_IPP)
      set(la_IPPLIBS ipp)
    else()
      set(la_IPPLIBS IPP::msdk IPP::j IPP::vc IPP::cc IPP::cv IPP::i IPP::s IPP::core)
    endif()
  else()
      set(la_IPPLIBS IPP::msdk IPP::j IPP::vc IPP::cc IPP::cv IPP::i IPP::s IPP::core)
  endif()

  target_link_libraries(${plugin_name}
    PRIVATE
      lpla
      genx
      asc
      $<$<BOOL:${MFX_ENABLE_USER_ENCTOOLS}>:enctools_hw>
      mctf_hw
      vm_plus
      mfx_trace
      mfx_plugin_properties
      mfx_static_lib
      mfx_shared_lib
      ${ITT_LIBRARIES}
      ${la_IPPLIBS}
      Threads::Threads
      mfx_require_sse4_properties
    )

set_property(TARGET ${plugin_name} PROPERTY FOLDER "plugins")

endif()

###############################################################################
## HEVC Decoder                                                              ##
###############################################################################
if( DEFINED HEVC_DECODER_HW_GUID )

  set( sources "" )

  ## plugin info: name, description and version
  set( HEVC_PRODUCT_NAME "Intel(R) Media SDK" )
  set( hw_HEVC_DECODER_DESCRIPTION "Intel(R) Media SDK HEVC Hardware Decode Plug-in - Compatibility Stub" )

  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set( plugin_name mfx_hevcd_hw64 )
  else()
    set( plugin_name mfx_hevcd_hw32 )
  endif()

  list( APPEND sources
    src/mfx_hevc_dec_plugin.cpp
    src/mfx_stub_dec_plugin.cpp
  )

  add_library(${plugin_name} SHARED ${sources})

  ## defs
  gen_plugins_cfg("HEVC_Decoder_HW" ${HEVC_DECODER_HW_GUID} ${plugin_name} "01" "HEVC")

  install( TARGETS ${plugin_name} LIBRARY DESTINATION ${MFX_PLUGINS_DIR} )

  target_include_directories(${plugin_name} PRIVATE ${include_dirs})

  target_compile_definitions(${plugin_name}
    PRIVATE
      ${API_FLAGS}
      ${WARNING_FLAGS}
      MFX_VA
      VA_HEVC_DECODER
      AS_HEVCD_PLUGIN
      MFX_PLUGIN_PRODUCT_NAME=\"${HEVC_PRODUCT_NAME}\"
      MFX_FILE_DESCRIPTION=\"${hw_HEVC_DECODER_DESCRIPTION}\"
      ${version_defs}
    )

  target_link_libraries(${plugin_name} PRIVATE
    mfx_plugin_properties
  )

  set_property(TARGET ${plugin_name} PROPERTY FOLDER "plugins")

  if (0)
    if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
      set( eval_plugin_name mfx_eval_hevcd_hw64 )
    else()
      set( eval_plugin_name mfx_eval_hevcd_hw32 )
    endif()

    gen_plugins_cfg("HEVC_Decoder_HW" ${HEVC_DECODER_HW_GUID} ${eval_plugin_name} "01" "HEVC" "eval")
    add_library( ${eval_plugin_name} SHARED ${sources})

    install( TARGETS ${eval_plugin_name} LIBRARY DESTINATION ${MFX_PLUGINS_DIR} )

    target_include_directories(${eval_plugin_name} PRIVATE ${include_dirs})

    target_compile_definitions(${eval_plugin_name}
      PRIVATE
        ${API_FLAGS}
        ${WARNING_FLAGS}
        MFX_VA
        VA_HEVC_DECODER
        AS_HEVCD_PLUGIN
        HEVCD_EVALUATION
        MFX_PLUGIN_PRODUCT_NAME=\"${HEVC_PRODUCT_NAME}\"
        MFX_FILE_DESCRIPTION=\"${hw_HEVC_DECODER_DESCRIPTION}\"
        ${version_defs}
      )

    target_link_libraries(${eval_plugin_name}
      PRIVATE
        mfx_plugin_properties
    )

    set_property(TARGET ${eval_plugin_name} PROPERTY FOLDER "plugins")

  endif()

endif()

# =============================================================================
# VP8
# =============================================================================
if( DEFINED VP8_DECODER_HW_GUID )

  ## cleanup
  set( sources "" )

  ## include, source and libs
  list( APPEND sources ${plugin_common_sources} )


  ## plugin info: name, description and version
  set (VP8_PRODUCT_NAME "Intel(R) Media SDK")
  set( hw_VP8_DECODER_DESCRIPTION "Intel(R) Media SDK VP8 Hardware Decode Plug-in - Compatibility Stub" )

  if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
    set( plugin_name mfx_vp8d_hw64 )
  else()
    set( plugin_name mfx_vp8d_hw32 )
  endif()

  gen_plugins_cfg("VP8_Decoder_HW" ${VP8_DECODER_HW_GUID} ${plugin_name} "01" "VP8")

  add_library(${plugin_name} SHARED
    src/mfx_vp8_dec_plugin.cpp
    src/mfx_stub_dec_plugin.cpp
    )

  install( TARGETS ${plugin_name} LIBRARY DESTINATION ${MFX_PLUGINS_DIR} )

  target_include_directories(${plugin_name} PRIVATE ${include_dirs})

  target_compile_definitions(${plugin_name}
    PRIVATE
      ${API_FLAGS}
      ${WARNING_FLAGS}
      MFX_VA
      AS_VP8D_PLUGIN
      MFX_PLUGIN_PRODUCT_NAME=\"${VP8_PRODUCT_NAME}\"
      MFX_FILE_DESCRIPTION=\"${hw_VP8_DECODER_DESCRIPTION}\"
      ${version_defs}
    )

  target_link_libraries(${plugin_name} PRIVATE
    mfx_plugin_properties
  )

  set_property(TARGET ${plugin_name} PROPERTY FOLDER "plugins")

  install( TARGETS ${plugin_name} LIBRARY DESTINATION ${MFX_PLUGINS_DIR} )
endif()

# =============================================================================
# VP9
# =============================================================================
if( DEFINED VP9_DECODER_HW_GUID )
  ## cleanup
  set( sources "" )

  ## include, source and libs
  list( APPEND sources ${plugin_common_sources} )


  ## plugin info: name, description and version
  set (VP9_PRODUCT_NAME "Intel(R) Media SDK")
  set( hw_VP9_DECODER_DESCRIPTION "Intel(R) Media SDK VP9 Hardware Decode Plug-in - Compatibility Stub" )

  if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
    set( plugin_name mfx_vp9d_hw64 )
  else()
    set( plugin_name mfx_vp9d_hw32 )
  endif()

  add_library(${plugin_name} SHARED
    src/mfx_vp9_dec_plugin.cpp
    src/mfx_stub_dec_plugin.cpp
    )

  ## defs
  gen_plugins_cfg("VP9_Decoder_HW" ${VP9_DECODER_HW_GUID} ${plugin_name} "01" "VP9")

  install( TARGETS ${plugin_name} LIBRARY DESTINATION ${MFX_PLUGINS_DIR} )

  target_include_directories(${plugin_name} PRIVATE ${include_dirs})

  target_compile_definitions(${plugin_name}
    PRIVATE
      ${API_FLAGS}
      ${WARNING_FLAGS}
      MFX_VA
      AS_VP9D_PLUGIN
      MFX_PLUGIN_PRODUCT_NAME=\"${VP9_PRODUCT_NAME}\"
      MFX_FILE_DESCRIPTION=\"${hw_VP9_DECODER_DESCRIPTION}\"
      ${version_defs}
    )

  target_link_libraries(${plugin_name} PRIVATE
    mfx_plugin_properties
  )

  set_property(TARGET ${plugin_name} PROPERTY FOLDER "plugins")

  install( TARGETS ${plugin_name} LIBRARY DESTINATION ${MFX_PLUGINS_DIR} )
endif()

include(sources_ext.cmake OPTIONAL)
