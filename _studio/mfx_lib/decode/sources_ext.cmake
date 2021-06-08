if (OPEN_SOURCE)
  return()
endif()

list(APPEND sources
    ${UMC_CODECS}/h265_dec/src/umc_h265_va_packer_dxva.cpp
    ${UMC_CODECS}/mpeg2_dec/src/umc_mpeg2_dxva_packer.cpp
    ${UMC_CODECS}/av1_dec/src/umc_av1_va_packer_dxva.cpp
    ${UMC_CODECS}/av1_dec/src/umc_av1_va_packer_msft.cpp
    ${MSDK_UMC_ROOT}/core/umc/src/umc_default_frame_allocator.cpp
)

list(APPEND sources_hw
    ${CMAKE_CURRENT_SOURCE_DIR}/vp8/src/mfx_vp8_dec_decode_hw_dxva.cpp
)

set(IPP_LIBS "")
if (MFX_BUNDLED_IPP)
  set(IPP_LIBS ipp)
else()
  set(IPP_LIBS IPP::msdk IPP::j IPP::vc IPP::cc IPP::cv IPP::i IPP::s IPP::core)
endif()