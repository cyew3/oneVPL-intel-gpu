#ptir_plugin shared lib

set( USE_STRICT_NAME TRUE )

add_library( mfx_camera_hw64 SHARED )

target_include_directories(mfx_camera_hw64
  PUBLIC
    include
    ${MSDK_LIB_ROOT}/plugin/include
  )

target_link_options(mfx_camera_hw64
  PRIVATE
    $<$<PLATFORM_ID:Linux>:-Wl,--version-script=${MSDK_LIB_ROOT}/plugin/libmfxsw_plugin.map>
  )

target_sources(mfx_camera_hw64
  PRIVATE
    src/mfx_camera_plugin_cpu.cpp
    src/mfx_camera_plugin_dx9.cpp
    src/mfx_camera_plugin_dx11.cpp
    src/mfx_camera_plugin_utils.cpp
    src/mfx_camera_plugin.cpp
    src/mfx_camera_plugin_cm.cpp
    ${MSDK_LIB_ROOT}/shared/src/libmfxsw_vpp.cpp
    ${MSDK_LIB_ROOT}/vpp/src/mfx_vpp_utils.cpp
    ${MSDK_LIB_ROOT}/vpp/src/mfx_vpp_main.cpp
    ${MSDK_LIB_ROOT}/vpp/src/mfx_vpp_sw_core.cpp
    ${MSDK_LIB_ROOT}/vpp/src/mfx_vpp_sw_internal.cpp
    ${MSDK_LIB_ROOT}/vpp/src/mfx_vpp_sw_threading.cpp
    ${MSDK_LIB_ROOT}/vpp/src/mfx_vpp_factory.cpp
    ${MSDK_LIB_ROOT}/vpp/src/mfx_vpp_service.cpp
    ${MSDK_LIB_ROOT}/vpp/src/mfx_vpp_hw.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/mfx_vpp_vaapi.cpp
    ${MSDK_LIB_ROOT}/vpp/src/mfx_vpp_svc.cpp
    ${MSDK_LIB_ROOT}/vpp/src/mfx_vpp_mvc.cpp
    ${MSDK_LIB_ROOT}/vpp/src/mfx_denoise_vpp.cpp
    ${MSDK_LIB_ROOT}/vpp/src/mfx_resize_vpp.cpp
    ${MSDK_LIB_ROOT}/vpp/src/mfx_frame_rate_conversion_vpp.cpp
    ${MSDK_LIB_ROOT}/vpp/src/mfx_deinterlace_vpp.cpp
    ${MSDK_LIB_ROOT}/vpp/src/mfx_shift_vpp.cpp
    ${MSDK_LIB_ROOT}/vpp/src/mfx_color_space_conversion_vpp.cpp
    ${MSDK_LIB_ROOT}/vpp/src/mfx_video_signal_conversion_vpp.cpp
    ${MSDK_LIB_ROOT}/vpp/src/mfx_procamp_vpp.cpp
    ${MSDK_LIB_ROOT}/vpp/src/mfx_detail_enhancement_vpp.cpp
    ${MSDK_LIB_ROOT}/vpp/src/mfx_video_analysis_vpp.cpp
    ${MSDK_LIB_ROOT}/scheduler/linux/src/mfx_scheduler_core.cpp
    ${MSDK_LIB_ROOT}/scheduler/linux/src/mfx_scheduler_core_iunknown.cpp
    ${MSDK_LIB_ROOT}/scheduler/linux/src/mfx_scheduler_core_ischeduler.cpp
    ${MSDK_LIB_ROOT}/scheduler/linux/src/mfx_scheduler_core_task.cpp
    ${MSDK_LIB_ROOT}/scheduler/linux/src/mfx_scheduler_core_task_management.cpp
    ${MSDK_LIB_ROOT}/scheduler/linux/src/mfx_scheduler_core_thread.cpp
    ${MSDK_LIB_ROOT}/cmrt_cross_platform/src/cmrt_cross_platform.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/cm_mem_copy.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/fast_copy_c_impl.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/fast_copy_sse4_impl.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/fast_copy.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/fast_copy_multithreading.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/libmfx_allocator.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/libmfx_allocator_vaapi.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/libmfx_core.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/libmfx_core_factory.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/libmfx_core_hw.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/libmfx_core_vaapi.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/mfx_umc_alloc_wrapper.cpp
    ${MSDK_LIB_ROOT}/shared/src/mfx_check_hardware_support.cpp
    ${MSDK_LIB_ROOT}/shared/src/mfx_common_int.cpp
    ${MSDK_LIB_ROOT}/shared/src/mfx_common_decode_int.cpp
    ${MSDK_LIB_ROOT}/shared/src/libmfxsw.cpp
    ${MSDK_LIB_ROOT}/shared/src/libmfxsw_async.cpp
    ${MSDK_LIB_ROOT}/shared/src/libmfxsw_query.cpp
    ${MSDK_LIB_ROOT}/shared/src/libmfxsw_session.cpp
    ${MSDK_LIB_ROOT}/shared/src/mfx_session.cpp
    ${MSDK_LIB_ROOT}/shared/src/mfx_enc_common.cpp
    genx/src/genx_hsw_camerapipe_isa.cpp
    genx/src/genx_bdw_camerapipe_isa.cpp
    genx/src/genx_skl_camerapipe_isa.cpp
    genx/src/genx_cnl_camerapipe_isa.cpp
  )


if(NOT OPEN_SOURCE)
  if (MFX_BUNDLED_IPP)
    set(IPPLIBS ipp)
  else()
    set(IPPLIBS IPP::msdk IPP::j IPP::vc IPP::cc IPP::cv IPP::i IPP::s IPP::core)
  endif()
endif()

target_link_libraries(mfx_camera_hw64
  PUBLIC
    mfx_shared_lib
    umc_codecs_merged
    color_space_converter_sw
    scene_analyzer
    media_buffers
    umc_io
    umc_va_hw
    umc
    vm
    vm_plus
    genx
    mfx_trace
    ${ITT_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
    ${CMAKE_DL_LIBS}
    ${IPPLIBS}
#====MCTF
    asc
    mctf_hw
  PRIVATE
    mfx_sdl_properties
  )

target_compile_definitions(mfx_camera_hw64
  PRIVATE
    AS_CAMERA_PLUGIN
    AS_VPP_PLUGIN
    MFX_PLUGIN_PRODUCT_NAME=\"\\\"${CAMERA_PRODUCT_NAME}\"\\\"
    MFX_FILE_DESCRIPTION=\"\\\"${CAMERA_DESCRIPTION}\"\\\"
    MFX_PLUGIN_PRODUCT_VERSION=\"\\\"${MEDIA_VERSION_STR}\"\\\"
    ${camera_version_defs}"
  )