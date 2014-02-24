include_directories (
  ${CMAKE_SOURCE_DIR}/sample_common/include
  ${CMAKE_SOURCE_DIR}/sample_decode/include
)

list( APPEND LIBS_VARIANT sample_common )
list( APPEND LIBS dl pthread )
set(NEED_DISPATCHER 1)

foreach( variant sw drm x11 )
  check_variant( ${variant} configured )
  if( configured )
    make_executable( shortname ${variant} )
  endif( )
endforeach( )
