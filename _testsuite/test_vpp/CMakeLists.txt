mfx_include_dirs( )

include_directories (
  ${CMAKE_HOME_DIRECTORY}/_testsuite/test_vpp/include
  ${CMAKE_HOME_DIRECTORY}/_testsuite/shared/include
  ${CMAKE_HOME_DIRECTORY}/samples/sample_common/include
)

# =============================================================================

set( sources "" )
set( sources.plus "" )
file( GLOB_RECURSE srcs "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp" )
list( APPEND sources ${srcs})

foreach( variant sw drm x11 )
  check_variant( ${variant} configured )
  if( configured )
    set( LIBS "" )
    list( APPEND LIBS
        shared_utils
        sample_common_${variant}
        mfx
        umc_core_merged
        ippcore_l
        dl
        pthread
    )
    make_executable( test_vpp ${variant} static )
  endif( )
endforeach( )
