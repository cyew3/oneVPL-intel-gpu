#
#
set(MFX_ORIG_LDFLAGS "${MFX_LDFLAGS}" )

set(HEVC_DECODER_HW_GUID   "33a61c0b4c27454ca8d85dde757c6f8e")
set(HEVC_DECODER_SW_GUID   "15dd936825ad475ea34e35f3f54217a6")
set(HEVC_ENCODER_GACC_GUID "e5400a06c74d41f5b12d430bbaa23d0b")
set(HEVC_ENCODER_SW_GUID   "2fca99749fdb49aeb121a5b63ef568f7")
set(H264LA_ENCODER_GUID    "588f1185d47b42968dea377bb5d0dcb4")
set(H265FEI_ENCODER_GUID   "87e0e80207375240852515cf4a5edde6")
set(VP8_DECODER_HW_GUID    "f622394d8d87452f878c51f2fc9b4131")
set(VP8_DECODER_SW_GUID    "bca276ec7a854a598c30e0dc5748f1f6")
set(VP9_DECODER_HW_GUID    "a922394d8d87452f878c51f2fc9b4131")
set(VP9_DECODER_SW_GUID    "a9a276ec7a854a598c30e0dc5748f1f6")
# =============================================================================

mfx_include_dirs( )

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/include )

include_directories( ${MSDK_LIB_ROOT}/decode/h265/include )
include_directories( ${MSDK_LIB_ROOT}/encode/h265/include )
include_directories( ${MSDK_LIB_ROOT}/fei/h264_preenc )
include_directories( ${MSDK_LIB_ROOT}/fei/include )
include_directories( ${MSDK_LIB_ROOT}/fei/h265/include )
include_directories( ${MSDK_LIB_ROOT}/fs_det/include )

list( APPEND umc_dirs
  common
  brc scene_analyzer
  mpeg2_dec h264_dec h264_enc
  vc1_common vc1_dec vc1_spl                       ## open-source-cut-line
  mpeg2_enc jpeg_enc color_space_converter         ## open-source-cut-line
  h265_dec
  jpeg_common jpeg_dec )

if ( ENABLE_HEVC )
  message( STATUS "Building HEVC with software libraries: SW Decoder & Encoder, GACC." )
else( )
  message( STATUS "Building HEVC with only hardware libraries." )
endif( )

foreach( dir ${umc_dirs} )
  include_directories( ${MSDK_STUDIO_ROOT}/shared/umc/codec/${dir}/include )
endforeach( )

list( APPEND vdecs
  h264 mpeg2 mjpeg vp8 vp9
  vc1                       ## open-source-cut-line
  )

list( APPEND vencs
  h264 mpeg2 mjpeg mvc svc
  )

foreach( dec ${vdecs} )
## open-source-cut-start
  list( APPEND vdirs
    brc/${dec}
    )
## open-source-cut-end

  list( APPEND vdirs_impl
    decode/${dec}
    )
endforeach( )

foreach( enc ${vencs} )
## open-source-cut-start
  list( APPEND vdirs
    enc/${enc} encode/${enc} pak/${enc}
    )
## open-source-cut-end
  list( APPEND vdirs_hw
    enc_hw/${enc}        ## open-source-cut-line
    encode_hw/${enc}
    )
endforeach( )


list( APPEND vdirs_impl vpp cmrt_cross_platform genx/h264_encode genx/h265_encode)

foreach( dir  ${vdirs} ${vdirs_impl} ${vdirs_hw} )
  include_directories( ${MSDK_LIB_ROOT}/${dir}/include )
endforeach( )

###################################################################################################################
# =============================================================================
# Plugins section
# =============================================================================

foreach( prefix ${MSDK_LIB_ROOT}/shared/src )
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

endforeach()

foreach( prefix ${MSDK_STUDIO_ROOT}/shared/src )
  list( APPEND plugin_common_sources
    ${prefix}/cm_mem_copy.cpp
    ${prefix}/fast_copy.cpp
    ${prefix}/libmfx_allocator.cpp
    ${prefix}/libmfx_allocator_vaapi.cpp
    ${prefix}/libmfx_core.cpp
    ${prefix}/libmfx_core_factory.cpp
    ${prefix}/libmfx_core_hw.cpp
    ${prefix}/libmfx_core_vaapi.cpp
    ${prefix}/mfx_umc_alloc_wrapper.cpp
  )
endforeach()
list( APPEND plugin_common_sources
  ${MSDK_LIB_ROOT}/cmrt_cross_platform/src/cmrt_cross_platform.cpp
)

foreach( prefix ${MSDK_LIB_ROOT}/scheduler/src )
  list( APPEND plugin_common_sources
    ${prefix}/mfx_scheduler_core.cpp
    ${prefix}/mfx_scheduler_core_iunknown.cpp
    ${prefix}/mfx_scheduler_core_ischeduler.cpp
    ${prefix}/mfx_scheduler_core_task.cpp
    ${prefix}/mfx_scheduler_core_task_management.cpp
    ${prefix}/mfx_scheduler_core_thread.cpp
  )
endforeach()

# HEVC Common Part

if( NOT DEFINED ENV{MFX_HEVC_VERSION} )
  set( hevc_version 0.0.000.0000 )
else( )
  set( hevc_version $ENV{MFX_HEVC_VERSION} )
endif( )

if( __TARGET_PLATFORM STREQUAL "BXTMIN" OR __TARGET_PLATFORM STREQUAL "BXT" )
  set (HEVC_PRODUCT_NAME "Intel(R) Media SDK 2017 for Embedded Linux")
  set (sw_HEVC_DECODER_DESCRIPTION "Intel(R) Media SDK 2017 for Embedded Linux - HEVC Software Decode Plug-in")
  set (hw_HEVC_DECODER_DESCRIPTION "Intel(R) Media SDK 2017 for Embedded Linux - HEVC Hardware Decode Plug-in")
  set (sw_HEVC_ENCODER_DESCRIPTION "Intel(R) Media SDK 2017 for Embedded Linux - HEVC Software Encode Plug-in")
  set (hw_HEVC_ENCODER_DESCRIPTION "Intel(R) Media SDK 2017 for Embedded Linux - HEVC GPU Accelerated Encode Plug-in")
else()
  set (HEVC_PRODUCT_NAME "Intel(R) Media Server Studio 2017 - HEVC Decode and Encode")
  set (sw_HEVC_DECODER_DESCRIPTION "Intel(R) Media Server Studio 2017 - HEVC Software Decode Plug-in")
  set (hw_HEVC_DECODER_DESCRIPTION "Intel(R) Media Server Studio 2017 - HEVC Hardware Decode Plug-in")
  set (sw_HEVC_ENCODER_DESCRIPTION "Intel(R) Media Server Studio 2017 - HEVC Software Encode Plug-in")
  set (hw_HEVC_ENCODER_DESCRIPTION "Intel(R) Media Server Studio 2017 - HEVC GPU Accelerated Encode Plug-in")
endif()

  if( Linux OR Darwin )
    execute_process(
      COMMAND echo
      COMMAND cut -f 1 -d.
      COMMAND date "+.%-y.%-m.%-d"
      OUTPUT_VARIABLE cur_date
      OUTPUT_STRIP_TRAILING_WHITESPACE
      )
    string( SUBSTRING ${hevc_version} 0 1 ver )

    set(hevc_version_defs  "-DMFX_PLUGIN_FILE_VERSION=\"\\\"${ver}${cur_date}\"\\\" -DMFX_PLUGIN_PRODUCT_VERSION=\"\\\"${hevc_version}\"\\\"")
  endif( )

set( sources "" )
set( sources.plus "" )

list( APPEND sources ${plugin_common_sources} )

include_directories( ${MSDK_LIB_ROOT}/scheduler/include )

list( APPEND sources ${MSDK_LIB_ROOT}/shared/src/libmfxsw_decode.cpp )
list( APPEND sources ${MSDK_LIB_ROOT}/decode/h265/src/mfx_h265_dec_decode.cpp )

set( sources.plus "" )
foreach( prefix ${CMAKE_CURRENT_SOURCE_DIR}/src )
  list( APPEND sources.plus
    ${prefix}/mfx_hevc_dec_plugin.cpp
  )
endforeach()

set( USE_STRICT_NAME TRUE )
set(MFX_LDFLAGS "${MFX_ORIG_LDFLAGS} -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/libmfxsw_plugin.map" )

# HEVC Decoder
if( __TARGET_PLATFORM STREQUAL "BXTMIN" )
  message( STATUS "Disabling HEVCd HW libraries for BXTMIN." )
  set( hevcd_variants "" )
else()
  set( hevcd_variants hw )
  if (ENABLE_HEVC)
    set( hevcd_variants
        sw  ## open-source-cut-line
        hw )
  endif()
endif()

