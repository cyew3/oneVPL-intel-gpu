find_path( FEIH265_INCLUDE mfxfeih265.h PATHS ${MFX_API_HOME}/include )

if(FEIH265_INCLUDE MATCHES NOTFOUND)
  set(skipping _force_skipping)
endif()

include_directories (
  ${CMAKE_SOURCE_DIR}/sample_common/include
  ${CMAKE_SOURCE_DIR}/sample_user_modules/plugin_api
  ${CMAKE_SOURCE_DIR}/sample_utilities/vpp_plugin/include
  ${CMAKE_SOURCE_DIR}/sample_h265_gaa/include
  ${CMAKE_SOURCE_DIR}/../../mdp_msdk-api/include
)

list( APPEND LIBS_VARIANT sample_common )

set(DEPENDENCIES ${skipping} _enable_sw libmfx dl pthread my_check)
make_executable( shortname sw )

set(DEPENDENCIES ${skipping} libmfx libva-drm libdrm libva dl pthread)
make_executable( shortname drm )

set(DEPENDENCIES ${skipping} libmfx libva-x11 x11 libva dl pthread)
make_executable( shortname x11 )
