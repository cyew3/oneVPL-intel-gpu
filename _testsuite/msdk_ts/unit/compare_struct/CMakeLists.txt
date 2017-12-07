mfx_include_dirs( )
###################################################################################################
# compare_struct

# Includes
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/compare_struct )

# gtest/gmock
include_directories( ${CMAKE_HOME_DIRECTORY}/mdp_msdk-contrib/gmock/gtest/include )
include_directories( ${CMAKE_HOME_DIRECTORY}/mdp_msdk-contrib/gmock/include )

###################################################################################################
# Tests sources
file( GLOB_RECURSE srcs "${CMAKE_CURRENT_SOURCE_DIR}/compare_struct/*.cpp" )
list( APPEND sources ${srcs})

###################################################################################################
# Defines and Libs
#set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11" )

set( LIBS "" )
list( APPEND LIBS
    vm
    vm_plus
    ${ITT_LIBS}
    ippcore_l
    pthread dl
    mfx_common_hw
)

list( APPEND LIBS_SUFFIX gmock )

###################################################################################################
# Target
make_executable( compare_struct none )