foreach( variant ${hevcd_variants} )
  if( __ARCH MATCHES intel64 )
    set( plugin_name mfx_hevcd_${variant}64 )
    set( eval_plugin_name mfx_eval_hevcd_${variant}64 )
  else()
    set( plugin_name mfx_hevcd_${variant}32 )
    set( eval_plugin_name mfx_eval_hevcd_${variant}32 )
  endif()
  if( __CONFIG MATCHES debug )
    set( plugin_name ${plugin_name}_d )
    set( eval_plugin_name ${eval_plugin_name}_d )
  endif()

  set (description_name ${${variant}_HEVC_DECODER_DESCRIPTION})

  set( LIBS "" )

  list( APPEND LIBS
      color_space_converter_sw  ## open-source-cut-line
      umc_h265_${variant} )

  if( variant MATCHES sw )
    list( APPEND LIBS
      hevc_pp_dispatcher
      hevc_pp_atom
      hevc_pp_avx2
      hevc_pp_px
      hevc_pp_sse4
      hevc_pp_ssse3 )
  endif()

  list( APPEND LIBS
    media_buffers       ## open-source-cut-line
    umc_io
    umc
    vm
    vm_plus
    ${ITT_LIBS}
    ippmsdk_l ippdc_l ippj_l ippvc_l ippcc_l ippcv_l ippi_l ipps_l ippcore_l
    pthread
    dl )

  if( variant MATCHES hw )
    set( hw_defs "-DMFX_VA -DVA_HEVC_DECODER" )
    list( APPEND LIBS
      umc_va_elk_hw )
  endif()

  set( defs "-DAS_HEVCD_PLUGIN -DMFX_PLUGIN_PRODUCT_NAME=\"\\\"${HEVC_PRODUCT_NAME}\"\\\" -DMFX_FILE_DESCRIPTION=\"\\\"${description_name}\"\\\" ${hevc_version_defs} ${hw_defs}" )
  if( variant MATCHES hw)
    set( defs "${defs} ${WARNING_FLAGS}" )
    gen_plugins_cfg("HEVC_Decoder_HW" ${HEVC_DECODER_HW_GUID} ${plugin_name} "01" "HEVC" "not_eval")
  else( )
    gen_plugins_cfg("HEVC_Decoder_SW" ${HEVC_DECODER_SW_GUID} ${plugin_name} "01" "HEVC" "not_eval")
  endif( )
  make_library( ${plugin_name} ${variant} shared)

  set( defs "-DAS_HEVCD_PLUGIN -DHEVCD_EVALUATION -DMFX_PLUGIN_PRODUCT_NAME=\"\\\"${HEVC_PRODUCT_NAME}\"\\\" -DMFX_FILE_DESCRIPTION=\"\\\"${description_name}\"\\\" ${hevc_version_defs}" )
  if(${variant} MATCHES hw)
    set( defs "${defs} ${WARNING_FLAGS}" )
    gen_plugins_cfg("HEVC_Decoder_HW" ${HEVC_DECODER_HW_GUID} ${eval_plugin_name} "01" "HEVC" "eval")
  else()
    gen_plugins_cfg("HEVC_Decoder_SW" ${HEVC_DECODER_SW_GUID} ${eval_plugin_name} "01" "HEVC" "eval")
  endif()
  make_library( ${eval_plugin_name} ${variant} shared)

  install( TARGETS ${plugin_name} ${eval_plugin_name} LIBRARY DESTINATION ${MFX_PLUGINS_DIR} )

endforeach()

# HEVC Encoder (SW & GACC)

set( hevce_variants "" )
if (ENABLE_HEVC)
  set( hevce_variants
       sw  ## open-source-cut-line
       hw )
endif()
set( defs "" )

foreach( dir ${MSDK_LIB_ROOT}/encode/h265/src )
  file( GLOB_RECURSE srcs "${dir}/*.c" "${dir}/*.cpp" )
  list( APPEND sources ${srcs})
endforeach()

file( GLOB_RECURSE srcs "${MSDK_LIB_ROOT}/fs_det/src/*.cpp" )
list( APPEND sources ${srcs} )

foreach( prefix ${MSDK_LIB_ROOT}/shared/src )
  list( APPEND sources
    ${prefix}/libmfxsw_encode.cpp
    ${prefix}/mfx_enc_common.cpp
  )
endforeach()

set( sources.plus "" )
foreach( prefix ${CMAKE_CURRENT_SOURCE_DIR}/src )
  list( APPEND sources.plus
    ${prefix}/mfx_hevc_enc_plugin.cpp
    ${prefix}/watermark.cpp
  )
endforeach()

foreach( dir ${MSDK_LIB_ROOT}/genx/h265_encode/src )
  file( GLOB_RECURSE srcs "${dir}/*_isa.cpp" )
  list( APPEND sources ${srcs})
endforeach()

foreach( dir ${MSDK_LIB_ROOT}/fei/h265/src )
  file( GLOB_RECURSE srcs "${dir}/*.c" "${dir}/*.cpp" )
  list( APPEND sources.plus ${srcs})
endforeach()

