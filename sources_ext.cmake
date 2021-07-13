if (NOT BUILD_VAL_TOOLS)
  return()
endif()

get_filename_component( CROSS_REPO_SPACE "${BUILDER_ROOT}/../.." ABSOLUTE )
get_filename_component( MSDK_VAL_TOOLS_ROOT "${MFX_HOME}/../mdp_msdk-val-tools" ABSOLUTE )
get_filename_component( MSDK_CONTRIB_ROOT "${MFX_HOME}/../mdp_msdk-contrib" ABSOLUTE )

# include( builder/FindMFX.cmake )
include( builder/FindTrace.cmake )

set( CMAKE_SOURCE_DIR "${CROSS_REPO_SPACE}/mdp_msdk-val-tools" )
create_build_outside_tree("bs_parser")
create_build_outside_tree("_testsuite_common")

set( CMAKE_SOURCE_DIR "${CROSS_REPO_SPACE}/mdp_msdk-contrib" )
create_build_outside_tree("gmock")
# umc/vm required
create_build_outside_tree("SafeStringStaticLibrary")

foreach(dir _testsuite)
  set( CMAKE_SOURCE_DIR "${CMAKE_HOME_DIRECTORY}/${dir}" )
  create_build()
endforeach()

create_plugins_cfg(${CMAKE_BINARY_DIR})

