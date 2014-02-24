include_directories (
  ${CMAKE_SOURCE_DIR}/sample_common/include
  ${CMAKE_SOURCE_DIR}/sample_utilities/vpp_plugin/include
  ${CMAKE_SOURCE_DIR}/sample_user_modules/plugin_api
)

make_library( shortname none static )
