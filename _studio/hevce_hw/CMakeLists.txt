
set( HEVC_Encoder_HW_GUID "6fadc791a0c2eb479ab6dcd5ea9da347" )

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set( plugin_name mfx_hevce_hw64 )
else()
  set( plugin_name mfx_hevce_hw32 )
endif()

# Plugin version info
set(version_defs "")
set_file_and_product_version( ${MEDIA_VERSION_STR} version_defs )

add_library( ${plugin_name} SHARED )

target_include_directories(${plugin_name}
  PRIVATE
    h265/include
    ${MSDK_LIB_ROOT}/shared/include
  )

target_sources(${plugin_name}
  PRIVATE
    h265/include/mfx_h265_encode_plugin_hw.h
    h265/src/mfx_h265_encode_plugin.cpp
  )

target_link_libraries(${plugin_name} PUBLIC mfx_shared_lib mfx_plugin_properties)

gen_plugins_cfg( "HEVC_Encoder_HW" ${HEVC_Encoder_HW_GUID} ${plugin_name} "02" "HEVC" )

target_compile_definitions(${plugin_name}
  PUBLIC
    AS_HEVCE_PLUGIN
    ${version_defs}
  )

install( TARGETS ${plugin_name} LIBRARY DESTINATION ${MFX_PLUGINS_DIR} )

include(sources_ext.cmake OPTIONAL)
