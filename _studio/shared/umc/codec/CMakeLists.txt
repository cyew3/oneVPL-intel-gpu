list( APPEND dirs_splmux
  asf_spl
  avi_spl
  common
  demuxer
  h264_spl
  mpeg4_spl  
  spl_common
  vc1_common
  vc1_spl
  jpeg_common
  )

list( APPEND adirs
  aac_common
  aac_dec
  aac_enc
  mp3_common
  mp3_dec
  )

list( APPEND dirs
  asf_spl
  avi_spl
  common
  demuxer
  h264_spl
  mpeg2_enc
  mpeg4_spl
  scene_analyzer
  spl_common
  vc1_common
  vc1_enc
  vc1_spl
  jpeg_enc
  jpeg_common
  )

list( APPEND dirs_impl
  color_space_converter
#  h265_dec
  mpeg2_dec
  h264_dec
  vc1_dec
  jpeg_dec
  )

# =============================================================================

mfx_include_dirs( )

foreach( dir ${adirs} ${dirs} brc h264_enc ${dirs_impl} )
  include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/${dir}/include )
endforeach( )

# =============================================================================

set( sources "" )
foreach( dir ${adirs} )
  file( GLOB_RECURSE srcs "${dir}/*.c" "${dir}/*.cpp" )
  list( APPEND sources ${srcs})
endforeach()

make_library( umc_acodecs_merged none static )

# =============================================================================

set( defs "-DAS_HEVCD_PLUGIN -DAS_HEVCE_PLUGIN -DAS_H264LA_PLUGIN" )
set( sources "" )
foreach( dir ${dirs} )
  file( GLOB_RECURSE srcs "${dir}/*.c" "${dir}/*.cpp" )
  list( APPEND sources ${srcs})
endforeach()

make_library( umc_codecs_merged none static )
set( defs "" )

# =============================================================================

set( defs "-DAS_HEVCD_PLUGIN -DAS_HEVCE_PLUGIN -DAS_H264LA_PLUGIN" )
set( sources "" )
foreach( dir ${dirs_impl} )
  file( GLOB_RECURSE srcs "${dir}/*.c" "${dir}/*.cpp" )
  list( APPEND sources ${srcs}) 
endforeach()

make_library( umc_codecs_merged hw static )
list(REMOVE_ITEM sources ${CMAKE_CURRENT_SOURCE_DIR}/jpeg_dec/src/umc_mjpeg_mfx_decode_hw.cpp)
make_library( umc_codecs_merged sw static )
set( defs "" )

# =============================================================================

set( sources "" )
file( GLOB_RECURSE srcs "brc/*.c" "brc/*.cpp" )
list( APPEND sources ${srcs}) 

make_library( umc_brc sw static )

# =============================================================================

set( sources "" )
file( GLOB_RECURSE srcs "color_space_converter/*.c" "color_space_converter/*.cpp" )
list( APPEND sources ${srcs}) 

make_library( umc_csc sw static )

# =============================================================================

set( sources "" )
file( GLOB_RECURSE srcs "h264_enc/*.c" "h264_enc/*.cpp" )
list( APPEND sources ${srcs}) 

make_library( umc_h264_enc sw static )

# =============================================================================

if( NOT MFX_LIB_ONLY )

  set( defs "-DAS_HEVCD_PLUGIN -DAS_HEVCE_PLUGIN -DAS_H264LA_PLUGIN" )
  set( sources "" )
  file( GLOB_RECURSE srcs "h265_dec/*.c" "h265_dec/*.cpp" )
  list( APPEND sources ${srcs}) 
  include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/h265_dec/include )

  foreach( variant sw hw )
    make_library( umc_h265 ${variant} static )
  endforeach( )

endif()

set( defs "" )

# =============================================================================

set( sources "" )
foreach( dir ${dirs_splmux} )
  file( GLOB_RECURSE srcs "${dir}/*.c" "${dir}/*.cpp" )
  list( APPEND sources ${srcs})
endforeach()

make_library( umc_splmux_merged none static )
