if(OPEN_SOURCE)
  return()
endif()

if(CMAKE_SYSTEM_NAME MATCHES Windows)
  add_library(genx_av1_encode_embeded STATIC)
  set_property(TARGET genx_av1_encode_embeded PROPERTY FOLDER "kernels")

  target_sources(genx_av1_encode_embeded
    PRIVATE
      av1_encode/src/genx_av1_hme_and_me_p32_4mv_gen12lp_isa.cpp
      av1_encode/src/genx_av1_interpolate_decision_single_gen12lp_isa.cpp
      av1_encode/src/genx_av1_interpolate_decision_gen12lp_isa.cpp
      av1_encode/src/genx_av1_intra_gen12lp_isa.cpp
      av1_encode/src/genx_av1_mepu_gen12lp_isa.cpp
      av1_encode/src/genx_av1_me_p16_4mv_and_refine_32x32_gen12lp_isa.cpp
      av1_encode/src/genx_av1_mode_decision_pass2_gen12lp_isa.cpp
      av1_encode/src/genx_av1_mode_decision_gen12lp_isa.cpp
      av1_encode/src/genx_av1_prepare_src_gen12lp_isa.cpp
      av1_encode/src/genx_av1_refine_me_p_64x64_gen12lp_isa.cpp
  )

  target_link_libraries(genx_av1_encode_embeded
    PUBLIC
      mfx_static_lib
    PRIVATE
      mfx_sdl_properties
    )

  target_include_directories(genx_av1_encode_embeded
    PUBLIC
      av1_encode/include
  )
endif()

