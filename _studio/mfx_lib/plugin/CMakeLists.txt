#
#
set(MFX_ORIG_LDFLAGS "${MFX_LDFLAGS}" )

set(HEVC_DECODER_HW_GUID   "33a61c0b4c27454ca8d85dde757c6f8e")
set(H264LA_ENCODER_GUID    "588f1185d47b42968dea377bb5d0dcb4")
set(VP8_DECODER_HW_GUID    "f622394d8d87452f878c51f2fc9b4131")
set(VP9_DECODER_HW_GUID    "a922394d8d87452f878c51f2fc9b4131")
# =============================================================================

mfx_include_dirs( )

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../../shared/asc/include )
include_directories( ${MSDK_LIB_ROOT}/scheduler/include )

include_directories( ${MSDK_LIB_ROOT}/decode/h265/include )
include_directories( ${MSDK_LIB_ROOT}/encode/h265/include )
include_directories( ${MSDK_LIB_ROOT}/fei/h264_preenc )
include_directories( ${MSDK_LIB_ROOT}/fei/include )
include_directories( ${MSDK_LIB_ROOT}/fei/h265/include )
include_directories( ${MSDK_LIB_ROOT}/fs_det/include )

list( APPEND umc_dirs
  common
  brc scene_analyzer
  mpeg2_dec/hw mpeg2_dec/sw h264_dec h264_enc
  vc1_common vc1_dec vc1_spl
  mpeg2_enc jpeg_enc color_space_converter

  h265_dec
  jpeg_common jpeg_dec )

foreach( dir ${umc_dirs} )
  include_directories( ${MSDK_STUDIO_ROOT}/shared/umc/codec/${dir}/include )
endforeach( )

list( APPEND vdecs
  h264 mpeg2/hw mpeg2/sw mjpeg vp8 vp9
  vc1
  )

list( APPEND vencs
  h264 mpeg2 mjpeg mvc svc
  )

foreach( dec ${vdecs} )

  list( APPEND vdirs_impl
    decode/${dec}
    )
endforeach( )

foreach( enc ${vencs} )
  list( APPEND vdirs
    enc/${enc} encode/${enc} pak/${enc}
    )

  list( APPEND vdirs_hw
    enc_hw/${enc}
    encode_hw/${enc}
    )
endforeach( )


list( APPEND vdirs_impl vpp cmrt_cross_platform genx/h264_encode genx/h265_encode genx/copy_kernels)

foreach( dir  ${vdirs} ${vdirs_impl} ${vdirs_hw} )
  include_directories( ${MSDK_LIB_ROOT}/${dir}/include )
endforeach( )

###################################################################################################################
# =============================================================================
# Plugins section
# =============================================================================

set( prefix ${MSDK_LIB_ROOT}/shared/src )
  list( APPEND plugin_common_sources
    ${prefix}/mfx_check_hardware_support.cpp
    ${prefix}/mfx_common_int.cpp
    ${prefix}/mfx_common_decode_int.cpp
    ${prefix}/libmfxsw.cpp
    ${prefix}/libmfxsw_async.cpp
    ${prefix}/libmfxsw_query.cpp
    ${prefix}/libmfxsw_session.cpp
    ${prefix}/mfx_session.cpp
    ${prefix}/mfx_user_plugin.cpp
    ${prefix}/mfx_enc_common.cpp
    ${prefix}/mfx_critical_error_handler.cpp
  )

set( prefix ${MSDK_STUDIO_ROOT}/shared/src )
  list( APPEND plugin_common_sources
    ${prefix}/cm_mem_copy.cpp
    ${prefix}/libmfx_allocator.cpp
    ${prefix}/libmfx_allocator_vaapi.cpp
    ${prefix}/libmfx_core.cpp
    ${prefix}/libmfx_core_factory.cpp
    ${prefix}/libmfx_core_hw.cpp
    ${prefix}/libmfx_core_vaapi.cpp
    ${prefix}/mfx_umc_alloc_wrapper.cpp
  )
if(NOT OPEN_SOURCE)
  list( APPEND plugin_common_sources
    ${prefix}/fast_copy.cpp
    )
endif()

list( APPEND plugin_common_sources
  ${MSDK_LIB_ROOT}/cmrt_cross_platform/src/cmrt_cross_platform.cpp
)

