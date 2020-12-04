# Copyright (c) 2020 Intel Corporation
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

if( Linux )
  set( os_arch "lin" )
elseif( Darwin )
  set( os_arch "darwin" )
elseif( Windows )
  set( os_arch "win" )
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set( os_arch "${os_arch}_x64" )
else()
  set( os_arch "${os_arch}_ia32" )
endif()

if( NOT DEFINED API )
  message( FATAL_ERROR "API is not defined")
elseif (${API} STREQUAL "master"
     OR ${API} STREQUAL "latest"
     OR ${API} VERSION_LESS 2.0 )
  set( MFX_API_HOME ${MFX_API_HOME} )
elseif( ${API} VERSION_GREATER_EQUAL 2.0 )
  set( MFX_API_HOME ${MFX_API_HOME}/mfx2)
else()
  message( FATAL_ERROR "Unknown API = ${API}")
endif()

unset( MFX_INCLUDE CACHE )
find_path( MFX_INCLUDE
  NAMES mfxdefs.h
  PATHS "${MFX_API_HOME}/include"
  NO_CMAKE_FIND_ROOT_PATH
)

include (${CMAKE_ROOT}/Modules/FindPackageHandleStandardArgs.cmake)
find_package_handle_standard_args(MFX REQUIRED_VARS MFX_INCLUDE)

if (NOT MFX_FOUND)
  message( FATAL_ERROR "Unknown API = ${API}")
endif()

macro( make_api_target target)

  add_library( ${target}-api INTERFACE )
  target_include_directories(${target}-api
    INTERFACE
    ${MFX_API_HOME}/include
    ${MFX_API_HOME}/../mediasdk_structures
    $<$<BOOL:${API_USE_VPL}>:${MFX_API_HOME}/include/private>
  )
  target_compile_definitions(${target}-api
    INTERFACE $<$<BOOL:${API_USE_VPL}>:MFX_ONEVPL>
  )

  add_library( ${target}::api ALIAS ${target}-api )
  set (MFX_API_TARGET ${target}::api)

endmacro()

function( get_mfx_version mfx_version_major mfx_version_minor )
  file(STRINGS ${MFX_API_HOME}/include/mfxdefs.h major REGEX "#define MFX_VERSION_MAJOR" LIMIT_COUNT 1)
  if(major STREQUAL "") # old style version
     file(STRINGS ${MFX_API_HOME}/include/mfxvideo.h major REGEX "#define MFX_VERSION_MAJOR")
  endif()
  file(STRINGS ${MFX_API_HOME}/include/mfxdefs.h minor REGEX "#define MFX_VERSION_MINOR" LIMIT_COUNT 1)
  if(minor STREQUAL "") # old style version
     file(STRINGS ${MFX_API_HOME}/include/mfxvideo.h minor REGEX "#define MFX_VERSION_MINOR")
  endif()
  string(REPLACE "#define MFX_VERSION_MAJOR " "" major ${major})
  string(REPLACE "#define MFX_VERSION_MINOR " "" minor ${minor})
  set(${mfx_version_major} ${major} PARENT_SCOPE)
  set(${mfx_version_minor} ${minor} PARENT_SCOPE)
endfunction()

# Potential source of confusion here. MFX_VERSION should contain API version i.e. 1025 for API 1.25, 
# Product version stored in MEDIA_VERSION_STR
if( ${API} STREQUAL "master")
  set( API_FLAGS "")
  set( API_USE_LATEST FALSE )
  get_mfx_version(major_vers minor_vers)
else( )
  if( ${API} STREQUAL "latest" )
    # This would enable all latest non-production features
    set( API_FLAGS -DMFX_VERSION_USE_LATEST )
    set( API_USE_LATEST TRUE )
    get_mfx_version(major_vers minor_vers)
  else()
    set( VERSION_REGEX "[0-9]+\\.[0-9]+" )

    # Breaks up a string in the form maj.min into two parts and stores
    # them in major, minor.  version should be a value, not a
    # variable, while major and minor should be variables.
    macro( split_api_version version major minor )
      if(${version} MATCHES ${VERSION_REGEX})
        string(REGEX REPLACE "^([0-9]+)\\.[0-9]+" "\\1" ${major} "${version}")
        string(REGEX REPLACE "^[0-9]+\\.([0-9]+)" "\\1" ${minor} "${version}")
      else(${version} MATCHES ${VERSION_REGEX})
        message("macro( split_api_version ${version} ${major} ${minor} ")
        message(FATAL_ERROR "Problem parsing API version string.")
      endif(${version} MATCHES ${VERSION_REGEX})
    endmacro( split_api_version )

    split_api_version(${API} major_vers minor_vers)
      # Compute a version number
    math(EXPR version_number "${major_vers} * 1000 + ${minor_vers}" )
    set(API_FLAGS -DMFX_VERSION=${version_number})
  endif()
endif()

set( API_VERSION "${major_vers}.${minor_vers}")

if ( ${API_VERSION} VERSION_GREATER_EQUAL 2.0 )
  set( API_USE_VPL TRUE )
  set( API_USE_LATEST TRUE )
  make_api_target( onevpl )
else()
  set( API_USE_VPL FALSE )
  make_api_target( mfx )
endif()

message(STATUS "Enabling API ${major_vers}.${minor_vers} feature set with flags ${API_FLAGS}")
