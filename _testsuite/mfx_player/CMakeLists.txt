if(MFX_BUNDLED_IPP OR (NOT BUILD_SAMPLES) OR (MFX_DISABLE_SW_FALLBACK AND NOT BUILD_VAL_TOOLS))
  return()
endif()

if (CMAKE_SYSTEM_NAME MATCHES Linux)
  pkg_check_modules(PKG_LIBDRM REQUIRED libdrm IMPORTED_TARGET)
endif()

set(common_mfx_pipeline_sources
  src/mf_hw_platform.cpp
  src/mfx_advance_decoder.cpp
  src/mfx_anchors_decoder.cpp
  src/mfx_base_allocator.cpp
  src/mfx_bayer_reader.cpp
  src/mfx_bitrate_limited_reader.cpp
  src/mfx_bitstream_buffer.cpp
  src/mfx_bitstream_decoder.cpp
  src/mfx_bitstream_reader_encryptor.cpp
  src/mfx_buffered_bs_reader.cpp
  src/mfx_burst_render.cpp
  src/mfx_cmd_option_processor.cpp
  src/mfx_commands.cpp
  src/mfx_commands_initializer.cpp
  src/mfx_component_params.cpp
  src/mfx_d3d_allocator.cpp
  src/mfx_d3d11_allocator.cpp
  src/mfx_d3d11_device.cpp
  src/mfx_d3d9_device.cpp
  src/mfx_decode_pipeline_config.cpp
  src/mfx_default_pipeline_mgr.cpp
  src/mfx_directory_reader.cpp
  src/mfx_extended_buffer.cpp
  src/mfx_factory_default.cpp
  src/mfx_fps_limit_render.cpp
  src/mfx_frame_constructor.cpp
  src/mfx_latency_decoder.cpp
  src/mfx_latency_vpp.cpp
  src/mfx_loop_decoder.cpp
  src/mfx_lucas_yuv_reader.h
  src/mfx_mediasdk_spl_wrapper.cpp
  src/mfx_metric_calc.cpp
  src/mfx_mkv_reader.cpp
  src/mfx_multi_render.cpp
  src/mfx_mvc_decoder.cpp
  src/mfx_mvc_target_views_decoder.cpp
  src/mfx_outline_render.cpp
  src/mfx_pipeline_dec.cpp
  src/mfx_pipeline_protected.cpp
  src/mfx_pipeline_utils.cpp
  src/mfx_pts_based_activator.cpp
  src/mfx_render_android.cpp
  src/mfx_renders.cpp
  src/mfx_reorder_render.cpp
  src/mfx_screen_render.cpp
  src/mfx_serializer.cpp
  src/mfx_spl_wrapper.cpp
  src/mfx_sysmem_allocator.cpp
  src/mfx_timeout.cpp
  src/mfx_vaapi_device.cpp
  src/mfx_vaapi_device_android.cpp
  src/mfx_vaapi_render.cpp
  src/mfx_view_ordered_render.cpp
  src/mfx_yuv_decoder.cpp
  src/trace_utils.cpp
  src/umc_corruption_reader.cpp
  src/vaapi_allocator.cpp
  src/vaapi_utils.cpp
  src/vaapi_utils_android.cpp
  src/vaapi_utils_drm.cpp
  src/vaapi_utils_x11.cpp
  src/video_window.cpp
  src/xvideo_window.cpp
   $<$<BOOL:${API_USE_VPL}>:${MSDK_STUDIO_ROOT}/shared/src/mfx_dxva2_device.cpp>
)

set(common_linux_defs
  LINUX32
  $<$<EQUAL:${CMAKE_SIZEOF_VOID_P},8>:LINUX64>
)

set(mfx_dispatch_include "")
if (API_USE_VPL)
  set(mfx_dispatch_include
    ${MFX_API_HOME}/../mfx_dispatch/vpl/$<IF:$<PLATFORM_ID:Linux>,linux,windows>
    ${MFX_API_HOME}/../mfx_dispatch/vpl
  )
else()
  set(mfx_dispatch_include ${MFX_API_HOME}/../mfx_dispatch/$<IF:$<PLATFORM_ID:Linux>,linux,windows>/include)
endif()

# ============================= mfx_pipeline ================================================

add_library(mfx_pipeline STATIC ${common_mfx_pipeline_sources})

target_compile_options(mfx_pipeline
  PRIVATE
    $<IF:$<PLATFORM_ID:Linux>,-U MFX_DISABLE_SW_FALLBACK,/UMFX_DISABLE_SW_FALLBACK>
)

target_include_directories(mfx_pipeline
  PUBLIC
    ${MSDK_STUDIO_ROOT}/shared/include
    ${MSDK_UMC_ROOT}/test_suite/spy_test_component/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/include
    ${mfx_dispatch_include}
)

target_compile_definitions(mfx_pipeline
  PRIVATE
    MFX_VERSION_USE_LATEST
    $<$<PLATFORM_ID:Linux>:${common_linux_defs}>
    $<$<BOOL:${API_USE_VPL}>:MFX_ONEVPL>
)

