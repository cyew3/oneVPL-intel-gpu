if(OPEN_SOURCE)
  return()
endif()

set(IPP_LIBS "")
if (MFX_BUNDLED_IPP)
  set(IPP_LIBS ipp)
else()
  set(IPP_LIBS IPP::msdk IPP::j IPP::vc IPP::cc IPP::cv IPP::i IPP::s IPP::core)
endif()

target_link_libraries(enc PUBLIC mfx_static_lib $<$<TARGET_EXISTS:mpeg2_enc>:mpeg2_enc> umc_va_hw ${IPP_LIBS})