foreach( variant ${hevce_variants} )
  if( __ARCH MATCHES intel64 )
    if( variant MATCHES hw )
      set( plugin_name mfx_hevce_gacc64 )
      set( eval_plugin_name mfx_eval_hevce_gacc64 )
    else()
      set( plugin_name mfx_hevce_${variant}64 )
      set( eval_plugin_name mfx_eval_hevce_${variant}64 )
    endif()
  else()
    if( variant MATCHES hw )
      set( plugin_name mfx_hevce_gacc32 )
      set( eval_plugin_name mfx_eval_hevce_gacc32 )
    else()
      set( plugin_name mfx_hevce_${variant}32 )
      set( eval_plugin_name mfx_eval_hevce_${variant}32 )
    endif()
  endif()
  if( __CONFIG MATCHES debug )
    set( plugin_name ${plugin_name}_d )
    set( eval_plugin_name ${eval_plugin_name}_d )
  endif()

  set( description_name ${${variant}_HEVC_ENCODER_DESCRIPTION} )
  set( LIBS "" )
  list( APPEND LIBS
## open-source-cut-start
    hevc_pp_dispatcher
    hevc_pp_atom
    hevc_pp_avx2
    hevc_pp_px
    hevc_pp_sse4
    hevc_pp_ssse3
    media_buffers
## open-source-cut-end
    umc_io
    umc
    vm
    vm_plus
    ${ITT_LIBS}
    ippmsdk_l ippdc_l ippj_l ippvc_l ippcc_l ippcv_l ippi_l ipps_l ippcore_l
    pthread
    dl
    )

  if( variant MATCHES hw)
    list( APPEND LIBS
      umc_va_elk_hw
    )
  endif()

  set( defs "-DAS_HEVCE_PLUGIN -DMFX_PLUGIN_PRODUCT_NAME=\"\\\"${HEVC_PRODUCT_NAME}\"\\\" -DMFX_FILE_DESCRIPTION=\"\\\"${description_name}\"\\\" ${hevc_version_defs}" )
  if(${variant} MATCHES hw)
    gen_plugins_cfg("HEVC_Encoder_GACC" ${HEVC_ENCODER_GACC_GUID} ${plugin_name} "02" "HEVC" "not_eval")
  else()
    gen_plugins_cfg("HEVC_Encoder_SW" ${HEVC_ENCODER_SW_GUID} ${plugin_name} "02" "HEVC" "not_eval")
  endif()
  make_library( ${plugin_name} ${variant} shared)

  set( defs "-DAS_HEVCE_PLUGIN -DHEVCE_EVALUATION -DMFX_PLUGIN_PRODUCT_NAME=\"\\\"${HEVC_PRODUCT_NAME}\"\\\" -DMFX_FILE_DESCRIPTION=\"\\\"${description_name}\"\\\" ${hevc_version_defs}" )
  if(${variant} MATCHES hw)
    gen_plugins_cfg("HEVC_Encoder_GACC" ${HEVC_ENCODER_GACC_GUID} ${eval_plugin_name} "02" "HEVC" "eval")
  else()
    gen_plugins_cfg("HEVC_Encoder_SW" ${HEVC_ENCODER_SW_GUID} ${eval_plugin_name} "02" "HEVC" "eval")
  endif()
  make_library( ${eval_plugin_name} ${variant} shared)

  install( TARGETS ${plugin_name} ${eval_plugin_name} LIBRARY DESTINATION ${MFX_PLUGINS_DIR} )

endforeach()
set( defs "" )

# =============================================================================
# 1toN Lookahead
# =============================================================================

if( __TARGET_PLATFORM STREQUAL "BXTMIN" OR __TARGET_PLATFORM STREQUAL "BXT" )
  set (H264LA_PRODUCT_NAME "Intel(R) Media SDK 2017 for Embedded Linux")
  set (hw_H264LA_ENCODER_DESCRIPTION "Intel(R) Media SDK 2017 for Embedded Linux - SDK Advanced AVC Hardware Encode Plug-in")
else()
  set (H264LA_PRODUCT_NAME "Intel(R) Media Server Studio 2017 - SDK Advanced AVC Encode Pack")
  set (hw_H264LA_ENCODER_DESCRIPTION "Intel(R) Media Server Studio 2017 - SDK Advanced AVC Hardware Encode Plug-in")
endif()

  if( NOT DEFINED ENV{MFX_H264LA_VERSION} )
    set( h264la_version 0.0.000.0000 )
  else( )
    set( h264la_version $ENV{MFX_H264LA_VERSION} )
  endif( )

  if( Linux OR Darwin )
    execute_process(
      COMMAND echo
      COMMAND cut -f 1 -d.
      COMMAND date "+.%-y.%-m.%-d"
      OUTPUT_VARIABLE cur_date
      OUTPUT_STRIP_TRAILING_WHITESPACE
      )
    string( SUBSTRING ${h264la_version} 0 1 ver )

    set(h264la_version_defs  "-DMFX_PLUGIN_FILE_VERSION=\"\\\"${ver}${cur_date}\"\\\" -DMFX_PLUGIN_PRODUCT_VERSION=\"\\\"${h264la_version}\"\\\"")
  endif( )


set( sources "" )
set( sources.plus "" )

list( APPEND sources ${plugin_common_sources} )

foreach( prefix ${MSDK_LIB_ROOT}/shared/src )
  list( APPEND sources
    ${prefix}/mfx_enc_common.cpp
    ${prefix}/mfx_brc_common.cpp
    ${prefix}/mfx_ddi_enc_dump.cpp  ## open-source-cut-line
    ${prefix}/mfx_h264_enc_common_hw.cpp
    ${prefix}/mfx_h264_encode_factory.cpp
    ${prefix}/mfx_h264_encode_vaapi.cpp
    ${prefix}/mfx_mpeg2_enc_common_hw.cpp
    ${prefix}/mfx_mpeg2_encode_factory.cpp
    ${prefix}/mfx_mpeg2_encode_vaapi.cpp
    ${prefix}/libmfxsw_enc.cpp
  )
endforeach()

foreach( prefix ${MSDK_LIB_ROOT}/encode_hw/h264/src )
  list( APPEND sources
    ${prefix}/mfx_h264_encode_cm.cpp
    ${prefix}/mfx_h264_encode_hw_utils.cpp
    ${prefix}/mfx_h264_encode_hw_utils_new.cpp
    ${prefix}/mfx_h264_encode_hw.cpp
  )
endforeach()

foreach( prefix ${MSDK_LIB_ROOT}/genx/h264_encode/src )
  list( APPEND sources
    ${prefix}/genx_hsw_simple_me_isa.cpp
    ${prefix}/genx_bdw_simple_me_isa.cpp
    ${prefix}/genx_skl_simple_me_isa.cpp
    ${prefix}/genx_hsw_simple_me_proto.cpp
    ${prefix}/genx_hsw_histogram_isa.cpp
    ${prefix}/genx_skl_histogram_isa.cpp
  )
