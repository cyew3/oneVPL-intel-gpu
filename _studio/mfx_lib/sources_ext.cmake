if (CMAKE_SYSTEM_NAME MATCHES Windows)
  set(HWA_LIBS av1e_core_hwa av1e_fei_hwa av1_pp_avx2 av1_pp_ssse3 av1_pp_px genx_av1_encode_embeded)
  target_link_libraries(${mfxlibname}
    PRIVATE
      ${HWA_LIBS}
  )
endif()

if (MFX_DISABLE_SW_FALLBACK)
  return()
endif()

return()

#include(sources_libaudio.cmake)
#include(sources_librt.cmake)
