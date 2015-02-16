include_directories (
  ${CMAKE_SOURCE_DIR}/sample_common/include
  ${CMAKE_SOURCE_DIR}/sample_videoconf/include
)

list( APPEND LIBS_VARIANT sample_common )

set(DEPENDENCIES _enable_sw libmfx dl pthread)
make_executable( shortname sw )

set(DEPENDENCIES libmfx libva-drm libdrm libva dl pthread)
make_executable( shortname drm )

set(DEPENDENCIES libmfx libva-x11 x11 libva dl pthread)
make_executable( shortname x11 )
