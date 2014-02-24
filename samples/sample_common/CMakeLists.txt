include_directories (
  ${CMAKE_SOURCE_DIR}/sample_common/include
)

make_library( shortname none static )

foreach( variant sw drm x11 )
  check_variant( ${variant}, configured )
  if( configured )
    make_library( shortname ${variant} static )
  endif( )
endforeach( )
