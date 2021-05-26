if(NOT MFX_DISABLE_SW_FALLBACK)
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

  add_library( ${mfxrtlibname} SHARED )

  target_link_options(${mfxrtlibname}
    PRIVATE
      $<$<PLATFORM_ID:Linux>:-Wl,--version-script=${MSDK_LIB_ROOT}/libmfxhw.map>
    )

  target_sources(${mfxrtlibname}
    PRIVATE
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
      ${CMAKE_CURRENT_SOURCE_DIR}/scheduler/linux/src/mfx_scheduler_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/scheduler/linux/src/mfx_scheduler_core_ischeduler.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/scheduler/linux/src/mfx_scheduler_core_iunknown.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/scheduler/linux/src/mfx_scheduler_core_task.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/scheduler/linux/src/mfx_scheduler_core_task_management.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/scheduler/linux/src/mfx_scheduler_core_thread.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/vpp/src/mfx_vpp_factory.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/auxiliary_device.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/cm_mem_copy.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/fast_compositing_ddi.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/fast_copy_sse4_impl.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/fast_copy_c_impl.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/fast_copy.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/fast_copy_multithreading.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/libmfx_allocator.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/libmfx_allocator_vaapi.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/libmfx_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/libmfx_core_vaapi.cpp
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

  set(IPP_LIBS "")
  if (NOT CMAKE_SYSTEM_NAME MATCHES Windows AND MFX_BUNDLED_IPP)
      set(IPPLIBS ipp)
  else()
      set(IPPLIBS IPP::msdk IPP::j IPP::vc IPP::cc IPP::cv IPP::i IPP::s IPP::core)
  endif()

  target_link_libraries(${mfxrtlibname}
      PUBLIC
        vmedia_buffers
        umc_io
        umc_va_hw
        umc
        vm
        vm_plus
        ${GENX_LIB}
        mfx_common_hw
        mfx_trace
        ${ITT_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT}
        ${CMAKE_DL_LIBS}
        ${IPP_LIBS}
      )

  target_compile_definitions(${mfxrtlibname}
    PRIVATE
      ${defs}
    )
endif()
