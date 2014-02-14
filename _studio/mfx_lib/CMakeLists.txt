set(MFX_ORIG_LDFLAGS "${MFX_LDFLAGS}" )

# =============================================================================

list( APPEND adecs
  aac mp3
  )

list( APPEND aencs
  aac
  )

foreach( dec ${adecs} )
  list( APPEND adirs
    audio_decode/${dec}
    )
endforeach( )

foreach( enc ${aencs} )
  list( APPEND adirs 
    audio_encode/${enc}
    )
endforeach( )

# =============================================================================

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

list( APPEND vdir_optimization optimization/h265 )

list( APPEND vdirs scheduler )
list( APPEND vdirs_impl vpp genx/h264_encode cmrt_cross_platform )

# =============================================================================

list( APPEND umc_dirs
  common
  aac_common aac_dec aac_enc mp3_common mp3_dec
  mpeg2_enc h264_enc jpeg_enc
  h265_dec h264_dec mpeg2_dec vc1_dec jpeg_dec
  vc1_common vc1_spl jpeg_common
  brc scene_analyzer color_space_converter jpeg_common
  )

# =============================================================================

mfx_include_dirs( )

foreach( dir ${umc_dirs} )
  include_directories( ${CMAKE_HOME_DIRECTORY}/_studio/shared/umc/codec/${dir}/include )
endforeach( )

foreach( dir ${adirs} ${vdirs} ${vdirs_impl} ${vdirs_hw} ${vdir_optimization} )
  include_directories( ${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/${dir}/include )
endforeach( )

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/decode/h265/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/encode/h265/include )


# =============================================================================
set ( HEVCPP_LIBS "" )
# =============================================================================

set( defs "-DAS_HEVCD_PLUGIN -DAS_HEVCE_PLUGIN" )
set( sources "" )
set( sources.plus "" )

list( APPEND sources 
  ${vdir_optimization}/src/mfx_h265_dispatcher.cpp
)

make_library( hevc_pp_dispatcher none static )
set( defs "" )
list( APPEND HEVCPP_LIBS hevc_pp_dispatcher )

# =============================================================================

set( defs "-DMFX_MAKENAME_ATOM -DAS_HEVCD_PLUGIN -DAS_HEVCE_PLUGIN" )
set( sources "" )
set( sources.plus "" )

list( APPEND sources 
  ${vdir_optimization}/src/mfx_h265_deblocking_sse.cpp
  ${vdir_optimization}/src/mfx_h265_interpolation_atom.cpp
  ${vdir_optimization}/src/mfx_h265_interp_tmpl_sse.cpp
  ${vdir_optimization}/src/mfx_h265_intrapred_sse.cpp
  ${vdir_optimization}/src/mfx_h265_quantization_sse.cpp
  ${vdir_optimization}/src/mfx_h265_sad_general_sse.cpp
  ${vdir_optimization}/src/mfx_h265_sad_sse.cpp
  ${vdir_optimization}/src/mfx_h265_sao_sse.cpp
  ${vdir_optimization}/src/mfx_h265_transform_fwd_sse.cpp
  ${vdir_optimization}/src/mfx_h265_transform_inv_sse.cpp
  ${vdir_optimization}/src/mfx_h265_weighted_pred_sse.cpp
)

make_library( hevc_pp_atom none static )
set( defs "" )
list( APPEND HEVCPP_LIBS hevc_pp_atom )

# =============================================================================

set( defs "-DMFX_MAKENAME_AVX2 -DAS_HEVCD_PLUGIN -DAS_HEVCE_PLUGIN" )
set( sources "" )
set( sources.plus "" )

list( APPEND sources 
  ${vdir_optimization}/src/mfx_h265_interpolation_avx2.cpp
  ${vdir_optimization}/src/mfx_h265_interp_tmpl_avx2.cpp
  ${vdir_optimization}/src/mfx_h265_quantization_avx2.cpp
  ${vdir_optimization}/src/mfx_h265_reordering_avx2.cpp
  ${vdir_optimization}/src/mfx_h265_sad_avx2.cpp
  ${vdir_optimization}/src/mfx_h265_sad_general_avx2.cpp
  ${vdir_optimization}/src/mfx_h265_transform_fwd_32x32_avx2_nablet.cpp
  ${vdir_optimization}/src/mfx_h265_transform_fwd_avx2.cpp
  ${vdir_optimization}/src/mfx_h265_transform_inv_32x32_avx2.cpp
  ${vdir_optimization}/src/mfx_h265_transform_inv_avx2.cpp
  ${vdir_optimization}/src/mfx_h265_weighted_pred_avx2.cpp
)

