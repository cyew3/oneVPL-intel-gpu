##******************************************************************************
## Copyright (C) 2016 Intel Corporation.  All rights reserved.
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

cmake_minimum_required( VERSION 2.6.2 FATAL_ERROR )
project( mfx )

set( CMAKE_VERBOSE_MAKEFILE             TRUE )
set( CMAKE_LIB_DIR ${CMAKE_BINARY_DIR}/__lib )

if( CMAKE_SYSTEM_NAME MATCHES Windows )
  add_definitions(-DMINGW_HAS_SECURE_API -DWIN32)

  if(__ARCH MATCHES intel64)
    add_definitions(-DWIN64)
  endif( )
else( )

  # If user did not override CMAKE_INSTALL_PREFIX, then set the default prefix
  # to /opt/intel/mediasdk instead of cmake's default
  if( CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT )
    set( CMAKE_INSTALL_PREFIX /opt/intel/mediasdk CACHE PATH "Install Path Prefix" FORCE )
  endif( )
  message( STATUS "CMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}" )

  if( NOT DEFINED MFX_PLUGINS_DIR )
    set( MFX_PLUGINS_DIR ${CMAKE_INSTALL_PREFIX}/plugins )
  endif( )
  add_definitions( -DMFX_PLUGINS_DIR="${MFX_PLUGINS_DIR}" )
  message( STATUS "MFX_PLUGINS_DIR=${MFX_PLUGINS_DIR}" )

  if( NOT DEFINED MFX_MODULES_DIR )
    if(__ARCH MATCHES intel64)
        set( MFX_MODULES_DIR ${CMAKE_INSTALL_PREFIX}/lib64 )
    else( )
        set( MFX_MODULES_DIR ${CMAKE_INSTALL_PREFIX}/lib )
    endif( )
  endif( )
  add_definitions( -DMFX_MODULES_DIR="${MFX_MODULES_DIR}" )
  message( STATUS "MFX_MODULES_DIR=${MFX_MODULES_DIR}" )

  add_definitions(-DUNIX)

  if( ${CMAKE_SYSTEM_NAME} MATCHES Linux )
    add_definitions(-D__USE_LARGEFILE64 -D_FILE_OFFSET_BITS=64 -DLINUX -DLINUX32)

    if(__ARCH MATCHES intel64)
      add_definitions(-DLINUX64)
    endif( )
  endif( )

  if( ${CMAKE_SYSTEM_NAME} MATCHES Darwin )
    add_definitions(-DOSX)
    add_definitions(-DOSX32)

    if(__ARCH MATCHES intel64)
      add_definitions(-DOSX64)
    endif( )
  endif( )

  set(no_warnings "-Wno-unknown-pragmas -Wno-unused")

  set(CMAKE_CXX_FLAGS "-pipe -fPIC")
  set(CMAKE_CXX_FLAGS_DEBUG   "-O0 -Wall ${no_warnings} -Wformat -Wformat-security -g -D_DEBUG")
  set(CMAKE_CXX_FLAGS_RELEASE "-O2 -D_FORTIFY_SOURCE=2 -fstack-protector -Wall ${no_warnings} -Wformat -Wformat-security  -DNDEBUG")

  if (DEFINED CMAKE_FIND_ROOT_PATH)
    append("--sysroot=${CMAKE_FIND_ROOT_PATH} " LINK_FLAGS)
  endif (DEFINED CMAKE_FIND_ROOT_PATH)
endif( )

include_directories (
  ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

list(APPEND sources
  ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_critical_section.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_critical_section_linux.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_dispatcher.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_function_table.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_library_iterator.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_library_iterator_linux.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_load_dll.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_load_dll_linux.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_win_reg_key.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_dxva2_device.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_plugin_hive.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_plugin_hive_linux.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_plugin_cfg_parser.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_load_plugin.cpp
  )

function( append_property target property_name property )
  get_target_property( property ${ARGV0} ${ARGV1} )
  if( property MATCHES NOTFOUND)
    set( property "" )
  endif( )
  string( REPLACE ";" " " property "${ARGV2} ${property}" )
  set_target_properties( ${ARGV0} PROPERTIES ${ARGV1} "${property}" )
endfunction( )

function( make_static_library name )
  if( sources.plus )
    list( APPEND sources ${sources.plus} )
  endif( )

  set( target ${ARGV0} )

  add_library( ${target} STATIC ${include} ${sources} )

  set( link_flags_list "-Wl,--no-undefined,-z,relro,-z,now,-z,noexecstack")
  append_property( ${target} LINK_FLAGS "${link_flags_list} -fstack-protector" )

  if( defs )
    append_property( ${target} COMPILE_FLAGS ${defs} )
  endif( )

  set_target_properties( ${target} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_LIB_DIR}/${CMAKE_BUILD_TYPE} FOLDER ${target} )
endfunction( )

set( defs "${WARNING_FLAGS}" )
make_static_library( mfx )

if(__ARCH MATCHES intel64)
  install( TARGETS mfx ARCHIVE DESTINATION lib64 )
else( )
  install( TARGETS mfx ARCHIVE DESTINATION lib )
endif( )

install( DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../include/ DESTINATION include FILES_MATCHING PATTERN *.h )

set( defs "-DMFX_DISPATCHER_EXPOSED_PREFIX ${WARNING_FLAGS}" )
make_static_library( dispatch_shared )

list(APPEND sources.plus
  ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_dispatcher_log.cpp
  )

set( defs "-DMFX_DISPATCHER_LOG -DDXVA2DEVICE_LOG ${WARNING_FLAGS}" )
make_static_library( dispatch_trace )

get_mfx_version(MFX_VERSION_MAJOR MFX_VERSION_MINOR)
set( PKG_CONFIG_FNAME "${CMAKE_LIB_DIR}/${CMAKE_BUILD_TYPE}/${PROJECT_NAME}.pc")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/pkg-config.pc.cmake" ${PKG_CONFIG_FNAME} @ONLY)

if(__ARCH MATCHES intel64)
  install( TARGETS mfx ARCHIVE DESTINATION lib/lin_x64 )
  install( FILES ${PKG_CONFIG_FNAME} DESTINATION lib/lin_x64/pkgconfig )
else( )
  install( TARGETS mfx ARCHIVE DESTINATION lib/lin_x86 )
  install( FILES ${PKG_CONFIG_FNAME} DESTINATION lib/lin_x86/pkgconfig )
endif( )
install( DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../include/ DESTINATION include FILES_MATCHING PATTERN *.h )