endforeach()

list( APPEND sources ${MSDK_LIB_ROOT}/cmrt_cross_platform/src/cmrt_cross_platform.cpp )
list( APPEND sources ${MSDK_LIB_ROOT}/encode_hw/mvc/src/mfx_mvc_encode_hw.cpp )
list( APPEND sources ${MSDK_LIB_ROOT}/encode_hw/svc/src/mfx_svc_encode_hw.cpp )
list( APPEND sources ${MSDK_LIB_ROOT}/encode_hw/svc/src/mfx_svc_encode_simulcast_over_avc.cpp )
list( APPEND sources ${MSDK_LIB_ROOT}/enc_hw/h264/src/mfx_h264_enc_hw.cpp )
list( APPEND sources ${MSDK_LIB_ROOT}/fei/h264_la/mfx_h264_la.cpp )

set( sources.plus "" )
foreach( prefix ${CMAKE_CURRENT_SOURCE_DIR}/src )
  list( APPEND sources.plus
    ${prefix}/mfx_h264la_plugin.cpp
  )
endforeach()


set( USE_STRICT_NAME TRUE )
set(MFX_LDFLAGS "${MFX_ORIG_LDFLAGS} -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/libmfxsw_plugin.map" )

# H264LA

foreach( variant hw )
  if( __ARCH MATCHES intel64 )
    set( plugin_name mfx_h264la_${variant}64 )
#    set( eval_plugin_name mfx_eval_h264la_${variant}64 )
  else()
    set( plugin_name mfx_h264la_${variant}32 )
#    set( eval_plugin_name mfx_eval_h264la_${variant}32 )
  endif()
  if( __CONFIG MATCHES debug )
    set( plugin_name ${plugin_name}_d )
#    set( eval_plugin_name ${eval_plugin_name}_d )
  endif()

  set (description_name ${${variant}_H264LA_ENCODER_DESCRIPTION})

  list( APPEND sources
    ${CMAKE_CURRENT_SOURCE_DIR}/../../shared/umc/codec/brc/src/umc_video_brc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../shared/umc/codec/brc/src/umc_h264_brc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../shared/umc/codec/brc/src/umc_brc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../shared/umc/codec/brc/src/umc_mpeg2_brc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../shared/umc/codec/brc/src/umc_svc_brc.cpp
  )

  set( LIBS "" )
  list( APPEND LIBS
    h264_la
    brc				## open-source-cut-line
    color_space_converter_sw    ## open-source-cut-line
    h264_enc_sw
    media_buffers               ## open-source-cut-line
    umc_io
    umc_va_elk_hw
    umc
    vm
    vm_plus
    ${ITT_LIBS}
    ippmsdk_l ippj_l ippvc_l ippcc_l ippcv_l ippi_l ipps_l ippcore_l
    pthread
    dl
    )

  set( defs "-DAS_H264LA_PLUGIN -DMFX_PLUGIN_PRODUCT_NAME=\"\\\"${H264LA_PRODUCT_NAME}\"\\\" -DMFX_FILE_DESCRIPTION=\"\\\"${description_name}\"\\\" ${h264la_version_defs}" )
  set( defs "${defs} ${WARNING_FLAGS}" )
  gen_plugins_cfg("H264la_Encoder" ${H264LA_ENCODER_GUID} ${plugin_name} "04" "AVC")
  make_library( ${plugin_name} ${variant} shared)

  install( TARGETS ${plugin_name} LIBRARY DESTINATION ${MFX_PLUGINS_DIR} )

endforeach()

# H265FEI

if( ENABLE_HEVC_FEI )
  set (H265FEI_PRODUCT_NAME "Intel(R) Media Server Studio 2017")
  set (hw_H265FEI_ENCODER_DESCRIPTION "HEVC GPU Assist APIs Plug-in")

  if( NOT DEFINED ENV{MFX_H265FEI_VERSION} )
    set( h265fei_version 0.0.000.0000 )
  else( )
    set( h265fei_version $ENV{MFX_H265FEI_VERSION} )
  endif( )

  if( Linux OR Darwin )
    execute_process(
      COMMAND echo
      COMMAND cut -f 1 -d.
      COMMAND date "+.%-y.%-m.%-d"
      OUTPUT_VARIABLE cur_date
      OUTPUT_STRIP_TRAILING_WHITESPACE
      )
    string( SUBSTRING ${h265fei_version} 0 1 ver )

    set(h265fei_version_defs "-DMFX_PLUGIN_FILE_VERSION=\"\\\"${ver}${cur_date}\"\\\" -DMFX_PLUGIN_PRODUCT_VERSION=\"\\\"${h265fei_version}\"\\\"")
  endif( )

  set( sources "" )
  set( sources.plus "" )
  set( defs "" )

  list( APPEND sources ${plugin_common_sources} )

  foreach( dir ${MSDK_LIB_ROOT}/genx/h265_encode/src )
    file( GLOB_RECURSE srcs "${dir}/*_isa.cpp" )
    list( APPEND sources ${srcs})
  endforeach()

  foreach( prefix ${MSDK_LIB_ROOT}/shared/src )
    list( APPEND sources
      ${prefix}/libmfxsw_enc.cpp
    )
  endforeach()

  foreach( prefix ${MSDK_LIB_ROOT}/plugin/src )
    list( APPEND sources
      ${prefix}/mfx_h265fei_plugin.cpp
    )
  endforeach()

