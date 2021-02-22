if(NOT MFX_DISABLE_SW_FALLBACK)
  set( USE_STRICT_NAME TRUE )

  if( MFX_ENABLE_KERNELS )
    set(MCTF_LIB "mctf_hw")
  else()
    set(MCTF_LIB "")
  endif()

  if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
    set( mfxlibname mfxaudiosw64 )
  else()
    set( mfxlibname mfxaudiosw32 )
  endif()

  add_library( ${mfxlibname} SHARED )

  target_link_options(${mfxlibname}
    PRIVATE
      $<$<PLATFORM_ID:Linux>:-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/libmfxaudio.map>
    )

  target_sources(${mfxlibname}
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/scheduler/linux/src/mfx_scheduler_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/scheduler/linux/src/mfx_scheduler_core_ischeduler.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/scheduler/linux/src/mfx_scheduler_core_iunknown.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/scheduler/linux/src/mfx_scheduler_core_task.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/scheduler/linux/src/mfx_scheduler_core_task_management.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/scheduler/linux/src/mfx_scheduler_core_thread.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/shared/src/libmfxsw.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/shared/src/libmfxsw_async.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/shared/src/libmfxsw_audio_decode.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/shared/src/libmfxsw_audio_encode.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/shared/src/libmfxsw_ext.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/shared/src/libmfxsw_session.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/shared/src/mfx_session.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/shared/src/mfx_common_int.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/fast_copy_sse4_impl.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/fast_copy_c_impl.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/fast_copy.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/fast_copy_multithreading.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/libmfx_allocator.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/libmfx_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/libmfx_core_factory.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/mfx_umc_alloc_wrapper.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/mfx_umc_mjpeg_vpp.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/mfx_search_dll.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/mfx_timing.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/mfx_static_assert_structs.cpp
    )

  set(IPP_LIBS "")
  if (NOT CMAKE_SYSTEM_NAME MATCHES Windows AND MFX_BUNDLED_IPP)
      set(IPPLIBS ipp)
  else()
      set(IPPLIBS IPP::cc IPP::dc IPP::ac IPP::i IPP::s IPP::core)
  endif()

  target_link_libraries(${mfxlibname}
    PUBLIC
      mfx_shared_lib
      enc
      encode
      pak
      decode_sw
      asc
      ${MCTF_LIB}
      vpp_sw
      cmrt_cross_platform_hw
      genx
      audio_decode
      audio_encode
      umc_acodecs_merged
      umc_codecs_merged
      media_buffers
      umc_va_hw
      umc_io
      umc
      vm
      vm_plus
      mfx_trace
    PRIVATE
      mfx_sdl_properties 
      ${ITT_LIBRARIES}
      ${CMAKE_THREAD_LIBS_INIT}
      ${CMAKE_DL_LIBS}
      ${IPP_LIBS}
    )

  target_compile_definitions(${mfxlibname}
    PRIVATE
      ${defs}
    )
endif()