make_library( hevc_pp_avx2 none static )
append_property( hevc_pp_avx2 COMPILE_FLAGS "-mavx2 -march=core-avx2")
append_property( hevc_pp_avx2 LINK_FLAGS "-mavx2 -march=core-avx2")
set( defs "" )
list( APPEND HEVCPP_LIBS hevc_pp_avx2 )

# =============================================================================

set( defs "-DMFX_MAKENAME_PX -DAS_HEVCD_PLUGIN -DAS_HEVCE_PLUGIN" )
set( sources "" )
set( sources.plus "" )

list( APPEND sources 
    ${vdir_optimization}/src/mfx_h265_deblocking_px.cpp
    ${vdir_optimization}/src/mfx_h265_interpolation_16u_px.cpp
    ${vdir_optimization}/src/mfx_h265_interpolation_px.cpp
    ${vdir_optimization}/src/mfx_h265_intrapred_16u_px.cpp
    ${vdir_optimization}/src/mfx_h265_intrapred_px.cpp
    ${vdir_optimization}/src/mfx_h265_quantization_px.cpp
    ${vdir_optimization}/src/mfx_h265_sad_ipp.cpp
    ${vdir_optimization}/src/mfx_h265_sao_16u_px.cpp
    ${vdir_optimization}/src/mfx_h265_sao_px.cpp
    ${vdir_optimization}/src/mfx_h265_transform_16u_px.cpp
    ${vdir_optimization}/src/mfx_h265_transform_px.cpp
    ${vdir_optimization}/src/mfx_h265_weighted_pred_16u_px.cpp
    ${vdir_optimization}/src/mfx_h265_weighted_pred_px.cpp
)

make_library( hevc_pp_px none static )
set( defs "" )
list( APPEND HEVCPP_LIBS hevc_pp_px )

# =============================================================================

set( defs "-DMFX_MAKENAME_SSE4 -DAS_HEVCD_PLUGIN -DAS_HEVCE_PLUGIN" )
set( sources "" )
set( sources.plus "" )

list( APPEND sources 
  ${vdir_optimization}/src/mfx_h265_deblocking_sse.cpp
  ${vdir_optimization}/src/mfx_h265_interpolation_sse.cpp
  ${vdir_optimization}/src/mfx_h265_interp_tmpl_sse.cpp
  ${vdir_optimization}/src/mfx_h265_intrapred_sse.cpp
  ${vdir_optimization}/src/mfx_h265_quantization_sse.cpp
  ${vdir_optimization}/src/mfx_h265_sad_general_sse.cpp
  ${vdir_optimization}/src/mfx_h265_sad_sse.cpp
  ${vdir_optimization}/src/mfx_h265_sao_sse.cpp
  ${vdir_optimization}/src/mfx_h265_transform_consts.cpp
  ${vdir_optimization}/src/mfx_h265_transform_fwd_sse.cpp
  ${vdir_optimization}/src/mfx_h265_transform_inv_sse.cpp
  ${vdir_optimization}/src/mfx_h265_weighted_pred_sse.cpp
)

make_library( hevc_pp_sse4 none static )
set( defs "" )
list( APPEND HEVCPP_LIBS hevc_pp_sse4 )

# =============================================================================

set( defs "-DMFX_MAKENAME_SSSE3 -DAS_HEVCD_PLUGIN -DAS_HEVCE_PLUGIN" )
set( sources "" )
set( sources.plus "" )

