if(MFX_DISABLE_SW_FALLBACK)
  return()
endif()

if (NOT CMAKE_CXX_COMPILER_ID MATCHES Intel)
  return()
endif()

list(APPEND
  common_sources
    ${MSDK_LIB_ROOT}/fs_det/src/Cleanup.cpp
    ${MSDK_LIB_ROOT}/fs_det/src/Dim.cpp
    ${MSDK_LIB_ROOT}/fs_det/src/FaceDet.cpp
    ${MSDK_LIB_ROOT}/fs_det/src/FaceDetTmpl.cpp
    ${MSDK_LIB_ROOT}/fs_det/src/FrameBuff.cpp
    ${MSDK_LIB_ROOT}/fs_det/src/FSapi.cpp
    ${MSDK_LIB_ROOT}/fs_det/src/Morphology.cpp
    ${MSDK_LIB_ROOT}/fs_det/src/Params.cpp
    ${MSDK_LIB_ROOT}/fs_det/src/SkinDetAlg.cpp
    ${MSDK_LIB_ROOT}/fs_det/src/SkinDetBuff.cpp
    ${MSDK_LIB_ROOT}/fs_det/src/SkinHisto.cpp
    ${MSDK_LIB_ROOT}/fs_det/src/SkinSegm.cpp
    ${MSDK_LIB_ROOT}/fs_det/src/Video.cpp
)

add_library(h265_enc_hw STATIC)

target_include_directories(h265_enc_hw
  PUBLIC
    ${MSDK_LIB_ROOT}/fei/h265/include
    ${MSDK_LIB_ROOT}/fs_det/include
    ${MSDK_LIB_ROOT}/optimization/h265/include
    h265/include
)

if( CMAKE_SYSTEM_NAME MATCHES Linux )
  target_link_options(h265_enc_hw
    PRIVATE
      -mavx2
      -march=core-avx2
    )

  target_compile_options(h265_enc_hw
    PRIVATE
      -mavx2
      -march=core-avx2
    )
endif()

target_sources(h265_enc_hw
  PRIVATE
    h265/include/mfx_h265_bitstream.h
    h265/include/mfx_h265_brc.h
    h265/include/mfx_h265_cabac.h
    h265/include/mfx_h265_cabac_tables.h
    h265/include/mfx_h265_cmcopy.h
    h265/include/mfx_h265_ctb.h
    h265/include/mfx_h265_defaults.h
    h265/include/mfx_h265_defs.h
    h265/include/mfx_h265_enc.h
    h265/include/mfx_h265_encode_api.h
    h265/include/mfx_h265_encode.h
    h265/include/mfx_h265_frame.h
    h265/include/mfx_h265_lookahead.h
    h265/include/mfx_h265_quant.h
    h265/include/mfx_h265_quant_rdo.h
    h265/include/mfx_h265_sao_filter.h
    h265/include/mfx_h265_set.h
    h265/include/mfx_h265_tables.h

    h265/src/mfx_h265_bitstream.cpp
    h265/src/mfx_h265_brc.cpp
    h265/src/mfx_h265_cabac.cpp
    h265/src/mfx_h265_cabac_init.cpp
    h265/src/mfx_h265_cabac_tables.cpp
    h265/src/mfx_h265_cmcopy.cpp
    h265/src/mfx_h265_ctb.cpp
    h265/src/mfx_h265_deblocking.cpp
    h265/src/mfx_h265_defaults.cpp
    h265/src/mfx_h265_enc.cpp
    h265/src/mfx_h265_encode_api.cpp
    h265/src/mfx_h265_encode.cpp
    h265/src/mfx_h265_frame.cpp
    h265/src/mfx_h265_interpred.cpp
    h265/src/mfx_h265_intrapred.cpp
    h265/src/mfx_h265_lookahead.cpp
    h265/src/mfx_h265_quant.cpp
    h265/src/mfx_h265_quant_rdo.cpp
    h265/src/mfx_h265_sao_filter.cpp
    h265/src/mfx_h265_set.cpp
    h265/src/mfx_h265_tables.cpp
    h265/src/mfx_h265_transform.cpp
    ${common_sources}
)

target_link_libraries(h265_enc_hw
  PUBLIC
    mfx_static_lib
    umc
    cmrt_cross_platform_hw
  PRIVATE
    mfx_sdl_properties
  )

