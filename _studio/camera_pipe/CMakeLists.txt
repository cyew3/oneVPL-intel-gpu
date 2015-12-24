set(MFX_ORIG_LDFLAGS "${MFX_LDFLAGS}" )

# Plugin version info

set (CAMERA_PRODUCT_NAME "Intel(R) Media Server Studio 2015 - RAW Accelerator")
set (CAMERA_DESCRIPTION "Intel(R) Media Server Studio 2015 - RAW Accelerator Plug-in")

  if( NOT DEFINED ENV{MFX_CAMERA_VERSION} )
    set( camera_version 0.0.000.0000 )
  else( )
    set( camera_version $ENV{MFX_CAMERA_VERSION} )
  endif( )

  if( Linux OR Darwin )
    execute_process(
      COMMAND echo
      COMMAND cut -f 1 -d.
      COMMAND date "+.%-y.%-m.%-d"
      OUTPUT_VARIABLE cur_date
      OUTPUT_STRIP_TRAILING_WHITESPACE
      )
    string( SUBSTRING ${camera_version} 0 1 ver )

    set(camera_version_defs  "-DMFX_PLUGIN_FILE_VERSION=\"\\\"${ver}${cur_date}\"\\\" -DMFX_PLUGIN_PRODUCT_VERSION=\"\\\"${camera_version}\"\\\"")
  endif( )


# =============================================================================

mfx_include_dirs( )

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../mfx_lib/plugin/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/genx/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/genx/src )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../mfx_lib/cmrt_cross_platform/include )

# =============================================================================

#ptir_plugin shared lib
set( sources "" )
set( sources.plus "" )

set( USE_STRICT_NAME TRUE )
set(MFX_LDFLAGS "${MFX_ORIG_LDFLAGS} -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/../mfx_lib/plugin/libmfxsw_plugin.map" )


list( APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/genx/src/genx_hsw_camerapipe_isa.cpp )
list( APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/genx/src/genx_bdw_camerapipe_isa.cpp )
list( APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/genx/src/genx_skl_camerapipe_isa.cpp )
foreach( prefix ${CMAKE_CURRENT_SOURCE_DIR}/../mfx_lib/shared/src )

  list( APPEND sources
    ${prefix}/mfx_check_hardware_support.cpp
    ${prefix}/mfx_common_int.cpp
    ${prefix}/mfx_common_decode_int.cpp
    ${prefix}/libmfxsw.cpp
    ${prefix}/libmfxsw_async.cpp
    ${prefix}/libmfxsw_query.cpp
    ${prefix}/libmfxsw_session.cpp
    ${prefix}/mfx_session.cpp
    ${prefix}/mfx_enc_common.cpp
  )

endforeach()

foreach( prefix ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src )
  list( APPEND sources
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
list( APPEND sources
  ${CMAKE_CURRENT_SOURCE_DIR}/../mfx_lib/cmrt_cross_platform/src/cmrt_cross_platform.cpp
)

foreach( prefix ${CMAKE_CURRENT_SOURCE_DIR}/../mfx_lib/scheduler/src )
  list( APPEND sources
    ${prefix}/mfx_scheduler_core.cpp
    ${prefix}/mfx_scheduler_core_iunknown.cpp
    ${prefix}/mfx_scheduler_core_ischeduler.cpp
    ${prefix}/mfx_scheduler_core_task.cpp
    ${prefix}/mfx_scheduler_core_task_management.cpp
    ${prefix}/mfx_scheduler_core_thread.cpp
  )
endforeach()

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../mfx_lib/vpp/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../mfx_lib/scheduler/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../shared/umc/codec/color_space_converter/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../shared/umc/codec/scene_analyzer/include/ )

list( APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/../mfx_lib/shared/src/libmfxsw_vpp.cpp )
list( APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/../mfx_lib/vpp/src/mfx_vpp_utils.cpp )
list( APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/../mfx_lib/vpp/src/mfx_vpp_main.cpp )
list( APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/../mfx_lib/vpp/src/mfx_vpp_sw_core.cpp )
list( APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/../mfx_lib/vpp/src/mfx_vpp_sw_internal.cpp )
list( APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/../mfx_lib/vpp/src/mfx_vpp_sw_threading.cpp )
list( APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/../mfx_lib/vpp/src/mfx_vpp_factory.cpp )
list( APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/../mfx_lib/vpp/src/mfx_vpp_service.cpp )
list( APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/../mfx_lib/vpp/src/mfx_vpp_hw.cpp )
list( APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/mfx_vpp_vaapi.cpp )
list( APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/../mfx_lib/vpp/src/mfx_vpp_svc.cpp )
list( APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/../mfx_lib/vpp/src/mfx_vpp_mvc.cpp )
list( APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/../mfx_lib/vpp/src/mfx_denoise_vpp.cpp )
list( APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/../mfx_lib/vpp/src/mfx_resize_vpp.cpp )
list( APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/../mfx_lib/vpp/src/mfx_frame_rate_conversion_vpp.cpp )
list( APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/../mfx_lib/vpp/src/mfx_deinterlace_vpp.cpp )
list( APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/../mfx_lib/vpp/src/mfx_shift_vpp.cpp )
list( APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/../mfx_lib/vpp/src/mfx_color_space_conversion_vpp.cpp )
list( APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/../mfx_lib/vpp/src/mfx_video_signal_conversion_vpp.cpp )
list( APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/../mfx_lib/vpp/src/mfx_procamp_vpp.cpp )
list( APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/../mfx_lib/vpp/src/mfx_detail_enhancement_vpp.cpp )
list( APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/../mfx_lib/vpp/src/mfx_video_analysis_vpp.cpp )

# ===============================================
list( APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/../shared/umc/codec/scene_analyzer/src/umc_scene_analyzer_base.cpp )
list( APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/../shared/umc/codec/scene_analyzer/src/umc_scene_analyzer.cpp )
list( APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/../shared/umc/codec/scene_analyzer/src/umc_scene_analyzer_p_func.cpp )
list( APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/../shared/umc/codec/scene_analyzer/src/umc_scene_analyzer_p_func_mb.cpp )
list( APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/../shared/umc/codec/scene_analyzer/src/umc_scene_analyzer_mb_func.cpp )
list( APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/../shared/umc/codec/scene_analyzer/src/umc_scene_analyzer_frame.cpp )
list( APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/../shared/umc/codec/scene_analyzer/src/umc_scene_analyzer_p.cpp )
list( APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/../shared/umc/io/umc_va/src/umc_va_linux.cpp )
list( APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/../shared/umc/io/umc_va/src/umc_va_video_processing.cpp )
list( APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/../shared/umc/io/umc_va/src/umc_va_linux_protected.cpp )
list( APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/../shared/umc/io/umc_va/src/umc_va_dxva2_protected.cpp )
# ===============================================

foreach( dir ${CMAKE_CURRENT_SOURCE_DIR}/src )
  file( GLOB_RECURSE srcs "${dir}/*.c" "${dir}/*.cpp" )
  list( APPEND sources ${srcs})
endforeach()

  set( LIBS "" )
  list( APPEND LIBS
    umc_codecs_merged
    color_space_converter_sw
    media_buffers
    umc_io
    umc_va_elk
    umc
    vm
    vm_plus
    ${ITT_LIBS}
    pthread
    dl
    ippmsdk_l ippj_l ippvc_l ippcc_l ippcv_l ippi_l ipps_l ippcore_l
  )

set( defs "-DAS_CAMERA_PLUGIN -DAS_VPP_PLUGIN -DMFX_PLUGIN_PRODUCT_NAME=\"\\\"${CAMERA_PRODUCT_NAME}\"\\\" -DMFX_FILE_DESCRIPTION=\"\\\"${CAMERA_DESCRIPTION}\"\\\" ${camera_version_defs}" )

make_library( mfx_camera_hw64 hw shared )
