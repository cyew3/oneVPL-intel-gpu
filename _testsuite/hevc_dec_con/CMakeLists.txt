mfx_include_dirs( )

include_directories (
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/smpl_common/include
)

include_directories( ${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/decode/h265/include )
include_directories( ${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/encode/h265/include )
include_directories( ${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/cmrt_cross_platform/include )
include_directories( ${CMAKE_HOME_DIRECTORY}/_studio/shared/umc/codec/h265_dec/include )

foreach( prefix ${CMAKE_CURRENT_SOURCE_DIR}/smpl_common/src/vm )

  list( APPEND sample_common_sources
    ${prefix}/thread_linux.cpp
    ${prefix}/thread.cpp
    ${prefix}/time.cpp
    ${prefix}/atomic.cpp
    ${prefix}/atomic_linux.cpp
    ${prefix}/shared_object_linux.cpp
    ${prefix}/shared_object.cpp
    ${prefix}/time_linux.cpp
  )

endforeach()

foreach( prefix ${CMAKE_CURRENT_SOURCE_DIR}/smpl_common/src )
list( APPEND sample_common_sources
    ${prefix}/vaapi_utils_x11.cpp
    ${prefix}/vaapi_allocator.cpp
    ${prefix}/vaapi_utils_drm.cpp
    ${prefix}/vaapi_utils_android.cpp
    ${prefix}/vaapi_device.cpp
    ${prefix}/vaapi_utils.cpp
    ${prefix}/sysmem_allocator.cpp
    ${prefix}/d3d_allocator.cpp
    ${prefix}/base_allocator.cpp
    ${prefix}/mfx_buffering.cpp
    ${prefix}/mfx_decode_buffering.cpp
    ${prefix}/general_allocator.cpp
    ${prefix}/vpp_ex.cpp
    ${prefix}/sample_utils.cpp
    ${prefix}/d3d_device.cpp
    ${prefix}/d3d11_allocator.cpp
    ${prefix}/mfx_plugin_uids.cpp
    ${prefix}/d3d11_device.cpp
  )

endforeach()


set (HEVC_PRODUCT_NAME "Intel(R) Media SDK HEVC Software Pack")
set (sw_HEVC_DECODER_DESCRIPTION "Intel(R) Media SDK HEVC Software Standalone Decoder")
set (hw_HEVC_DECODER_DESCRIPTION "Intel(R) Media SDK HEVC Hardware Standalone Decoder")

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

    set(hevc_version_defs  "-DMFX_PLUGIN_FILE_VERSION=\"\\\"${ver}${cur_date}\"\\\" -DMFX_PLUGIN_PRODUCT_VERSION=\"\\\"${hevc_version}\"\\\"")
  endif( )

foreach( prefix ${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/shared/src )

  list( APPEND plugin_common_sources
    ${prefix}/mfx_check_hardware_support.cpp
    ${prefix}/mfx_common_int.cpp
    ${prefix}/mfx_common_decode_int.cpp
    ${prefix}/libmfxsw.cpp
    ${prefix}/libmfxsw_async.cpp
    ${prefix}/libmfxsw_query.cpp
    ${prefix}/libmfxsw_session.cpp
    ${prefix}/mfx_session.cpp
    ${prefix}/mfx_user_plugin.cpp
  )

endforeach()

foreach( prefix ${CMAKE_HOME_DIRECTORY}/_studio/shared/src )
  list( APPEND plugin_common_sources
    ${prefix}/cm_mem_copy.cpp
    ${prefix}/fast_copy.cpp
    ${prefix}/libmfx_allocator.cpp
    ${prefix}/libmfx_allocator_vaapi.cpp
    ${prefix}/libmfx_core.cpp
    ${prefix}/libmfx_core_factory.cpp
    ${prefix}/libmfx_core_hw.cpp
    ${prefix}/libmfx_core_vaapi.cpp
    ${prefix}/mfx_umc_alloc_wrapper.cpp
  )
endforeach()
list( APPEND plugin_common_sources
  ${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/cmrt_cross_platform/src/cmrt_cross_platform.cpp
)

foreach( prefix ${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/scheduler/src )
  list( APPEND plugin_common_sources
    ${prefix}/mfx_scheduler_core.cpp
    ${prefix}/mfx_scheduler_core_iunknown.cpp
    ${prefix}/mfx_scheduler_core_ischeduler.cpp
    ${prefix}/mfx_scheduler_core_task.cpp
    ${prefix}/mfx_scheduler_core_task_management.cpp
    ${prefix}/mfx_scheduler_core_thread.cpp
  )
endforeach()

# HEVC

set( sources "" )
set( sources.plus "" )

list( APPEND sources ${sample_common_sources} ${plugin_common_sources} )

include_directories( ${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/scheduler/include )
include_directories( ${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/plugin/include )

list( APPEND sources ${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/shared/src/libmfxsw_decode.cpp )
list( APPEND sources ${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/decode/h265/src/mfx_h265_dec_decode.cpp )

set( sources.plus "" )
foreach( prefix ${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/plugin/src )
  list( APPEND sources.plus
    ${prefix}/mfx_hevc_dec_plugin.cpp
  )
endforeach()

foreach( prefix ${CMAKE_CURRENT_SOURCE_DIR}/src )
  list( APPEND sources.plus
    ${prefix}/sample_decode.cpp
    ${prefix}/pipeline_decode.cpp
  )
endforeach()


set( USE_STRICT_NAME TRUE )
set(MFX_LDFLAGS "${MFX_ORIG_LDFLAGS} -Wl,--version-script=${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/plugin/libmfxsw_plugin.map" )

# HEVC Standalone Decoder

set (description_name ${${variant}_HEVC_DECODER_DESCRIPTION})

set( LIBS "" )
list( APPEND LIBS
    umc_h265_sw
    umc_csc_sw
    hevc_pp_dispatcher
    hevc_pp_atom
    hevc_pp_avx2
    hevc_pp_px
    hevc_pp_sse4
    hevc_pp_ssse3
    umc_io_merged_sw
    umc_core_merged 
    ${ITT_LIBS}
    ippmsdk_l ippj_l ippvc_l ippcc_l ippcv_l ippi_l ipps_l ippcore_l
    pthread
    dl
)

set( defs "-DMFX_ONE_THREAD -DAS_HEVCD_PLUGIN -DMFX_PLUGIN_PRODUCT_NAME=\"\\\"${HEVC_PRODUCT_NAME}\"\\\" -DMFX_FILE_DESCRIPTION=\"\\\"${description_name}\"\\\" ${hevc_version_defs}" )

make_executable( hevc_dec_con none )
