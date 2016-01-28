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
  include_directories( ${CMAKE_SOURCE_DIR}/shared/umc/codec/${dir}/include )
endforeach( )

# =============================================================================

set( defs "-Wall -Werror" )
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

set( defs "${defs} -Wall -Werror" )
make_library( umc_codecs_merged none static )
set( defs "" )

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
