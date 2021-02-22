if(OPEN_SOURCE)
  return()
endif()

if (MFX_BUNDLED_IPP)
  set(IPP_LIBS ipp)
else()
  set(IPP_LIBS IPP::msdk IPP::j IPP::vc IPP::cc IPP::cv IPP::i IPP::s IPP::core)
endif()

target_link_libraries(pak
  PUBLIC
    mfx_static_lib
    umc_va_hw
  PRIVATE
    mfx_sdl_properties
    ${IPP_LIBS}
  )
