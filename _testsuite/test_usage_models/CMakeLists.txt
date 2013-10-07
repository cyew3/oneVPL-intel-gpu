mfx_include_dirs( )

include_directories (
  ${CMAKE_HOME_DIRECTORY}/_testsuite/test_usage_models/include
  ${CMAKE_HOME_DIRECTORY}/_testsuite/shared/include
  ${CMAKE_HOME_DIRECTORY}/samples/sample_common/include
)

foreach( variant x11 drm )
  check_variant( ${variant} configured )
  if( configured )
    set( LIBS "" )
    list( APPEND LIBS
        shared_utils
        umc_core_merged
        sample_common_${variant}
        mfx
        ipps_l ippcore_l
        dl
        pthread
    )
    make_executable( shortname ${variant} )
  endif()
endforeach()
