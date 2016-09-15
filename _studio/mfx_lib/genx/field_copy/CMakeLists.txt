if( 0 )
  # NOTE: that's a placeholder. igfxcmrt and igfxcmjit don't have the 'lib' prefix.
  # It means that the right way to load the libs is to use dlopen instead of linking agains the libs.
  # If anyone needs to build genx_field_copy they should make symbol links with name libigfxcmrt${mdf_lib_suffix}.so, libigfxcmjit{mdf_lib_suffix}.so
  mfx_include_dirs( )

  include_directories (${CMAKE_CURRENT_SOURCE_DIR}/include)

  set( sources "" )
  set( sources.plus "" )
  file( GLOB_RECURSE srcs "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp" )
  list( APPEND sources ${srcs})

  link_directories( "${mdf_root}/compiler/lib/${mdf_arch}" )

  set( LIBS "" )

  list( APPEND LIBS
      igfxcmrt${mdf_lib_suffix}
      igfxcmjit${mdf_lib_suffix}
      cm${mdf_lib_suffix}
  )

  set( defs "${defs} ${WARNING_FLAGS}" )
  make_executable( genx_field_copy universal static )
  set( defs "" )

endif()