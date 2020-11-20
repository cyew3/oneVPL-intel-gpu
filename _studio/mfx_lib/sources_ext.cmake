if (MFX_DISABLE_SW_FALLBACK)
  return()
endif()

return()

include(sources_libaudio.cmake)
include(sources_librt.cmake)
