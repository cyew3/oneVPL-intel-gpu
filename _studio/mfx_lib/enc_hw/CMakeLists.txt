set(MFX_ORIG_LDFLAGS "${MFX_LDFLAGS}" )

list( APPEND vdirs
  h264 mpeg2
  )
mfx_include_dirs( )
foreach( dir ${vdirs} )
  include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/${dir}/include )
endforeach()

set( defs "${API_FLAGS} ${WARNING_FLAGS}" )
set( sources "" )
set( sources.plus "" )
foreach( dir ${vdirs} )
  file( GLOB_RECURSE srcs "${dir}/src/*.c" "${dir}/src/*.cpp" )
  list( APPEND sources.plus ${srcs})
endforeach()

make_library( enc hw static )
set( defs "" )
