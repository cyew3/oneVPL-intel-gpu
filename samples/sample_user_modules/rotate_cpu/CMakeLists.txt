set( PLUGIN_API ${CMAKE_SOURCE_DIR}/sample_user_modules/plugin_api )

include_directories (
  ${CMAKE_SOURCE_DIR}/sample_common/include
  ${CMAKE_SOURCE_DIR}/sample_user_modules/rotate_cpu/include
  ${PLUGIN_API}
)

set(MFX_LDFLAGS "${MFX_ORIG_LDFLAGS} -Wl,--version-script=${PLUGIN_API}/mfx_plugin.map" )

list(APPEND sources.plus "${PLUGIN_API}/mfx_plugin_module.cpp")

list( APPEND LIBS sample_common)

make_library(sample_rotate_plugin none shared)
