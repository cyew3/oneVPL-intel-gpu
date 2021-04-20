if(MFX_DISABLE_SW_FALLBACK)
  return()
endif()

list( APPEND vdirs
  h264 h265 mjpeg mpeg2 vc1 vp8 vp9 av1
)

list( APPEND cdirs
  h265_dec h264_dec mpeg2_dec vp9_dec av1_dec vc1_dec jpeg_dec vc1_common color_space_converter jpeg_common
)

foreach( dir ${vdirs} )
  list (APPEND include ${CMAKE_CURRENT_SOURCE_DIR}/${dir}/include )
endforeach()

list (APPEND include ${MSDK_LIB_ROOT}/vpp/include )

foreach( dir ${cdirs} )
  list (APPEND include ${MSDK_UMC_ROOT}/codec/${dir}/include )
endforeach( )

set(UMC_CODECS ${MSDK_UMC_ROOT}/codec)

list(APPEND source
  ${UMC_CODECS}/jpeg_dec/src/jpegdec.cpp
)

# ================================ decode_ext_hw ================================

target_include_directories(decode_hw PRIVATE ${include})

target_sources(decode_hw PRIVATE ${source})

target_link_libraries(decode_hw
  PUBLIC
    h264_dispatcher # optimized SW-path
  PRIVATE 
    media_buffers
  )

target_compile_definitions(decode_hw PRIVATE $<$<PLATFORM_ID:Windows>:MFX_DX9ON11>)

# ================================ decode_ext_hw ================================

target_include_directories(decode_sw PRIVATE ${include})

target_sources(decode_sw PRIVATE ${source})
target_link_libraries(decode_sw PRIVATE h264_dispatcher umc umc_io vm vm_plus media_buffers) 

