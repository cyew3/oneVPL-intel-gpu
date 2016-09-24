mfx_include_dirs( )
###################################################################################################
# scheduler_tests

# Includes
include_directories( ${CMAKE_CURRENT_SOURCE_DIR} )
include_directories( ${CMAKE_HOME_DIRECTORY}/mdp_msdk-lib/_studio/mfx_lib/scheduler/include )
# gtest/gmock
include_directories( ${CMAKE_HOME_DIRECTORY}/mdp_msdk-contrib/gmock/gtest/include )
include_directories( ${CMAKE_HOME_DIRECTORY}/mdp_msdk-contrib/gmock/include )

# scheduler sources
file( GLOB_RECURSE srcs "${CMAKE_HOME_DIRECTORY}/mdp_msdk-lib/_studio/mfx_lib/scheduler/src/*.cpp" )
list( APPEND sources ${srcs})

# Tests sources
file( GLOB_RECURSE srcs "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp" )
list( APPEND sources ${srcs})

# Defines and Libs
set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11" )

set( LIBS "" )
list( APPEND LIBS
    vm
    vm_plus
    ${ITT_LIBS}
    ippcore_l
    pthread dl
)

list( APPEND LIBS_SUFFIX gmock )

###################################################################################################
# Target
make_executable( scheduler_tests none )
