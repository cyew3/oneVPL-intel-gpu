include_directories (
  ${CMAKE_HOME_DIRECTORY}/${SAMPLES_DIR}/samples/sample_common/include
  ${CMAKE_HOME_DIRECTORY}/${SAMPLES_DIR}/samples/deprecated/sample_videoconf/include
)

list( APPEND LIBS_VARIANT sample_common )

set(DEPENDENCIES libmfx dl pthread)
make_executable( shortname universal )

