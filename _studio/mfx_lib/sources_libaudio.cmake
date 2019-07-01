if(NOT OPEN_SOURCE)
  set(sources
      ${CMAKE_CURRENT_SOURCE_DIR}/scheduler/src/mfx_scheduler_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/scheduler/src/mfx_scheduler_core_ischeduler.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/scheduler/src/mfx_scheduler_core_iunknown.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/scheduler/src/mfx_scheduler_core_task.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/scheduler/src/mfx_scheduler_core_task_management.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/scheduler/src/mfx_scheduler_core_thread.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/shared/src/libmfxsw.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/shared/src/libmfxsw_async.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/shared/src/libmfxsw_audio_decode.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/shared/src/libmfxsw_audio_encode.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/shared/src/libmfxsw_ext.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/shared/src/libmfxsw_session.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/shared/src/mfx_session.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/shared/src/mfx_common_int.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/fast_copy.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/libmfx_allocator.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/libmfx_core.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/libmfx_core_factory.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/mfx_umc_alloc_wrapper.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/mfx_umc_mjpeg_vpp.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/mfx_search_dll.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/mfx_timing.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/../shared/src/mfx_static_assert_structs.cpp
      )
  set(sources.plus "")

  set( USE_STRICT_NAME TRUE )
  set(MFX_LDFLAGS "${MFX_ORIG_LDFLAGS} -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/libmfxaudio.map" )

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

  set( LIBS "" )
  list( APPEND LIBS
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
    ${ITT_LIBRARIES}
    ippcc_l ippdc_l ippac_l ippi_l ipps_l ippcore_l
    pthread
    dl
    )

  set( defs "${API_FLAGS} ${WARNING_FLAGS} ${defs}" )
  make_library( ${mfxlibname} sw shared)
  set( defs "")
endif()
