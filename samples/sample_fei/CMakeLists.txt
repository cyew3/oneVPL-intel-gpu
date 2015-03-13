find_path( FEI_INCLUDE mfxfei.h PATHS ${MFX_API_HOME}/include )

if(FEI_INCLUDE MATCHES NOTFOUND)
  set(skipping _force_skipping)
endif()

include_directories (
  ${CMAKE_SOURCE_DIR}/sample_common/include
  ${CMAKE_SOURCE_DIR}/sample_fei/include
  ${CMAKE_SOURCE_DIR}/sample_plugins/rotate_cpu/include
)

list( APPEND LIBS_VARIANT sample_common )

set(DEPENDENCIES ${skipping} libmfx libva-drm libdrm libva dl pthread)
make_executable( shortname drm )
