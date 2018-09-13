set( MFX_ORIG_LDFLAGS "${MFX_LDFLAGS}" )

mfx_include_dirs()

set( HEVC_Encoder_HW_GUID "6fadc791a0c2eb479ab6dcd5ea9da347" )

# Plugin version info
set( HEVCE_HW_PRODUCT_NAME "Intel(R) Media SDK" )
set( HEVCE_HW_DESCRIPTION "Intel(R) Media SDK HEVCe plugin" )

if( NOT DEFINED ENV{MFX_HEVC_VERSION} )
  set( hevc_version 0.0.000.0000 )
else()
  set( hevc_version $ENV{MFX_HEVC_VERSION} )
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set( plugin_name mfx_hevce_hw64 )
else()
  set( plugin_name mfx_hevce_hw32 )
endif()

set_file_and_product_version( ${hevc_version} version_defs )

set( sources "" )
set( sources.plus "" )
set( LIBS "" )
set( defs "" )

include_directories( ${MSDK_STUDIO_ROOT}/hevce_hw/include )
include_directories( ${MSDK_STUDIO_ROOT}/hevce_hw/h265/include )
set( dir ${MSDK_STUDIO_ROOT}/hevce_hw/h265/src )
file( GLOB_RECURSE srcs "${dir}/*.c" "${dir}/*.cpp" )
list( APPEND sources ${srcs} )

list( APPEND LIBS
  mfx_common_hw
  media_buffers
  umc_io
  umc_va_elk_hw
  umc
  vm
  vm_plus
  ${ITT_LIBS}
  pthread
  dl
  ippmsdk_l ippj_l ippvc_l ippcc_l ippcv_l ippi_l ipps_l ippcore_l
)

set( USE_STRICT_NAME TRUE )
set( MFX_LDFLAGS "${MFX_ORIG_LDFLAGS} -Wl,--version-script=${MSDK_STUDIO_ROOT}/mfx_lib/plugin/libmfxsw_plugin.map" )
set( defs "${defs} -DMFX_PLUGIN_PRODUCT_NAME=\"\\\"${HEVCE_HW_PRODUCT_NAME}\"\\\"" )
set( defs "${defs} -DMFX_FILE_DESCRIPTION=\"\\\"${HEVCE_HW_DESCRIPTION}\"\\\"" )
set( defs "${defs} ${version_defs}" )
set( defs "-DAS_HEVCE_PLUGIN ${defs} " )
gen_plugins_cfg( "HEVC_Encoder_HW" ${HEVC_Encoder_HW_GUID} ${plugin_name} "02" "HEVC" )
make_library( ${plugin_name} hw shared )
install( TARGETS ${plugin_name} LIBRARY DESTINATION ${MFX_PLUGINS_DIR} )
