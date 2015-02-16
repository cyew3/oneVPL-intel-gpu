include_directories (
  ${CMAKE_SOURCE_DIR}/sample_common/include
)

make_library( shortname none static )

set(DEPENDENCIES _enable_sw)
make_library( shortname sw static )

set(DEPENDENCIES libva-drm libdrm libva)
make_library( shortname drm static )

set(DEPENDENCIES libva-x11 x11 libva)
make_library( shortname x11 static )
