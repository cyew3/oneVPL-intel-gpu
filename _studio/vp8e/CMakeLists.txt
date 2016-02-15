
mfx_include_dirs( )

# =============================================================================
# VP8E
# =============================================================================

set (VP8_PRODUCT_NAME "Intel(R) Media Server Studio 2015 - SDK VP8 Pack")
set (hw_VP8_ENCODER_DESCRIPTION "Intel(R) Media Server Studio 2015 - SDK VP8 Hardware Encode Plug-in")

if( NOT DEFINED ENV{MFX_VP8_VERSION} )
  set( vp8_version 0.0.000.0000 )
else( )
  set( vp8_version $ENV{MFX_VP8_VERSION} )
endif( )

if( Linux OR Darwin )
  execute_process(
    COMMAND echo
    COMMAND cut -f 1 -d.
    COMMAND date "+.%-y.%-m.%-d"
    OUTPUT_VARIABLE cur_date
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )
  string( SUBSTRING ${vp8_version} 0 1 ver )

  set(vp8_version_defs  "-DMFX_PLUGIN_FILE_VERSION=\"\\\"${ver}${cur_date}\"\\\" -DMFX_PLUGIN_PRODUCT_VERSION=\"\\\"${vp8_version}\"\\\"")
endif( )


set( sources "" )
set( sources.plus "" )

list( APPEND sources ${plugin_common_sources} )

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../mfx_lib/scheduler/include )
include_directories( ${CMAKE_HOME_DIRECTORY}/mdp_msdk-contrib/vp8_libvpx )

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/vp8e_plugin/include )

list( APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/../mfx_lib/shared/src/mfx_enc_common.cpp )
list( APPEND sources ${CMAKE_CURRENT_SOURCE_DIR}/../mfx_lib/shared/src/mfx_common_int.cpp )

set( USE_STRICT_NAME TRUE )
set(MFX_LDFLAGS "${MFX_ORIG_LDFLAGS} -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/libmfx_vp8e_plugin.map" )

set( sources.plus "" )
foreach( prefix ${CMAKE_CURRENT_SOURCE_DIR}/vp8e_plugin/src )
  list( APPEND sources.plus
    ${prefix}/vp8e_plugin.cpp
  )
endforeach()

# sw veriants of VP8E deprecated as newer versions of libvpx incompatible with MSDK
foreach( variant hw )

  if( __ARCH MATCHES intel64 )
    set( plugin_name mfx_vp8e_${variant}64 )
  else()
    set( plugin_name mfx_vp8e_${variant}32 )
  endif()
  if( __CONFIG MATCHES debug )
    set( plugin_name ${plugin_name}_d )
  endif()

  set (description_name ${${variant}_VP8_ENCODER_DESCRIPTION})

  set( LIBS "" )

  list( APPEND LIBS va )

  foreach( prefix ${CMAKE_CURRENT_SOURCE_DIR}/vp8e_plugin/src )
  list( APPEND sources
    ${prefix}/mfx_vp8_enc_common_hw.cpp    
    ${prefix}/mfx_vp8_encode_ddi_hw.cpp
    ${prefix}/mfx_vp8_encode_hybrid_bse.cpp
    ${prefix}/mfx_vp8_encode_utils_hybrid_hw.cpp      
  )
  endforeach()


  foreach( prefix ${CMAKE_CURRENT_SOURCE_DIR}/../shared/umc/io/umc_va/src )
  list( APPEND sources
    ${prefix}/umc_va_linux.cpp
  )
  endforeach()

  set( defs "-DAS_VP8E_PLUGIN -DMFX_VA -DMFX_ENABLE_VP8_VIDEO_ENCODE_HW -DMFX_PLUGIN_PRODUCT_NAME=\"\\\"${VP8_PRODUCT_NAME}\"\\\" -DMFX_FILE_DESCRIPTION=\"\\\"${description_name}\"\\\" ${vp8_version_defs}")

  list( APPEND LIBS
    vpx
    color_space_converter_sw
    umc
    vm
    vm_plus
    media_buffers
    umc_io
    umc_va_elk_hw
    ${ITT_LIBS}
    ippmsdk_l ippj_l ippvc_l ippcc_l ippcv_l ippi_l ipps_l ippcore_l
    pthread
    dl
    )

  set( defs "${defs} -Wall -Werror" )
  make_library( ${plugin_name} ${variant} shared)
  set( defs "" )

endforeach()

# =============================================================================
