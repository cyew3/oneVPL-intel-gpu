if(0)
include_directories (
  ${CMAKE_HOME_DIRECTORY}/${SAMPLES_DIR}/samples/sample_common/include
  ${CMAKE_HOME_DIRECTORY}/${SAMPLES_DIR}/samples/sample_camera/include
)

list( APPEND LIBS_NOVARIANT sample_common )

set(DEPENDENCIES libmfx dl pthread)
make_executable( shortname none "nosafestring" )

install( TARGETS ${target} RUNTIME DESTINATION ${MFX_SAMPLES_INSTALL_BIN_DIR} )

endif(0)