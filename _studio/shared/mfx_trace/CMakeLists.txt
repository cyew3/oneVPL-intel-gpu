if( VTUNE_FOUND )

mfx_include_dirs( )

  include_directories (
    ${CMAKE_CURRENT_SOURCE_DIR}/include 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../umc/core/vm/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../mdp_msdk-api/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../mdp_msdk-api/mediasdk_structures
  )
  
  file( GLOB_RECURSE srcs "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp" )
  list( APPEND sources ${srcs})

  set( defs "${API_FLAGS} ${WARNING_FLAGS}" )
  make_library( mfx_trace none static )
  make_library( mfx_trace hw static )
  set( defs "" )
  
endif()
