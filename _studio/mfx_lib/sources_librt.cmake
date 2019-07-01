if(NOT OPEN_SOURCE)
  set(sources
      ${CMAKE_CURRENT_SOURCE_DIR}/cmrt_cross_platform/src/cmrt_cross_platform.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/shared/src/libmfxsw.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/shared/src/libmfxsw_async.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/shared/src/libmfxsw_decode.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/shared/src/libmfxsw_enc.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/shared/src/libmfxsw_encode.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/shared/src/libmfxsw_plugin.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/shared/src/libmfxsw_query.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/shared/src/libmfxsw_session.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/shared/src/libmfxsw_vpp.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/shared/src/mfx_session.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/shared/src/mfx_user_plugin.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/shared/src/mfx_check_hardware_support.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/scheduler/src/mfx_scheduler_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/scheduler/src/mfx_scheduler_core_ischeduler.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/scheduler/src/mfx_scheduler_core_iunknown.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/scheduler/src/mfx_scheduler_core_task.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/scheduler/src/mfx_scheduler_core_task_management.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/scheduler/src/mfx_scheduler_core_thread.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/vpp/src/mfx_vpp_factory.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/auxiliary_device.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/cm_mem_copy.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/fast_compositing_ddi.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/fast_copy.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/libmfx_allocator.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/libmfx_allocator_vaapi.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/libmfx_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/libmfx_core_vaapi.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/libmfx_core_vdaapi.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/libmfx_core_factory.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/libmfx_core_hw.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/mfx_dxva2_device.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/mfx_search_dll.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/mfx_timing.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/mfx_umc_alloc_wrapper.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/mfx_umc_mjpeg_vpp.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/mfx_vpp_vaapi.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/mfx_static_assert_structs.cpp
      )
  set( sources.plus "" )

  if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
    set( mfxrtlibname mfxrt64 )
  else()
    set( mfxrtlibname mfxrt32 )
  endif()

  if( MFX_ENABLE_KERNELS )
    set(GENX_LIB "genx")
  else()
    set(GENX_LIB "")
  endif()

  # libmfxrt reuse the same .map file as full library
  set( MFX_LDFLAGS "${MFX_ORIG_LDFLAGS} -Wl,--version-script=${MSDK_LIB_ROOT}/libmfxhw.map" )

  set( LIBS "" )
  list( APPEND LIBS
    media_buffers
    umc_io
    umc_va_hw
    umc
    vm
    vm_plus
    ${GENX_LIB}
    mfx_common_hw
    mfx_trace
    ${ITT_LIBRARIES}
    ippmsdk_l ippj_l ippvc_l ippcc_l ippcv_l ippi_l ipps_l ippcore_l
    pthread
    dl )

  set( defs "${API_FLAGS} ${WARNING_FLAGS} ${defs}" )
  make_library( ${mfxrtlibname} hw shared)
  append_property( ${mfxrtlibname} COMPILE_FLAGS " -DMFX_RT" )
  set( defs "")
endif()
