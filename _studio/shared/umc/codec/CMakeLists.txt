list( APPEND adirs
  aac_common
  aac_dec
  aac_enc
  )

list( APPEND dirs
  asf_spl
  avi_spl
  brc
  common
  demuxer
  h264_enc
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
  h265_dec
  mpeg2_dec
  h264_dec
  vc1_dec
  jpeg_dec
  )

# =============================================================================

mfx_include_dirs( )

foreach( dir ${adirs} ${dirs} ${dirs_impl} )
  include_directories( ${CMAKE_HOME_DIRECTORY}/_studio/shared/umc/codec/${dir}/include )
endforeach( )

# =============================================================================

set( sources "" )
foreach( dir ${adirs} )
  file( GLOB_RECURSE srcs "${dir}/*.c" "${dir}/*.cpp" )
  list( APPEND sources ${srcs})
endforeach()

make_library( umc_acodecs_merged none static )

# =============================================================================

set( sources "" )
foreach( dir ${dirs} )
  file( GLOB_RECURSE srcs "${dir}/*.c" "${dir}/*.cpp" )
  list( APPEND sources ${srcs})
endforeach()

make_library( umc_codecs_merged none static )

# =============================================================================

set( sources "" )
foreach( dir ${dirs_impl} )
  file( GLOB_RECURSE srcs "${dir}/*.c" "${dir}/*.cpp" )
  list( APPEND sources ${srcs}) 
endforeach() 

foreach( variant sw hw )
  make_library( umc_codecs_merged ${variant} static )
endforeach( )
