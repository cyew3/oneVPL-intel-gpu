set(MFX_ORIG_LDFLAGS "${MFX_LDFLAGS}" )

# =============================================================================

mfx_include_dirs( )

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/include )

include_directories( ${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/decode/h265/include )
include_directories( ${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/encode/h265/include )


list( APPEND umc_dirs
  common
  brc scene_analyzer color_space_converter
  mpeg2_dec mpeg2_enc
  vc1_common vc1_dec vc1_spl
  h264_dec h264_enc
  h265_dec
  jpeg_common jpeg_dec jpeg_enc)

foreach( dir ${umc_dirs} )
  include_directories( ${CMAKE_HOME_DIRECTORY}/_studio/shared/umc/codec/${dir}/include )
endforeach( )

list( APPEND vdecs
  h264 mpeg2 vc1 mjpeg vp8
  )

list( APPEND vencs
  h264 mpeg2 vc1 mjpeg mvc svc vp8
  )

foreach( dec ${vdecs} )
  list( APPEND vdirs
    brc/${dec} bsd/${dec} dec/${dec}
    )

  list( APPEND vdirs_impl
    decode/${dec}
    )
endforeach( )

foreach( enc ${vencs} )
  list( APPEND vdirs 
    enc/${enc} encode/${enc} pak/${enc}          
    )

  list( APPEND vdirs_hw
    enc_hw/${enc} encode_hw/${enc}
    )
endforeach( )

list( APPEND vdirs_impl vpp cmrt_cross_platform genx/h264_encode genx/h265_encode)

foreach( dir  ${vdirs} ${vdirs_impl} ${vdirs_hw} )
  include_directories( ${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/${dir}/include )
endforeach( )


# =============================================================================

set( sources "" )
set( sources.plus "" )
foreach( dir ${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/encode/h265/src )
  file( GLOB_RECURSE srcs "${dir}/*.c" "${dir}/*.cpp" )
  list( APPEND sources ${srcs})
endforeach()

set( defs "-DAS_HEVCE_PLUGIN" )
make_library( h265_enc none static )

set( defs "-DAS_HEVCE_PLUGIN -DHEVCE_EVALUATION" )
make_library( eval_h265_enc none static )

set( defs "" )

###################################################################################################################
# =============================================================================
# Plugins section
# =============================================================================

set (HEVC_PRODUCT_NAME "Intel(R) Media SDK HEVC Software Pack")
set (sw_HEVC_DECODER_DESCRIPTION "Intel(R) Media SDK HEVC Software Decode Plug-in")
set (hw_HEVC_DECODER_DESCRIPTION "Intel(R) Media SDK HEVC Hardware Decode Plug-in")
set (sw_HEVC_ENCODER_DESCRIPTION "Intel(R) Media SDK HEVC Software Encode Plug-in")
set (hw_HEVC_ENCODER_DESCRIPTION "Intel(R) Media SDK HEVC Hardware Encode Plug-in")

  if( NOT DEFINED ENV{MFX_HEVC_VERSION} )
    set( hevc_version 0.0.000.0000 )
  else( )
    set( hevc_version $ENV{MFX_HEVC_VERSION} )
  endif( )

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


foreach( prefix ${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/shared/src )

  list( APPEND plugin_common_sources
    ${prefix}/mfx_check_hardware_support.cpp
    ${prefix}/mfx_common_int.cpp
    ${prefix}/mfx_common_decode_int.cpp
    ${prefix}/libmfxsw.cpp
    ${prefix}/libmfxsw_async.cpp
    ${prefix}/libmfxsw_plugin.cpp
    ${prefix}/libmfxsw_query.cpp
    ${prefix}/libmfxsw_session.cpp
    ${prefix}/mfx_session.cpp
    ${prefix}/mfx_user_plugin.cpp
    ${prefix}/mfx_enc_common.cpp
  )

endforeach()

foreach( prefix ${CMAKE_HOME_DIRECTORY}/_studio/shared/src )
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
  ${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/cmrt_cross_platform/src/cmrt_cross_platform.cpp
)

foreach( prefix ${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/scheduler/src )
  list( APPEND plugin_common_sources
    ${prefix}/mfx_scheduler_core.cpp
    ${prefix}/mfx_scheduler_core_iunknown.cpp
    ${prefix}/mfx_scheduler_core_ischeduler.cpp
    ${prefix}/mfx_scheduler_core_task.cpp
    ${prefix}/mfx_scheduler_core_task_management.cpp
    ${prefix}/mfx_scheduler_core_thread.cpp
  )
endforeach()

# HEVC

set( sources "" )
set( sources.plus "" )

list( APPEND sources ${plugin_common_sources} )

include_directories( ${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/scheduler/include )

list( APPEND sources ${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/shared/src/libmfxsw_decode.cpp )
list( APPEND sources ${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/decode/h265/src/mfx_h265_dec_decode.cpp )

set( sources.plus "" )
foreach( prefix ${CMAKE_CURRENT_SOURCE_DIR}/src )
  list( APPEND sources.plus
    ${prefix}/mfx_hevc_dec_plugin.cpp
  )
endforeach()

set( USE_STRICT_NAME TRUE )
set(MFX_LDFLAGS "${MFX_ORIG_LDFLAGS} -Wl,--version-script=${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/plugin/libmfxsw_plugin.map" )

# HEVC

#foreach( variant sw hw )
foreach( variant sw )
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
    umc_h265_sw
    umc_csc_sw
    hevc_pp_dispatcher
    hevc_pp_atom
    hevc_pp_avx2
    hevc_pp_px
    hevc_pp_sse4
    hevc_pp_ssse3
    umc_io_merged_${variant}
    umc_core_merged 
    ${ITT_LIBS}
    ippmsdk_l ippj_l ippvc_l ippcc_l ippcv_l ippi_l ipps_l ippcore_l
    pthread
    dl
    )
  
  set( defs "-DAS_HEVCD_PLUGIN -DMFX_PLUGIN_PRODUCT_NAME=\"\\\"${HEVC_PRODUCT_NAME}\"\\\" -DMFX_FILE_DESCRIPTION=\"\\\"${description_name}\"\\\" ${hevc_version_defs}" )
  make_library( ${plugin_name} ${variant} shared)

  set( defs "-DAS_HEVCD_PLUGIN -DHEVCD_EVALUATION -DMFX_PLUGIN_PRODUCT_NAME=\"\\\"${HEVC_PRODUCT_NAME}\"\\\" -DMFX_FILE_DESCRIPTION=\"\\\"${description_name}\"\\\" ${hevc_version_defs}" )
  make_library( ${eval_plugin_name} ${variant} shared)

endforeach()

set( defs "" )

foreach( prefix ${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/shared/src )
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
foreach( prefix ${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/genx/h265_encode/src )
  list( APPEND sources.plus
    ${prefix}/genx_h265_cmcode_stub.cpp
    ${prefix}/genx_h265_cmcode_isa.cpp
  )
endforeach()

foreach( variant sw )
  if( __ARCH MATCHES intel64 )
    set( plugin_name mfx_hevce_${variant}64 )
    set( eval_plugin_name mfx_eval_hevce_${variant}64 )
  else()
    set( plugin_name mfx_hevce_${variant}32 )
    set( eval_plugin_name mfx_eval_hevce_${variant}32 )
  endif()
  if( __CONFIG MATCHES debug )
    set( plugin_name ${plugin_name}_d )
    set( eval_plugin_name ${eval_plugin_name}_d )
  endif()

  set (description_name ${${variant}_HEVC_ENCODER_DESCRIPTION})  

  set( LIBS "" )
  list( APPEND LIBS
    h265_enc
    umc_h265_sw
    umc_csc_sw
    hevc_pp_dispatcher
    hevc_pp_atom
    hevc_pp_avx2
    hevc_pp_px
    hevc_pp_sse4
    hevc_pp_ssse3
    umc_io_merged_${variant}
    umc_core_merged
    ${ITT_LIBS}
    ippmsdk_l ippj_l ippvc_l ippcc_l ippcv_l ippi_l ipps_l ippcore_l
    pthread
    dl
    )

  set( defs "-DAS_HEVCE_PLUGIN -DMFX_PLUGIN_PRODUCT_NAME=\"\\\"${HEVC_PRODUCT_NAME}\"\\\" -DMFX_FILE_DESCRIPTION=\"\\\"${description_name}\"\\\" ${hevc_version_defs}" )
  make_library( ${plugin_name} ${variant} shared)

  set( LIBS "" )
  list( APPEND LIBS
    eval_h265_enc
    umc_h265_sw
    umc_csc_sw
    hevc_pp_dispatcher
    hevc_pp_atom
    hevc_pp_avx2
    hevc_pp_px
    hevc_pp_sse4
    hevc_pp_ssse3
    umc_io_merged_${variant}
    umc_core_merged 
    ${ITT_LIBS}
    ippmsdk_l ippj_l ippvc_l ippcc_l ippcv_l ippi_l ipps_l ippcore_l
    pthread
    dl
    )

  set( defs "-DAS_HEVCE_PLUGIN -DHEVCE_EVALUATION -DMFX_PLUGIN_PRODUCT_NAME=\"\\\"${HEVC_PRODUCT_NAME}\"\\\" -DMFX_FILE_DESCRIPTION=\"\\\"${description_name}\"\\\" ${hevc_version_defs}" )
  make_library( ${eval_plugin_name} ${variant} shared)

endforeach()
set( defs "" )

# =============================================================================
# 1toN Lookahead
# =============================================================================

set (H264LA_PRODUCT_NAME "Intel(R) Media SDK Advanced AVC Encode Pack")
set (hw_H264LA_ENCODER_DESCRIPTION "Intel(R) Media SDK H264 Advanced Hardware Encode Plug-in")

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
    string( SUBSTRING ${hevc_version} 0 1 ver )

    set(h264la_version_defs  "-DMFX_PLUGIN_FILE_VERSION=\"\\\"${ver}${cur_date}\"\\\" -DMFX_PLUGIN_PRODUCT_VERSION=\"\\\"${h264la_version}\"\\\"")
  endif( )


set( sources "" )
set( sources.plus "" )

list( APPEND sources ${plugin_common_sources} )

foreach( prefix ${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/shared/src )
  list( APPEND sources
    ${prefix}/mfx_enc_common.cpp
    ${prefix}/mfx_brc_common.cpp
    ${prefix}/mfx_ddi_enc_dump.cpp
    ${prefix}/mfx_h264_enc_common_hw.cpp
    ${prefix}/mfx_h264_encode_factory.cpp
    ${prefix}/mfx_h264_encode_vaapi.cpp
    ${prefix}/mfx_mpeg2_enc_common_hw.cpp
    ${prefix}/mfx_mpeg2_encode_factory.cpp
    ${prefix}/mfx_mpeg2_encode_vaapi.cpp
    ${prefix}/mfx_vp8_enc_common_hw.cpp
    ${prefix}/libmfxsw_enc.cpp
  )
endforeach()

foreach( prefix ${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/encode_hw/h264/src )
  list( APPEND sources
    ${prefix}/mfx_h264_encode_cm.cpp
    ${prefix}/mfx_h264_encode_hw_utils.cpp
    ${prefix}/mfx_h264_encode_hw_utils_new.cpp
    ${prefix}/mfx_h264_encode_hw.cpp    
  )
endforeach()

foreach( prefix ${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/genx/h264_encode/src )
  list( APPEND sources
    ${prefix}/genx_hsw_simple_me_isa.cpp
    ${prefix}/genx_hsw_simple_me_proto.cpp
  )
endforeach()

list( APPEND sources ${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/cmrt_cross_platform/src/cmrt_cross_platform.cpp )
list( APPEND sources ${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/encode_hw/mvc/src/mfx_mvc_encode_hw.cpp )
list( APPEND sources ${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/encode_hw/svc/src/mfx_svc_encode_hw.cpp )
list( APPEND sources ${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/encode_hw/svc/src/mfx_svc_encode_simulcast_over_avc.cpp )
list( APPEND sources ${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/enc_hw/h264/src/mfx_h264_enc_hw.cpp )
list( APPEND sources ${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/fei/h264_la/mfx_h264_la.cpp )

set( sources.plus "" )
foreach( prefix ${CMAKE_CURRENT_SOURCE_DIR}/src )
  list( APPEND sources.plus
    ${prefix}/mfx_h264la_plugin.cpp
  )
endforeach()


set( USE_STRICT_NAME TRUE )
set(MFX_LDFLAGS "${MFX_ORIG_LDFLAGS} -Wl,--version-script=${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/plugin/libmfxsw_plugin.map" )

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

  set( LIBS "" )
  list( APPEND LIBS
    h264_la
    umc_brc_sw
    umc_csc_sw
    umc_h264_enc_sw
    umc_io_merged_${variant}
    umc_core_merged 
    ${ITT_LIBS}
    ippmsdk_l ippj_l ippvc_l ippcc_l ippcv_l ippi_l ipps_l ippcore_l
    pthread
    dl
    )
  
  set( defs "-DAS_H264LA_PLUGIN -DMFX_PLUGIN_PRODUCT_NAME=\"\\\"${H264LA_PRODUCT_NAME}\"\\\" -DMFX_FILE_DESCRIPTION=\"\\\"${description_name}\"\\\" ${h264la_version_defs}" )
  make_library( ${plugin_name} ${variant} shared)

#  set( defs "-DAS_H264LA_PLUGIN -DH264LA_EVALUATION -DMFX_PLUGIN_PRODUCT_NAME=\"\\\"${HEVC_PRODUCT_NAME}\"\\\" -DMFX_FILE_DESCRIPTION=\"\\\"${description_name}\"\\\" ${hevc_version_defs}" )
#  make_library( ${eval_plugin_name} ${variant} shared)

endforeach()

set( defs "" )

# =============================================================================
# VP8
# =============================================================================


set( sources "" )
set( sources.plus "" )

list( APPEND sources ${plugin_common_sources} )

include_directories( ${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/scheduler/include )
include_directories( ${CMAKE_HOME_DIRECTORY}/thirdparty/vp8_libvpx )

include_directories( ${CMAKE_HOME_DIRECTORY}/_studio/shared/umc/codec/vp8_dec/include )

list( APPEND sources ${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/shared/src/libmfxsw_decode.cpp )
list( APPEND sources ${CMAKE_HOME_DIRECTORY}/_studio/shared/umc/codec/vp8_dec/src/vp8_tables.cpp )

set( USE_STRICT_NAME TRUE )
set(MFX_LDFLAGS "${MFX_ORIG_LDFLAGS} -Wl,--version-script=${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/plugin/libmfxsw_plugin.map" )

set( sources.plus "" )
foreach( prefix ${CMAKE_CURRENT_SOURCE_DIR}/src )
  list( APPEND sources.plus
    ${prefix}/mfx_vp8_dec_plugin.cpp
  )
endforeach()

foreach( variant sw hw )

  if( __ARCH MATCHES intel64 )
    set( plugin_name mfx_vp8d_${variant}64 )
  else()
    set( plugin_name mfx_vp8d_${variant}32 )
  endif()
  if( __CONFIG MATCHES debug )
    set( plugin_name ${plugin_name}_d )
  endif()

  set( LIBS "" )

  if(${variant} MATCHES hw)

    list( APPEND LIBS va )

    foreach( prefix ${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/decode/vp8/src )
    list( APPEND sources
      ${prefix}/mfx_vp8_dec_decode_hw.cpp
      ${prefix}/mfx_vp8_dec_decode.cpp
      ${prefix}/mfx_vp8_dec_decode_common.cpp
      ${prefix}/mfx_vp8_dec_decode_vaapi.cpp
    )
    endforeach()

    foreach( prefix ${CMAKE_HOME_DIRECTORY}/_studio/shared/umc/io/umc_va/src )
    list( APPEND sources
      ${prefix}/umc_va_linux.cpp
    )
    endforeach()

    set( defs "-DAS_VP8D_PLUGIN -DAS_VP8DHW_PLUGIN -DUMC_ENABLE_VP8_VIDEO_DECODER -DMFX_VA")

  else()

    foreach( prefix ${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/decode/vp8/src )
    list( APPEND sources
      ${prefix}/mfx_vp8_dec_decode.cpp
      ${prefix}/mfx_vp8_dec_decode_common.cpp
    )
    endforeach()

    set( defs "-DAS_VP8D_PLUGIN -DUMC_ENABLE_VP8_VIDEO_DECODER")

  endif()

  list( APPEND LIBS
    vpx
    umc_csc_sw
    umc_io_merged_${variant}
    umc_core_merged
    ${ITT_LIBS}
    ippmsdk_l ippj_l ippvc_l ippcc_l ippcv_l ippi_l ipps_l ippcore_l
    pthread
    dl
    )

  make_library( ${plugin_name} ${variant} shared)

endforeach()

# =============================================================================