set( prefix ${MSDK_LIB_ROOT}/scheduler/src )
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
  set( defs "" )
  set( sources "" )
  set( sources.plus "" )
  set( LIBS "" )

  ## plugin info: name, description and version
  set (H264LA_PRODUCT_NAME "Intel(R) Media SDK")
  set( hw_H264LA_ENCODER_DESCRIPTION "Intel(R) Media SDK Advanced AVCe Plug-in" )

  if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
    set( plugin_name mfx_h264la_hw64 )
  else()
    set( plugin_name mfx_h264la_hw32 )
  endif()

  set_file_and_product_version( ${MEDIA_VERSION_STR} version_defs )

  ## include, source and libs
  list( APPEND sources ${plugin_common_sources} )

  set( prefix ${MSDK_LIB_ROOT}/shared/src )
  list( APPEND sources
    ${prefix}/mfx_enc_common.cpp
    ${prefix}/mfx_brc_common.cpp
    ${prefix}/mfx_h264_enc_common_hw.cpp
    ${prefix}/mfx_h264_encode_factory.cpp
    ${prefix}/mfx_h264_encode_vaapi.cpp
    ${prefix}/libmfxsw_enc.cpp
  )

  set( prefix ${MSDK_LIB_ROOT}/encode_hw/h264/src )
  list( APPEND sources
    ${prefix}/mfx_h264_encode_cm.cpp
    ${prefix}/mfx_h264_encode_hw_utils.cpp
    ${prefix}/mfx_h264_encode_hw_utils_new.cpp
    ${prefix}/mfx_h264_encode_hw.cpp
  )

  set( prefix ${CMAKE_CURRENT_SOURCE_DIR}/../../shared/umc/codec/brc/src/ )
  list( APPEND sources
    ${prefix}/umc_video_brc.cpp
    ${prefix}/umc_h264_brc.cpp
    ${prefix}/umc_brc.cpp
  )

  list( APPEND sources ${MSDK_LIB_ROOT}/cmrt_cross_platform/src/cmrt_cross_platform.cpp )
  list( APPEND sources ${MSDK_LIB_ROOT}/encode_hw/mvc/src/mfx_mvc_encode_hw.cpp )
  list( APPEND sources ${MSDK_LIB_ROOT}/fei/h264_la/mfx_h264_la.cpp )

  set( prefix ${MSDK_LIB_ROOT}/plugin/src )
  list( APPEND sources.plus
    ${prefix}/mfx_h264la_plugin.cpp
  )

  if(NOT OPEN_SOURCE)
    set(la_IPPLIBS ippmsdk_l ippj_l ippvc_l ippcc_l ippcv_l ippi_l ipps_l ippcore_l)
  endif()

  list( APPEND LIBS
    h264_la
    umc_va_hw
    umc
    vm
    vm_plus
    genx
    asc
    mfx_trace
    ${ITT_LIBRARIES}
    ${la_IPPLIBS}
    pthread
    dl
    )

  set( USE_STRICT_NAME TRUE )
  set(MFX_LDFLAGS "${MFX_ORIG_LDFLAGS} -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/libmfxsw_plugin.map" )
  set( defs "-DAS_H264LA_PLUGIN -DMFX_PLUGIN_PRODUCT_NAME=\"\\\"${H264LA_PRODUCT_NAME}\"\\\"" )
  set( defs "${defs} -DMFX_FILE_DESCRIPTION=\"\\\"${hw_H264LA_ENCODER_DESCRIPTION}\"\\\"" )
  set( defs "${defs} ${version_defs}" )
  set( defs "${API_FLAGS} ${WARNING_FLAGS} ${defs}" )
  gen_plugins_cfg("H264la_Encoder" ${H264LA_ENCODER_GUID} ${plugin_name} "04" "AVC")
  make_library( ${plugin_name} hw shared)
  install( TARGETS ${plugin_name} LIBRARY DESTINATION ${MFX_PLUGINS_DIR} )
endif()

###############################################################################
## HEVC Decoder                                                              ##
###############################################################################
if( DEFINED HEVC_DECODER_HW_GUID )
  set( defs "" )
  set( sources "" )
  set( sources.plus "" )
  set( LIBS "" )

  ## plugin info: name, description and version
  set( HEVC_PRODUCT_NAME "Intel(R) Media SDK" )
  set( hw_HEVC_DECODER_DESCRIPTION "Intel(R) Media SDK HEVC Hardware Decode Plug-in - Compatibility Stub" )
  
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set( plugin_name mfx_hevcd_hw64 )
  else()
    set( plugin_name mfx_hevcd_hw32 )
  endif()

  set_file_and_product_version( ${MEDIA_VERSION_STR} version_defs )

  list( APPEND sources
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_hevc_dec_plugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_stub_dec_plugin.cpp
  )

  ## defs
  set( USE_STRICT_NAME TRUE )
  set( MFX_LDFLAGS "${MFX_ORIG_LDFLAGS} -Wl,--version-script=${MSDK_LIB_ROOT}/plugin/libmfxsw_plugin.map" )
  set( defs "${defs} -DMFX_VA -DVA_HEVC_DECODER -DAS_HEVCD_PLUGIN" )
  set( defs "${defs} -DMFX_PLUGIN_PRODUCT_NAME=\"\\\"${HEVC_PRODUCT_NAME}\"\\\"" )
  set( defs "${defs} -DMFX_FILE_DESCRIPTION=\"\\\"${hw_HEVC_DECODER_DESCRIPTION}\"\\\"" )
  set( defs "${defs} ${version_defs}" )
  gen_plugins_cfg("HEVC_Decoder_HW" ${HEVC_DECODER_HW_GUID} ${plugin_name} "01" "HEVC")
  make_library( ${plugin_name} hw shared)
  install( TARGETS ${plugin_name} LIBRARY DESTINATION ${MFX_PLUGINS_DIR} )

  if(NOT OPEN_SOURCE)
    if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
      set( eval_plugin_name mfx_eval_hevcd_hw64 )
    else()
      set( eval_plugin_name mfx_eval_hevcd_hw32 )
    endif()

    set( defs "${defs} -DHEVCD_EVALUATION" )
    gen_plugins_cfg("HEVC_Decoder_HW" ${HEVC_DECODER_HW_GUID} ${eval_plugin_name} "01" "HEVC" "eval")
    make_library( ${eval_plugin_name} hw shared)

    install( TARGETS ${eval_plugin_name} LIBRARY DESTINATION ${MFX_PLUGINS_DIR} )
  endif()

