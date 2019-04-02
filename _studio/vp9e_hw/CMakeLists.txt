
add_definitions(-DVP9_LOGGING)

set(MFX_ORIG_LDFLAGS "${MFX_LDFLAGS}" )

set(VP9_Encoder_HW_GUID "ce44ef6f1a6d2246b412bb38d6e45182")

# Plugin version info
if( NOT DEFINED ENV{MFX_VP9_VERSION} )
  set( vp9_version 0.0.000.0000 )
else( )
  set( vp9_version $ENV{MFX_VP9_VERSION} )
endif( )

# =============================================================================

mfx_include_dirs( )

set(MFX_ROOTDIR "${CMAKE_CURRENT_SOURCE_DIR}/..")

#include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/include )
include_directories( ${MFX_ROOTDIR}/vp9e_hw/include )
include_directories( ${MFX_ROOTDIR}/shared/include )
include_directories( ${MFX_ROOTDIR}/mfx_lib/shared/include )
include_directories( ${MFX_ROOTDIR}/mfx_lib/scheduler/include )

# =============================================================================

#shared lib
set( sources "" )
set( sources.plus "" )

set( USE_STRICT_NAME TRUE )
set(MFX_LDFLAGS "${MFX_ORIG_LDFLAGS} -Wl,--version-script=${MFX_ROOTDIR}/mfx_lib/plugin/libmfxsw_plugin.map" )

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../mfx_lib/shared/include )

list( APPEND sources "${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_vp9_encode_plugin.cpp")

gen_plugins_cfg("VP9_Encoder_HW" ${VP9_Encoder_HW_GUID} "mfx_vp9e_hw64" "02" "VP9")

set( defs " -DAS_VP9E_PLUGIN ${API_FLAGS} ${WARNING_FLAGS} ${defs}" )
make_library( mfx_vp9e_hw64 hw shared )
msdk_install( mfx_vp9e_hw64 ${MFX_PLUGINS_DIR} )
set( defs "" )
