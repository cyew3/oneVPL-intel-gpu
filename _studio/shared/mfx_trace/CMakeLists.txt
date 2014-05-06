if( VTUNE_FOUND )
  include_directories (
    ${CMAKE_CURRENT_SOURCE_DIR}/include 
    ${CMAKE_HOME_DIRECTORY}/_studio/shared/include
  )
  
  file( GLOB_RECURSE srcs "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp" )
  list( APPEND sources ${srcs})

  make_library( mfx_trace none static )
  
endif()
