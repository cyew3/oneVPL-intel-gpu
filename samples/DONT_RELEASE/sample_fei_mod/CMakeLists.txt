include_directories (
  ${CMAKE_SOURCE_DIR}/sample_common/include
  ${CMAKE_SOURCE_DIR}/sample_fei/include
  ${CMAKE_SOURCE_DIR}/DONT_RELEASE/sample_fei_mod/include
)

add_definitions(-DMOD_FEI)

file( GLOB_RECURSE include "[^.]*.h" "${CMAKE_SOURCE_DIR}/sample_fei/*.h" )
file( GLOB_RECURSE sources "[^.]*.c" "[^.]*.cpp" "${CMAKE_SOURCE_DIR}/sample_fei/*.c" "${CMAKE_SOURCE_DIR}/sample_fei/*.cpp" )


list( APPEND LIBS_VARIANT sample_common )

set(DEPENDENCIES itt libmfx dl pthread)
make_executable( shortname universal )