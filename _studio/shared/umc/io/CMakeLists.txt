mfx_include_dirs( ${CMAKE_SOURCE_DIR}/.. )

file( GLOB_RECURSE sources
  "media_buffers/*.cpp"
  "umc_io/*.cpp"
  )

make_library( umc_io_merged sw static )

set( sources.plus "" )
file( GLOB_RECURSE sources.plus
  "umc_va/*.cpp"
  )

make_library( umc_io_merged hw static )
