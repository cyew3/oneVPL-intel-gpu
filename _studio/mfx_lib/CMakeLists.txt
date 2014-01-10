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
  h265 h264 mpeg2 vc1 mjpeg vp8
  )

list( APPEND vencs
  h265 h264 mpeg2 vc1 mjpeg mvc svc
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

# =============================================================================

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
append_property( hevc_pp_atom COMPILE_FLAGS "-DMFX_MAKENAME_ATOM -DAS_HEVCD_PLUGIN" )

# =============================================================================

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
append_property( hevc_pp_avx2 COMPILE_FLAGS "-DMFX_MAKENAME_AVX2 -DAS_HEVCD_PLUGIN -mavx2")
append_property( hevc_pp_avx2 LINK_FLAGS "-mavx2")

# =============================================================================

set( sources "" )
set( sources.plus "" )

list( APPEND sources 
    ${vdir_optimization}/src/mfx_h265_deblocking_16u_px.cpp
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
append_property( hevc_pp_px COMPILE_FLAGS "-DMFX_MAKENAME_PX -DAS_HEVCD_PLUGIN" )

# =============================================================================

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
append_property( hevc_pp_sse4  COMPILE_FLAGS "-DMFX_MAKENAME_SSE4 -DAS_HEVCD_PLUGIN" )

# =============================================================================

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
append_property( hevc_pp_ssse3  COMPILE_FLAGS "-DMFX_MAKENAME_SSSE3 -DAS_HEVCD_PLUGIN -mavx2" )
append_property( hevc_pp_ssse3  LINK_FLAGS "-mavx2" )

# =============================================================================

set( sources "" )
set( sources.plus "" )

list( APPEND sources 
  ${vdir_optimization}/src/mfx_h265_dispatcher.cpp
)

make_library( hevc_pp_dispatcher none static )
append_property( hevc_pp_dispatcher  COMPILE_FLAGS "-DAS_HEVCD_PLUGIN" )

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

set( sources "" )
set( sources.plus "" )

foreach( prefix ${CMAKE_CURRENT_SOURCE_DIR}/shared/src )
  list( APPEND sources
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
  )
endforeach()

foreach( prefix ${CMAKE_HOME_DIRECTORY}/_studio/shared/src )
  list( APPEND sources
    ${prefix}/fast_copy.cpp
    ${prefix}/libmfx_allocator.cpp
    ${prefix}/libmfx_allocator_vaapi.cpp
    ${prefix}/libmfx_core.cpp
    ${prefix}/libmfx_core_factory.cpp
    ${prefix}/libmfx_core_vaapi.cpp
    ${prefix}/mfx_umc_alloc_wrapper.cpp
  )
endforeach()

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/scheduler/include )

foreach( prefix ${CMAKE_CURRENT_SOURCE_DIR}/scheduler/src )
  list( APPEND sources
    ${prefix}/mfx_scheduler_core.cpp
    ${prefix}/mfx_scheduler_core_iunknown.cpp
    ${prefix}/mfx_scheduler_core_task_management.cpp
    ${prefix}/mfx_scheduler_core_ischeduler.cpp
    ${prefix}/mfx_scheduler_core_task.cpp 
    ${prefix}/mfx_scheduler_core_thread.cpp
  )
endforeach()

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/decode/h265/include )

foreach( prefix ${CMAKE_CURRENT_SOURCE_DIR}/decode/h265/src )
  list( APPEND sources
    ${prefix}/mfx_h265_dec_decode.cpp
  )
endforeach()

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/plugin/include )

foreach( prefix ${CMAKE_CURRENT_SOURCE_DIR}/plugin/src )
  list( APPEND sources
    ${prefix}/mfx_hevc_dec_plugin.cpp
  )
endforeach()

set( USE_STRICT_NAME TRUE )
set(MFX_LDFLAGS "${MFX_ORIG_LDFLAGS} -Wl,--version-script=${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/plugin/libmfxsw_plugin.map" )

#foreach( variant sw hw )
foreach( variant sw )
  if( __ARCH MATCHES intel64 )
    set( plugin_name mfxplugin64_hevcd_${variant} )
  else()
    set( plugin_name mfxplugin32_hevcd_${variant} )
  endif()
  if( __CONFIG MATCHES debug )
    set( plugin_name ${plugin_name}_d )
  endif()

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

  make_library( ${plugin_name} ${variant} shared)
  append_property( ${plugin_name} COMPILE_FLAGS "-DAS_HEVCD_PLUGIN" )  

#  make_library( mfx${variant}_static ${variant} static)
#  message( STATUS "CMAKE_CURRENT_SOURCE_DIR", ${CMAKE_CURRENT_SOURCE_DIR} )
#  message( STATUS "xxxxxxxxxxxxxxx __ARCH: "${__ARCH}, "plugin_name: "${plugin_name}, "variant: "${variant})
endforeach()

# =============================================================================