endif()

# =============================================================================
# VP8
# =============================================================================
if( DEFINED VP8_DECODER_HW_GUID )
  ## cleanup
  set( defs "" )
  set( sources "" )
  set( sources.plus "" )
  set( LIBS "" )

  ## plugin info: name, description and version
  set (VP8_PRODUCT_NAME "Intel(R) Media SDK")
  set( hw_VP8_DECODER_DESCRIPTION "Intel(R) Media SDK VP8 Hardware Decode Plug-in - Compatibility Stub" )

  if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
    set( plugin_name mfx_vp8d_hw64 )
  else()
    set( plugin_name mfx_vp8d_hw32 )
  endif()

  set_file_and_product_version( ${MEDIA_VERSION_STR} version_defs )

  ## include, source and libs
  list( APPEND sources
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_vp8_dec_plugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_stub_dec_plugin.cpp
  )

  ## defs
  set( USE_STRICT_NAME TRUE )
  set(MFX_LDFLAGS "${MFX_ORIG_LDFLAGS} -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/libmfxsw_plugin.map" )
  set( defs "-DAS_VP8D_PLUGIN -DMFX_VA" )
  set( defs "${defs} -DMFX_PLUGIN_PRODUCT_NAME=\"\\\"${VP8_PRODUCT_NAME}\"\\\"" )
  set( defs "${defs} -DMFX_FILE_DESCRIPTION=\"\\\"${hw_VP8_DECODER_DESCRIPTION}\"\\\"" )
  set( defs "${defs} ${version_defs}")
  set( defs "${API_FLAGS} ${WARNING_FLAGS} ${defs}" )
  gen_plugins_cfg("VP8_Decoder_HW" ${VP8_DECODER_HW_GUID} ${plugin_name} "01" "VP8")
  make_library( ${plugin_name} hw shared)
  install( TARGETS ${plugin_name} LIBRARY DESTINATION ${MFX_PLUGINS_DIR} )
endif()

# =============================================================================
# VP9
# =============================================================================
if( DEFINED VP9_DECODER_HW_GUID )
  ## cleanup
  set( defs "" )
  set( sources "" )
  set( sources.plus "" )
  set( LIBS "" )

  ## plugin info: name, description and version
  set (VP9_PRODUCT_NAME "Intel(R) Media SDK")
  set( hw_VP9_DECODER_DESCRIPTION "Intel(R) Media SDK VP9 Hardware Decode Plug-in - Compatibility Stub" )

  if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
    set( plugin_name mfx_vp9d_hw64 )
  else()
    set( plugin_name mfx_vp9d_hw32 )
  endif()

  set_file_and_product_version( ${MEDIA_VERSION_STR} version_defs )

  ## include, source and libs
  list( APPEND sources
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_vp9_dec_plugin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_stub_dec_plugin.cpp
  )

  ## defs
  set( USE_STRICT_NAME TRUE )
  set(MFX_LDFLAGS "${MFX_ORIG_LDFLAGS} -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/libmfxsw_plugin.map" )
  set( defs "-DAS_VP9D_PLUGIN -DMFX_VA" )
  set( defs "${defs} -DMFX_PLUGIN_PRODUCT_NAME=\"\\\"${VP9_PRODUCT_NAME}\"\\\"" )
  set( defs "${defs} -DMFX_FILE_DESCRIPTION=\"\\\"${hw_VP9_DECODER_DESCRIPTION}\"\\\"" )
  set( defs "${defs} ${version_defs}")
  set( defs "${API_FLAGS} ${WARNING_FLAGS} ${defs}" )
  gen_plugins_cfg("VP9_Decoder_HW" ${VP9_DECODER_HW_GUID} ${plugin_name} "01" "VP9")
  make_library( ${plugin_name} hw shared)
  install( TARGETS ${plugin_name} LIBRARY DESTINATION ${MFX_PLUGINS_DIR} )
endif()

include(sources_ext.cmake OPTIONAL)