list( APPEND sources 
  ${vdir_optimization}/src/mfx_h265_deblocking_sse.cpp
  ${vdir_optimization}/src/mfx_h265_interpolation_sse.cpp
  ${vdir_optimization}/src/mfx_h265_interp_tmpl_sse.cpp
  ${vdir_optimization}/src/mfx_h265_intrapred_sse.cpp
  ${vdir_optimization}/src/mfx_h265_sad_general_sse.cpp
  ${vdir_optimization}/src/mfx_h265_sad_sse.cpp
  ${vdir_optimization}/src/mfx_h265_sao_sse.cpp
  ${vdir_optimization}/src/mfx_h265_transform_fwd_sse.cpp
  ${vdir_optimization}/src/mfx_h265_transform_inv_sse.cpp
  ${vdir_optimization}/src/mfx_h265_weighted_pred_sse.cpp
)

make_library( hevc_pp_ssse3 none static )
append_property( hevc_pp_ssse3 COMPILE_FLAGS "-mssse3")
append_property( hevc_pp_ssse3 LINK_FLAGS "-mssse3" )
set( defs "" )
list( APPEND HEVCPP_LIBS hevc_pp_ssse3 )

# =============================================================================

#set( sources "" )
#set( sources.plus "" )
#foreach( dir ${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/decode/h265 )
#  file( GLOB_RECURSE srcs "${dir}/*.c" "${dir}/*.cpp" )
#  list( APPEND sources ${srcs})
#endforeach()


#make_library( h265_dec none static )

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

# =============================================================================
set( sources "" )
set( sources.plus "" )
foreach( dir ${adirs} )
  file( GLOB_RECURSE srcs "${dir}/*.c" "${dir}/*.cpp" )
  list( APPEND sources ${srcs})
endforeach()

make_library( mfx_alib_merged none static )

# =============================================================================

set( sources "" )
set( sources.plus "" )
foreach( dir ${vdirs} )
  file( GLOB_RECURSE srcs "${dir}/*.c" "${dir}/*.cpp" )
  list( APPEND sources ${srcs})
endforeach()

make_library( mfx_vlib_merged none static )

# =============================================================================

set( sources "" )
set( sources.plus "" )
foreach( dir ${vdirs_impl} )
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

make_library( mfx_vlib_merged sw static )

foreach( dir ${vdirs_hw} )
  file( GLOB_RECURSE srcs "${dir}/*.c" "${dir}/*.cpp" )
  list( APPEND sources.plus ${srcs})  
endforeach()
foreach( prefix ${CMAKE_CURRENT_SOURCE_DIR}/shared/src )
  list( APPEND sources.plus
    ${prefix}/mfx_ddi_enc_dump.cpp
    ${prefix}/mfx_h264_enc_common_hw.cpp
    ${prefix}/mfx_vp8_enc_common_hw.cpp
    ${prefix}/mfx_h264_encode_vaapi.cpp
    ${prefix}/mfx_h264_encode_factory.cpp
    ${prefix}/mfx_mpeg2_enc_common_hw.cpp
    ${prefix}/mfx_mpeg2_encode_factory.cpp
    ${prefix}/mfx_mpeg2_encode_vaapi.cpp
    )
endforeach()

make_library( mfx_vlib_merged hw static )

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
    ${prefix}/cm_mem_copy.cpp
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
list( APPEND sources
  ${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/cmrt_cross_platform/src/cmrt_cross_platform.cpp
)

list( APPEND sources
  ${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/cmrt_cross_platform/src/cmrt_cross_platform.cpp
)

set( USE_STRICT_NAME TRUE )
set(MFX_LDFLAGS "${MFX_ORIG_LDFLAGS} -Wl,--version-script=${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/libmfx.map" )
    
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
    mfx_vlib_merged
    mfx_vlib_merged_${variant}
    umc_codecs_merged_${variant}
    umc_codecs_merged
    umc_io_merged_${variant}
    umc_core_merged 
    ${ITT_LIBS}
    ippmsdk_l ippj_l ippvc_l ippcc_l ippcv_l ippi_l ipps_l ippcore_l
    pthread
    dl
  )

  make_library( ${mfxlibname} ${variant} shared)
  make_library( mfx${variant}_static ${variant} static)
  #message( STATUS "xxxxxxxxxxxxxxx __ARCH: "${__ARCH}, "mfxlibname: "${mfxlibname}, "variant: "${variant})
endforeach()

# =============================================================================

# libmfxrt reuse the same .map file as full library
set( sources "" )
set( sources.plus "" )

# mfx_common_hw lib part
foreach( prefix ${CMAKE_CURRENT_SOURCE_DIR}/shared/src )
  list( APPEND sources
    ${prefix}/mfx_ddi_enc_dump.cpp
    ${prefix}/mfx_h264_enc_common_hw.cpp
    ${prefix}/mfx_vp8_enc_common_hw.cpp
    ${prefix}/mfx_h264_encode_vaapi.cpp
    ${prefix}/mfx_h264_encode_factory.cpp
    ${prefix}/mfx_mpeg2_enc_common_hw.cpp
    ${prefix}/mfx_mpeg2_encode_vaapi.cpp
    ${prefix}/mfx_mpeg2_encode_factory.cpp
  )
endforeach()

# mfx_common lib part
foreach( prefix ${CMAKE_CURRENT_SOURCE_DIR}/shared/src )
  list( APPEND sources
    ${prefix}/mfx_brc_common.cpp
    ${prefix}/mfx_common_decode_int.cpp
    ${prefix}/mfx_common_int.cpp
    ${prefix}/mfx_enc_common.cpp
    ${prefix}/mfx_h264_enc_common.cpp
    ${prefix}/mfx_mpeg2_dec_common.cpp
    ${prefix}/mfx_vc1_dec_common.cpp
  )
endforeach()

# mfxrt library itself
foreach( prefix ${CMAKE_HOME_DIRECTORY}/_studio/shared/src )
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
  )
