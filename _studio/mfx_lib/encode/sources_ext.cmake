if (OPEN_SOURCE)
  return()
endif()

set( av1e_core_hwa_sources
    av1/include/mfx_av1_binarization.h
    av1/include/mfx_av1_bitwriter.h
    av1/include/mfx_av1_bit_count.h
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
    av1/include/mfx_av1_probabilities.h
    av1/include/mfx_av1_prob_opt.h
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
    av1/src/mfx_av1_interpred.cpp
    av1/src/mfx_av1_interp_scale.cpp
    av1/src/mfx_av1_intrabc.cpp
    av1/src/mfx_av1_intrapred.cpp
    av1/src/mfx_av1_lookahead.cpp
    av1/src/mfx_av1_mv_refs.cpp
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
)

add_library(av1e_core_hwa STATIC ${av1e_core_hwa_sources})

set_property(TARGET av1e_core_hwa PROPERTY FOLDER "av1e_hwa")

target_compile_definitions(av1e_core_hwa
  PRIVATE
    $<$<PLATFORM_ID:Windows>:MFX_VA_WIN> # w/a for missed config inclusion in hevc/av1
)
  
target_include_directories(av1e_core_hwa
  PUBLIC
    av1/include
    ${MSDK_LIB_ROOT}/fei/av1/include
    ${MSDK_LIB_ROOT}/optimization/av1
    ${MSDK_LIB_ROOT}/cmrt_cross_platform/include
    ${MSDK_LIB_ROOT}/encode_hw/av1
    ${MSDK_STUDIO_ROOT}/shared/include
    ${MSDK_STUDIO_ROOT}/shared/umc/core/umc/include
    ${MSDK_STUDIO_ROOT}/shared/umc/core/vm/include
    ${MSDK_STUDIO_ROOT}/shared/umc/core/vm_plus/include
)

set(IPP_LIBS "")
if (MFX_BUNDLED_IPP)
  set(IPP_LIBS ipp)
else()
  set(IPP_LIBS IPP::msdk IPP::j IPP::vc IPP::cc IPP::cv IPP::i IPP::s IPP::core)
endif()

target_link_libraries(av1e_core_hwa
  PUBLIC
    mfx_static_lib
    ${IPP_LIBS}
)
