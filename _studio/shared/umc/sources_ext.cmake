if (OPEN_SOURCE)
  return()
endif()

target_sources(umc_va_hw
  PRIVATE
    io/umc_va/include/umc_av1_ddi.h
    io/umc_va/include/umc_hevc_ddi.h
    io/umc_va/include/umc_jpeg_ddi.h
    io/umc_va/include/umc_mf_decl.h
    io/umc_va/include/umc_mvc_ddi.h
    io/umc_va/include/umc_profile_level.h
    io/umc_va/include/umc_svc_ddi.h
    io/umc_va/include/umc_va_dxva2.h
    io/umc_va/include/umc_va_dxva2_protected.h
    io/umc_va/include/umc_vp8_ddi.h
    io/umc_va/include/umc_vp9_ddi.h
    io/umc_va/include/umc_win_event_cache.h

    io/umc_va/src/umc_profile_level.cpp
    io/umc_va/src/umc_va_dxva2.cpp
    io/umc_va/src/umc_va_dxva2_protected.cpp
    io/umc_va/src/umc_win_event_cache.cpp
  )