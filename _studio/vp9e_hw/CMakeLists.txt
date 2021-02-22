add_library( mfx_vp9e_hw64 SHARED )

set(VP9_Encoder_HW_GUID "ce44ef6f1a6d2246b412bb38d6e45182")

# Plugin version info
set( vp9_version ${MEDIA_VERSION_STR} )

target_include_directories(mfx_vp9e_hw64
  PUBLIC
    include
    ${MSDK_STUDIO_ROOT}/shared/include
    ${MSDK_STUDIO_ROOT}/mfx_lib/shared/include
    ${MSDK_STUDIO_ROOT}/mfx_lib/scheduler/linux/include
  )

target_sources(mfx_vp9e_hw64
  PRIVATE
    src/mfx_vp9_encode_plugin.cpp
  )

gen_plugins_cfg("VP9_Encoder_HW" ${VP9_Encoder_HW_GUID} "mfx_vp9e_hw64" "02" "VP9")

msdk_install( mfx_vp9e_hw64 ${MFX_PLUGINS_DIR} )

if(NOT OPEN_SOURCE)
  if (MFX_BUNDLED_IPP)
    set(IPPLIBS ipp)
  else()
    set(IPPLIBS IPP::msdk IPP::j IPP::vc IPP::cc IPP::cv IPP::i IPP::s IPP::core)
  endif()
endif()

target_link_libraries(mfx_vp9e_hw64
  PUBLIC 
    mfx_shared_lib
    mfx_plugin_properties
    ${IPPLIBS}
    umc
  PRIVATE
    mfx_sdl_properties)

target_compile_definitions(mfx_vp9e_hw64
  PRIVATE
    AS_VP9E_PLUGIN
    VP9_LOGGING
  )
