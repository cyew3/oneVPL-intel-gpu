add_library(fast_copy_sse4 OBJECT
  ${MSDK_STUDIO_ROOT}/shared/include/fast_copy_sse4_impl.h
  ${MSDK_STUDIO_ROOT}/shared/src/fast_copy_sse4_impl.cpp
)
target_include_directories(fast_copy_sse4 PRIVATE ${MSDK_STUDIO_ROOT}/shared/include)
target_link_libraries(fast_copy_sse4 PRIVATE bitrate_control mfx_require_sse4_properties)

set(sources_common
    scheduler/$<IF:$<PLATFORM_ID:Linux>,linux,windows>/src/mfx_scheduler_core.cpp
    scheduler/$<IF:$<PLATFORM_ID:Linux>,linux,windows>/src/mfx_scheduler_core_ischeduler.cpp
    scheduler/$<IF:$<PLATFORM_ID:Linux>,linux,windows>/src/mfx_scheduler_core_iunknown.cpp
    scheduler/$<IF:$<PLATFORM_ID:Linux>,linux,windows>/src/mfx_scheduler_core_task.cpp
    scheduler/$<IF:$<PLATFORM_ID:Linux>,linux,windows>/src/mfx_scheduler_core_task_management.cpp
    scheduler/$<IF:$<PLATFORM_ID:Linux>,linux,windows>/src/mfx_scheduler_core_thread.cpp

    shared/src/libmfxsw.cpp
    shared/src/libmfxsw_async.cpp
    shared/src/libmfxsw_decode.cpp
    shared/src/libmfxsw_encode.cpp
    shared/src/libmfxsw_functions.cpp
    shared/src/libmfxsw_enc.cpp
    shared/src/libmfxsw_pak.cpp
    shared/src/libmfxsw_plugin.cpp
    $<$<NOT:$<BOOL:${API_USE_VPL}>>:
      shared/src/mfx_user_plugin.cpp
    >
    shared/src/libmfxsw_query.cpp
    shared/src/libmfxsw_session.cpp
    shared/src/libmfxsw_vpp.cpp
    shared/src/libmfxsw_decode_vp.cpp
    shared/src/mfx_check_hardware_support.cpp
    shared/src/mfx_session.cpp

    shared/include/feature_blocks/mfx_feature_blocks_base.h
    shared/include/feature_blocks/mfx_feature_blocks_decl_blocks.h
    shared/include/feature_blocks/mfx_feature_blocks_init_macros.h
    shared/include/feature_blocks/mfx_feature_blocks_utils.h
    shared/include/mfx_utils_extbuf.h
    ${MSDK_CMAKE_BINARY_ROOT}/mfxconfig.h  # Cmake-generated configuration derived from BuildOptions
    ${MSDK_STUDIO_ROOT}/shared/include/mfx_config.h

    ${MSDK_STUDIO_ROOT}/shared/include/mfx_common.h

    ${MSDK_STUDIO_ROOT}/shared/include/cm_mem_copy.h
    ${MSDK_STUDIO_ROOT}/shared/include/dispatch_session.h
    ${MSDK_STUDIO_ROOT}/shared/include/fast_copy_c_impl.h
    ${MSDK_STUDIO_ROOT}/shared/include/fast_copy.h
    ${MSDK_STUDIO_ROOT}/shared/include/libmfx_allocator.h
    ${MSDK_STUDIO_ROOT}/shared/include/libmfx_core.h
    ${MSDK_STUDIO_ROOT}/shared/include/libmfx_core_factory.h
    ${MSDK_STUDIO_ROOT}/shared/include/libmfx_core_interface.h
    ${MSDK_STUDIO_ROOT}/shared/include/libmfx_core_operation.h
    ${MSDK_STUDIO_ROOT}/shared/include/mfxaudio++int.h
    ${MSDK_STUDIO_ROOT}/shared/include/mfxenctools-int.h
    ${MSDK_STUDIO_ROOT}/shared/include/mfxstructures-int.h
    ${MSDK_STUDIO_ROOT}/shared/include/mfxstructurespro.h
    ${MSDK_STUDIO_ROOT}/shared/include/mfxvideo++int.h
    ${MSDK_STUDIO_ROOT}/shared/include/mfx_cenc.h
    ${MSDK_STUDIO_ROOT}/shared/include/mfx_common.h
    ${MSDK_STUDIO_ROOT}/shared/include/mfx_config.h
    ${MSDK_STUDIO_ROOT}/shared/include/mfx_ext_buffers.h
    ${MSDK_STUDIO_ROOT}/shared/include/mfx_reflect.h
    ${MSDK_STUDIO_ROOT}/shared/include/mfx_timing.h
    ${MSDK_STUDIO_ROOT}/shared/include/mfx_trace.h
    ${MSDK_STUDIO_ROOT}/shared/include/mfx_umc_alloc_wrapper.h
    ${MSDK_STUDIO_ROOT}/shared/include/mfx_utils.h
    ${MSDK_STUDIO_ROOT}/shared/include/mfx_vpp_interface.h

    ${MSDK_STUDIO_ROOT}/shared/src/fast_copy_c_impl.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/fast_copy.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/libmfx_allocator.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/libmfx_core.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/libmfx_core_factory.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/mfx_umc_alloc_wrapper.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/mfx_umc_mjpeg_vpp.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/mfx_timing.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/mfx_static_assert_structs.cpp

    $<$<PLATFORM_ID:Windows>:$<IF:$<BOOL:${API_USE_VPL}>,libvpl.def,libmfx.def>>
    $<$<PLATFORM_ID:Windows>:$<IF:$<BOOL:${API_USE_VPL}>,libmfx-gen.rc,libmfxhw.rc>>

    $<TARGET_OBJECTS:fast_copy_sse4>
  )

