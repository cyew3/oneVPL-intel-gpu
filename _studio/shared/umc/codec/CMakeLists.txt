list( APPEND dirs_splmux
  asf_spl
  avi_spl
  common
  demuxer
  h264_spl
  mpeg4_spl  
  spl_common
  vc1_spl
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
  mpeg4_spl
  spl_common
  vc1_spl
  )

# =============================================================================

mfx_include_dirs( )

foreach( dir ${adirs} ${dirs} )
  include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/${dir}/include )
endforeach( )
list( APPEND cdirs
  vc1_common
  )
foreach( dir ${cdirs} )
  include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/${dir}/include )
endforeach( )

# =============================================================================

set( defs "${WARNING_FLAGS}" )
set( sources "" )
foreach( dir ${adirs} )
  file( GLOB_RECURSE srcs "${dir}/*.c" "${dir}/*.cpp" )
  list( APPEND sources ${srcs})
endforeach()

make_library( umc_acodecs_merged none static )
set( defs "" )

# =============================================================================

set( defs "-DAS_HEVCD_PLUGIN -DAS_HEVCE_PLUGIN -DAS_H264LA_PLUGIN" )
set( sources "" )
foreach( dir ${dirs} )
  file( GLOB_RECURSE srcs "${dir}/*.c" "${dir}/*.cpp" )
  list( APPEND sources ${srcs})
endforeach()

set( defs "${defs} ${WARNING_FLAGS}" )
make_library( umc_codecs_merged none static )
set( defs "" )

# =============================================================================

set( sources "" )
foreach( dir ${dirs_splmux} )
  file( GLOB_RECURSE srcs "${dir}/*.c" "${dir}/*.cpp" )
  list( APPEND sources ${srcs})
endforeach()
set( defs "${defs} ${WARNING_FLAGS}" )
make_library( umc_splmux_merged none static )
