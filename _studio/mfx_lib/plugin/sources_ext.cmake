if(NOT OPEN_SOURCE)
  set(HEVC_DECODER_SW_GUID   "15dd936825ad475ea34e35f3f54217a6")
  #set(VP8_DECODER_SW_GUID    "bca276ec7a854a598c30e0dc5748f1f6")
  #set(VP9_DECODER_SW_GUID    "a9a276ec7a854a598c30e0dc5748f1f6")

  set(HEVC_ENCODER_GACC_GUID "e5400a06c74d41f5b12d430bbaa23d0b")
  set(HEVC_ENCODER_SW_GUID   "2fca99749fdb49aeb121a5b63ef568f7")
  set(H265FEI_ENCODER_GUID   "87e0e80207375240852515cf4a5edde6")
  set(AV1_ENCODER_GACC_GUID  "343d689665ba4548829942a32a386569")

  if ( NOT ENABLE_HEVC )
    message( STATUS "Building HEVC with only hardware libraries." )
  else( )
    message( STATUS "Building HEVC with software libraries: SW Decoder & Encoder, GACC." )

    # HEVC Common Part
    if( NOT DEFINED ENV{MFX_HEVC_VERSION} )
      set( hevc_version 0.0.000.0000 )
    else( )
      set( hevc_version $ENV{MFX_HEVC_VERSION} )
    endif( )

    set (HEVC_PRODUCT_NAME "Intel(R) Media SDK")
    set (sw_HEVC_DECODER_DESCRIPTION "Intel(R) Media SDK - HEVC Software Decode Plug-in")
    set (sw_HEVC_ENCODER_DESCRIPTION "Intel(R) Media SDK - HEVC Software Encode Plug-in")
    set (hw_HEVC_ENCODER_DESCRIPTION "Intel(R) Media SDK - HEVC GPU Accelerated Encode Plug-in")

    set_file_and_product_version( ${hevc_version} hevc_version_defs )

    set( sources "" )
    set( sources.plus "" )

    list( APPEND sources ${plugin_common_sources} )

    list( APPEND sources ${MSDK_LIB_ROOT}/shared/src/libmfxsw_decode.cpp )
    list( APPEND sources ${MSDK_LIB_ROOT}/decode/h265/src/mfx_h265_dec_decode.cpp )

    set( prefix ${CMAKE_CURRENT_SOURCE_DIR}/src )
      list( APPEND sources.plus
        ${prefix}/mfx_hevc_dec_plugin.cpp
        ${prefix}/mfx_stub_dec_plugin.cpp
      )

    # HEVC Decoder
    if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
      set( plugin_name mfx_hevcd_sw64 )
      set( eval_plugin_name mfx_eval_hevcd_sw64 )
    else()
      set( plugin_name mfx_hevcd_sw32 )
      set( eval_plugin_name mfx_eval_hevcd_sw32 )
    endif()

    set (description_name ${sw_HEVC_DECODER_DESCRIPTION})

    set( LIBS "" )

    list( APPEND LIBS
        color_space_converter_sw
        umc_h265_sw )

    list( APPEND LIBS
      hevc_pp_dispatcher
      hevc_pp_atom
      hevc_pp_avx2
      hevc_pp_px
      hevc_pp_sse4
      hevc_pp_ssse3 )

    list( APPEND LIBS
      media_buffers
      umc_io
      umc
      vm
      vm_plus
      mfx_trace
      ${ITT_LIBRARIES}
      ippmsdk_l ippdc_l ippj_l ippvc_l ippcc_l ippcv_l ippi_l ipps_l ippcore_l
      pthread
      dl )

    set( USE_STRICT_NAME TRUE )
    set(MFX_LDFLAGS "${MFX_ORIG_LDFLAGS} -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/libmfxsw_plugin.map" )

    set( defs "-DAS_HEVCD_PLUGIN -DMFX_PLUGIN_PRODUCT_NAME=\"\\\"${HEVC_PRODUCT_NAME}\"\\\" -DMFX_FILE_DESCRIPTION=\"\\\"${description_name}\"\\\" ${hevc_version_defs} ${hw_defs}" )
    set( defs "${API_FLAGS} ${WARNING_FLAGS} ${defs}" )
    gen_plugins_cfg("HEVC_Decoder_SW" ${HEVC_DECODER_SW_GUID} ${plugin_name} "01" "HEVC" "not_eval")
    make_library( ${plugin_name} sw shared)

    set( defs "-DAS_HEVCD_PLUGIN -DHEVCD_EVALUATION -DMFX_PLUGIN_PRODUCT_NAME=\"\\\"${HEVC_PRODUCT_NAME}\"\\\" -DMFX_FILE_DESCRIPTION=\"\\\"${description_name}\"\\\" ${hevc_version_defs}" )
    set( defs "${API_FLAGS} ${WARNING_FLAGS} ${defs}" )
    gen_plugins_cfg("HEVC_Decoder_SW" ${HEVC_DECODER_SW_GUID} ${eval_plugin_name} "01" "HEVC" "eval")
    make_library( ${eval_plugin_name} sw shared)

    if( Windows )
      install( TARGETS ${plugin_name} ${eval_plugin_name} RUNTIME DESTINATION ${MFX_PLUGINS_DIR} )
    else()
      install( TARGETS ${plugin_name} ${eval_plugin_name} LIBRARY DESTINATION ${MFX_PLUGINS_DIR} )
    endif()

    # HEVC Encoder (SW & GACC)

    set( sources "" )
    set( sources.plus "" )
    list( APPEND sources ${plugin_common_sources} )

    set( hevce_variants "" )
    if (ENABLE_HEVC)
      set( hevce_variants
          sw
          hw )
    endif()
    set( defs "" )

    foreach( dir ${MSDK_LIB_ROOT}/encode/h265/src )
      file( GLOB_RECURSE srcs "${dir}/*.c" "${dir}/*.cpp" )
      list( APPEND sources ${srcs})
    endforeach()

    file( GLOB_RECURSE srcs "${MSDK_LIB_ROOT}/fs_det/src/*.cpp" )
    list( APPEND sources ${srcs} )

    foreach( prefix ${MSDK_LIB_ROOT}/shared/src )
      list( APPEND sources
        ${prefix}/libmfxsw_encode.cpp
        ${prefix}/mfx_enc_common.cpp
      )
    endforeach()

    set( sources.plus "" )
    foreach( prefix ${CMAKE_CURRENT_SOURCE_DIR}/src )
      list( APPEND sources.plus
        ${prefix}/mfx_hevc_enc_plugin.cpp
        ${prefix}/watermark.cpp
      )
    endforeach()

    foreach( dir ${MSDK_LIB_ROOT}/genx/h265_encode/src )
      file( GLOB_RECURSE srcs "${dir}/*_isa.cpp" )
      list( APPEND sources ${srcs})
    endforeach()

    foreach( dir ${MSDK_LIB_ROOT}/fei/h265/src )
      file( GLOB_RECURSE srcs "${dir}/*.c" "${dir}/*.cpp" )
      list( APPEND sources.plus ${srcs})
    endforeach()

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
      set( LIBS "" )
      list( APPEND LIBS
        hevc_pp_dispatcher
        hevc_pp_atom
        hevc_pp_avx2
        hevc_pp_px
        hevc_pp_sse4
        hevc_pp_ssse3
        media_buffers
        genx
        umc_io
        umc
        vm
        vm_plus
        mfx_trace
        ${ITT_LIBRARIES}
        ippmsdk_l ippdc_l ippj_l ippvc_l ippcc_l ippcv_l ippi_l ipps_l ippcore_l
        pthread
        dl
        )

      if( variant MATCHES hw)
        list( APPEND LIBS
          umc_va_hw
        )
      endif()

      set( USE_STRICT_NAME TRUE )
      set(MFX_LDFLAGS "${MFX_ORIG_LDFLAGS} -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/libmfxsw_plugin.map" )
      set( defs "-DAS_HEVCE_PLUGIN ${API_FLAGS} -DMFX_PLUGIN_PRODUCT_NAME=\"\\\"${HEVC_PRODUCT_NAME}\"\\\" -DMFX_FILE_DESCRIPTION=\"\\\"${description_name}\"\\\" ${hevc_version_defs}" )
      if(${variant} MATCHES hw)
        gen_plugins_cfg("HEVC_Encoder_GACC" ${HEVC_ENCODER_GACC_GUID} ${plugin_name} "02" "HEVC" "not_eval")
      else()
        gen_plugins_cfg("HEVC_Encoder_SW" ${HEVC_ENCODER_SW_GUID} ${plugin_name} "02" "HEVC" "not_eval")
      endif()
      make_library( ${plugin_name} ${variant} shared)

      set( defs "-DAS_HEVCE_PLUGIN -DHEVCE_EVALUATION ${API_FLAGS} -DMFX_PLUGIN_PRODUCT_NAME=\"\\\"${HEVC_PRODUCT_NAME}\"\\\" -DMFX_FILE_DESCRIPTION=\"\\\"${description_name}\"\\\" ${hevc_version_defs}" )
      if(${variant} MATCHES hw)
        gen_plugins_cfg("HEVC_Encoder_GACC" ${HEVC_ENCODER_GACC_GUID} ${eval_plugin_name} "02" "HEVC" "eval")
      else()
        gen_plugins_cfg("HEVC_Encoder_SW" ${HEVC_ENCODER_SW_GUID} ${eval_plugin_name} "02" "HEVC" "eval")
      endif()
      make_library( ${eval_plugin_name} ${variant} shared)

      if( Windows )
        install( TARGETS ${plugin_name} ${eval_plugin_name} RUNTIME DESTINATION ${MFX_PLUGINS_DIR} )
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
    set( av1e_variants hw)
  else( )
    message( STATUS "Building AV1 disabled." )
  endif( )

  set( sources "" )
  set( sources.plus "" )
  set(plugin_common_sources_av1 "")

  list( APPEND plugin_common_sources_av1
    ${MSDK_LIB_ROOT}/cmrt_cross_platform/src/cmrt_cross_platform.cpp
  )

  foreach( prefix ${MSDK_LIB_ROOT}/genx/copy_kernels/src )
    list( APPEND plugin_common_sources_av1
      ${prefix}/genx_cht_copy_isa.cpp
      ${prefix}/genx_skl_copy_isa.cpp
      ${prefix}/genx_cnl_copy_isa.cpp
      ${prefix}/genx_icl_copy_isa.cpp
      ${prefix}/genx_icllp_copy_isa.cpp
      ${prefix}/genx_tgllp_copy_isa.cpp
      ${prefix}/genx_tgl_copy_isa.cpp
    )
  endforeach()

  list( APPEND sources ${plugin_common_sources_av1} )

  set (AV1_PRODUCT_NAME "Intel(R) Media SDK")
  set (sw_AV1_DECODER_DESCRIPTION "Intel(R) Media SDK - AV1 Software Decode Plug-in")
  set (hw_AV1_DECODER_DESCRIPTION "Intel(R) Media SDK - AV1 Hardware Decode Plug-in")
  set (sw_AV1_ENCODER_DESCRIPTION "Intel(R) Media SDK - AV1 Software Encode Plug-in")
  set (hw_AV1_ENCODER_DESCRIPTION "Intel(R) Media SDK - AV1 GPU Accelerated Encode Plug-in")

  include_directories( ${MSDK_LIB_ROOT}/encode/av1/include )
  include_directories( ${MSDK_LIB_ROOT}/optimization/av1 )
  include_directories( ${MSDK_LIB_ROOT}/fei/av1/include )
  include_directories( ${MSDK_LIB_ROOT}/genx/av1_encode/include )

  set( defs "" )

  foreach( dir ${MSDK_LIB_ROOT}/encode/av1/src )
    file( GLOB_RECURSE srcs "${dir}/*.c" "${dir}/*.cpp" )
    list( APPEND sources ${srcs})
  endforeach()

  foreach( prefix ${CMAKE_CURRENT_SOURCE_DIR}/src )
    list( APPEND sources.plus
      ${prefix}/mfx_av1_enc_plugin.cpp
      ${prefix}/watermark.cpp
    )
  endforeach()

  foreach( dir ${MSDK_LIB_ROOT}/genx/av1_encode/src )
    file( GLOB_RECURSE srcs "${dir}/*_isa.cpp" )
    list( APPEND sources ${srcs})
  endforeach()

  foreach( dir ${MSDK_LIB_ROOT}/fei/av1/src )
    file( GLOB_RECURSE srcs "${dir}/*.c" "${dir}/*.cpp" )
    list( APPEND sources.plus ${srcs})
  endforeach()

  foreach( variant ${av1e_variants} )
    if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
      if( variant MATCHES hw )
        set( plugin_name mfx_av1e_gacc64 )
        set( eval_plugin_name mfx_eval_av1e_gacc64 )
      else()
        set( plugin_name mfx_av1e_${variant}64 )
        set( eval_plugin_name mfx_eval_av1e_${variant}64 )
      endif()
    else()
      if( variant MATCHES hw )
        set( plugin_name mfx_av1e_gacc32 )
        set( eval_plugin_name mfx_eval_av1e_gacc32 )
      else()
        set( plugin_name mfx_av1e_${variant}32 )
        set( eval_plugin_name mfx_eval_av1e_${variant}32 )
      endif()
    endif()

    set( description_name ${${variant}_AV1_ENCODER_DESCRIPTION} )
    set( LIBS "" )
    list( APPEND LIBS
      av1_pp_avx2
      av1_pp_px
      av1_pp_ssse3
      media_buffers
      umc_io
      umc
      vm
      vm_plus
      mfx_trace
      ${ITT_LIBRARIES}
      ippmsdk_l ippdc_l ippj_l ippvc_l ippcc_l ippcv_l ippi_l ipps_l ippcore_l
      pthread
      dl
      )

    if( variant MATCHES hw)
      list( APPEND LIBS
        umc_va_hw
      )
    endif()

    set( USE_STRICT_NAME TRUE )
    set(MFX_LDFLAGS "${MFX_ORIG_LDFLAGS} -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/libmfxsw_plugin.map" )
    set( defs "-DAS_AV1E_PLUGIN ${API_FLAGS} -DMFX_PLUGIN_PRODUCT_NAME=\"\\\"${AV1_PRODUCT_NAME}\"\\\" -DMFX_FILE_DESCRIPTION=\"\\\"${description_name}\"\\\" ${av1_version_defs}" )
    if(${variant} MATCHES hw)
      gen_plugins_cfg("AV1_Encoder_GACC" ${AV1_ENCODER_GACC_GUID} ${plugin_name} "02" "AV1" "not_eval")
    else()
      gen_plugins_cfg("AV1_Encoder_SW" ${AV1_ENCODER_SW_GUID} ${plugin_name} "02" "AV1" "not_eval")
    endif()
    make_library( ${plugin_name} ${variant} shared)

    set( defs "-DAS_AV1E_PLUGIN -DAV1E_EVALUATION ${API_FLAGS} -DMFX_PLUGIN_PRODUCT_NAME=\"\\\"${AV1_PRODUCT_NAME}\"\\\" -DMFX_FILE_DESCRIPTION=\"\\\"${description_name}\"\\\" ${av1_version_defs}" )
    if(${variant} MATCHES hw)
      gen_plugins_cfg("AV1_Encoder_GACC" ${AV1_ENCODER_GACC_GUID} ${eval_plugin_name} "02" "AV1" "eval")
    else()
      gen_plugins_cfg("AV1_Encoder_SW" ${AV1_ENCODER_SW_GUID} ${eval_plugin_name} "02" "AV1" "eval")
    endif()
    make_library( ${eval_plugin_name} ${variant} shared)

    if( Windows )
      install( TARGETS ${plugin_name} ${eval_plugin_name} RUNTIME DESTINATION ${MFX_PLUGINS_DIR} )
    else()
      install( TARGETS ${plugin_name} ${eval_plugin_name} LIBRARY DESTINATION ${MFX_PLUGINS_DIR} )
    endif()

  endforeach()

  set( defs "" )

  # H265FEI
  if( ENABLE_HEVC_FEI )
    set (H265FEI_PRODUCT_NAME "Intel(R) Media SDK")
    set (hw_H265FEI_ENCODER_DESCRIPTION "Intel(R) Media SDK - HEVC GPU Assist APIs Plug-in")

    if( NOT DEFINED ENV{MFX_H265FEI_VERSION} )
      set( h265fei_version 0.0.000.0000 )
    else( )
      set( h265fei_version $ENV{MFX_H265FEI_VERSION} )
    endif( )

    set_file_and_product_version( ${h265fei_version} h265fei_version_defs )

    set( sources "" )
    set( sources.plus "" )
    set( defs "" )

    list( APPEND sources ${plugin_common_sources} )

    foreach( dir ${MSDK_LIB_ROOT}/genx/h265_encode/src )
      file( GLOB_RECURSE srcs "${dir}/*_isa.cpp" )
      list( APPEND sources ${srcs})
    endforeach()

    set( prefix ${MSDK_LIB_ROOT}/shared/src )
    list( APPEND sources
      ${prefix}/libmfxsw_enc.cpp
    )

    set( prefix ${MSDK_LIB_ROOT}/plugin/src )
    list( APPEND sources
      ${prefix}/mfx_h265fei_plugin.cpp
    )

    # list separately instead of just including h265_fei lib because different
    # define needed for plugin
    set( prefix ${MSDK_LIB_ROOT}/fei/h265/src )
    list( APPEND sources
      ${prefix}/mfx_h265_enc_cm_fei.cpp
      ${prefix}/mfx_h265_enc_cm_plugin.cpp
      ${prefix}/mfx_h265_enc_cm_utils.cpp
    )

    list( REMOVE_ITEM sources
      ${MSDK_STUDIO_ROOT}/shared/src/mfx_umc_alloc_wrapper.cpp )

    if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
      set( plugin_name mfx_h265fei_hw64 )
    else()
      set( plugin_name mfx_h265fei_hw32 )
    endif()
    set (description_name ${hw_H265FEI_ENCODER_DESCRIPTION})

    set( LIBS "" )
    list( APPEND LIBS
      media_buffers
      umc_io
      umc
      vm
      vm_plus
      mfx_trace
      ${ITT_LIBRARIES}
      ippmsdk_l ippj_l ippvc_l ippcc_l ippcv_l ippi_l ipps_l ippcore_l
      pthread
      dl
      )
    list( APPEND LIBS
      genx
      umc_va_hw
      )

    set( USE_STRICT_NAME TRUE )
    set(MFX_LDFLAGS "${MFX_ORIG_LDFLAGS} -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/libmfxsw_plugin.map" )
    set( defs "-DAS_H265FEI_PLUGIN -DAS_HEVCE_PLUGIN ${API_FLAGS} -DMFX_PLUGIN_PRODUCT_NAME=\"\\\"${H265FEI_PRODUCT_NAME}\"\\\" -DMFX_FILE_DESCRIPTION=\"\\\"${description_name}\"\\\" ${h265fei_version_defs}")
    gen_plugins_cfg("H265FEI_Encoder" ${H265FEI_ENCODER_GUID} ${plugin_name} "04" "HEVC")
    make_library( ${plugin_name} hw shared)

    msdk_install( ${plugin_name} ${MFX_PLUGINS_DIR} )

    set( defs "" )
  endif()
endif()
