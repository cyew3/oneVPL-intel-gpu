#!cmake
#
cmake_minimum_required( VERSION 3.6 )

cmake_policy(SET CMP0048 NEW) # version handled by project()


# Througout this project MFX_VERSION refers to uAPI version and MEDIA_VERSION refers to product version
set(MEDIA_VERSION 20.2)
set(MEDIA_VERSION_STR "${MEDIA_VERSION}.pre${MEDIA_VERSION_EXTRA}" CACHE STRING "" FORCE)

project(mfx1
  VERSION ${MEDIA_VERSION}
  LANGUAGES CXX C ASM)

# Default to `release` build if nothing was specified
if(NOT CMAKE_BUILD_TYPE)
  message(STATUS "No build type selected, default to release")
  set(CMAKE_BUILD_TYPE release)
endif()

if( NOT DEFINED SAMPLES_DIR )
    message( STATUS "SAMPLES_DIR not found setting to default" )
    SET( SAMPLES_DIR "mdp_msdk-lib" )
endif()
message( STATUS "SAMPLES_DIR is set to ${SAMPLES_DIR}" )

if( NOT DEFINED API )
    message( STATUS "API define not found using default" )
else()
    message( STATUS "API is set to ${API}" )
endif()

if( NOT DEFINED API_DIR )
    message( STATUS "API_DIR not found setting to default" )
    SET( API_DIR "mdp_msdk-lib/api" )
endif()
message( STATUS "API_DIR is set to ${API_DIR}" )

if( NOT DEFINED CMAKE_MFX_HOME )
  set( CMAKE_MFX_HOME "${CMAKE_HOME_DIRECTORY}/${API_DIR}" )
endif()

if(NOT DEFINED $ENV{MFX_HOME} )
  set( MFX_HOME ${CMAKE_HOME_DIRECTORY} )
else()
  set( MFX_HOME $ENV{MFX_HOME} )
endif()

option( ENABLE_HEVC_ON_GCC "Build SW versions of HEVC plugins using GCC?" OFF )
option( ENABLE_OPENCL "Build targets dependent on OpenCL?" ON )
if( CMAKE_SYSTEM_NAME MATCHES Linux )
  option( ENABLE_X11_DRI3 "Build X11 DRI3 versions of the targets?" OFF )
  option( ENABLE_WAYLAND "Build WAYLAND versions of the targets?" OFF )
endif()

include( mdp_msdk-lib/builder/FindOpenCL.cmake )
include( mdp_msdk-lib/builder/FindFunctions.cmake )
include( mdp_msdk-lib/builder/FindMFX.cmake )
include( mdp_msdk-lib/builder/FindGlobals.cmake )
include( mdp_msdk-lib/builder/FindIPP.cmake )
include( mdp_msdk-lib/builder/FindMDF.cmake )
include( mdp_msdk-lib/builder/FindPackages.cmake )
include( mdp_msdk-lib/builder/FindInternals.cmake )
include( mdp_msdk-lib/builder/FindTrace.cmake )

add_subdirectory( mdp_msdk-lib/api/mfx_dispatch/linux)

include( mdp_msdk-lib/builder/ConfTargets.cmake )

foreach( dir
  ${API_DIR}/mediasdk_tracer_v2 ## open-source-cut-line
  mdp_msdk-contrib
  mdp_msdk-lib/_studio
  mdp_msdk-lib/_testsuite         ## open-source-cut-start
  mdp_msdk-val-tools
  mdp_msdk-hevc-stream-generator  ## open-source-cut-end
)
  set( CMAKE_SOURCE_DIR "${CMAKE_HOME_DIRECTORY}/${dir}" )
  create_build()
endforeach()

add_subdirectory( mdp_msdk-lib/samples )

message("")
message("********************** Media SDK Configuration Summary **********************")
message("MediaSDK:")
message("  API (request to use this version)       : ${API}")
message("  API_VERSION                             : ${API_VERSION}")
message("  API_FLAGS                               : ${API_FLAGS}")
message("  MFX_CONFIG_FILE                         : ${MFX_CONFIG_FILE}")
message("  See detailed per-component mediasdk configuration in the auto-generated file:")
message("    ${CMAKE_CURRENT_BINARY_DIR}/mfxconfig.h")
message("Cmake:")
message("  CMAKE_VERSION                           : ${CMAKE_VERSION}")
message("  CMAKE_GENERATOR                         : ${CMAKE_GENERATOR}")
message("  CMAKE_BUILD_TOOL                        : ${CMAKE_BUILD_TOOL}")
message("Target:")
message("  CMAKE_SYSTEM_NAME                       : ${CMAKE_SYSTEM_NAME}")
message("  CMAKE_SYSTEM_VERSION                    : ${CMAKE_SYSTEM_VERSION}")
message("  CMAKE_SYSTEM_PROCESSOR                  : ${CMAKE_SYSTEM_PROCESSOR}")
message("General:")
message("  CMAKE_BUILD_TYPE                        : ${CMAKE_BUILD_TYPE}")
message("  CMAKE_TOOLCHAIN_FILE                    : ${CMAKE_TOOLCHAIN_FILE}")
message("  CMAKE_C_COMPILER                        : ${CMAKE_C_COMPILER}")
message("  CMAKE_CXX_COMPILER                      : ${CMAKE_CXX_COMPILER}")
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  message("  Building architecture                   : 64-bit")
else()
  message("  Building architecture                   : 32-bit")
endif()
message("Flags:")
message("  CMAKE_C_FLAGS                           : ${CMAKE_C_FLAGS}")
message("  CMAKE_CXX_FLAGS                         : ${CMAKE_CXX_FLAGS}")
message("Release flags:")
message("  CMAKE_C_FLAGS_RELEASE                   : ${CMAKE_C_FLAGS_RELEASE}")
message("  CMAKE_CXX_FLAGS_RELEASE                 : ${CMAKE_CXX_FLAGS_RELEASE}")
message("Debug flags:")
message("  CMAKE_C_FLAGS_DEBUG                     : ${CMAKE_C_FLAGS_DEBUG}")
message("  CMAKE_CXX_FLAGS_DEBUG                   : ${CMAKE_CXX_FLAGS_DEBUG}")
message("Install:")
message("  CMAKE_INSTALL_PREFIX                    : ${CMAKE_INSTALL_PREFIX}")
message("  CMAKE_INSTALL_FULL_INCLUDEDIR           : ${CMAKE_INSTALL_FULL_INCLUDEDIR}")
message("  CMAKE_INSTALL_FULL_LIBDIR               : ${CMAKE_INSTALL_FULL_LIBDIR}")
message("  CMAKE_INSTALL_FULL_DATADIR              : ${CMAKE_INSTALL_FULL_DATADIR}")
message("  ENABLE_OPENCL                           : ${ENABLE_OPENCL}")
if( CMAKE_SYSTEM_NAME MATCHES Linux )
  message("  ENABLE_X11_DRI3                         : ${ENABLE_X11_DRI3}")
  message("  ENABLE_WAYLAND                          : ${ENABLE_WAYLAND}")
endif()
message("*****************************************************************************")

create_plugins_cfg(${CMAKE_BINARY_DIR})