if(ENABLE_HEVC AND CMAKE_SYSTEM_NAME MATCHES Windows)
  add_library(genx_h265_encode_embeded STATIC)
  set_property(TARGET genx_h265_encode_embeded PROPERTY FOLDER "kernels")

  target_sources(genx_h265_encode_embeded
    PRIVATE
      h265_encode/src/genx_hevce_analyze_gradient_32x32_best_bdw_isa.cpp
      h265_encode/src/genx_hevce_analyze_gradient_32x32_best_hsw_isa.cpp
      h265_encode/src/genx_hevce_analyze_gradient_32x32_best_icllp_isa.cpp
      h265_encode/src/genx_hevce_analyze_gradient_32x32_best_icl_isa.cpp
      h265_encode/src/genx_hevce_analyze_gradient_32x32_best_skl_isa.cpp
      h265_encode/src/genx_hevce_birefine_bdw_isa.cpp
      h265_encode/src/genx_hevce_birefine_hsw_isa.cpp
      h265_encode/src/genx_hevce_birefine_icllp_isa.cpp
      h265_encode/src/genx_hevce_birefine_icl_isa.cpp
      h265_encode/src/genx_hevce_birefine_skl_isa.cpp
      h265_encode/src/genx_hevce_deblock_bdw_isa.cpp
      h265_encode/src/genx_hevce_deblock_hsw_isa.cpp
      h265_encode/src/genx_hevce_deblock_icllp_isa.cpp
      h265_encode/src/genx_hevce_deblock_icl_isa.cpp
      h265_encode/src/genx_hevce_deblock_skl_isa.cpp
      h265_encode/src/genx_hevce_hme_and_me_p32_4mv_bdw_isa.cpp
      h265_encode/src/genx_hevce_hme_and_me_p32_4mv_hsw_isa.cpp
      h265_encode/src/genx_hevce_hme_and_me_p32_4mv_icllp_isa.cpp
      h265_encode/src/genx_hevce_hme_and_me_p32_4mv_icl_isa.cpp
      h265_encode/src/genx_hevce_hme_and_me_p32_4mv_skl_isa.cpp
      h265_encode/src/genx_hevce_ime_3tiers_4mv_bdw_isa.cpp
      h265_encode/src/genx_hevce_ime_3tiers_4mv_hsw_isa.cpp
      h265_encode/src/genx_hevce_ime_3tiers_4mv_icllp_isa.cpp
      h265_encode/src/genx_hevce_ime_3tiers_4mv_icl_isa.cpp
      h265_encode/src/genx_hevce_ime_4mv_bdw_isa.cpp
      h265_encode/src/genx_hevce_ime_4mv_hsw_isa.cpp
      h265_encode/src/genx_hevce_ime_4mv_icllp_isa.cpp
      h265_encode/src/genx_hevce_ime_4mv_icl_isa.cpp
      h265_encode/src/genx_hevce_interpolate_frame_bdw_isa.cpp
      h265_encode/src/genx_hevce_interpolate_frame_hsw_isa.cpp
      h265_encode/src/genx_hevce_interpolate_frame_icllp_isa.cpp
      h265_encode/src/genx_hevce_interpolate_frame_icl_isa.cpp
      h265_encode/src/genx_hevce_interpolate_frame_skl_isa.cpp
      h265_encode/src/genx_hevce_intra_avc_bdw_isa.cpp
      h265_encode/src/genx_hevce_intra_avc_hsw_isa.cpp
      h265_encode/src/genx_hevce_intra_avc_icllp_isa.cpp
      h265_encode/src/genx_hevce_intra_avc_icl_isa.cpp
      h265_encode/src/genx_hevce_intra_avc_skl_isa.cpp
      h265_encode/src/genx_hevce_me_p16_4mv_and_refine_32x32_bdw_isa.cpp
      h265_encode/src/genx_hevce_me_p16_4mv_and_refine_32x32_hsw_isa.cpp
      h265_encode/src/genx_hevce_me_p16_4mv_and_refine_32x32_icllp_isa.cpp
      h265_encode/src/genx_hevce_me_p16_4mv_and_refine_32x32_icl_isa.cpp
      h265_encode/src/genx_hevce_me_p16_4mv_and_refine_32x32_skl_isa.cpp
      h265_encode/src/genx_hevce_me_p32_4mv_bdw_isa.cpp
      h265_encode/src/genx_hevce_me_p32_4mv_hsw_isa.cpp
      h265_encode/src/genx_hevce_me_p32_4mv_icllp_isa.cpp
      h265_encode/src/genx_hevce_me_p32_4mv_icl_isa.cpp
      h265_encode/src/genx_hevce_prepare_src_bdw_isa.cpp
      h265_encode/src/genx_hevce_prepare_src_hsw_isa.cpp
      h265_encode/src/genx_hevce_prepare_src_icllp_isa.cpp
      h265_encode/src/genx_hevce_prepare_src_icl_isa.cpp
      h265_encode/src/genx_hevce_prepare_src_skl_isa.cpp
      h265_encode/src/genx_hevce_refine_me_p_16x32_bdw_isa.cpp
      h265_encode/src/genx_hevce_refine_me_p_16x32_hsw_isa.cpp
      h265_encode/src/genx_hevce_refine_me_p_16x32_icllp_isa.cpp
      h265_encode/src/genx_hevce_refine_me_p_16x32_icl_isa.cpp
      h265_encode/src/genx_hevce_refine_me_p_16x32_skl_isa.cpp
      h265_encode/src/genx_hevce_refine_me_p_32x16_bdw_isa.cpp
      h265_encode/src/genx_hevce_refine_me_p_32x16_hsw_isa.cpp
      h265_encode/src/genx_hevce_refine_me_p_32x16_icllp_isa.cpp
      h265_encode/src/genx_hevce_refine_me_p_32x16_icl_isa.cpp
      h265_encode/src/genx_hevce_refine_me_p_32x16_skl_isa.cpp
      h265_encode/src/genx_hevce_refine_me_p_32x32_icllp_isa.cpp
      h265_encode/src/genx_hevce_refine_me_p_32x32_icl_isa.cpp
      h265_encode/src/genx_hevce_refine_me_p_32x32_sad_bdw_isa.cpp
      h265_encode/src/genx_hevce_refine_me_p_32x32_sad_hsw_isa.cpp
      h265_encode/src/genx_hevce_refine_me_p_32x32_sad_icllp_isa.cpp
      h265_encode/src/genx_hevce_refine_me_p_32x32_sad_icl_isa.cpp
      h265_encode/src/genx_hevce_refine_me_p_32x32_sad_skl_isa.cpp
      h265_encode/src/genx_hevce_refine_me_p_32x32_skl_isa.cpp
      h265_encode/src/genx_hevce_refine_me_p_64x64_bdw_isa.cpp
      h265_encode/src/genx_hevce_refine_me_p_64x64_hsw_isa.cpp
      h265_encode/src/genx_hevce_refine_me_p_64x64_icllp_isa.cpp
      h265_encode/src/genx_hevce_refine_me_p_64x64_icl_isa.cpp
      h265_encode/src/genx_hevce_refine_me_p_64x64_skl_isa.cpp
      h265_encode/src/genx_hevce_sao_apply_bdw_isa.cpp
      h265_encode/src/genx_hevce_sao_apply_hsw_isa.cpp
      h265_encode/src/genx_hevce_sao_apply_icllp_isa.cpp
      h265_encode/src/genx_hevce_sao_apply_icl_isa.cpp
      h265_encode/src/genx_hevce_sao_bdw_isa.cpp
      h265_encode/src/genx_hevce_sao_hsw_isa.cpp
      h265_encode/src/genx_hevce_sao_icllp_isa.cpp
      h265_encode/src/genx_hevce_sao_icl_isa.cpp
      h265_encode/src/genx_hevce_sao_skl_isa.cpp
  )

  target_link_libraries(genx_h265_encode_embeded
    PUBLIC
      mfx_static_lib
    PRIVATE
      mfx_sdl_properties
    )

  target_include_directories(genx_h265_encode_embeded
    PUBLIC
      h265_encode/include
  )
endif()

if(MFX_ENABLE_KERNELS)
  
  target_include_directories(genx
    PUBLIC
      av1_encode/include
    )

  target_sources(genx
    PRIVATE
      asc/isa/genx_scd_xehp_sdv_isa.cpp
      
      copy_kernels/isa/genx_copy_kernel_xehp_sdv_isa.cpp
      copy_kernels/isa/genx_copy_kernel_dg2_isa.cpp
      
      field_copy/isa/genx_fcopy_xehp_sdv_isa.cpp

      mctf/isa/genx_mc_xehp_sdv_isa.cpp
      mctf/isa/genx_me_xehp_sdv_isa.cpp
      mctf/isa/genx_sd_xehp_sdv_isa.cpp
  )

endif()