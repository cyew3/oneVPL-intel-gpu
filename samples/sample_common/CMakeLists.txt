
add_library(sample_common STATIC)

if (CMAKE_SYSTEM_NAME MATCHES Linux)
  pkg_check_modules(PKG_LIBDRM    REQUIRED QUIET libdrm>=2.4.91 IMPORTED_TARGET)
  pkg_check_modules(PKG_LIBVA     REQUIRED QUIET libva>=1.9     IMPORTED_TARGET GLOBAL)
  pkg_check_modules(PKG_LIBVA_DRM REQUIRED QUIET libva-drm>=1.9 IMPORTED_TARGET GLOBAL)
endif()

target_include_directories(sample_common
  PUBLIC
    include
    include/vm
    ${MSDK_SAMPLES_ROOT}/sample_misc/wayland/include
  )

target_compile_definitions(sample_common
  PUBLIC
    $<$<PLATFORM_ID:Windows>:NOMINMAX>
    $<$<PLATFORM_ID:Linux>: LIBVA_SUPPORT>
    $<$<PLATFORM_ID:Linux>: LIBVA_DRM_SUPPORT>
  PRIVATE
    ${WARNING_FLAGS}
  )

target_compile_options(sample_common
  PUBLIC
    $<$<AND:$<PLATFORM_ID:Windows>,$<BOOL:${MFX_ENABLE_SPECTRE_MITIGATIONS}>>:/Qspectre>
  )

target_compile_features(sample_common PUBLIC cxx_std_11)
target_link_libraries(sample_common
  PUBLIC
    $<$<PLATFORM_ID:Windows>:dxva2>
    $<$<PLATFORM_ID:Windows>:D3D11>
    $<$<PLATFORM_ID:Windows>:DXGI>
    $<$<PLATFORM_ID:Windows>:d3d9>
    $<$<PLATFORM_ID:Linux>:PkgConfig::PKG_LIBVA>
    $<$<PLATFORM_ID:Linux>:PkgConfig::PKG_LIBVA_DRM>
    $<$<PLATFORM_ID:Linux>:PkgConfig::PKG_LIBDRM>
    mfx_common_properties
    ${MFX_DISPATCHER}
    ${CMAKE_THREAD_LIBS_INIT}
    ${CMAKE_DL_LIBS}
  )

set_property(TARGET sample_common PROPERTY FOLDER "samples")

target_sources(sample_common
  PRIVATE
    include/abstract_splitter.h
    include/avc_bitstream.h
    include/avc_headers.h
    include/avc_nal_spl.h
    include/avc_spl.h
    include/avc_structures.h
    include/base_allocator.h
    include/brc_routines.h
    include/current_date.h
    include/d3d11_allocator.h
    include/d3d11_device.h
    include/d3d_allocator.h
    include/d3d_device.h
    include/decode_render.h
    include/general_allocator.h
    include/hw_device.h
    include/intrusive_ptr.h
    include/mfx_buffering.h
    include/mfx_itt_trace.h
    include/mfx_plugin_base.h
    include/mfx_plugin_module.h
    include/mfx_samples_config.h
    include/parameters_dumper.h
    include/plugin_loader.h
    include/plugin_utils.h
    include/preset_manager.h
    include/sample_defs.h
    include/sample_types.h
    include/sample_utils.h
    include/surface_auto_lock.h
    include/sysmem_allocator.h
    include/time_statistics.h
    include/v4l2_util.h
    include/vaapi_allocator.h
    include/vaapi_device.h
    include/vaapi_utils_android.h
    include/vaapi_utils_drm.h
    include/vaapi_utils.h
    include/vaapi_utils_x11.h
    include/version.h
    include/vpp_ex.h

    include/vm/atomic_defs.h
    include/vm/file_defs.h
    include/vm/so_defs.h
    include/vm/strings_defs.h
    include/vm/thread_defs.h
    include/vm/time_defs.h

    src/avc_bitstream.cpp
    src/avc_nal_spl.cpp
    src/avc_spl.cpp
    src/base_allocator.cpp
    src/brc_routines.cpp
    src/d3d11_allocator.cpp
    src/d3d11_device.cpp
    src/d3d_allocator.cpp
    src/d3d_device.cpp
    src/decode_render.cpp
    src/general_allocator.cpp
    src/mfx_buffering.cpp
    src/parameters_dumper.cpp
    src/plugin_utils.cpp
    src/preset_manager.cpp
    src/sample_utils.cpp
    src/sysmem_allocator.cpp
    src/v4l2_util.cpp
    src/vaapi_allocator.cpp
    src/vaapi_device.cpp
    src/vaapi_utils_android.cpp
    src/vaapi_utils.cpp
    src/vaapi_utils_drm.cpp
    src/vaapi_utils_x11.cpp
    src/vpp_ex.cpp

    src/vm/atomic.cpp
    src/vm/atomic_linux.cpp
    src/vm/shared_object.cpp
    src/vm/shared_object_linux.cpp
    src/vm/thread_linux.cpp
    src/vm/thread_windows.cpp
    src/vm/time.cpp
    src/vm/time_linux.cpp
  )