target_compile_definitions(h265_enc_hw PRIVATE AS_HEVCE_PLUGIN MFX_D3D11_ENABLED)

if(NOT ENABLE_AV1)
  return()
endif()

add_library(av1_enc_hw STATIC)

target_include_directories(av1_enc_hw
  PUBLIC
    ${MSDK_LIB_ROOT}/fs_det/include
    ${MSDK_LIB_ROOT}/fei/av1/include
    ${MSDK_LIB_ROOT}/optimization/av1/
    av1/include
)


target_sources(av1_enc_hw
  PRIVATE
    av1/include/mfx_av1_binarization.h
    av1/include/mfx_av1_bit_count.h
    av1/include/mfx_av1_bitwriter.h
    av1/include/mfx_av1_brc.h
    av1/include/mfx_av1_cdef.h
    av1/include/mfx_av1_copy.h
    av1/include/mfx_av1_core_iface_wrapper.h
    av1/include/mfx_av1_ctb.h
    av1/include/mfx_av1_deblocking.h
    av1/include/mfx_av1_defaults.h
    av1/include/mfx_av1_defs.h
    av1/include/mfx_av1_dependency_graph.h
    av1/include/mfx_av1_dispatcher.h
    av1/include/mfx_av1_dispatcher_fptr.h
    av1/include/mfx_av1_dispatcher_proto.h
    av1/include/mfx_av1_dispatcher_wrappers.h
    av1/include/mfx_av1_enc.h
    av1/include/mfx_av1_encode.h
    av1/include/mfx_av1_encode_api.h
    av1/include/mfx_av1_frame.h
    av1/include/mfx_av1_get_context.h
    av1/include/mfx_av1_gpu_mode_decision.h
    av1/include/mfx_av1_hash_table.h
    av1/include/mfx_av1_lookahead.h
    av1/include/mfx_av1_prob_opt.h
    av1/include/mfx_av1_probabilities.h
    av1/include/mfx_av1_quant.h
    av1/include/mfx_av1_scan.h
    av1/include/mfx_av1_scroll_detector.h
    av1/include/mfx_av1_superres.h
    av1/include/mfx_av1_tables.h
    av1/include/mfx_av1_trace.h

    av1/src/mfx_av1_bit_count.cpp
    av1/src/mfx_av1_brc.cpp
    av1/src/mfx_av1_cdef.cpp
    av1/src/mfx_av1_copy.cpp
    av1/src/mfx_av1_ctb.cpp
    av1/src/mfx_av1_deblocking.cpp
    av1/src/mfx_av1_defaults.cpp
    av1/src/mfx_av1_dispatcher.cpp
    av1/src/mfx_av1_dispatcher_fptr.cpp
    av1/src/mfx_av1_enc.cpp
    av1/src/mfx_av1_encode.cpp
    av1/src/mfx_av1_encode_api.cpp
    av1/src/mfx_av1_entropy.cpp
    av1/src/mfx_av1_frame.cpp
    av1/src/mfx_av1_get_context.cpp
    av1/src/mfx_av1_gpu_mode_decision.cpp
    av1/src/mfx_av1_hash_table.cpp
    av1/src/mfx_av1_interp_scale.cpp
    av1/src/mfx_av1_interpred.cpp
    av1/src/mfx_av1_intrabc.cpp
    av1/src/mfx_av1_intrapred.cpp
    av1/src/mfx_av1_lookahead.cpp
    av1/src/mfx_av1_mv_refs.cpp
    # av1/src/mfx_av1_prob_opt.cpp
    av1/src/mfx_av1_probabilities.cpp
    av1/src/mfx_av1_quant.cpp
    av1/src/mfx_av1_recon10bit.cpp
    av1/src/mfx_av1_scan.cpp
    av1/src/mfx_av1_scroll_detector.cpp
    av1/src/mfx_av1_set.cpp
    av1/src/mfx_av1_set_hwpak.cpp
    av1/src/mfx_av1_superres.cpp
    av1/src/mfx_av1_tables.cpp
    av1/src/mfx_av1_trace.cpp
    ${common_sources}
  )

  target_compile_definitions(av1_enc_hw PRIVATE AS_AV1E_PLUGIN MFX_D3D11_ENABLED MFX_VA)

target_link_libraries(av1_enc_hw
  PUBLIC
    mfx_static_lib
    umc
    cmrt_cross_platform_hw
  PRIVATE
    mfx_sdl_properties)