# list separately instead of just including h265_fei lib because different
# define needed for plugin
  foreach( prefix ${MSDK_LIB_ROOT}/fei/h265/src )
    list( APPEND sources
      ${prefix}/mfx_h265_enc_cm_fei.cpp
      ${prefix}/mfx_h265_enc_cm_plugin.cpp
      ${prefix}/mfx_h265_enc_cm_utils.cpp
    )
  endforeach()

  list( REMOVE_ITEM sources
  ${MSDK_STUDIO_ROOT}/shared/src/mfx_umc_alloc_wrapper.cpp )

  foreach( variant hw )
    if( __ARCH MATCHES intel64 )
      set( plugin_name mfx_h265fei_${variant}64 )
    else()
      set( plugin_name mfx_h265fei_${variant}32 )
    endif()
    if( __CONFIG MATCHES debug )
      set( plugin_name ${plugin_name}_d )
    endif()
    set (description_name ${${variant}_H265FEI_ENCODER_DESCRIPTION})

    set( LIBS "" )
    list( APPEND LIBS
      media_buffers         ## open-source-cut-line
      umc_io
      umc
      vm
      vm_plus
      ${ITT_LIBS}
      ippmsdk_l ippj_l ippvc_l ippcc_l ippcv_l ippi_l ipps_l ippcore_l
      pthread
      dl
      )
    if(${variant} MATCHES hw)
      list( APPEND LIBS
        umc_va_elk_hw
      )
    endif()

    set( defs "-DAS_H265FEI_PLUGIN -DAS_HEVCE_PLUGIN -DMFX_PLUGIN_PRODUCT_NAME=\"\\\"${H265FEI_PRODUCT_NAME}\"\\\" -DMFX_FILE_DESCRIPTION=\"\\\"${description_name}\"\\\" ${h265fei_version_defs}")
    gen_plugins_cfg("H265FEI_Encoder" ${H265FEI_ENCODER_GUID} ${plugin_name} "04" "HEVC")
    make_library( ${plugin_name} ${variant} shared)

    install( TARGETS ${plugin_name} LIBRARY DESTINATION ${MFX_PLUGINS_DIR} )

  endforeach()
endif()

set( defs "" )

# =============================================================================
# VP8
# =============================================================================
if( __TARGET_PLATFORM STREQUAL "BXTMIN" OR __TARGET_PLATFORM STREQUAL "BXT" )
  set (VP8_PRODUCT_NAME "Intel(R) Media SDK 2017 for Embedded Linux")
  set (sw_VP8_DECODER_DESCRIPTION "Intel(R) Media SDK 2017 for Embedded Linux - SDK VP8 Software Decode Plug-in")
  set (hw_VP8_DECODER_DESCRIPTION "Intel(R) Media SDK 2017 for Embedded Linux - SDK VP8 Hardware Decode Plug-in")
else()
  set (VP8_PRODUCT_NAME "Intel(R) Media Server Studio 2017 - SDK VP8 Pack")
  set (sw_VP8_DECODER_DESCRIPTION "Intel(R) Media Server Studio 2017 - SDK VP8 Software Decode Plug-in")
  set (hw_VP8_DECODER_DESCRIPTION "Intel(R) Media Server Studio 2017 - SDK VP8 Hardware Decode Plug-in")
endif()

  if( NOT DEFINED ENV{MFX_VP8_VERSION} )
    set( vp8_version 0.0.000.0000 )
  else( )
    set( vp8_version $ENV{MFX_VP8_VERSION} )
  endif( )

  if( Linux OR Darwin )
    execute_process(
      COMMAND echo
      COMMAND cut -f 1 -d.
      COMMAND date "+.%-y.%-m.%-d"
      OUTPUT_VARIABLE cur_date
      OUTPUT_STRIP_TRAILING_WHITESPACE
      )
    string( SUBSTRING ${vp8_version} 0 1 ver )

    set(vp8_version_defs  "-DMFX_PLUGIN_FILE_VERSION=\"\\\"${ver}${cur_date}\"\\\" -DMFX_PLUGIN_PRODUCT_VERSION=\"\\\"${vp8_version}\"\\\"")
  endif( )



set( sources "" )
set( sources.plus "" )

list( APPEND sources ${plugin_common_sources} )

include_directories( ${MSDK_LIB_ROOT}/scheduler/include )

## open-source-cut-start
include_directories( ${CMAKE_HOME_DIRECTORY}/mdp_msdk-contrib/vp8_libvpx )
include_directories( ${MSDK_STUDIO_ROOT}/shared/umc/codec/vp8_dec/include )

list( APPEND sources ${MSDK_STUDIO_ROOT}/shared/umc/codec/vp8_dec/src/vp8_tables.cpp )
## open-source-cut-end
list( APPEND sources ${MSDK_LIB_ROOT}/shared/src/libmfxsw_decode.cpp )

set( USE_STRICT_NAME TRUE )
set(MFX_LDFLAGS "${MFX_ORIG_LDFLAGS} -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/libmfxsw_plugin.map" )

