set(VPP_SCD_GUID "4f4e2f8290079c47bf7aff37dd5fcd4f")
set(ENC_SCD_GUID "dfc215b3e3d3904d7fa504127ef564d5")

# Plugin version info
set(SCD_version_defs  MFX_PLUGIN_PRODUCT_VERSION=\"\\\"${MEDIA_VERSION_STR}\"\\\")

# =============================================================================

#shared lib

set(USE_STRICT_NAME TRUE)

add_library( mfx_vpp_scd64 SHARED )

target_include_directories(mfx_vpp_scd64
  PUBLIC
    include
    ${MSDK_STUDIO_ROOT}/shared/include
    ${MSDK_STUDIO_ROOT}/mfx_lib/cmrt_cross_platform/include
    ${MSDK_STUDIO_ROOT}/mfx_lib/cmut
    ${MSDK_STUDIO_ROOT}/shared/asc/include
    ${MSDK_STUDIO_ROOT}/mfx_lib/genx/asc/isa
    ${MSDK_STUDIO_ROOT}/shared/include
    ${MSDK_STUDIO_ROOT}/mfx_lib/plugin/include
    ${MSDK_STUDIO_ROOT}/mfx_lib/scheduler/include
    ${MSDK_STUDIO_ROOT}/mfx_lib/scheduler/linux/include
  )

target_link_options(mfx_vpp_scd64
  PRIVATE
    $<$<PLATFORM_ID:Linux>:-Wl,--version-script=${MSDK_STUDIO_ROOT}/mfx_lib/plugin/libmfxsw_plugin.map>
  )

target_sources(mfx_vpp_scd64
  PRIVATE
    src/export.cpp
    src/scd_tools.cpp
    src/vpp_scd.cpp
  )

set(IPP_LIBS "")
if (NOT CMAKE_SYSTEM_NAME MATCHES Windows AND MFX_BUNDLED_IPP)
    set(IPPLIBS ipp)
else()
    set(IPPLIBS IPP::msdk IPP::j IPP::vc IPP::cc IPP::cv IPP::i IPP::s IPP::core)
endif()

target_link_libraries(mfx_vpp_scd64
    PUBLIC
        vm
        vm_plus
        cmrt_cross_platform_hw
        genx
        mfx_trace
        ${ITT_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT}
        ${CMAKE_DL_LIBS}
        ${IPP_LIBS}
        asc
    )

target_compile_definitions(mfx_vpp_scd64
  PRIVATE
    AS_VPP_SCD_PLUGIN
    ${SCD_version_defs}
    ${WARNING_FLAGS}
    ${defs}
  )

gen_plugins_cfg("VPP_SCD" ${VPP_SCD_GUID} "mfx_vpp_scd64" "03" "")
install( TARGETS mfx_vpp_scd64 LIBRARY DESTINATION ${MFX_PLUGINS_DIR} )

# =============================================================================

add_library( mfx_enc_scd64 SHARED )

target_include_directories(mfx_enc_scd64
  PUBLIC
    include
    ${MSDK_STUDIO_ROOT}/shared/include
    ${MSDK_STUDIO_ROOT}/mfx_lib/cmrt_cross_platform/include
    ${MSDK_STUDIO_ROOT}/mfx_lib/cmut
    ${MSDK_STUDIO_ROOT}/shared/asc/include
    ${MSDK_STUDIO_ROOT}/mfx_lib/genx/asc/isa
    ${MSDK_STUDIO_ROOT}/shared/include
    ${MSDK_STUDIO_ROOT}/mfx_lib/plugin/include
    ${MSDK_STUDIO_ROOT}/mfx_lib/scheduler/linux/include
  )

target_link_options(mfx_enc_scd64
  PRIVATE
    $<$<PLATFORM_ID:Linux>:-Wl,--version-script=${MSDK_STUDIO_ROOT}/mfx_lib/plugin/libmfxsw_plugin.map>
  )

target_sources(mfx_enc_scd64
  PRIVATE
    src/export.cpp
    src/scd_tools.cpp
    src/enc_scd.cpp
  )

set(IPP_LIBS "")
if (NOT CMAKE_SYSTEM_NAME MATCHES Windows AND MFX_BUNDLED_IPP)
    set(IPPLIBS ipp)
else()
    set(IPPLIBS IPP::msdk IPP::j IPP::vc IPP::cc IPP::cv IPP::i IPP::s IPP::core)
endif()

target_link_libraries(mfx_enc_scd64
    PUBLIC
        vm
        vm_plus
        cmrt_cross_platform_hw
        genx
        mfx_trace
        ${ITT_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT}
        ${CMAKE_DL_LIBS}
        ${IPP_LIBS}
        asc
    )

target_compile_definitions(mfx_enc_scd64
  PRIVATE
    AS_VPP_SCD_PLUGIN
    ${SCD_version_defs}
    ${WARNING_FLAGS}
    ${defs}
  )

gen_plugins_cfg("ENC_SCD" ${ENC_SCD_GUID} "mfx_enc_scd64" "04" "")
install( TARGETS mfx_enc_scd64 LIBRARY DESTINATION ${MFX_PLUGINS_DIR} )