set(sources_hw
    ${MSDK_STUDIO_ROOT}/shared/include/libmfx_allocator_vaapi.h
    ${MSDK_STUDIO_ROOT}/shared/include/libmfx_core_hw.h
    ${MSDK_STUDIO_ROOT}/shared/include/libmfx_core_vaapi.h
    ${MSDK_STUDIO_ROOT}/shared/include/mfx_ext_ddi.h
    ${MSDK_STUDIO_ROOT}/shared/include/mfx_vpp_vaapi.h

    ${MSDK_STUDIO_ROOT}/shared/src/auxiliary_device.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/cm_mem_copy.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/fast_compositing_ddi.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/mfx_vpp_vaapi.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/libmfx_allocator_vaapi.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/libmfx_allocator_d3d9.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/libmfx_allocator_d3d11.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/libmfx_core_hw.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/libmfx_core_vaapi.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/libmfx_core_d3d9.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/libmfx_core_d3d11.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/libmfx_core_d3d9on11.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/d3d11_decode_accelerator.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/d3d11_video_processor.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/mfx_dxva2_device.cpp
  )

if(NOT MFX_DISABLE_SW_FALLBACK)
  list(APPEND sources_hw
    ${MSDK_STUDIO_ROOT}/shared/src/fast_copy_multithreading.cpp
  )
endif()

get_mfx_version(mfx_version_major mfx_version_minor)

if(MFX_DISABLE_SW_FALLBACK OR ${API_USE_VPL})
  set(VARIANTS hw)
else()
  set(VARIANTS sw hw)
endif()

set(IPP_LIBS "")
if (MFX_BUNDLED_IPP)
  set(IPP_LIBS ipp)
else()
  set(IPP_LIBS IPP::msdk IPP::j IPP::vc IPP::cc IPP::cv IPP::i IPP::s IPP::core)
endif()

if (CMAKE_SYSTEM_NAME MATCHES Windows)

  add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/media_sdk_etw.rc ${CMAKE_BINARY_DIR}/media_sdk_etw.h
    SOURCES ${MSDK_STUDIO_ROOT}/shared/mfx_trace/media_sdk_etw.man
    COMMENT Running Windows Message Compiler to compile Media SDK ETW manifest
    COMMAND mc ARGS -h "${CMAKE_BINARY_DIR}" -r "${CMAKE_BINARY_DIR}" -z media_sdk_etw -um "${MSDK_STUDIO_ROOT}/shared/mfx_trace/media_sdk_etw.man"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
  )

  add_custom_target(windows_etw_generator 
    DEPENDS 
      ${CMAKE_BINARY_DIR}/media_sdk_etw.rc
      ${CMAKE_BINARY_DIR}/media_sdk_etw.h
  )
endif()