set( sources.plus "" )
foreach( prefix ${CMAKE_CURRENT_SOURCE_DIR}/src )
  list( APPEND sources.plus
    ${prefix}/mfx_vp8_dec_plugin.cpp
  )
endforeach()

foreach( variant
         sw  ## open-source-cut-line
         hw )

  if( __ARCH MATCHES intel64 )
    set( plugin_name mfx_vp8d_${variant}64 )
  else()
    set( plugin_name mfx_vp8d_${variant}32 )
  endif()
  if( __CONFIG MATCHES debug )
    set( plugin_name ${plugin_name}_d )
  endif()

  set (description_name ${${variant}_VP8_DECODER_DESCRIPTION})
  set( LIBS "" )

  if(${variant} MATCHES hw)

    list( APPEND LIBS
      va
      umc_va_elk_hw
    )

    foreach( prefix ${MSDK_LIB_ROOT}/decode/vp8/src )
    list( APPEND sources
      ${prefix}/mfx_vp8_dec_decode_hw.cpp
      ${prefix}/mfx_vp8_dec_decode.cpp  ## open-source-cut-line
      ${prefix}/mfx_vp8_dec_decode_common.cpp
      ${prefix}/mfx_vp8_dec_decode_hw_vaapi.cpp
      ${prefix}/mfx_vp8_dec_decode_tables.cpp
    )
    endforeach()

    foreach( prefix ${MSDK_STUDIO_ROOT}/shared/umc/io/umc_va/src )
    list( APPEND sources
      ${prefix}/umc_va_linux.cpp
    )
    endforeach()

    set( defs "-DAS_VP8D_PLUGIN -DUMC_ENABLE_VP8_VIDEO_DECODER -DMFX_VA -DMFX_PLUGIN_PRODUCT_NAME=\"\\\"${VP8_PRODUCT_NAME}\"\\\" -DMFX_FILE_DESCRIPTION=\"\\\"${description_name}\"\\\" ${vp8_version_defs}")

    gen_plugins_cfg("VP8_Decoder_HW" ${VP8_DECODER_HW_GUID} ${plugin_name} "01" "VP8")
## open-source-cut-start
  else()
    foreach( prefix ${MSDK_LIB_ROOT}/decode/vp8/src )
    list( APPEND sources
      ${prefix}/mfx_vp8_dec_decode.cpp
      ${prefix}/mfx_vp8_dec_decode_common.cpp
      ${prefix}/mfx_vp8_dec_decode_tables.cpp
    )
    endforeach()

    set( defs "-DAS_VP8D_PLUGIN -DUMC_ENABLE_VP8_VIDEO_DECODER -DMFX_PLUGIN_PRODUCT_NAME=\"\\\"${VP8_PRODUCT_NAME}\"\\\" -DMFX_FILE_DESCRIPTION=\"\\\"${description_name}\"\\\" ${vp8_version_defs}")

    gen_plugins_cfg("VP8_Decoder_SW" ${VP8_DECODER_SW_GUID} ${plugin_name} "01" "VP8")
## open-source-cut-end
  endif()

  list( APPEND LIBS
    vpx  ## open-source-cut-line
    color_space_converter_sw  ## open-source-cut-line
    media_buffers      ## open-source-cut-line
    umc_io
    umc
    vm
    vm_plus
    ${ITT_LIBS}
    ippmsdk_l ippj_l ippvc_l ippcc_l ippcv_l ippi_l ipps_l ippcore_l
    pthread
    dl
    )

  set( defs "${defs} ${WARNING_FLAGS}" )
  make_library( ${plugin_name} ${variant} shared)

  install( TARGETS ${plugin_name} LIBRARY DESTINATION ${MFX_PLUGINS_DIR} )

endforeach()

set( defs "" )

# =============================================================================
# VP9
# =============================================================================
if( __TARGET_PLATFORM STREQUAL "BXTMIN" OR __TARGET_PLATFORM STREQUAL "BXT" )
  set (VP9_PRODUCT_NAME "Intel(R) Media SDK 2017 for Embedded Linux")
  set (sw_VP9_DECODER_DESCRIPTION "Intel(R) Media SDK 2017 for Embedded Linux - SDK VP9 Software Decode Plug-in")
  set (hw_VP9_DECODER_DESCRIPTION "Intel(R) Media SDK 2017 for Embedded Linux - SDK VP9 Hardware Decode Plug-in")
else()
  set (VP9_PRODUCT_NAME "Intel(R) Media Server Studio 2017 - SDK VP9 Pack")
  set (sw_VP9_DECODER_DESCRIPTION "Intel(R) Media Server Studio 2017 - SDK VP9 Software Decode Plug-in")
  set (hw_VP9_DECODER_DESCRIPTION "Intel(R) Media Server Studio 2017 - SDK VP9 Hardware Decode Plug-in")
