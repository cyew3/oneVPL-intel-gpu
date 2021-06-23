if (OPEN_SOURCE)
  return()
endif()

if (CMAKE_SYSTEM_NAME MATCHES Windows)
  set(HWA_LIBS av1e_core_hwa av1e_fei_hwa av1_pp_avx2 av1_pp_ssse3 av1_pp_px genx_av1_encode_embeded)
  target_link_libraries(mfxcore
    PRIVATE
      ${HWA_LIBS}
  )

  set(sources_etw
    ${CMAKE_BINARY_DIR}/media_sdk_etw.h
    ${CMAKE_BINARY_DIR}/media_sdk_etw.rc
  )
  set_source_files_properties(${sources_etw}
    PROPERTIES
      GENERATED TRUE
  )


  target_sources(${mfxlibname}
    PRIVATE
      libvpl.def
      libmfx-gen.rc

      ${sources_etw}
  )
endif()

target_sources(mfxcore
  PRIVATE
    shared/src/mfx_check_hardware_support.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/mfx_timing.cpp

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
