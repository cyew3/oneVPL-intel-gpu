add_library(vpp_plugin STATIC)
set_property(TARGET vpp_plugin PROPERTY FOLDER "samples")

target_include_directories(vpp_plugin
  PUBLIC
    include
  )

target_compile_definitions(vpp_plugin
  PRIVATE
    ${WARNING_FLAGS}
  )

target_link_libraries(vpp_plugin
  PUBLIC
    sample_common
  )

target_sources(vpp_plugin
  PRIVATE
   include/mfx_multi_vpp.h
   include/mfx_vpp_plugin.h

   src/mfx_vpp_plugin.cpp
  )

install( TARGETS vpp_plugin
         ARCHIVE DESTINATION ${MFX_SAMPLES_INSTALL_LIB_DIR}
         LIBRARY DESTINATION ${MFX_SAMPLES_INSTALL_LIB_DIR} )