endif()

  if( NOT DEFINED ENV{MFX_VP9_VERSION} )
    set( vp9_version 0.0.000.0000 )
  else( )
    set( vp9_version $ENV{MFX_VP9_VERSION} )
  endif( )

  if( Linux OR Darwin )
    execute_process(
      COMMAND echo
      COMMAND cut -f 1 -d.
      COMMAND date "+.%-y.%-m.%-d"
      OUTPUT_VARIABLE cur_date
      OUTPUT_STRIP_TRAILING_WHITESPACE
      )
    string( SUBSTRING ${vp9_version} 0 1 ver )

    set(vp9_version_defs  "-DMFX_PLUGIN_FILE_VERSION=\"\\\"${ver}${cur_date}\"\\\" -DMFX_PLUGIN_PRODUCT_VERSION=\"\\\"${vp9_version}\"\\\"")
  endif( )



set( sources "" )
set( sources.plus "" )

list( APPEND sources ${plugin_common_sources} )

include_directories( ${MSDK_LIB_ROOT}/scheduler/include )
include_directories( ${CMAKE_HOME_DIRECTORY}/mdp_msdk-contrib/libvpx/libvpx )  ## open-source-cut-line

include_directories( ${MSDK_STUDIO_ROOT}/shared/umc/codec/vp9_dec/include )

list( APPEND sources ${MSDK_LIB_ROOT}/shared/src/libmfxsw_decode.cpp )

set( USE_STRICT_NAME TRUE )
set(MFX_LDFLAGS "${MFX_ORIG_LDFLAGS} -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/libmfxsw_plugin.map" )

set( sources.plus "" )
list( APPEND sources.plus
  ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_vp9_dec_plugin.cpp
)

foreach( variant
         sw  ## open-source-cut-line
         hw )

  if( __ARCH MATCHES intel64 )
    set( plugin_name mfx_vp9d_${variant}64 )
  else()
    set( plugin_name mfx_vp9d_${variant}32 )
  endif()
  if( __CONFIG MATCHES debug )
    set( plugin_name ${plugin_name}_d )
  endif()

  set (description_name ${${variant}_VP9_DECODER_DESCRIPTION})
  set( LIBS "" )

  if(${variant} MATCHES hw)

    list( APPEND LIBS
      va
      umc_va_elk_hw
    )

    list( APPEND sources
      ${MSDK_LIB_ROOT}/decode/vp9/src/mfx_vp9_dec_decode.cpp
      ${MSDK_LIB_ROOT}/decode/vp9/src/mfx_vp9_dec_decode_hw.cpp
      ${MSDK_LIB_ROOT}/decode/vp9/src/mfx_vp9_dec_decode_utils.cpp
    )

    list( APPEND sources
      ${MSDK_STUDIO_ROOT}/shared/umc/io/umc_va/src/umc_va_linux.cpp
    )

    list( APPEND sources
      ${MSDK_STUDIO_ROOT}/shared/umc/codec/vp9_dec/src/umc_vp9_bitstream.cpp
      ${MSDK_STUDIO_ROOT}/shared/umc/codec/vp9_dec/src/umc_vp9_utils.cpp
      ${MSDK_STUDIO_ROOT}/shared/umc/codec/vp9_dec/src/umc_vp9_va_packer.cpp
    )

    set( defs "-DAS_VP9D_PLUGIN -DUMC_ENABLE_VP9_VIDEO_DECODER -DMFX_VA -DMFX_PLUGIN_PRODUCT_NAME=\"\\\"${VP9_PRODUCT_NAME}\"\\\" -DMFX_FILE_DESCRIPTION=\"\\\"${description_name}\"\\\" ${vp9_version_defs}")

    gen_plugins_cfg("VP9_Decoder_HW" ${VP9_DECODER_HW_GUID} ${plugin_name} "01" "VP9")
## open-source-cut-start
  else()

    list( APPEND sources
      ${MSDK_LIB_ROOT}/decode/vp9/src/mfx_vp9_dec_decode.cpp
      ${MSDK_LIB_ROOT}/decode/vp9/src/mfx_vp9_dec_decode_utils.cpp
    )

    list( APPEND sources
      ${MSDK_STUDIO_ROOT}/shared/umc/codec/vp9_dec/src/umc_vp9_bitstream.cpp
      ${MSDK_STUDIO_ROOT}/shared/umc/codec/vp9_dec/src/umc_vp9_utils.cpp
    )

    set( defs "-DAS_VP9D_PLUGIN -DUMC_ENABLE_VP9_VIDEO_DECODER -DMFX_PLUGIN_PRODUCT_NAME=\"\\\"${VP9_PRODUCT_NAME}\"\\\" -DMFX_FILE_DESCRIPTION=\"\\\"${description_name}\"\\\" ${vp9_version_defs}")

    gen_plugins_cfg("VP9_Decoder_SW" ${VP9_DECODER_SW_GUID} ${plugin_name} "01" "VP9")
## open-source-cut-end
  endif()

  list( APPEND LIBS
    vpx  ## open-source-cut-line
    media_buffers   ## open-source-cut-line
    umc_io
    umc
    vm
    vm_plus
    ${ITT_LIBS}
    ippmsdk_l ippj_l ippvc_l ippcc_l ippcv_l ippi_l ipps_l ippcore_l
    pthread
    dl
    )

  set( defs "${defs} ${WARNING_FLAGS}" )
  make_library( ${plugin_name} ${variant} shared)

  install( TARGETS ${plugin_name} LIBRARY DESTINATION ${MFX_PLUGINS_DIR} )

endforeach()
