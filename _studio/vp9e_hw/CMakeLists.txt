
add_definitions(-DVP9_LOGGING)

set(MFX_ORIG_LDFLAGS "${MFX_LDFLAGS}" )

set(VP9_Encoder_HW_GUID "ce44ef6f1a6d2246b412bb38d6e45182")

# Plugin version info
if( __TARGET_PLATFORM STREQUAL "BDWMIN" OR __TARGET_PLATFORM STREQUAL "BDW" )
  set (VP9E_HW_PRODUCT_NAME "Intel(R) Media SDK 2018 for Embedded Linux")
  set (VP9E_HW_DESCRIPTION "Intel(R) Media SDK 2018 for Embedded Linux - VP9 Hardware Encode Plug-in")
else()
  set (VP9E_HW_PRODUCT_NAME "Intel(R) Media Server Studio 2018 - VP9 Decode and Encode")
  set (VP9E_HW_DESCRIPTION "Intel(R) Media Server Studio 2018 - VP9 Hardware Encode Plug-in")
endif()

if( NOT DEFINED ENV{MFX_VP9_VERSION} )
  set( vp9_version 0.0.000.0000 )
else( )
  set( vp9_version $ENV{MFX_VP9_VERSION} )
endif( )

if( Linux OR Darwin )
  execute_process(
    COMMAND echo
    COMMAND cut -f 1 -d.
    COMMAND date "+.%-y.%-m.%-d"
    OUTPUT_VARIABLE cur_date
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )
  string( SUBSTRING ${vp9_version} 0 1 ver )

  set(VP9E_HW_version_defs  "-DMFX_PLUGIN_FILE_VERSION=\"\\\"${ver}${cur_date}\"\\\" -DMFX_PLUGIN_PRODUCT_VERSION=\"\\\"${vp9_version}\"\\\"")
endif( )

# =============================================================================

mfx_include_dirs( )

set(MFX_ROOTDIR "${CMAKE_CURRENT_SOURCE_DIR}/..")

#include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/include )
include_directories( ${MFX_ROOTDIR}/vp9e_hw/include )
include_directories( ${MFX_ROOTDIR}/shared/include )
include_directories( ${MFX_ROOTDIR}/mfx_lib/plugin/include )
include_directories( ${MFX_ROOTDIR}/mfx_lib/scheduler/include )

# =============================================================================

#shared lib
set( sources "" )
set( sources.plus "" )

set( USE_STRICT_NAME TRUE )
set(MFX_LDFLAGS "${MFX_ORIG_LDFLAGS} -Wl,--version-script=${MFX_ROOTDIR}/mfx_lib/plugin/libmfxsw_plugin.map" )

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/include )

list( APPEND sources "${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_vp9_encode_plugin.cpp")

set( defs "-DMFX_PLUGIN_PRODUCT_NAME=\"\\\"${VP9E_HW_PRODUCT_NAME}\"\\\" -DMFX_FILE_DESCRIPTION=\"\\\"${VP9E_HW_DESCRIPTION}\"\\\" ${VP9E_HW_version_defs}" )

gen_plugins_cfg("VP9_Encoder_HW" ${VP9_Encoder_HW_GUID} "mfx_vp9e_hw64" "02" "VP9")

set( defs " -DAS_VP9E_PLUGIN ${API_FLAGS} ${WARNING_FLAGS} ${defs}" )
make_library( mfx_vp9e_hw64 hw shared )
msdk_install( mfx_vp9e_hw64 ${MFX_PLUGINS_DIR} )
set( defs "" )
