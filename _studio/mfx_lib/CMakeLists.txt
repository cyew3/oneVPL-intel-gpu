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
  h264 mpeg2 vc1 mjpeg vp8 vp9
  )

list( APPEND vencs
  h264 mpeg2 vc1 mjpeg mvc svc
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

list( APPEND vdir_optimization optimization/h265 optimization/h264)

list( APPEND vdirs scheduler )
list( APPEND vdirs_impl vpp genx/h264_encode cmrt_cross_platform )

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/genx/h265_encode/include )

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
  include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../shared/umc/codec/${dir}/include )
endforeach( )

foreach( dir ${adirs} ${vdirs} ${vdirs_impl} ${vdirs_hw} ${vdir_optimization} )
  include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/${dir}/include )
endforeach( )

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/decode/h265/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/encode/h265/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/fei/h264_preenc )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/fei/h264_enc )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/fei/h264_pak )
include_directories( ${CMAKE_HOME_DIRECTORY}/mdp_msdk-contrib/vp8_libvpx )

# =============================================================================

set( sources "" )
set( sources.plus "" )
foreach( dir ${CMAKE_CURRENT_SOURCE_DIR}/fei/h264_la )
  file( GLOB_RECURSE srcs "${dir}/*.c" "${dir}/*.cpp" )
  list( APPEND sources ${srcs})
endforeach()

set( defs "-DMFX_VA -DAS_H264LA_PLUGIN" )
make_library( h264_la none static )

set( defs "" )

# =============================================================================
set( sources "" )
set( sources.plus "" )
foreach( dir ${CMAKE_CURRENT_SOURCE_DIR}/fei/h264_preenc )
  file( GLOB_RECURSE srcs "${dir}/*.c" "${dir}/*.cpp" )
  list( APPEND sources ${srcs})
endforeach()
set( defs "-DMFX_VA" )
make_library( h264_preenc none static )

set( defs "" )

# =============================================================================
set( sources "" )
set( sources.plus "" )
foreach( dir ${CMAKE_CURRENT_SOURCE_DIR}/fei/h264_enc )
  file( GLOB_RECURSE srcs "${dir}/*.c" "${dir}/*.cpp" )
  list( APPEND sources ${srcs})
endforeach()
set( defs "-DMFX_VA" )
make_library( h264_enc none static )

set( defs "" )

# =============================================================================
set( sources "" )
set( sources.plus "" )
foreach( dir ${CMAKE_CURRENT_SOURCE_DIR}/fei/h264_pak )
  file( GLOB_RECURSE srcs "${dir}/*.c" "${dir}/*.cpp" )
  list( APPEND sources ${srcs})
endforeach()
set( defs "-DMFX_VA" )
make_library( h264_pak none static )

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

# too many test_XXX files in this dir, so have to manually specify needed ones
foreach( dir ${CMAKE_CURRENT_SOURCE_DIR}/genx/h265_encode/src ) 
#  list( APPEND sources ${dir}/embed_isa.c )
#  list( APPEND sources ${dir}/genx_h265_cmcode.cpp )
#  list( APPEND sources ${dir}/genx_h265_cmcode_bdw.cpp )
#  list( APPEND sources ${dir}/genx_h265_cmcode_bdw_isa.cpp )
#  list( APPEND sources ${dir}/genx_h265_cmcode_isa.cpp )
#  list( APPEND sources ${dir}/genx_h265_cmcode_stub.cpp )
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
    ${prefix}/mfx_h264_fei_vaapi.cpp
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
    ${prefix}/libmfx_core_vaapi.cpp
    ${prefix}/libmfx_core_vdaapi.cpp
    ${prefix}/mfx_umc_alloc_wrapper.cpp
    ${prefix}/mfx_dxva2_device.cpp
    ${prefix}/mfx_static_assert_structs.cpp
  )
endforeach()

list( APPEND sources
  ${CMAKE_CURRENT_SOURCE_DIR}/cmrt_cross_platform/src/cmrt_cross_platform.cpp
)

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
    mfx_vlib_merged
    mfx_vlib_merged_${variant}
    umc_codecs_merged_${variant}
    umc_codecs_merged
    umc_io_merged_${variant}
    umc_core_merged
    umc_brc_sw
    umc_csc_sw
    umc_h264_enc_sw
    h264_la
    h264_preenc
    h264_enc
    h264_pak
    h264_px
    h264_dispatcher
    h264_sse3
    vpx
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

make_library( ${mfxrtlibname} hw shared)
append_property( ${mfxrtlibname} COMPILE_FLAGS " -DMFX_RT" )  

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

# =============================================================================
