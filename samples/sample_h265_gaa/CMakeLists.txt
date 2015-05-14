find_path( FEIH265_INCLUDE mfxfeih265.h PATHS ${MFX_API_HOME}/include )

if(FEIH265_INCLUDE MATCHES NOTFOUND)
  set(skipping _force_skipping)
endif()

include_directories (
  ${CMAKE_SOURCE_DIR}/sample_common/include
  ${CMAKE_SOURCE_DIR}/sample_utilities/vpp_plugin/include
  ${CMAKE_SOURCE_DIR}/sample_h265_gaa/include
)

list( APPEND LIBS_VARIANT sample_common )

set(DEPENDENCIES ${skipping} libmfx dl pthread)
make_executable( shortname universal )
