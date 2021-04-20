if (MFX_DISABLE_SW_FALLBACK)
  return()
endif()

add_library(vpp_sw STATIC ${sources})
set_property(TARGET vpp_sw PROPERTY FOLDER "sw_fallback")

target_include_directories(vpp_sw
PUBLIC
  include
)

target_link_libraries(vpp_sw
  PUBLIC
    mfx_static_lib
    umc_va_hw
    scene_analyzer
    cmrt_cross_platform_hw
    mctf_hw
  PRIVATE
    mfx_sdl_properties
    genx
  )

target_sources(vpp_hw PRIVATE ${sources})

target_link_libraries(vpp_hw
  PUBLIC
    scene_analyzer
  PRIVATE
  )

target_compile_definitions(vpp_hw PRIVATE $<$<PLATFORM_ID:Windows>:MFX_DX9ON11>)
