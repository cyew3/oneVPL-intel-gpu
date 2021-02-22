##******************************************************************************
## Copyright (C) 2016-2020 Intel Corporation.  All rights reserved.
##
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that the following conditions are met:
## - Redistributions of source code must retain the above copyright notice,
## this list of conditions and the following disclaimer.
## - Redistributions in binary form must reproduce the above copyright notice,
## this list of conditions and the following disclaimer in the documentation
## and/or other materials provided with the distribution.
## - Neither the name of Intel Corporation nor the names of its contributors
## may be used to endorse or promote products derived from this software
## without specific prior written permission.
##
## THIS SOFTWARE IS PROVIDED BY INTEL CORPORATION "AS IS" AND ANY EXPRESS OR
## IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
## OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
## IN NO EVENT SHALL INTEL CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT,
## INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
## NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
## DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
## THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
## (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
## THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
##******************************************************************************

cmake_minimum_required( VERSION 3.14 FATAL_ERROR )
cmake_policy(SET CMP0048 NEW) # version handled by project()

project(mfx VERSION ${MEDIA_VERSION} LANGUAGES CXX)

set( MFX_API_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}/../../include )

# While equal to get_mfx_version in samples/builder, this function should remain separate to make this file self-sufficient
function( get_api_version mfx_version_major mfx_version_minor )
  file(STRINGS ${MFX_API_FOLDER}/mfxdefs.h major REGEX "#define MFX_VERSION_MAJOR" LIMIT_COUNT 1)
  file(STRINGS ${MFX_API_FOLDER}/mfxdefs.h minor REGEX "#define MFX_VERSION_MINOR" LIMIT_COUNT 1)
  string(REPLACE "#define MFX_VERSION_MAJOR " "" major ${major})
  string(REPLACE "#define MFX_VERSION_MINOR " "" minor ${minor})
  set(${mfx_version_major} ${major} PARENT_SCOPE)
  set(${mfx_version_minor} ${minor} PARENT_SCOPE)
endfunction()

set( CMAKE_LIB_DIR ${CMAKE_BINARY_DIR}/__bin )

# If user did not override CMAKE_INSTALL_PREFIX, then set the default prefix
# to /opt/intel/mediasdk instead of cmake's default
if( CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT )
  set( CMAKE_INSTALL_PREFIX /opt/intel/mediasdk CACHE PATH "Install Path Prefix" FORCE )
endif( )
message( STATUS "CMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}" )
 
include( GNUInstallDirs )

if( NOT DEFINED MFX_PLUGINS_CONF_DIR )
  set( MFX_PLUGINS_CONF_DIR ${CMAKE_INSTALL_FULL_DATADIR}/mfx )
endif( )

message( STATUS "MFX_PLUGINS_CONF_DIR=${MFX_PLUGINS_CONF_DIR}" )

if( NOT DEFINED MFX_MODULES_DIR )
  set( MFX_MODULES_DIR ${CMAKE_INSTALL_FULL_LIBDIR} )
endif( )
message( STATUS "MFX_MODULES_DIR=${MFX_MODULES_DIR}" )

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pipe -fPIC ${warnings} ${no_warnings}")

add_library(mfx SHARED 
  mfxloader.cpp
  mfxparser.cpp
)

target_compile_features(mfx PRIVATE cxx_std_11)

target_include_directories(mfx PUBLIC ${MFX_API_FOLDER})

target_link_libraries(mfx PRIVATE  ${CMAKE_DL_LIBS})

target_compile_definitions(mfx PRIVATE
  __USE_LARGEFILE64
  _FILE_OFFSET_BITS=64
  LINUX
  LINUX32
  $<$<EQUAL:${CMAKE_SIZEOF_VOID_P},8>:LINUX64>
  MFX_PLUGINS_CONF_DIR="${MFX_PLUGINS_CONF_DIR}"
  MFX_MODULES_DIR="${MFX_MODULES_DIR}"
  $<$<CONFIG:Debug>:_DEBUG>
  )

target_compile_options(mfx PRIVATE
  -pipe
  -Wall
  -Wformat
  -Wformat-security
  -Wno-unknown-pragmas
  -Wno-unused
  )

get_api_version(MFX_VERSION_MAJOR MFX_VERSION_MINOR)

target_link_options(mfx PRIVATE
  LINKER:--version-script=${CMAKE_CURRENT_SOURCE_DIR}/libmfx.map
  LINKER:--no-undefined,-z,relro,-z,now,-z,noexecstack
  -fstack-protector
  )

set_target_properties( mfx PROPERTIES POSITION_INDEPENDENT_CODE ON)
set_target_properties( mfx PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_LIB_DIR}/${CMAKE_BUILD_TYPE} FOLDER mfx )
set_target_properties( mfx PROPERTIES   VERSION ${MFX_VERSION_MAJOR}.${MFX_VERSION_MINOR})
set_target_properties( mfx PROPERTIES SOVERSION ${MFX_VERSION_MAJOR})

install(TARGETS mfx LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

set( PKG_CONFIG_FNAME "${CMAKE_LIB_DIR}/${CMAKE_BUILD_TYPE}/lib${PROJECT_NAME}.pc")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/pkg-config.pc.cmake" ${PKG_CONFIG_FNAME} @ONLY)

install( FILES ${PKG_CONFIG_FNAME} DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig )
install( DIRECTORY ${MFX_API_FOLDER}/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/mfx FILES_MATCHING PATTERN *.h )

# For backwards compatibility, create a relative symbolic link without the "lib"
# prefix to the .pc file.
set( PKG_CONFIG_LFNAME "${CMAKE_LIB_DIR}/${CMAKE_BUILD_TYPE}/${PROJECT_NAME}.pc" )
add_custom_target(pc_link_target ALL COMMAND ${CMAKE_COMMAND} -E create_symlink lib${PROJECT_NAME}.pc ${PKG_CONFIG_LFNAME})
install( FILES ${PKG_CONFIG_LFNAME} DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig )
