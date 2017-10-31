set(MFX_ORIG_LDFLAGS "${MFX_LDFLAGS}" )

set(HEVC_FEI_Encoder_HW_GUID "5418a70666f94d5cb4f7b1caee86339b")

# Plugin version info
set (HEVC_FEI_HW_PRODUCT_NAME "Intel(R) Media Server Studio 2017 - HEVC FEI Encode")
set (HEVC_FEI_HW_DESCRIPTION "Intel(R) Media Server Studio 2017 - HEVC FEI Hardware Encode Plug-in")


if( NOT DEFINED ENV{MFX_HEVC_VERSION} )
  set( hevc_version 0.0.000.0000 )
else( )
  set( hevc_version $ENV{MFX_HEVC_VERSION} )
endif( )

if( Linux OR Darwin )
  execute_process(
    COMMAND echo
    COMMAND cut -f 1 -d.
    COMMAND date "+.%-y.%-m.%-d"
    OUTPUT_VARIABLE cur_date
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )
  string( SUBSTRING ${hevc_version} 0 1 ver )

  set(HEVCE_HW_version_defs  "-DMFX_PLUGIN_FILE_VERSION=\"\\\"${ver}${cur_date}\"\\\" -DMFX_PLUGIN_PRODUCT_VERSION=\"\\\"${hevc_version}\"\\\"")
  set(HEVCE_FEI_HW_version_defs  "-DMFX_PLUGIN_FILE_VERSION=\"\\\"${ver}${cur_date}\"\\\" -DMFX_PLUGIN_PRODUCT_VERSION=\"\\\"${hevc_version}\"\\\"")
endif( )

# =============================================================================

mfx_include_dirs( )

set(MFX_ROOTDIR "${CMAKE_CURRENT_SOURCE_DIR}/..")

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/include )
include_directories( ${MFX_ROOTDIR}/shared/include )
include_directories( ${MFX_ROOTDIR}/mfx_lib/plugin/include )
include_directories( ${MFX_ROOTDIR}/mfx_lib/scheduler/include )
include_directories( ${MFX_ROOTDIR}/mfx_lib/cmrt_cross_platform/include )
#include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/genx/include )
#include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/genx/src )

# =============================================================================

set( sources "" )
set( sources.plus "" )

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../hevce_hw/h265/include )

foreach( dir ${CMAKE_CURRENT_SOURCE_DIR}/../hevce_hw/h265/src )
  file( GLOB_RECURSE srcs "${dir}/*.c" "${dir}/*.cpp" )
  list( APPEND sources ${srcs})
endforeach()


set( defs "-DMFX_PLUGIN_PRODUCT_NAME=\"\\\"${HEVCE_HW_PRODUCT_NAME}\"\\\" -DMFX_FILE_DESCRIPTION=\"\\\"${HEVCE_HW_DESCRIPTION}\"\\\" ${HEVCE_HW_version_defs}" )

set( defs " -DAS_HEVCE_PLUGIN ${API_FLAGS} ${WARNING_FLAGS} ${defs}" )
make_library( mfx_hevce_lib hw static )

set( defs "" )

# =============================================================================

#shared lib
set( sources "" )
set( sources.plus "" )

set( USE_STRICT_NAME TRUE )
set(MFX_LDFLAGS "${MFX_ORIG_LDFLAGS} -Wl,--version-script=${MFX_ROOTDIR}/mfx_lib/plugin/libmfxsw_plugin.map" )

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../hevce_hw/h265/include )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/h265_fei/include )

foreach( dir ${CMAKE_CURRENT_SOURCE_DIR}/h265_fei/src )
  file( GLOB_RECURSE srcs "${dir}/*.c" "${dir}/*.cpp" )
  list( APPEND sources ${srcs})
endforeach()

  set( LIBS "" )
  list( APPEND LIBS
    mfx_hevce_lib_hw
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

set( defs "${API_FLAGS} -DMFX_PLUGIN_PRODUCT_NAME=\"\\\"${HEVC_FEI_HW_PRODUCT_NAME}\"\\\" -DMFX_FILE_DESCRIPTION=\"\\\"${HEVC_FEI_HW_DESCRIPTION}\"\\\" ${HEVCE_FEI_HW_version_defs}" )

gen_plugins_cfg("HEVC_Encoder_HW" ${HEVC_FEI_Encoder_HW_GUID} "mfx_hevc_fei_hw64" "02" "HEVC")

set( defs " -DAS_HEVCE_PLUGIN -DAS_HEVC_FEI_ENCODE_PLUGIN ${API_FLAGS} ${WARNING_FLAGS} ${defs}" )
make_library( mfx_hevc_fei_hw64 hw shared )
msdk_install( mfx_hevc_fei_hw64 ${MFX_PLUGINS_DIR} )
set( defs "" )
