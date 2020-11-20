if(ENABLE_AV1)
  add_library(av1_fei STATIC
    av1/src/mfx_av1_enc_cm_fei.cpp
    av1/src/mfx_av1_enc_cm_plugin.cpp
    av1/src/mfx_av1_enc_cm_utils.cpp
    av1/include/mfx_av1_enc_cm_fei.h
    av1/include/mfx_av1_enc_cm_plugin.h
    av1/include/mfx_av1_enc_cm_utils.h
    av1/include/mfx_av1_fei.h
  )

  target_include_directories(av1_fei PUBLIC av1/include)
  target_compile_definitions(av1_fei
    PRIVATE
      MFX_VA
  )
  target_link_libraries(av1_fei PUBLIC mfx_static_lib genx_av1_encode_embeded umc cmrt_cross_platform_hw)
endif()

if(ENABLE_HEVC)
  add_library(h265_fei STATIC
    h265/src/mfx_h265_enc_cm_fei.cpp
    h265/src/mfx_h265_enc_cm_plugin.cpp
    h265/src/mfx_h265_enc_cm_utils.cpp
    h265/include/mfx_h265_enc_cm_fei.h
    h265/include/mfx_h265_enc_cm_plugin.h
    h265/include/mfx_h265_enc_cm_utils.h
    h265/include/mfx_h265_fei.h
  )

  target_include_directories(h265_fei PUBLIC h265/include)
  target_compile_definitions(h265_fei
    PRIVATE
      MFX_VA
  )
  target_link_libraries(h265_fei PUBLIC mfx_static_lib genx_h265_encode_embeded umc cmrt_cross_platform_hw)
endif()