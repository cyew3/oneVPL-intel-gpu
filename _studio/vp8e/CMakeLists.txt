
# =============================================================================
# VP8E
# =============================================================================

set (VP8_PRODUCT_NAME "Intel(R) Media SDK")
set (hw_VP8_ENCODER_DESCRIPTION "Intel(R) Media SDK - VP8 Hardware Encode Plug-in")

set( vp8_version ${MEDIA_VERSION_STR} )

foreach( variant hw )

  if( CMAKE_SIZEOF_VOID_P EQUAL 8 )
    set( plugin_name mfx_vp8e_${variant}64 )
  else()
    set( plugin_name mfx_vp8e_${variant}32 )
  endif()

  add_library(${plugin_name} SHARED)

  target_sources(${plugin_name}
    PRIVATE
      ${plugin_common_sources}
      ../mfx_lib/shared/src/mfx_enc_common.cpp
      ../mfx_lib/shared/src/mfx_common_int.cpp
      ../shared/umc/io/umc_va/src/umc_va_linux.cpp
      vp8e_plugin/src/mfx_vp8_enc_common_hw.cpp
      vp8e_plugin/src/mfx_vp8_encode_ddi_hw.cpp
      vp8e_plugin/src/mfx_vp8_encode_hybrid_bse.cpp
      vp8e_plugin/src/mfx_vp8_encode_utils_hybrid_hw.cpp
      vp8e_plugin/src/vp8e_plugin.cpp
    )

  target_include_directories(${plugin_name}
    PRIVATE
      ../mfx_lib/scheduler/linux/include
      ../mfx_lib/shared/include
      vp8e_plugin/include
    )

  target_link_libraries(${plugin_name}
    PUBLIC
      "${IPPLIBS}"
      mfx_shared_lib
      mfx_plugin_properties
      color_space_converter_sw
      umc
      vm
      vm_plus
      media_buffers
      umc_io
      umc_va_hw
      mfx_trace
      ${ITT_LIBRARIES}
      pthread
      dl
      va
    )

  target_link_libraries(${plugin_name}
    PUBLIC
      ${API_FLAGS}
      ${WARNING_FLAGS}
    )

  set (description_name ${${variant}_VP8_ENCODER_DESCRIPTION})
  target_compile_definitions(${plugin_name}
    PUBLIC
      AS_VP8E_PLUGIN
      MFX_VA
      MFX_ENABLE_VP8_VIDEO_ENCODE_HW
      "MFX_PLUGIN_PRODUCT_NAME=\"\\\"${VP8_PRODUCT_NAME}\"\\\""
      "MFX_FILE_DESCRIPTION=\"\\\"${description_name}\"\\\""
      "${vp8_version_defs}"
    )

endforeach()

# =============================================================================