endforeach()
list( APPEND sources
  ${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/cmrt_cross_platform/src/cmrt_cross_platform.cpp
)

foreach( prefix ${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib )
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
  set( mfxrtlibname mfxrt${variant}64 )
else()
  set( mfxrtlibname mfxrt${variant}32 )
endif()
if( __CONFIG MATCHES debug )
  set( mfxrtlibname ${mfxrtlibname}_d )
endif()

set( LIBS "" )
list( APPEND LIBS
  umc_io_merged_hw  
  umc_core_merged 
  ${ITT_LIBS}
  ippmsdk_l ippj_l ippvc_l ippcc_l ippcv_l ippi_l ipps_l ippcore_l
  pthread
  dl )

make_library( ${mfxrtlibname} none shared)
append_property( ${mfxrtlibname} COMPILE_FLAGS "-DMFX_VA -DMFX_VA_LINUX -DMFX_RT" )  

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
  )
endforeach()
foreach( prefix ${CMAKE_HOME_DIRECTORY}/_studio/shared/src )
  list( APPEND sources
    ${prefix}/fast_copy.cpp
    ${prefix}/libmfx_allocator.cpp
    ${prefix}/libmfx_core.cpp
    ${prefix}/libmfx_core_factory.cpp
    ${prefix}/mfx_umc_alloc_wrapper.cpp
    ${prefix}/mfx_search_dll.cpp
    ${prefix}/mfx_timing.cpp
  )
endforeach()

set( USE_STRICT_NAME TRUE )
set(MFX_LDFLAGS "${MFX_ORIG_LDFLAGS} -Wl,--version-script=${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/libmfxaudio.map" )
    
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
    mfx_vlib_merged
    mfx_vlib_merged_${variant}
    mfx_alib_merged
    umc_acodecs_merged
    umc_codecs_merged
    umc_io_merged_${variant}
    umc_core_merged 
    ${ITT_LIBS}
    ippcc_l ippdc_l ippac_l ippi_l ipps_l ippcore_l
    pthread
    dl
    )

  make_library( ${mfxlibname} ${variant} shared)
  #make_library( mfxaudio${variant}_static ${variant} static)
endforeach()

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


foreach( prefix ${CMAKE_CURRENT_SOURCE_DIR}/shared/src )

  list( APPEND plugin_common_sources
    ${prefix}/mfx_common_int.cpp
    ${prefix}/mfx_common_decode_int.cpp
    ${prefix}/libmfxsw.cpp
    ${prefix}/libmfxsw_async.cpp
    ${prefix}/libmfxsw_plugin.cpp
    ${prefix}/libmfxsw_query.cpp
    ${prefix}/libmfxsw_session.cpp
    ${prefix}/libmfxsw_decode.cpp
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
    ${prefix}/libmfx_core_vaapi.cpp
    ${prefix}/mfx_umc_alloc_wrapper.cpp
  )
