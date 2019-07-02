if(NOT OPEN_SOURCE)
  set(H265FEI_ENCODER_GUID   "87e0e80207375240852515cf4a5edde6")

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

    set( defs "-DAS_H265FEI_PLUGIN -DAS_HEVCE_PLUGIN ${API_FLAGS} -DMFX_PLUGIN_PRODUCT_NAME=\"\\\"${H265FEI_PRODUCT_NAME}\"\\\" -DMFX_FILE_DESCRIPTION=\"\\\"${description_name}\"\\\" ${h265fei_version_defs}")
    gen_plugins_cfg("H265FEI_Encoder" ${H265FEI_ENCODER_GUID} ${plugin_name} "04" "HEVC")
    make_library( ${plugin_name} hw shared)

    msdk_install( ${plugin_name} ${MFX_PLUGINS_DIR} )

    set( defs "" )
  endif()
endif()