target_link_libraries(mfx_pipeline
  PUBLIC
    mfx_static_lib
    spl_common
    media_buffers
    umc_io
    demuxer
    avi_spl
    vc1_spl
    asf_spl
    mpeg4_spl
    vm
    ${MFX_API_TARGET}
    ${MFX_DISPATCHER}
     $<$<AND:$<PLATFORM_ID:Windows>,$<NOT:$<BOOL:${API_USE_VPL}>>>:sample_protected>
    $<$<PLATFORM_ID:Windows>:dxva2_dump>
    $<$<AND:$<PLATFORM_ID:Windows>,$<NOT:$<BOOL:${API_USE_VPL}>>>:mfx_dispatch_trace>
    $<$<PLATFORM_ID:Windows>:sample_spl_mux_dispatcher_v15>
    $<$<PLATFORM_ID:Linux>:PkgConfig::PKG_LIBDRM>
    $<$<PLATFORM_ID:Linux>:sample_spl_mux_dispatcher>
    IPP::i IPP::s IPP::vm IPP::dc IPP::vc IPP::dc IPP::cc IPP::cp IPP::cv IPP::j IPP::msdk IPP::core
    ${CMAKE_DL_LIBS}
  PRIVATE
    mfx_va_properties
)

# ============================= mfx_pipeline_lucas ================================================

add_library(mfx_pipeline_lucas STATIC ${common_mfx_pipeline_sources})

target_include_directories(mfx_pipeline_lucas
  PUBLIC
    ${MSDK_STUDIO_ROOT}/shared/include
    ${MSDK_UMC_ROOT}/test_suite/spy_test_component/include
    ${CMAKE_HOME_DIRECTORY}/${SAMPLES_DIR}/samples/deprecated/sample_spl_mux/api
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/include
    ${mfx_dispatch_include}
)

target_compile_options(mfx_pipeline_lucas
  PRIVATE
    $<IF:$<PLATFORM_ID:Linux>,-U MFX_DISABLE_SW_FALLBACK,/UMFX_DISABLE_SW_FALLBACK>
)

target_link_libraries(mfx_pipeline_lucas
  PUBLIC
    vm
    mfx_static_lib
    media_buffers
    spl_common
    umc_io
    demuxer
    avi_spl
    vc1_spl
    asf_spl
    mpeg4_spl
    vm
    $<$<AND:$<PLATFORM_ID:Windows>,$<NOT:$<BOOL:${API_USE_VPL}>>>:sample_protected>
    $<$<PLATFORM_ID:Windows>:dxva2_dump>
    $<$<AND:$<PLATFORM_ID:Windows>,$<NOT:$<BOOL:${API_USE_VPL}>>>:mfx_dispatch_trace>
    $<$<PLATFORM_ID:Windows>:sample_spl_mux_dispatcher_v15>
    $<$<PLATFORM_ID:Linux>:PkgConfig::PKG_LIBDRM>
    $<$<PLATFORM_ID:Linux>:sample_spl_mux_dispatcher>
    IPP::i IPP::s IPP::vm IPP::dc IPP::vc IPP::dc  IPP::cc IPP::cp IPP::cv IPP::j IPP::msdk IPP::core
  PRIVATE
    mfx_va_properties
)

target_compile_definitions(mfx_pipeline_lucas
  PRIVATE
    MFX_VERSION_USE_LATEST
    LUCAS_DLL
    $<$<PLATFORM_ID:Linux>:${common_linux_defs}>
    $<$<BOOL:${API_USE_VPL}>:MFX_ONEVPL>
)

set_property(TARGET mfx_pipeline mfx_pipeline_lucas PROPERTY FOLDER "testsuite")


# ============================= mfx_player ================================================

add_executable(mfx_player)

target_sources(mfx_player
  PRIVATE
    src/mfx_player.cpp
)

target_compile_definitions(mfx_player
  PRIVATE
    MFX_VERSION_USE_LATEST
    $<$<PLATFORM_ID:Linux>:${common_linux_defs}>
    $<$<BOOL:${API_USE_VPL}>:MFX_ONEVPL>
)

target_link_libraries(mfx_player
  PUBLIC
    mfx_pipeline
    shared_utils
    umc
    vm
    vm_plus
    mfx_trace
    ${ITT_LIBRARIES}
    mfx_shared_lib
    media_buffers
    ${CMAKE_THREAD_LIBS_INIT}
    IPP::i IPP::s IPP::vm IPP::dc IPP::vc IPP::cc IPP::cp IPP::cv IPP::j IPP::msdk IPP::core
    ${CMAKE_DL_LIBS}
)

# ============================= mfx_player_lucas ================================================

add_library(mfx_player_lucas SHARED)

target_sources(mfx_player_lucas
  PRIVATE
    src/mfx_player.cpp
)

target_compile_definitions(mfx_player_lucas
  PRIVATE
    MFX_VERSION_USE_LATEST
    LUCAS_DLL
    $<$<PLATFORM_ID:Linux>:${common_linux_defs}>
)

target_link_libraries(mfx_player_lucas
  PUBLIC
    mfx_pipeline_lucas
    shared_utils_lucas
    umc
    vm
    vm_plus
    mfx_trace
    ${ITT_LIBRARIES}
    mfx_shared_lib
    ${CMAKE_THREAD_LIBS_INIT}
    ${CMAKE_DL_LIBS}
)

set_property(TARGET mfx_player mfx_player_lucas PROPERTY FOLDER "testsuite")
