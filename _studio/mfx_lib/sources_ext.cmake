if (CMAKE_SYSTEM_NAME MATCHES Windows)
  set(HWA_LIBS av1e_core_hwa av1e_fei_hwa av1_pp_avx2 av1_pp_ssse3 av1_pp_px genx_av1_encode_embeded)
  target_link_libraries(${mfxlibname}
    PRIVATE
      ${HWA_LIBS}
  )
endif()

list(APPEND sources_common
    shared/src/mfx_check_hardware_support.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/mfx_timing.cpp
    $<$<PLATFORM_ID:Windows>:libvpl.def>
    $<$<PLATFORM_ID:Windows>:libmfx-gen.rc>
)

list(APPEND sources_hw
    ${MSDK_STUDIO_ROOT}/shared/src/fast_compositing_ddi.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/libmfx_allocator_d3d9.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/libmfx_allocator_d3d11.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/libmfx_core_d3d9.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/libmfx_core_d3d11.cpp
    ${MSDK_STUDIO_ROOT}/shared/include/mfx_ext_ddi.h
    ${MSDK_STUDIO_ROOT}/shared/src/auxiliary_device.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/libmfx_core_d3d9on11.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/d3d11_decode_accelerator.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/d3d11_video_processor.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/mfx_dxva2_device.cpp
  )

return()

#include(sources_libaudio.cmake)
#include(sources_librt.cmake)
