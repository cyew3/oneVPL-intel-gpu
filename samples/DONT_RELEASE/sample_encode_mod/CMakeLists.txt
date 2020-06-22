include_directories (
  ${CMAKE_HOME_DIRECTORY}/${SAMPLES_DIR}/samples/sample_common/include
  ${CMAKE_HOME_DIRECTORY}/${SAMPLES_DIR}/samples/sample_misc/wayland/include
  ${CMAKE_HOME_DIRECTORY}/${SAMPLES_DIR}/samples/sample_encode/include
  ${CMAKE_HOME_DIRECTORY}/${SAMPLES_DIR}/samples/DONT_RELEASE/sample_encode_mod/include
  ${CMAKE_HOME_DIRECTORY}/${SAMPLES_DIR}/samples/sample_plugins/rotate_cpu/include
)

add_definitions(-DMOD_ENC)

file( GLOB_RECURSE include "[^.]*.h" "${CMAKE_HOME_DIRECTORY}/${SAMPLES_DIR}/samples/sample_encode/*.h" )
file( GLOB_RECURSE sources "[^.]*.c" "[^.]*.cpp" "${CMAKE_HOME_DIRECTORY}/${SAMPLES_DIR}/samples/sample_encode/*.c" "${CMAKE_HOME_DIRECTORY}/${SAMPLES_DIR}/samples/sample_encode/*.cpp" )


list( APPEND LIBS_VARIANT sample_common )

set(DEPENDENCIES libva libmfx dl pthread)
make_executable( shortname universal "nosafestring")
