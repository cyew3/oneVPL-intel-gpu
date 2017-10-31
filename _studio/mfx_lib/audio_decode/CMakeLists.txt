set(MFX_ORIG_LDFLAGS "${MFX_LDFLAGS}" )

list( APPEND vdirs
  aac mp3
  )
list( APPEND cdirs
  mp3_common aac_dec common mp3_dec aac_common
  )
mfx_include_dirs( )
foreach( dir ${vdirs} )
  include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/${dir}/include )
endforeach()
foreach( dir ${cdirs} )
  include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../../shared/umc/codec/${dir}/include )
endforeach( )

set( defs "${API_FLAGS} ${WARNING_FLAGS}" )
set( sources "" )
set( sources.plus "" )
foreach( dir ${vdirs} )
  file( GLOB_RECURSE srcs "${dir}/src/*.c" "${dit}/src/*.cpp" )
  list( APPEND sources ${srcs})
endforeach()

make_library( audio_decode none static )
set( defs "" )
