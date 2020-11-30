if (MFX_DISABLE_SW_FALLBACK)
  return()
endif()

list(APPEND sources
  src/mfx_color_space_conversion_vpp_avx2.cpp
  src/mfx_color_space_conversion_vpp.cpp
  src/mfx_color_space_conversion_vpp_sse.cpp
  src/mfx_deinterlace_vpp.cpp
  src/mfx_gamut_compression_vpp_config.cpp
  src/mfx_gamut_compression_vpp.cpp
  src/mfx_image_stabilization_vpp.cpp
  src/mfx_range_map_vc1_vpp.cpp
  src/mfx_resize_vpp.cpp
  src/mfx_shift_vpp.cpp
  src/mfx_video_analysis_vpp.cpp
  src/mfx_video_signal_conversion_vpp.cpp
  src/mfx_vpp_svc.cpp
  src/mfx_vpp_sw_threading.cpp
  src/vme/meforgen7_5_avs.cpp
  src/vme/meforgen7_5_bme.cpp
  src/vme/meforgen7_5.cpp
  src/vme/meforgen7_5_fme.cpp
  src/vme/meforgen7_5_ime.cpp
  src/vme/meforgen7_5_intra.cpp
  src/vme/meforgen7_5_sad.cpp
  src/vme/videovme.cpp
)

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
    genx
)

target_sources(vpp_hw PRIVATE ${sources})

target_link_libraries(vpp_hw
  PUBLIC
    scene_analyzer
  PRIVATE
  )
