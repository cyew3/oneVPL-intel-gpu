set(MFX_ORIG_LDFLAGS "${MFX_LDFLAGS}" )

mfx_include_dirs( )

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/include )

set( defs "${API_FLAGS} ${WARNING_FLAGS}" )
set( sources "" )
set( sources.plus "" )
list( APPEND sources
  ${CMAKE_CURRENT_SOURCE_DIR}/src/genx_skl_copy_isa.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/genx_cht_copy_isa.cpp
)
make_library( copy_kernels hw static )
set( defs "" )
