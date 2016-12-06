if( ENABLE_MONDELLO )

include_directories (
  ${CMAKE_SOURCE_DIR}/sample_common/include
  ${CMAKE_SOURCE_DIR}/sample_misc/wayland/include
  ${CMAKE_SOURCE_DIR}/sample_mondello/include
  ${CMAKE_SOURCE_DIR}/sample_plugins/rotate_cpu/include
)

list( APPEND LIBS_VARIANT sample_common gcss ia_camera ia_cipf camhal )

set(DEPENDENCIES libmfx dl pthread)
make_executable( shortname universal )

install( TARGETS ${target} RUNTIME DESTINATION ${MFX_SAMPLES_INSTALL_BIN_DIR} )

endif()
