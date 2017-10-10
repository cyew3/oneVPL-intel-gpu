set(MFX_ORIG_LDFLAGS "${MFX_LDFLAGS}" )

set(VPP_SCD_GUID "4f4e2f8290079c47bf7aff37dd5fcd4f")
set(ENC_SCD_GUID "dfc215b3e3d3904d7fa504127ef564d5")

# Plugin version info
if( __TARGET_PLATFORM STREQUAL "BXTMIN" OR __TARGET_PLATFORM STREQUAL "BXT" )
  set (VPP_SCD_PRODUCT_NAME "Intel(R) Media SDK 2017 for Embedded Linux")
  set (VPP_SCD_DESCRIPTION "Intel(R) Media SDK 2017 for Embedded Linux - VPP Scene Change Detection Plug-in")
  set (ENC_SCD_PRODUCT_NAME "Intel(R) Media SDK 2017 for Embedded Linux")
  set (ENC_SCD_DESCRIPTION "Intel(R) Media SDK 2017 for Embedded Linux - ENC Scene Change Detection Plug-in")
else()
  set (VPP_SCD_PRODUCT_NAME "Intel(R) Media Server Studio 2017 - VPP Scene Change Detection")
  set (VPP_SCD_DESCRIPTION "Intel(R) Media Server Studio 2017 - VPP Scene Change Detection Plug-in")
  set (ENC_SCD_PRODUCT_NAME "Intel(R) Media Server Studio 2017 - ENC Scene Change Detection")
  set (ENC_SCD_DESCRIPTION "Intel(R) Media Server Studio 2017 - ENC Scene Change Detection Plug-in")
endif()

if( NOT DEFINED ENV{MFX_SCD_VERSION} )
  set( scd_version 0.0.000.0000 )
else( )
  set( scd_version $ENV{MFX_SCD_VERSION} )
endif( )

if( Linux OR Darwin )
  execute_process(
    COMMAND echo
    COMMAND cut -f 1 -d.
    COMMAND date "+.%-y.%-m.%-d"
    OUTPUT_VARIABLE cur_date
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )
  string( SUBSTRING ${scd_version} 0 1 ver )

  set(SCD_version_defs  "-DMFX_PLUGIN_FILE_VERSION=\"\\\"${ver}${cur_date}\"\\\" -DMFX_PLUGIN_PRODUCT_VERSION=\"\\\"${scd_version}\"\\\"")
endif( )

# =============================================================================

mfx_include_dirs( )

set(MFX_ROOTDIR "${CMAKE_CURRENT_SOURCE_DIR}/..")

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/include )
include_directories( ${MFX_ROOTDIR}/shared/include )
include_directories( ${MFX_ROOTDIR}/mfx_lib/cmrt_cross_platform/include )
include_directories( ${MFX_ROOTDIR}/mfx_lib/cmut )
include_directories( ${MFX_ROOTDIR}/mfx_lib/genx/scd )
include_directories( ${MFX_ROOTDIR}/shared/include )
include_directories( ${MFX_ROOTDIR}/mfx_lib/plugin/include )
include_directories( ${MFX_ROOTDIR}/mfx_lib/scheduler/include )

# =============================================================================

#shared lib
set( sources "" )
set( sources.plus "" )

set(USE_STRICT_NAME TRUE)
set(MFX_LDFLAGS "${MFX_ORIG_LDFLAGS} -Wl,--version-script=${MFX_ROOTDIR}/mfx_lib/plugin/libmfxsw_plugin.map" )

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/include )

list( APPEND sources
    ${CMAKE_CURRENT_SOURCE_DIR}/src/export.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scd_tools.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vpp_scd.cpp
    ${MFX_ROOTDIR}/shared/src/mfx_scd.cpp
)

set( LIBS "" )
list( APPEND LIBS
    vm
    vm_plus
    cmrt_cross_platform_hw
    ${ITT_LIBS}
    pthread
    dl
    ippmsdk_l ippj_l ippvc_l ippcc_l ippcv_l ippi_l ipps_l ippcore_l
)

set( defs "-DMFX_VA -DMFX_VA_LINUX -DMFX_PLUGIN_PRODUCT_NAME=\"\\\"${VPP_SCD_PRODUCT_NAME}\"\\\" -DMFX_FILE_DESCRIPTION=\"\\\"${VPP_SCD_DESCRIPTION}\"\\\" ${SCD_version_defs}" )

gen_plugins_cfg("VPP_SCD" ${VPP_SCD_GUID} "mfx_vpp_scd64" "03" "")

set( defs " -DAS_VPP_SCD_PLUGIN ${defs} ${WARNING_FLAGS}" )
make_library( mfx_vpp_scd64 hw shared )
msdk_install( mfx_vpp_scd64 ${MFX_PLUGINS_DIR} )
set( defs "" )

# =============================================================================
set( sources "" )
set( sources.plus "" )

list( APPEND sources
    ${CMAKE_CURRENT_SOURCE_DIR}/src/export.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scd_tools.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/enc_scd.cpp
    ${MFX_ROOTDIR}/shared/src/mfx_scd.cpp
)

set( defs "-DMFX_VA -DMFX_VA_LINUX -DMFX_PLUGIN_PRODUCT_NAME=\"\\\"${ENC_SCD_PRODUCT_NAME}\"\\\" -DMFX_FILE_DESCRIPTION=\"\\\"${ENC_SCD_DESCRIPTION}\"\\\" ${SCD_version_defs}" )

gen_plugins_cfg("ENC_SCD" ${ENC_SCD_GUID} "mfx_enc_scd64" "04" "")

set( defs " -DAS_ENC_SCD_PLUGIN ${defs} ${WARNING_FLAGS}" )
make_library( mfx_enc_scd64 hw shared )
msdk_install( mfx_enc_scd64 ${MFX_PLUGINS_DIR} )
set( defs "" )

