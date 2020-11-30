if(NOT OPEN_SOURCE)
  set(HEVC_DECODER_SW_GUID   "15dd936825ad475ea34e35f3f54217a6")
  #set(VP8_DECODER_SW_GUID    "bca276ec7a854a598c30e0dc5748f1f6")
  #set(VP9_DECODER_SW_GUID    "a9a276ec7a854a598c30e0dc5748f1f6")

  set(HEVC_ENCODER_GACC_GUID "e5400a06c74d41f5b12d430bbaa23d0b")
  set(HEVC_ENCODER_SW_GUID   "2fca99749fdb49aeb121a5b63ef568f7")
  set(H265FEI_ENCODER_GUID   "87e0e80207375240852515cf4a5edde6")
  set(AV1_ENCODER_GACC_GUID  "343d689665ba4548829942a32a386569")

  set(IPP_LIBS "")
  if (MFX_BUNDLED_IPP)
    set(IPP_LIBS ipp)
  else()
    set(IPP_LIBS IPP::msdk IPP::j IPP::vc IPP::cc IPP::cv IPP::i IPP::s IPP::core)
  endif()


  if ( NOT ENABLE_HEVC )
    message( STATUS "Building HEVC with only hardware libraries." )
  else( )
    message( STATUS "Building HEVC with software libraries: SW Decoder & Encoder, GACC." )

    # HEVC Common Part

    if(CMAKE_SYSTEM_NAME MATCHES Windows)

      if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
        set( plugin_name mfxplugin_hw64 )
      else()
        set( plugin_name mfxplugin_hw32 )
      endif()

      add_library(${plugin_name} SHARED)

      target_include_directories(${plugin_name}
        PRIVATE
          include
          ${MSDK_STUDIO_ROOT}/hevce_hw/h265/include
          ${MSDK_STUDIO_ROOT}/vp9e_hw/include
          ${MSDK_STUDIO_ROOT}/camera_pipe/include
          ${MSDK_STUDIO_ROOT}/camera_pipe/genx/src
          ${MSDK_STUDIO_ROOT}/camera_pipe/genx/include
          ${MSDK_LIB_ROOT}/fei/include
          ${MSDK_STUDIO_ROOT}/shared/include/
          ${MSDK_LIB_ROOT}/shared/include/
          ${MSDK_LIB_ROOT}/scheduler/windows/include
      )
      target_link_libraries(${plugin_name}
        PRIVATE
          vm
          vm_plus
          genx
          umc
          media_buffers
          color_space_converter
          vc1_dec
          umc_codecs_merged
          vpp_hw
          mfx_trace
          mfx_static_lib
          mfx_shared_lib
          umc_io
          umc_va_hw
          hevc_pp_atom
          hevc_pp_avx2
          hevc_pp_dispatcher
          hevc_pp_px
          hevc_pp_sse4
          hevc_pp_ssse3
          genx_h265_encode_embeded
          h265_fei
          h265_enc_hw
          ${IPP_LIBS}
      )

      target_sources(${plugin_name}
        PRIVATE
          include/mfx_hevc_dec_plugin.h
          include/mfx_hevc_enc_plugin.h
          include/mfx_plugin_module.h
          include/mfx_stub_dec_plugin.h
          include/mfx_vp8_dec_plugin.h
          include/mfx_vp9_dec_plugin.h
          resource.h
          src/mfx_hevc_dec_plugin.cpp
          src/mfx_hevc_enc_plugin.cpp
          src/mfx_stub_dec_plugin.cpp
          src/mfx_unified_dec_plugin.cpp
          src/mfx_vp8_dec_plugin.cpp
          src/mfx_vp9_dec_plugin.cpp
          mfxplugin_hw.rc
          ${MSDK_STUDIO_ROOT}/camera_pipe/src/mfx_camera_plugin.cpp
          ${MSDK_STUDIO_ROOT}/camera_pipe/src/mfx_camera_plugin_cm.cpp
          ${MSDK_STUDIO_ROOT}/camera_pipe/src/mfx_camera_plugin_cpu.cpp
          ${MSDK_STUDIO_ROOT}/camera_pipe/src/mfx_camera_plugin_dx11.cpp
          ${MSDK_STUDIO_ROOT}/camera_pipe/src/mfx_camera_plugin_dx9.cpp
          ${MSDK_STUDIO_ROOT}/camera_pipe/src/mfx_camera_plugin_utils.cpp
          ${MSDK_STUDIO_ROOT}/camera_pipe/genx/src/genx_bdw_camerapipe_isa.cpp
          ${MSDK_STUDIO_ROOT}/camera_pipe/genx/src/genx_cnl_camerapipe_isa.cpp
          ${MSDK_STUDIO_ROOT}/camera_pipe/genx/src/genx_skl_camerapipe_isa.cpp
          ${MSDK_STUDIO_ROOT}/shared/src/auxiliary_device.cpp
          ${MSDK_STUDIO_ROOT}/shared/src/cm_mem_copy.cpp
          ${MSDK_STUDIO_ROOT}/shared/src/d3d11_decode_accelerator.cpp
          ${MSDK_STUDIO_ROOT}/shared/src/d3d11_video_processor.cpp
          ${MSDK_STUDIO_ROOT}/shared/src/fast_compositing_ddi.cpp
          ${MSDK_STUDIO_ROOT}/shared/src/fast_copy_c_impl.cpp
          ${MSDK_STUDIO_ROOT}/shared/src/fast_copy_sse4_impl.cpp
          ${MSDK_STUDIO_ROOT}/shared/src/fast_copy.cpp
          ${MSDK_STUDIO_ROOT}/shared/src/fast_copy_multithreading.cpp
          ${MSDK_STUDIO_ROOT}/shared/src/libmfx_allocator.cpp
          ${MSDK_STUDIO_ROOT}/shared/src/libmfx_allocator_d3d11.cpp
          ${MSDK_STUDIO_ROOT}/shared/src/libmfx_allocator_d3d9.cpp
          ${MSDK_STUDIO_ROOT}/shared/src/libmfx_core.cpp
          ${MSDK_STUDIO_ROOT}/shared/src/libmfx_core_d3d11.cpp
          ${MSDK_STUDIO_ROOT}/shared/src/libmfx_core_d3d9.cpp
          ${MSDK_STUDIO_ROOT}/shared/src/libmfx_core_factory.cpp
          ${MSDK_STUDIO_ROOT}/shared/src/libmfx_core_hw.cpp
          ${MSDK_STUDIO_ROOT}/shared/src/mfx_dxva2_device.cpp
          ${MSDK_STUDIO_ROOT}/shared/src/mfx_mfe_adapter_dxva.cpp
          ${MSDK_LIB_ROOT}/shared/src/libmfxsw.cpp
          ${MSDK_LIB_ROOT}/shared/src/libmfxsw_async.cpp
          ${MSDK_LIB_ROOT}/shared/src/libmfxsw_plugin.cpp
          ${MSDK_LIB_ROOT}/shared/src/libmfxsw_query.cpp
          ${MSDK_LIB_ROOT}/shared/src/libmfxsw_session.cpp
          ${MSDK_LIB_ROOT}/shared/src/libmfxsw_vpp.cpp
          ${MSDK_LIB_ROOT}/shared/src/mfx_brc_common.cpp
          ${MSDK_LIB_ROOT}/shared/src/mfx_check_hardware_support.cpp
          ${MSDK_LIB_ROOT}/shared/src/mfx_common_decode_int.cpp
          ${MSDK_LIB_ROOT}/shared/src/mfx_common_int.cpp
          ${MSDK_LIB_ROOT}/shared/src/mfx_critical_error_handler.cpp
          ${MSDK_LIB_ROOT}/shared/src/mfx_enc_common.cpp
          ${MSDK_LIB_ROOT}/shared/src/mfx_h264_enc_common.cpp
          ${MSDK_LIB_ROOT}/shared/src/mfx_log.cpp
          ${MSDK_LIB_ROOT}/shared/src/mfx_session.cpp
          ${MSDK_STUDIO_ROOT}/shared/src/mfx_timing.cpp
          ${MSDK_STUDIO_ROOT}/shared/src/mfx_umc_alloc_wrapper.cpp
          ${MSDK_LIB_ROOT}/shared/src/mfx_user_plugin.cpp
          ${MSDK_LIB_ROOT}/scheduler/windows/src/mfx_scheduler_core.cpp
          ${MSDK_LIB_ROOT}/scheduler/windows/src/mfx_scheduler_core_ischeduler.cpp
          ${MSDK_LIB_ROOT}/scheduler/windows/src/mfx_scheduler_core_iunknown.cpp
          ${MSDK_LIB_ROOT}/scheduler/windows/src/mfx_scheduler_core_task.cpp
          ${MSDK_LIB_ROOT}/scheduler/windows/src/mfx_scheduler_core_task_management.cpp
          ${MSDK_LIB_ROOT}/scheduler/windows/src/mfx_scheduler_core_thread.cpp
          ${MSDK_LIB_ROOT}/shared/src/mfx_vpx_dec_common.cpp
          ${MSDK_LIB_ROOT}/shared/src/mfx_win_event_cache.cpp

          ${MSDK_STUDIO_ROOT}/camera_pipe/include/mfx_camera_plugin.h
          ${MSDK_STUDIO_ROOT}/camera_pipe/include/mfx_camera_plugin_cm.h
          ${MSDK_STUDIO_ROOT}/camera_pipe/include/mfx_camera_plugin_cpu.h
          ${MSDK_STUDIO_ROOT}/camera_pipe/include/mfx_camera_plugin_dx11.h
          ${MSDK_STUDIO_ROOT}/camera_pipe/include/mfx_camera_plugin_dx9.h
          ${MSDK_STUDIO_ROOT}/camera_pipe/include/mfx_camera_plugin_utils.h
          ${MSDK_STUDIO_ROOT}/shared/include/auxiliary_device.h
          ${MSDK_STUDIO_ROOT}/shared/include/cm_mem_copy.h
          ${MSDK_STUDIO_ROOT}/shared/include/d3d11_decode_accelerator.h
          ${MSDK_STUDIO_ROOT}/shared/include/fast_copy_c_impl.h
          ${MSDK_STUDIO_ROOT}/shared/include/fast_copy_sse4_impl.h
          ${MSDK_STUDIO_ROOT}/shared/include/fast_copy.h
          ${MSDK_STUDIO_ROOT}/shared/include/libmfx_allocator.h
          ${MSDK_STUDIO_ROOT}/shared/include/libmfx_allocator_d3d11.h
          ${MSDK_STUDIO_ROOT}/shared/include/libmfx_allocator_d3d9.h
          ${MSDK_STUDIO_ROOT}/shared/include/libmfx_core.h
          ${MSDK_STUDIO_ROOT}/shared/include/libmfx_core_d3d11.h
          ${MSDK_STUDIO_ROOT}/shared/include/libmfx_core_d3d9.h
          ${MSDK_STUDIO_ROOT}/shared/include/libmfx_core_factory.h
          ${MSDK_STUDIO_ROOT}/shared/include/libmfx_core_hw.h
          ${MSDK_STUDIO_ROOT}/shared/include/libmfx_core_operation.h
          ${MSDK_STUDIO_ROOT}/shared/include/mfx_common.h
          ${MSDK_STUDIO_ROOT}/shared/include/mfx_dxva2_device.h
          ${MSDK_STUDIO_ROOT}/shared/include/mfx_vpp_interface.h
          ${MSDK_STUDIO_ROOT}/vp9e_hw/include/mfx_vp9_encode_plugin_hw.h
          ${MSDK_LIB_ROOT}/scheduler/windows/include/mfx_dependency_item.h
          ${MSDK_LIB_ROOT}/shared/include/encoder_ddi.hpp
          ${MSDK_LIB_ROOT}/shared/include/encoding_ddi.h
          ${MSDK_LIB_ROOT}/shared/include/mfx_check_hardware_support.h
          ${MSDK_LIB_ROOT}/shared/include/mfx_common_decode_int.h
          ${MSDK_LIB_ROOT}/shared/include/mfx_common_int.h
          ${MSDK_LIB_ROOT}/shared/include/mfx_critical_error_handler.h
          ${MSDK_LIB_ROOT}/shared/include/mfx_frames.h
          ${MSDK_LIB_ROOT}/shared/include/mfx_interface.h
          ${MSDK_LIB_ROOT}/shared/include/mfx_interface_scheduler.h
          ${MSDK_LIB_ROOT}/shared/include/mfx_log.h
          ${MSDK_LIB_ROOT}/shared/include/mfx_platform_headers.h
          ${MSDK_LIB_ROOT}/shared/include/mfx_session.h
          ${MSDK_LIB_ROOT}/shared/include/mfx_task.h
          ${MSDK_LIB_ROOT}/shared/include/mfx_task_threading_policy.h
          ${MSDK_LIB_ROOT}/shared/include/mfx_thread_task.h
          ${MSDK_LIB_ROOT}/shared/include/mfx_tools.h
          ${MSDK_LIB_ROOT}/shared/include/mfx_user_plugin.h
          ${MSDK_LIB_ROOT}/scheduler/windows/include/mfx_scheduler_core.h
          ${MSDK_LIB_ROOT}/scheduler/windows/include/mfx_scheduler_core_handle.h
          ${MSDK_LIB_ROOT}/scheduler/windows/include/mfx_scheduler_core_task.h
          ${MSDK_LIB_ROOT}/scheduler/windows/include/mfx_scheduler_core_thread.h
          ${MSDK_LIB_ROOT}/shared/include/mfx_vpx_dec_common.h
          ${MSDK_LIB_ROOT}/shared/include/mfx_win_event_cache.h

      )

      target_compile_definitions(${plugin_name}
        PRIVATE
          LIBMFXSW_EXPORTS
          MFX_VA
          MFX_D3D11_ENABLED
          AS_CAMERA_PLUGIN
          AS_HEVCD_PLUGIN
          AS_HEVCE_DP_PLUGIN
          AS_HEVCE_PLUGIN
          AS_VP8D_PLUGIN
          AS_VP9D_PLUGIN
          AS_VP9E_PLUGIN
          UNIFIED_PLUGIN
      )

      set_property(TARGET ${plugin_name} PROPERTY FOLDER "plugins")

    endif()

    set (HEVC_PRODUCT_NAME "Intel(R) Media SDK")
    set (sw_HEVC_DECODER_DESCRIPTION "Intel(R) Media SDK - HEVC Software Decode Plug-in")
    set (sw_HEVC_ENCODER_DESCRIPTION "Intel(R) Media SDK - HEVC Software Encode Plug-in")
    set (hw_HEVC_ENCODER_DESCRIPTION "Intel(R) Media SDK - HEVC GPU Accelerated Encode Plug-in")

    set(hevc_version_defs "")
    set_file_and_product_version( ${MEDIA_VERSION_STR} hevc_version_defs )

    # HEVC Decoder
    if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
      set( plugin_name mfx_hevcd_sw64 )
      set( eval_plugin_name mfx_eval_hevcd_sw64 )
    else()
      set( plugin_name mfx_hevcd_sw32 )
      set( eval_plugin_name mfx_eval_hevcd_sw32 )
    endif()

    set(hevcd_sw_common_sources
      ${MSDK_STUDIO_ROOT}/shared/src/fast_copy_c_impl.cpp
      ${MSDK_STUDIO_ROOT}/shared/src/fast_copy_sse4_impl.cpp
      ${MSDK_STUDIO_ROOT}/shared/src/fast_copy.cpp
      ${MSDK_STUDIO_ROOT}/shared/src/fast_copy_multithreading.cpp
      ${MSDK_STUDIO_ROOT}/shared/src/libmfx_allocator.cpp
      ${MSDK_STUDIO_ROOT}/shared/src/libmfx_core.cpp
      ${MSDK_STUDIO_ROOT}/shared/src/libmfx_core_factory.cpp
      ${MSDK_LIB_ROOT}/decode/h265/src/mfx_h265_dec_decode.cpp
      ${MSDK_LIB_ROOT}/shared/src/libmfxsw.cpp
      ${MSDK_LIB_ROOT}/shared/src/libmfxsw_async.cpp
      ${MSDK_LIB_ROOT}/shared/src/libmfxsw_decode.cpp
      ${MSDK_LIB_ROOT}/shared/src/libmfxsw_plugin.cpp
      ${MSDK_LIB_ROOT}/shared/src/libmfxsw_query.cpp
      ${MSDK_LIB_ROOT}/shared/src/libmfxsw_session.cpp
      ${MSDK_LIB_ROOT}/shared/src/mfx_common_decode_int.cpp
      ${MSDK_LIB_ROOT}/shared/src/mfx_common_int.cpp
      ${MSDK_LIB_ROOT}/shared/src/mfx_log.cpp
      ${MSDK_LIB_ROOT}/shared/src/mfx_session.cpp
      ${MSDK_STUDIO_ROOT}/shared/src/mfx_timing.cpp
      ${MSDK_STUDIO_ROOT}/shared/src/mfx_umc_alloc_wrapper.cpp
      ${MSDK_LIB_ROOT}/shared/src/mfx_user_plugin.cpp

      ${MSDK_LIB_ROOT}/scheduler/$<IF:$<PLATFORM_ID:Linux>,linux,windows>/src/mfx_scheduler_core.cpp
      ${MSDK_LIB_ROOT}/scheduler/$<IF:$<PLATFORM_ID:Linux>,linux,windows>/src/mfx_scheduler_core_ischeduler.cpp
      ${MSDK_LIB_ROOT}/scheduler/$<IF:$<PLATFORM_ID:Linux>,linux,windows>/src/mfx_scheduler_core_iunknown.cpp
      ${MSDK_LIB_ROOT}/scheduler/$<IF:$<PLATFORM_ID:Linux>,linux,windows>/src/mfx_scheduler_core_task.cpp
      ${MSDK_LIB_ROOT}/scheduler/$<IF:$<PLATFORM_ID:Linux>,linux,windows>/src/mfx_scheduler_core_task_management.cpp
      ${MSDK_LIB_ROOT}/scheduler/$<IF:$<PLATFORM_ID:Linux>,linux,windows>/src/mfx_scheduler_core_thread.cpp
    )
    add_library(${plugin_name} SHARED)

    target_include_directories(${plugin_name}
      PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${MSDK_LIB_ROOT}/scheduler/$<IF:$<PLATFORM_ID:Linux>,linux,windows>/include
        ${MSDK_LIB_ROOT}/fei/include
    )
    target_sources(${plugin_name}
      PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_hevc_dec_plugin.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_stub_dec_plugin.cpp
        $<$<PLATFORM_ID:Windows>:
          ${MSDK_LIB_ROOT}/plugin/libmfxsw_plugin_hevcd.rc
        >
        ${hevcd_sw_common_sources}
    )

    set (description_name ${sw_HEVC_DECODER_DESCRIPTION})

    target_link_libraries(${plugin_name}
      PUBLIC
        color_space_converter
        umc_h265_sw
        hevc_pp_dispatcher
        hevc_pp_atom
        hevc_pp_avx2
        hevc_pp_px
        hevc_pp_sse4
        hevc_pp_ssse3
        media_buffers
        umc_io
        umc
        vm
        vm_plus
        mfx_trace
        mfx_shared_lib
        ${ITT_LIBRARIES}
        ${IPP_LIBS}
        ${CMAKE_THREAD_LIBS_INIT}
        ${CMAKE_DL_LIBS}
    )

    target_link_options(${plugin_name}
      PRIVATE
        $<$<PLATFORM_ID:Linux>:--version-script=${CMAKE_CURRENT_SOURCE_DIR}/libmfxsw_plugin.map>
    )

    set( USE_STRICT_NAME TRUE )

    target_compile_definitions(${plugin_name}
      PRIVATE
        AS_HEVCD_PLUGIN
        LIBMFXSW_EXPORTS
        MFX_PLUGIN_PRODUCT_NAME=\"${HEVC_PRODUCT_NAME}\"
        MFX_FILE_DESCRIPTION=\"${description_name}\"
        ${hevc_version_defs}
    )

    gen_plugins_cfg("HEVC_Decoder_SW" ${HEVC_DECODER_SW_GUID} ${plugin_name} "01" "HEVC" "not_eval")

    set_property(TARGET ${plugin_name} PROPERTY FOLDER "plugins")

    if(CMAKE_SYSTEM_NAME MATCHES Linux)
      add_library(${eval_plugin_name} SHARED)

      target_sources(${eval_plugin_name}
        PRIVATE
          ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_hevc_dec_plugin.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_stub_dec_plugin.cpp
          $<$<PLATFORM_ID:Windows>:
            ${MSDK_LIB_ROOT}/plugin/libmfxsw_plugin_hevcd.rc
          >
          ${hevcd_sw_common_sources}
      )

      target_link_options(${eval_plugin_name}
        PRIVATE
          $<$<PLATFORM_ID:Linux>:--version-script=${CMAKE_CURRENT_SOURCE_DIR}/libmfxsw_plugin.map>
      )

      target_compile_definitions(${eval_plugin_name}
        PRIVATE
          AS_HEVCD_PLUGIN
          HEVCD_EVALUATION
          MFX_PLUGIN_PRODUCT_NAME=\"${HEVC_PRODUCT_NAME}\"
          MFX_FILE_DESCRIPTION=\"${description_name}\"
          ${hevc_version_defs}
      )

      target_link_libraries(${eval_plugin_name} PUBLIC ${plugin_name})
      gen_plugins_cfg("HEVC_Decoder_SW" ${HEVC_DECODER_SW_GUID} ${eval_plugin_name} "01" "HEVC" "eval")

      set_property(TARGET ${eval_plugin_name} PROPERTY FOLDER "plugins")
    endif()

    if( Windows )
      install( TARGETS ${plugin_name} RUNTIME DESTINATION ${MFX_PLUGINS_DIR} )
    else()
      install( TARGETS ${plugin_name} ${eval_plugin_name} LIBRARY DESTINATION ${MFX_PLUGINS_DIR} )
    endif()

    # HEVC Encoder (SW & GACC)
    set( hevce_variants "" )
    if (ENABLE_HEVC)
      set( hevce_variants
          sw
          hw )
    endif()

    foreach( variant ${hevce_variants} )
      if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
        if( variant MATCHES hw )
        set( plugin_name mfx_hevce_gacc64 )
          set( eval_plugin_name mfx_eval_hevce_gacc64 )
        else()
          set( plugin_name mfx_hevce_${variant}64 )
          set( eval_plugin_name mfx_eval_hevce_${variant}64 )
        endif()
      else()
        if( variant MATCHES hw )
          set( plugin_name mfx_hevce_gacc32 )
          set( eval_plugin_name mfx_eval_hevce_gacc32 )
        else()
          set( plugin_name mfx_hevce_${variant}32 )
          set( eval_plugin_name mfx_eval_hevce_${variant}32 )
        endif()
      endif()

      set( description_name ${${variant}_HEVC_ENCODER_DESCRIPTION} )

      add_library(${plugin_name} SHARED)

      target_include_directories(${plugin_name}
        PUBLIC
          ${CMAKE_CURRENT_SOURCE_DIR}/include
      )

      target_sources(${plugin_name}
        PRIVATE
          ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_hevc_enc_plugin.cpp
          ${CMAKE_CURRENT_SOURCE_DIR}/src/watermark.cpp
          ${MSDK_LIB_ROOT}/shared/src/mfx_enc_common.cpp
          ${MSDK_LIB_ROOT}/shared/src/mfx_common_int.cpp
          $<$<PLATFORM_ID:Windows>:
            ${MSDK_LIB_ROOT}/plugin/libmfxsw_plugin_hevce.rc
          >
      )

      target_link_libraries(${plugin_name}
        PUBLIC
          hevc_pp_dispatcher
          hevc_pp_atom
          hevc_pp_avx2
          hevc_pp_px
          hevc_pp_sse4
          hevc_pp_ssse3
          h265_enc_hw
          h265_fei
          genx_h265_encode_embeded
          media_buffers
          genx
          umc_va_hw
          umc_io
          umc
          vm
          vm_plus
          mfx_shared_lib
          mfx_trace
          ${ITT_LIBRARIES}
          ${IPP_LIBS}
          ${CMAKE_THREAD_LIBS_INIT}
          ${CMAKE_DL_LIBS}
      )

      if( variant MATCHES hw)
        target_link_libraries(${plugin_name}
          PUBLIC
            umc_va_hw
        )
      endif()

      set( USE_STRICT_NAME TRUE )

      target_link_options(${plugin_name}
        PRIVATE
          $<$<PLATFORM_ID:Linux>:--version-script=${CMAKE_CURRENT_SOURCE_DIR}/libmfxsw_plugin.map>
      )

      target_compile_definitions(${plugin_name}
        PRIVATE
          AS_HEVCE_PLUGIN
          LIBMFXSW_EXPORTS
          MFX_PLUGIN_PRODUCT_NAME=\"${HEVC_PRODUCT_NAME}\"
          MFX_FILE_DESCRIPTION=\"${description_name}\"
          ${hevc_version_defs}
      )

      if(${variant} MATCHES hw)
        gen_plugins_cfg("HEVC_Encoder_GACC" ${HEVC_ENCODER_GACC_GUID} ${plugin_name} "02" "HEVC" "not_eval")
      else()
        gen_plugins_cfg("HEVC_Encoder_SW" ${HEVC_ENCODER_SW_GUID} ${plugin_name} "02" "HEVC" "not_eval")
      endif()

      set_property(TARGET ${plugin_name} PROPERTY FOLDER "plugins")

      if(CMAKE_SYSTEM_NAME MATCHES Linux)
        add_library( ${eval_plugin_name} SHARED)

        target_link_libraries(${eval_plugin_name} PUBLIC ${plugin_name})

        target_sources(${eval_plugin_name}
          PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_hevc_enc_plugin.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/watermark.cpp
            ${MSDK_LIB_ROOT}/shared/src/mfx_enc_common.cpp
            ${MSDK_LIB_ROOT}/shared/src/mfx_common_int.cpp
            $<$<PLATFORM_ID:Windows>:
              ${MSDK_LIB_ROOT}/plugin/libmfxsw_plugin_hevce.rc
            >
        )

        target_compile_definitions(${eval_plugin_name}
          PRIVATE
            AS_HEVCE_PLUGIN
            HEVCE_EVALUATION
            MFX_PLUGIN_PRODUCT_NAME=\"${HEVC_PRODUCT_NAME}\"
            MFX_FILE_DESCRIPTION=\"${description_name}\"
            ${hevc_version_defs}
        )

        target_link_options(${eval_plugin_name}
          PRIVATE
            $<$<PLATFORM_ID:Linux>:--version-script=${CMAKE_CURRENT_SOURCE_DIR}/libmfxsw_plugin.map>
        )

        if(${variant} MATCHES hw)
          gen_plugins_cfg("HEVC_Encoder_GACC" ${HEVC_ENCODER_GACC_GUID} ${eval_plugin_name} "02" "HEVC" "eval")
        else()
          gen_plugins_cfg("HEVC_Encoder_SW" ${HEVC_ENCODER_SW_GUID} ${eval_plugin_name} "02" "HEVC" "eval")
        endif()
        set_property(TARGET ${eval_plugin_name} PROPERTY FOLDER "plugins")
      endif()

      if( Windows )
        install( TARGETS ${plugin_name} RUNTIME DESTINATION ${MFX_PLUGINS_DIR} )
      else()
        install( TARGETS ${plugin_name} ${eval_plugin_name} LIBRARY DESTINATION ${MFX_PLUGINS_DIR} )
      endif()

    endforeach()
    set( defs "" )
  endif() # if (ENABLE_HEVC)

  set( av1e_variants "" )

  # AV1 Encoder (GACC)
  if ( ENABLE_AV1 )
    message( STATUS "Building AV1 with software libraries: GACC." )

    set (AV1_PRODUCT_NAME "Intel(R) Media SDK")
    set (sw_AV1_DECODER_DESCRIPTION "Intel(R) Media SDK - AV1 Software Decode Plug-in")
    set (hw_AV1_DECODER_DESCRIPTION "Intel(R) Media SDK - AV1 Hardware Decode Plug-in")
    set (sw_AV1_ENCODER_DESCRIPTION "Intel(R) Media SDK - AV1 Software Encode Plug-in")
    set (hw_AV1_ENCODER_DESCRIPTION "Intel(R) Media SDK - AV1 GPU Accelerated Encode Plug-in")

      if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
        if(NOT CMAKE_SYSTEM_NAME MATCHES Windows)
          set( plugin_name mfx_av1e_gacc64 )
          set( eval_plugin_name mfx_eval_av1e_gacc64 )
        else()
          set( plugin_name mfxplugin64_av1e_gacc )
          set( eval_plugin_name mfxplugin64_eval_av1e_gacc )
        endif()
      else()
        if(NOT CMAKE_SYSTEM_NAME MATCHES Windows)
          set( plugin_name mfx_av1e_gacc32 )
          set( eval_plugin_name mfx_eval_av1e_gacc32 )
        else()
          set( plugin_name mfxplugin32_av1e_gacc )
          set( eval_plugin_name mfxplugin32_eval_av1e_gacc )
        endif()
      endif()

    message("plugin name ${plugin_name}")
    set (sources "")
    list( APPEND sources
      ${MSDK_LIB_ROOT}/plugin/src/mfx_av1_enc_plugin.cpp
      ${MSDK_LIB_ROOT}/plugin/src/watermark.cpp
      $<$<PLATFORM_ID:Windows>:
        ${MSDK_LIB_ROOT}/plugin/libmfxhw_plugin_av1e.rc
      >
    )

    add_library(${plugin_name} SHARED ${sources})

    target_include_directories(${plugin_name}
      PUBLIC
        ${MSDK_LIB_ROOT}/plugin/include
        ${MSDK_LIB_ROOT}/fs_det/include
    )


    set( description_name ${${variant}_AV1_ENCODER_DESCRIPTION} )

    target_link_libraries(${plugin_name} PUBLIC
      av1_fei
      genx_av1_encode_embeded
      av1_enc_hw
      av1_pp_avx2
      av1_pp_px
      av1_pp_ssse3
      media_buffers
      umc_io
      umc
      vm
      vm_plus
      mfx_trace
      mfx_shared_lib
      umc_va_hw
      ${ITT_LIBRARIES}
      ${IPP_LIBS}
      ${CMAKE_THREAD_LIBS_INIT}
      ${CMAKE_DL_LIBS}
    )

    target_compile_definitions(${plugin_name}
      PRIVATE
        AS_AV1E_PLUGIN
        MFX_VA
        LIBMFXSW_EXPORTS
        MFX_PLUGIN_PRODUCT_NAME=\"${AV1_PRODUCT_NAME}\"
        MFX_FILE_DESCRIPTION=\"${description_name}\"
        ${av1_version_defs}
    )

    if(${variant} MATCHES hw)
      gen_plugins_cfg("AV1_Encoder_GACC" ${AV1_ENCODER_GACC_GUID} ${plugin_name} "02" "AV1" "not_eval")
    else()
      gen_plugins_cfg("AV1_Encoder_SW" ${AV1_ENCODER_SW_GUID} ${plugin_name} "02" "AV1" "not_eval")
    endif()

    set_property(TARGET ${plugin_name} PROPERTY FOLDER "plugins")

    if(CMAKE_SYSTEM_NAME MATCHES Linux)
      add_library(${eval_plugin_name} SHARED ${sources})

      target_link_libraries(${eval_plugin_name} PUBLIC ${plugin_name})

      target_compile_definitions(${eval_plugin_name}
        PRIVATE
          AS_AV1E_PLUGIN
          MFX_VA
          AV1E_EVALUATION
          MFX_PLUGIN_PRODUCT_NAME=\"${AV1_PRODUCT_NAME}\"
          MFX_FILE_DESCRIPTION=\"${description_name}\"
          ${av1_version_defs}
      )

      if(${variant} MATCHES hw)
        gen_plugins_cfg("AV1_Encoder_GACC" ${AV1_ENCODER_GACC_GUID} ${eval_plugin_name} "02" "AV1" "eval")
      else()
        gen_plugins_cfg("AV1_Encoder_SW" ${AV1_ENCODER_SW_GUID} ${eval_plugin_name} "02" "AV1" "eval")
      endif()
      set_property(TARGET ${eval_plugin_name} PROPERTY FOLDER "plugins")
    endif()


    if( Windows )
      install( TARGETS ${plugin_name} RUNTIME DESTINATION ${MFX_PLUGINS_DIR} )
    else()
      install( TARGETS ${plugin_name} ${eval_plugin_name} LIBRARY DESTINATION ${MFX_PLUGINS_DIR} )
    endif()

  else( )
    message( STATUS "Building AV1 disabled." )
  endif( )

  set( defs "" )

  # H265FEI
  if( ENABLE_HEVC_FEI )
    set (H265FEI_PRODUCT_NAME "Intel(R) Media SDK")
    set (hw_H265FEI_ENCODER_DESCRIPTION "Intel(R) Media SDK - HEVC GPU Assist APIs Plug-in")

    set(h265fei_version_defs "")
    set_file_and_product_version( ${MEDIA_VERSION_STR} h265fei_version_defs )


    if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
      set( plugin_name mfx_h265fei_hw64 )
    else()
      set( plugin_name mfx_h265fei_hw32 )
    endif()
    set (description_name ${hw_H265FEI_ENCODER_DESCRIPTION})

    add_library(${plugin_name} SHARED)

    target_include_directories(${plugin_name}
      PUBLIC
        ${MSDK_LIB_ROOT}/plugin/include
        ${MSDK_LIB_ROOT}/fei/h265/include
        ${include_dirs}
        ${MSDK_LIB_ROOT}/scheduler/$<IF:$<PLATFORM_ID:Linux>,linux,windows>/include
    )

    target_sources(${plugin_name}
      PRIVATE
        ${MSDK_LIB_ROOT}/plugin/src/mfx_h265fei_plugin.cpp
        # list separately instead of just including h265_fei lib because different
        # define needed for plugin
        ${MSDK_LIB_ROOT}/fei/h265/src/mfx_h265_enc_cm_fei.cpp
        ${MSDK_LIB_ROOT}/fei/h265/src/mfx_h265_enc_cm_plugin.cpp
        ${MSDK_LIB_ROOT}/fei/h265/src/mfx_h265_enc_cm_utils.cpp
        $<$<PLATFORM_ID:Windows>:
          ${MSDK_LIB_ROOT}/plugin/libmfxhw_plugin_h265fei.rc
        >

        ${MSDK_LIB_ROOT}/shared/src/mfx_check_hardware_support.cpp
        ${MSDK_LIB_ROOT}/shared/src/mfx_common_int.cpp
        ${MSDK_LIB_ROOT}/shared/src/mfx_common_decode_int.cpp
        ${MSDK_LIB_ROOT}/shared/src/libmfxsw.cpp
        ${MSDK_LIB_ROOT}/shared/src/libmfxsw_async.cpp
        ${MSDK_LIB_ROOT}/shared/src/libmfxsw_query.cpp
        ${MSDK_LIB_ROOT}/shared/src/libmfxsw_enc.cpp
        ${MSDK_LIB_ROOT}/shared/src/libmfxsw_session.cpp
        ${MSDK_LIB_ROOT}/shared/src/mfx_session.cpp
        ${MSDK_LIB_ROOT}/shared/src/mfx_user_plugin.cpp
        ${MSDK_LIB_ROOT}/shared/src/mfx_enc_common.cpp
        ${MSDK_LIB_ROOT}/shared/src/mfx_critical_error_handler.cpp
        ${MSDK_LIB_ROOT}/shared/src/mfx_win_event_cache.cpp
        ${MSDK_LIB_ROOT}/vpp/src/mfx_vpp_factory.cpp

        ${MSDK_STUDIO_ROOT}/shared/src/cm_mem_copy.cpp
        ${MSDK_STUDIO_ROOT}/shared/src/fast_compositing_ddi.cpp
        ${MSDK_STUDIO_ROOT}/shared/src/libmfx_allocator.cpp
        ${MSDK_STUDIO_ROOT}/shared/src/libmfx_allocator_vaapi.cpp
        ${MSDK_STUDIO_ROOT}/shared/src/libmfx_allocator_d3d9.cpp
        ${MSDK_STUDIO_ROOT}/shared/src/libmfx_allocator_d3d11.cpp
        ${MSDK_STUDIO_ROOT}/shared/src/libmfx_core.cpp
        ${MSDK_STUDIO_ROOT}/shared/src/libmfx_core_factory.cpp
        ${MSDK_STUDIO_ROOT}/shared/src/libmfx_core_hw.cpp
        ${MSDK_STUDIO_ROOT}/shared/src/libmfx_core_vaapi.cpp
        ${MSDK_STUDIO_ROOT}/shared/src/libmfx_core_d3d9.cpp
        ${MSDK_STUDIO_ROOT}/shared/src/libmfx_core_d3d11.cpp
        ${MSDK_STUDIO_ROOT}/shared/src/mfx_dxva2_device.cpp
        ${MSDK_STUDIO_ROOT}/shared/src/d3d11_decode_accelerator.cpp
        ${MSDK_STUDIO_ROOT}/shared/src/d3d11_video_processor.cpp
        ${MSDK_STUDIO_ROOT}/shared/src/auxiliary_device.cpp

        ${MSDK_LIB_ROOT}/scheduler/$<IF:$<PLATFORM_ID:Linux>,linux,windows>/src/mfx_scheduler_core.cpp
        ${MSDK_LIB_ROOT}/scheduler/$<IF:$<PLATFORM_ID:Linux>,linux,windows>/src/mfx_scheduler_core_iunknown.cpp
        ${MSDK_LIB_ROOT}/scheduler/$<IF:$<PLATFORM_ID:Linux>,linux,windows>/src/mfx_scheduler_core_ischeduler.cpp
        ${MSDK_LIB_ROOT}/scheduler/$<IF:$<PLATFORM_ID:Linux>,linux,windows>/src/mfx_scheduler_core_task.cpp
        ${MSDK_LIB_ROOT}/scheduler/$<IF:$<PLATFORM_ID:Linux>,linux,windows>/src/mfx_scheduler_core_task_management.cpp
        ${MSDK_LIB_ROOT}/scheduler/$<IF:$<PLATFORM_ID:Linux>,linux,windows>/src/mfx_scheduler_core_thread.cpp

        ${MSDK_LIB_ROOT}/cmrt_cross_platform/src/cmrt_cross_platform.cpp
    )

    target_link_libraries(${plugin_name}
      PUBLIC
        genx_h265_encode_embeded
        genx
        umc_va_hw
        media_buffers
        umc_io
        umc
        vm
        h265_fei
        vm_plus
        mfx_trace
        mfx_shared_lib
        ${ITT_LIBRARIES}
        ${IPP_LIBS}
        ${CMAKE_THREAD_LIBS_INIT}
        ${CMAKE_DL_LIBS}
    )


    set( USE_STRICT_NAME TRUE )

    target_link_options(${plugin_name}
      PRIVATE
        $<$<PLATFORM_ID:Linux>:--version-script=${CMAKE_CURRENT_SOURCE_DIR}/libmfxsw_plugin.map>
    )

    target_compile_definitions(${plugin_name}
      PRIVATE
        AS_H265FEI_PLUGIN
        MFX_VA
        $<$<PLATFORM_ID:Windows>:MFX_VA_WIN>
        LIBMFXHW_EXPORTS
        MFX_PLUGIN_PRODUCT_NAME=\"${H265FEI_PRODUCT_NAME}\"
        MFX_FILE_DESCRIPTION=\"${description_name}\"
        ${h265fei_version_defs}
    )

    gen_plugins_cfg("H265FEI_Encoder" ${H265FEI_ENCODER_GUID} ${plugin_name} "04" "HEVC")

    if( Windows )
      install( TARGETS ${plugin_name} RUNTIME DESTINATION ${MFX_PLUGINS_DIR} )
    else()
      install( TARGETS ${plugin_name} LIBRARY DESTINATION ${MFX_PLUGINS_DIR} )
    endif()

    set_property(TARGET ${plugin_name} PROPERTY FOLDER "plugins")

  endif()
endif()
