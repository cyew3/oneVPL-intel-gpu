set(MFX_ORIG_LDFLAGS "${MFX_LDFLAGS}" )

mfx_include_dirs( )
foreach( dir ${vdirs} )
  include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/include )
endforeach()

set( sources "" )
set( sources.plus "" )
file( GLOB_RECURSE srcs "src/*.c" "src/*.cpp" )
list( APPEND sources.plus ${srcs})

set( defs "${WARNING_FLAGS}" )
make_library( genx_h264_encode_embeded none static )
set( defs "" )