foreach( variant ${VARIANTS} )

  if( DEFINED MFX_LIBNAME )
    set( mfxlibname "${MFX_LIBNAME}")
  else()
    make_runtime_name( ${variant} mfxlibname )
  endif()

  message( DEBUG "Runtime name: ${mfxlibname}")

  add_library(${mfxlibname} SHARED)

  target_sources(${mfxlibname} PRIVATE ${sources_common})
 

  if (CMAKE_SYSTEM_NAME MATCHES Windows)
    add_dependencies(${mfxlibname} windows_etw_generator)
    target_sources(${mfxlibname}
      PRIVATE
        ${CMAKE_BINARY_DIR}/media_sdk_etw.h
        ${CMAKE_BINARY_DIR}/media_sdk_etw.rc
    )
  endif()

  if (variant MATCHES hw)
    target_sources(${mfxlibname} PRIVATE ${sources_hw})
  endif()

  set (JPEG_SW_LIBS "")
  if (MFX_ENABLE_JPEG_SW_FALLBACK)
    set(JPEG_SW_LIBS encode encode_sw)
  endif ()
 
  set (SW_FALBACK_LIBS "")
  if (NOT MFX_DISABLE_SW_FALLBACK)
    set(SW_FALBACK_LIBS encode)

    if (NOT ${API_USE_VPL})
      list(APPEND SW_FALBACK_LIBS
        scene_analyzer
        color_space_converter
        media_buffers
        encode
        vc1_enc
        me
        mpeg2_enc
        h264_enc_sw
        h264_dispatcher
        h264_px
        h264_sse3
        enc_hw
        enc
        pak
        umc
        umc_io
      )
    endif()
  endif ()

  set(HW_ONLY_LIBS "")
  if (variant MATCHES hw)
    set(HW_ONLY_LIBS umc_va_hw cmrt_cross_platform_hw $<$<BOOL:${MFX_ENABLE_USER_ENCTOOLS}>:enctools_hw> asc genx)
  endif()

  set(LIBS
    bitrate_control
    ${JPEG_SW_LIBS}
    ${SW_FALBACK_LIBS}
    ${HW_ONLY_LIBS}
    decode_${variant}
    encode_${variant}
    mfx_common_${variant}
    vpp_${variant}
    mfx_trace
    mfx_shared_lib
    mfx_sdl_properties
    ${ITT_LIBRARIES}
    Threads::Threads
    ${CMAKE_DL_LIBS}
  )

  if (variant MATCHES hw)
    target_compile_definitions(${mfxlibname}
      PRIVATE
        MFX_VA
    )
  endif()

  target_include_directories(${mfxlibname}
    PRIVATE
      scheduler/include
      scheduler/$<IF:$<PLATFORM_ID:Linux>,linux,windows>/include
      fei/include
    )

  target_link_options(${mfxlibname}
    PRIVATE
      $<$<PLATFORM_ID:Linux>:LINKER:--version-script=${MSDK_LIB_ROOT}/libmfxhw.map>
    )
  target_link_libraries(${mfxlibname}
    PRIVATE
      ${LIBS}
      $<$<BOOL:${MFX_ENABLE_H264_VIDEO_FEI_ENCODE}>:h264_preenc>
      $<$<BOOL:${MFX_ENABLE_H264_VIDEO_FEI_ENCODE}>:h264_enc>
      $<$<BOOL:${MFX_ENABLE_H264_VIDEO_FEI_ENCODE}>:h264_pak>
      $<$<BOOL:${MFX_ENABLE_USER_ENCTOOLS}>:enctools_hw>
    )

  set(libversion   ${mfx_version_major}.${mfx_version_minor})
  set(libsoversion ${mfx_version_major})
  if(${API_USE_VPL})
    set(libversion   1.${libversion})
    set(libsoversion 1.${libsoversion})
  endif()

  set_target_properties(${mfxlibname} PROPERTIES
     VERSION ${libversion}
     SOVERSION ${libsoversion}
     PDB_NAME ${mfxlibname}_full 
     PDB_OUTPUT_DIR ${CMAKE_BINARY_DIR}
  )

  install(TARGETS ${mfxlibname} LIBRARY DESTINATION ${MFX_MODULES_DIR})

  if (CMAKE_SYSTEM_NAME MATCHES Linux)
    set( PKG_CONFIG_FNAME "${CMAKE_LIB_DIR}/${CMAKE_BUILD_TYPE}/lib${mfxlibname}.pc")
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/pkg-config.pc.cmake" ${PKG_CONFIG_FNAME} @ONLY)
    install( FILES ${PKG_CONFIG_FNAME} DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig )
  endif()

endforeach()

include(sources_ext.cmake OPTIONAL)
