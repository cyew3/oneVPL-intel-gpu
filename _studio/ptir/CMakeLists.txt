set(MFX_ORIG_LDFLAGS "${MFX_LDFLAGS}" )

# Plugin version info
set(ptir_version_defs MFX_PLUGIN_PRODUCT_VERSION=\"\\\"${MEDIA_VERSION_STR}\"\\\")


# =============================================================================

#PA (originally PA_cmd system reference application) static lib

add_library( pa STATIC )

target_include_directories(mfx_vpp_scd64
  PUBLIC
    ptir_plugin/include
    pa
    pa/deinterlacer
    pa/pfc
    pa/telecine

    pacm
    pacm/cmut
    pacm/kernels

    ../mfx_lib/cmrt_cross_platform/include
    ../mfx_lib/shared/include
  )

target_sources(pa
  PRIVATE
    pa/deinterlacer/deinterlacer.c
    pa/pfc/pfc.c
    pa/telecine/model.c
    pa/telecine/telecine.c
    pa/common.c
    pa/log_debug.c
  )

target_link_libraries(pa
  PRIVATE
    mfx_static_lib
    mfx_sdl_properties
  )

####################################################################

#PACM (originally CM-ported reference application) static lib

add_library( pacm STATIC )

target_include_directories(pacm
  PUBLIC
    ptir_plugin/include
    pa
    pa/deinterlacer
    pa/pfc
    pa/telecine

    pacm
    pacm/cmut
    pacm/kernels

    ../mfx_lib/cmrt_cross_platform/include
    ../mfx_lib/shared/include
  )

target_sources(pacm
  PRIVATE
    pa/deinterlacer/deinterlacer.c
    pa/pfc/pfc.c
    pa/telecine/model.c
    pa/telecine/telecine.c
    pa/common.c
    pa/log_debug.c
    pacm/kernels/deinterlace_genx_hsw.cpp
    pacm/kernels/deinterlace_genx_bdw.cpp
    pacm/deinterlacer.cpp
    pacm/utilities.cpp
    ../mfx_lib/cmrt_cross_platform/src/cmrt_cross_platform.cpp
  )

target_link_libraries(pacm
  PRIVATE
    mfx_static_lib
    mfx_sdl_properties
  )

target_compile_features(pacm PUBLIC cxx_std_11)

####################################################################

#ptir_plugin shared lib

set( USE_STRICT_NAME TRUE )

add_library( mfx_ptir_hw64 SHARED )

target_include_directories(mfx_ptir_hw64
  PUBLIC
    ptir_plugin/include
    pa
    pa/deinterlacer
    pa/pfc
    pa/telecine
    pacm
    pacm/cmut
    pacm/kernels
    ../mfx_lib/cmrt_cross_platform/include
    ../mfx_lib/shared/include
  )

target_link_options(mfx_ptir_hw64
  PRIVATE
    $<$<PLATFORM_ID:Linux>:-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/../mfx_lib/plugin/libmfxsw_plugin.map>
  )

target_sources(mfx_ptir_hw64
  PRIVATE
    pa/deinterlacer/deinterlacer.c
    pa/pfc/pfc.c
    pa/telecine/model.c
    pa/telecine/telecine.c
    pa/common.c
    pa/log_debug.c
    pacm/kernels/deinterlace_genx_hsw.cpp
    pacm/kernels/deinterlace_genx_bdw.cpp
    pacm/deinterlacer.cpp
    pacm/utilities.cpp
    ../mfx_lib/cmrt_cross_platform/src/cmrt_cross_platform.cpp
    ptir_plugin/src/hw_utils.cpp
    ptir_plugin/src/mfx_ptir_utils.cpp
    ptir_plugin/src/plg_frame_supply.cpp
    ptir_plugin/src/ptir_cm_wrap.cpp
    ptir_plugin/src/ptir_cpu_wrap.cpp
    ptir_plugin/src/ptir_vpp_plugin.cpp
  )

set(IPP_LIBS "")
if (NOT CMAKE_SYSTEM_NAME MATCHES Windows AND MFX_BUNDLED_IPP)
    set(IPPLIBS ipp)
else()
    set(IPPLIBS IPP::msdk IPP::j IPP::vc IPP::cc IPP::cv IPP::i IPP::s IPP::core)
endif()

target_link_libraries(mfx_ptir_hw64
    PUBLIC
      mfx_shared_lib
      pa
      pacm
      umc
      vm
      vm_plus
    PRIVATE
      mfx_sdl_properties
      ${CMAKE_DL_LIBS}
      ${IPP_LIBS}
    )

target_compile_definitions(mfx_ptir_hw64
  PRIVATE
    ${defs}
    ${ptir_version_defs}
  )
