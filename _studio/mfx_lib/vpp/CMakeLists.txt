set(MFX_ORIG_LDFLAGS "${MFX_LDFLAGS}" )

set( defs "${API_FLAGS} ${WARNING_FLAGS}" )

list( APPEND cdirs
  scene_analyzer
  )
mfx_include_dirs( )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/include )
foreach( dir ${cdirs} )
  include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../../shared/umc/codec/${dir}/include )
endforeach( )

#==============MCTF========================================================
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../mctf_package/mctf/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../genx/mctf/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../../shared/asc/include )

set(sources
    src/mfx_denoise_vpp.cpp
    src/mfx_detail_enhancement_vpp.cpp
    src/mfx_frame_rate_conversion_vpp.cpp
    src/mfx_procamp_vpp.cpp
    src/mfx_vpp_factory.cpp
    src/mfx_vpp_hw.cpp
    src/mfx_vpp_main.cpp
    src/mfx_vpp_mvc.cpp
    src/mfx_vpp_service.cpp
    src/mfx_vpp_sw_core.cpp
    src/mfx_vpp_sw_internal.cpp
    src/mfx_vpp_utils.cpp
    src/mfx_vpp_jpeg_d3d9.cpp
    )

if(NOT OPEN_SOURCE)
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
  make_library( vpp sw static )
endif()

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../cmrt_cross_platform/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../genx/field_copy/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../genx/asc/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../genx/copy_kernels/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../genx/mctf/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../cmut )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../../shared/include )

make_library( vpp hw static )
set( defs "" )
