include_directories (
  ${CMAKE_SOURCE_DIR}/sample_common/include
  ${CMAKE_SOURCE_DIR}/sample_misc/wayland/include
  ${CMAKE_SOURCE_DIR}/sample_encode/include
  ${CMAKE_SOURCE_DIR}/DONT_RELEASE/sample_encode_mod/include
  ${CMAKE_SOURCE_DIR}/sample_plugins/rotate_cpu/include
)

add_definitions(-DMOD_ENC)

file( GLOB_RECURSE include "[^.]*.h" "${CMAKE_SOURCE_DIR}/sample_encode/*.h" )
file( GLOB_RECURSE sources "[^.]*.c" "[^.]*.cpp" "${CMAKE_SOURCE_DIR}/sample_encode/*.c" "${CMAKE_SOURCE_DIR}/sample_encode/*.cpp" )


list( APPEND LIBS_VARIANT sample_common )

set(DEPENDENCIES libmfx dl pthread)
make_executable( shortname universal "nosafestring")
