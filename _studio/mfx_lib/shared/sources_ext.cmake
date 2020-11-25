if (MFX_DISABLE_SW_FALLBACK)
  return()
endif()

# ====================================== mfx_common_sw ======================================

add_library(mfx_common_sw STATIC)
set_property(TARGET mfx_common_sw PROPERTY FOLDER "sw_fallback")

target_sources(mfx_common_sw
  PRIVATE
    include/mfx_critical_error_handler.h
    src/mfx_critical_error_handler.cpp
  )

target_compile_definitions(mfx_common_sw PRIVATE MFX_RT MFX_NO_VA)
target_include_directories(mfx_common_sw PUBLIC include)

target_link_libraries(mfx_common_sw PUBLIC mfx_common umc_io)

# ====================================== mfx_common_hw ======================================
target_link_libraries(mfx_common_hw
  PRIVATE
    $<$<BOOL:${MFX_ENABLE_LP_LOOKAHEAD}>:lpla>
)
