mfx_include_dirs( )

include_directories (
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../shared/include
  ${CMAKE_CURRENT_SOURCE_DIR}/common/include
  ${CMAKE_CURRENT_SOURCE_DIR}/common/include/vm
)

# =============================================================================

set( sources "" )
set( sources.plus "" )
file( GLOB_RECURSE srcs "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp" )
list( APPEND sources ${srcs})
file( GLOB_RECURSE srcs "${CMAKE_CURRENT_SOURCE_DIR}/common/src/*.cpp" )
list( APPEND sources ${srcs})

set( LIBS "" )
list( APPEND LIBS
    shared_utils
    sample_common
    mfx
    umc
    vm
    vm_plus
    ippdc_l ippvc_l ippcc_l ippi_l ipps_l ippcore_l
    dl
    pthread
)

set( defs "${defs} ${WARNING_FLAGS}" )
make_executable( test_vpp universal static )
set( defs "" )
