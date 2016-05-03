set(MFX_ORIG_LDFLAGS "${MFX_LDFLAGS}" )

set(HEVC_Encoder_HW_GUID "6fadc791a0c2eb479ab6dcd5ea9da347")

# Plugin version info

set (HEVCE_HW_PRODUCT_NAME "Intel(R) Media Server Studio 2017 - HEVC Decode and Encode")
set (HEVCE_HW_DESCRIPTION "Intel(R) Media Server Studio 2017 - HEVC Hardware Encode Plug-in")

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

#shared lib
set( sources "" )
set( sources.plus "" )

set( USE_STRICT_NAME TRUE )
set(MFX_LDFLAGS "${MFX_ORIG_LDFLAGS} -Wl,--version-script=${MFX_ROOTDIR}/mfx_lib/plugin/libmfxsw_plugin.map" )

# foreach( prefix ${CMAKE_CURRENT_SOURCE_DIR}/../mfx_lib/shared/src )

#   list( APPEND sources
#     ${prefix}/mfx_check_hardware_support.cpp
#     ${prefix}/mfx_common_int.cpp
#     ${prefix}/mfx_common_decode_int.cpp
#     ${prefix}/libmfxsw.cpp
#     ${prefix}/libmfxsw_async.cpp
#     ${prefix}/libmfxsw_query.cpp
#     ${prefix}/libmfxsw_session.cpp
#     ${prefix}/mfx_session.cpp
#     ${prefix}/mfx_enc_common.cpp
#   )

# endforeach()

# foreach( prefix ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src )
#   list( APPEND sources
#     ${prefix}/cm_mem_copy.cpp
#     ${prefix}/fast_copy.cpp
#     ${prefix}/libmfx_allocator.cpp
#     ${prefix}/libmfx_allocator_vaapi.cpp
#     ${prefix}/libmfx_core.cpp
#     ${prefix}/libmfx_core_factory.cpp
#     ${prefix}/libmfx_core_hw.cpp
#     ${prefix}/libmfx_core_vaapi.cpp
#     ${prefix}/mfx_umc_alloc_wrapper.cpp
#   )
# endforeach()
# list( APPEND sources
#   ${CMAKE_CURRENT_SOURCE_DIR}/../mfx_lib/cmrt_cross_platform/src/cmrt_cross_platform.cpp
# )

# foreach( prefix ${CMAKE_CURRENT_SOURCE_DIR}/../mfx_lib/scheduler/src )
#   list( APPEND sources
#     ${prefix}/mfx_scheduler_core.cpp
#     ${prefix}/mfx_scheduler_core_iunknown.cpp
#     ${prefix}/mfx_scheduler_core_ischeduler.cpp
#     ${prefix}/mfx_scheduler_core_task.cpp
#     ${prefix}/mfx_scheduler_core_task_management.cpp
#     ${prefix}/mfx_scheduler_core_thread.cpp
#   )
# endforeach()

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/h265/include )

foreach( dir ${CMAKE_CURRENT_SOURCE_DIR}/h265/src )
  file( GLOB_RECURSE srcs "${dir}/*.c" "${dir}/*.cpp" )
  list( APPEND sources ${srcs})
endforeach()

  set( LIBS "" )
  list( APPEND LIBS
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

set( defs "-DMFX_PLUGIN_PRODUCT_NAME=\"\\\"${HEVCE_HW_PRODUCT_NAME}\"\\\" -DMFX_FILE_DESCRIPTION=\"\\\"${HEVCE_HW_DESCRIPTION}\"\\\" ${HEVCE_HW_version_defs}" )

gen_plugins_cfg("HEVC_Encoder_HW" ${HEVC_Encoder_HW_GUID} "mfx_hevce_hw64" "02" "HEVC")

set( defs "${defs} ${WARNING_FLAGS}" )
make_library( mfx_hevce_hw64 hw shared )
install( TARGETS mfx_hevce_hw64 LIBRARY DESTINATION ${MFX_PLUGINS_DIR} )
set( defs "" )
