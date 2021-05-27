if (OPEN_SOURCE)
  return()
endif()

  set( av1e_fei_hwa_sources
    av1/include/mfx_av1_enc_cm_fei.h
    av1/include/mfx_av1_enc_cm_utils.h
    av1/include/mfx_av1_fei.h

    av1/src/mfx_av1_enc_cm_fei.cpp
    av1/src/mfx_av1_enc_cm_utils.cpp
  )

  add_library(av1e_fei_hwa STATIC ${av1e_fei_hwa_sources})

  set_property(TARGET av1e_fei_hwa PROPERTY FOLDER "av1e_hwa")

  target_include_directories(av1e_fei_hwa
    PUBLIC
      av1/include
      ../encode/av1/include
      ../cmrt_cross_platform/include
      ../genx/av1_encode/include
      ${MSDK_STUDIO_ROOT}/shared/umc/core/umc/include
      ${MSDK_STUDIO_ROOT}/shared/umc/core/vm/include
      ${MSDK_STUDIO_ROOT}/shared/umc/core/vm_plus/include
  )
  target_link_libraries(av1e_fei_hwa
    PUBLIC
      mfx_static_lib
      ${IPP_LIBS}
  )

