# Copyright (c) 2017-2019 Intel Corporation

set( MFX_ORIG_LDFLAGS "${MFX_LDFLAGS}" )

mfx_include_dirs()

set( HEVC_Encoder_HW_GUID "6fadc791a0c2eb479ab6dcd5ea9da347" )

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set( plugin_name mfx_hevce_hw64 )
else()
  set( plugin_name mfx_hevce_hw32 )
endif()

# Plugin version info
set_file_and_product_version( ${MEDIA_VERSION_STR} version_defs )

set( sources "" )
set( sources.plus "" )
set( LIBS "" )
set( defs "" )

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/h265/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../mfx_lib/shared/include )
list( APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/h265/src/mfx_h265_encode_plugin.cpp )

set( USE_STRICT_NAME TRUE )
set( MFX_LDFLAGS "${MFX_ORIG_LDFLAGS} -Wl,--version-script=${MSDK_STUDIO_ROOT}/mfx_lib/plugin/libmfxsw_plugin.map" )
set( defs "${defs} ${version_defs}" )
set( defs "-DAS_HEVCE_PLUGIN ${defs} " )
gen_plugins_cfg( "HEVC_Encoder_HW" ${HEVC_Encoder_HW_GUID} ${plugin_name} "02" "HEVC" )
make_library( ${plugin_name} hw shared )
install( TARGETS ${plugin_name} LIBRARY DESTINATION ${MFX_PLUGINS_DIR} )
