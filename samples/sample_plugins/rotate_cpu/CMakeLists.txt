set( DEF_PATH ${CMAKE_SOURCE_DIR}/sample_plugins/def )

include_directories (
  ${CMAKE_SOURCE_DIR}/sample_common/include
  ${CMAKE_SOURCE_DIR}/sample_plugins/rotate_cpu/include
)

set(LDFLAGS "${LDFLAGS} -Wl,--version-script=${DEF_PATH}/mfx_plugin.map" )

list( APPEND LIBS sample_common)

make_library(sample_rotate_plugin none shared)
