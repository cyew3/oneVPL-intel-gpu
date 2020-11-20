add_library(sample_rotate_plugin SHARED)
set_property(TARGET sample_rotate_plugin PROPERTY FOLDER "samples")

target_include_directories(sample_rotate_plugin
  PUBLIC
    include
  )

target_compile_definitions(sample_rotate_plugin
  PRIVATE
    ${WARNING_FLAGS}
  )

set( PLUGINS_COMMON_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../plugins_common_files )

target_link_options(sample_rotate_plugin
  PRIVATE
    $<$<PLATFORM_ID:Linux>:-Wl,--version-script=${PLUGINS_COMMON_PATH}/mfx_plugin.map>
  )

target_link_libraries(sample_rotate_plugin
  PUBLIC
    sample_common
  )

target_sources(sample_rotate_plugin
  PRIVATE
    include/plugin_rotate.h
    include/rotate_plugin_api.h

    src/plugin_rotate.cpp

    ${PLUGINS_COMMON_PATH}/mfx_plugin_module.cpp
    
    $<$<PLATFORM_ID:Windows>:
      res/resource.h
      res/sample_rotate_plugin.rc
    >
  )

install( TARGETS sample_rotate_plugin LIBRARY DESTINATION ${MFX_SAMPLES_INSTALL_LIB_DIR} )