endforeach()
list( APPEND plugin_common_sources
  ${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/cmrt_cross_platform/src/cmrt_cross_platform.cpp
)

foreach( prefix ${CMAKE_CURRENT_SOURCE_DIR}/scheduler/src )
  list( APPEND plugin_common_sources
    ${prefix}/mfx_scheduler_core.cpp
    ${prefix}/mfx_scheduler_core_iunknown.cpp
    ${prefix}/mfx_scheduler_core_task_management.cpp
    ${prefix}/mfx_scheduler_core_ischeduler.cpp
    ${prefix}/mfx_scheduler_core_task.cpp
    ${prefix}/mfx_scheduler_core_thread.cpp
  )
endforeach()

# HEVC

set( sources "" )
set( sources.plus "" )

list( APPEND sources ${plugin_common_sources} )

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/scheduler/include )

foreach( prefix ${CMAKE_CURRENT_SOURCE_DIR}/decode/h265/src )
  list( APPEND sources
    ${prefix}/mfx_h265_dec_decode.cpp
  )
endforeach()

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/plugin/include )

set( sources.plus "" )
foreach( prefix ${CMAKE_CURRENT_SOURCE_DIR}/plugin/src )
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
    ${HEVCPP_LIBS}
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

#  append_property( ${plugin_name} COMPILE_FLAGS "-DAS_HEVCD_PLUGIN" )  

#  make_library( mfx${variant}_static ${variant} static)
#  message( STATUS "CMAKE_CURRENT_SOURCE_DIR", ${CMAKE_CURRENT_SOURCE_DIR} )
#  message( STATUS "xxxxxxxxxxxxxxx __ARCH: "${__ARCH}, "plugin_name: "${plugin_name}, "variant: "${variant})
endforeach()

set( defs "" )

foreach( prefix ${CMAKE_CURRENT_SOURCE_DIR}/shared/src )
  list( APPEND sources
    ${prefix}/libmfxsw_encode.cpp
    ${prefix}/mfx_enc_common.cpp
  )
endforeach()

set( sources.plus "" )
foreach( prefix ${CMAKE_CURRENT_SOURCE_DIR}/plugin/src )
  list( APPEND sources.plus
    ${prefix}/mfx_hevc_enc_plugin.cpp
    ${prefix}/watermark.cpp
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
    ${HEVCPP_LIBS}
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
    ${HEVCPP_LIBS}
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

# VP8

set( sources "" )
set( sources.plus "" )

list( APPEND sources ${plugin_common_sources} )

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/scheduler/include )
include_directories( ${CMAKE_HOME_DIRECTORY}/thirdparty/vp8_libvpx )

include_directories( ${CMAKE_HOME_DIRECTORY}/_studio/shared/umc/codec/vp8_dec/include )

foreach( prefix ${CMAKE_HOME_DIRECTORY}/_studio/shared/umc/codec/vp8_dec/src )
  list( APPEND sources
    ${prefix}/vp8_tables.cpp
  )
endforeach()

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/plugin/include )

set( USE_STRICT_NAME TRUE )
set(MFX_LDFLAGS "${MFX_ORIG_LDFLAGS} -Wl,--version-script=${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/plugin/libmfxsw_plugin.map" )

set( sources.plus "" )
foreach( prefix ${CMAKE_CURRENT_SOURCE_DIR}/plugin/src )
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

    foreach( prefix ${CMAKE_CURRENT_SOURCE_DIR}/decode/vp8/src )
    list( APPEND sources
      ${prefix}/mfx_vp8_dec_decode_hw.cpp
      ${prefix}/mfx_vp8_dec_decode.cpp
    )
    endforeach()

    foreach( prefix ${CMAKE_HOME_DIRECTORY}/_studio/shared/umc/io/umc_va/src )
    list( APPEND sources
      ${prefix}/umc_va_linux.cpp
    )
    endforeach()

    set( defs "-DAS_VP8D_PLUGIN -DAS_VP8DHW_PLUGIN -DUMC_ENABLE_VP8_VIDEO_DECODER -DMFX_VA")

  else()

    foreach( prefix ${CMAKE_CURRENT_SOURCE_DIR}/decode/vp8/src )
    list( APPEND sources
      ${prefix}/mfx_vp8_dec_decode.cpp
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
