mfx_include_dirs( )

include_directories (
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../shared/include
  ${CMAKE_HOME_DIRECTORY}/mdp_msdk-samples/samples/sample_common/include
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")

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
