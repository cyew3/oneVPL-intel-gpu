include_directories (
  ${CMAKE_SOURCE_DIR}/sample_common/include
  ${CMAKE_SOURCE_DIR}/sample_camera/include
)

list( APPEND LIBS_VARIANT sample_common )
list( APPEND LIBS dl pthread )
set(NEED_DISPATCHER 1)

#make_executable( shortname none )
