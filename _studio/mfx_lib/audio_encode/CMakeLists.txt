set(MFX_ORIG_LDFLAGS "${MFX_LDFLAGS}" )

list( APPEND vdirs
  aac
  )
list( APPEND cdirs
  aac_enc common
  )
mfx_include_dirs( )
foreach( dir ${vdirs} )
  include_directories( ${CMAKE_SOURCE_DIR}/mfx_lib/audio_encode/${dir}/include )
endforeach()
foreach( dir ${cdirs} )
  include_directories( ${CMAKE_SOURCE_DIR}/shared/umc/codec/${dir}/include )
endforeach( )

set( sources "" )
set( sources.plus "" )
foreach( dir ${vdirs} )
  file( GLOB_RECURSE srcs "${dir}/src/*.c" "${dir}/src/*.cpp" )
  list( APPEND sources ${srcs})
endforeach()

make_library( audio_encode none static )

