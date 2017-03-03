include_directories (
  ${CMAKE_SOURCE_DIR}/sample_common/include
  ${CMAKE_SOURCE_DIR}/sample_multi_transcode/include
  ${CMAKE_SOURCE_DIR}/sample_misc/wayland/include
  ${CMAKE_SOURCE_DIR}/DONT_RELEASE/sample_multi_transcode_mod/include
  ${CMAKE_SOURCE_DIR}/sample_plugins/vpp_plugin/include
  ${CMAKE_SOURCE_DIR}/sample_plugins/rotate_cpu/include
)

add_definitions(-DMOD_SMT)

file( GLOB_RECURSE include "[^.]*.h" "${CMAKE_SOURCE_DIR}/sample_multi_transcode/*.h" )
file( GLOB_RECURSE sources "[^.]*.c" "[^.]*.cpp" "${CMAKE_SOURCE_DIR}/sample_multi_transcode/*.c" "${CMAKE_SOURCE_DIR}/sample_multi_transcode/*.cpp" )


list( APPEND LIBS_VARIANT sample_common )
list( APPEND LIBS_NOVARIANT vpp_plugin )

set(DEPENDENCIES itt libmfx dl pthread)
make_executable( shortname universal )
