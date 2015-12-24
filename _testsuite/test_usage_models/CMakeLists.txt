mfx_include_dirs( )

include_directories (
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../shared/include
  ${CMAKE_HOME_DIRECTORY}/mdp_msdk-samples/samples/sample_common/include
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")

set( LIBS "" )
list( APPEND LIBS
    shared_utils
    umc
    vm
    vm_plus
    sample_common
    mfx
    ipps_l ippcore_l
    dl
    pthread
)
make_executable( shortname universal )
