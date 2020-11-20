add_executable( sample_decvpp )

target_include_directories (sample_decvpp
  PUBLIC
    ${CMAKE_HOME_DIRECTORY}/${SAMPLES_DIR}/samples/sample_common/include
    ${CMAKE_HOME_DIRECTORY}/${SAMPLES_DIR}/samples/sample_decvpp/include
  )

target_link_libraries( sample_decvpp
  PRIVATE
    libmfx
    dl
    pthread
  )
