##******************************************************************************
##  Copyright(C) 2012 Intel Corporation. All Rights Reserved.
##  
##  The source code, information  and  material ("Material") contained herein is
##  owned  by Intel Corporation or its suppliers or licensors, and title to such
##  Material remains  with Intel Corporation  or its suppliers or licensors. The
##  Material  contains proprietary information  of  Intel or  its  suppliers and
##  licensors. The  Material is protected by worldwide copyright laws and treaty
##  provisions. No  part  of  the  Material  may  be  used,  copied, reproduced,
##  modified, published, uploaded, posted, transmitted, distributed or disclosed
##  in any way  without Intel's  prior  express written  permission. No  license
##  under  any patent, copyright  or  other intellectual property rights  in the
##  Material  is  granted  to  or  conferred  upon  you,  either  expressly,  by
##  implication, inducement,  estoppel or  otherwise.  Any  license  under  such
##  intellectual  property  rights must  be express  and  approved  by  Intel in
##  writing.
##  
##  *Third Party trademarks are the property of their respective owners.
##  
##  Unless otherwise  agreed  by Intel  in writing, you may not remove  or alter
##  this  notice or  any other notice embedded  in Materials by Intel or Intel's
##  suppliers or licensors in any way.
##
##******************************************************************************
##  Content: Intel(R) Media SDK Samples projects creation and build
##******************************************************************************

set_property( GLOBAL PROPERTY USE_FOLDERS ON )
set( CMAKE_VERBOSE_MAKEFILE             TRUE )

# the following options should disable rpath in both build and install cases
set( CMAKE_INSTALL_RPATH "" )
set( CMAKE_BUILD_WITH_INSTALL_RPATH TRUE )
set( CMAKE_SKIP_BUILD_RPATH TRUE )

collect_oses( )
collect_arch( )

if( Windows )
  message( FATAL_ERROR "Windows is not currently supported!" )

else( )
  add_definitions(-DUNIX)

  if( Linux )
    add_definitions(-D__USE_LARGEFILE64 -D_FILE_OFFSET_BITS=64)

    add_definitions(-DLINUX)
    add_definitions(-DLINUX32)

    if(__ARCH MATCHES intel64)
      add_definitions(-DLINUX64)
    endif( )
  endif( )

  if( Darwin )
    add_definitions(-DOSX)
    add_definitions(-DOSX32)

    if(__ARCH MATCHES intel64)
      add_definitions(-DOSX64)
    endif( )
  endif( )

  if( NOT DEFINED ENV{MFX_VERSION} )
    set( version 0.0.000.0000 )
  else( )
    set( version $ENV{MFX_VERSION} )
  endif( )

  if( Linux OR Darwin )
    execute_process(
      COMMAND echo
      COMMAND cut -f 1 -d.
      COMMAND date "+.%-y.%-m.%-d"
      OUTPUT_VARIABLE cur_date
      OUTPUT_STRIP_TRAILING_WHITESPACE
      )
    string( SUBSTRING ${version} 0 1 ver )

    add_definitions( -DMFX_FILE_VERSION=\"${ver}${cur_date}\")
    add_definitions( -DMFX_PRODUCT_VERSION=\"${version}\" )
  endif( )

  set(no_warnings "-Wno-deprecated-declarations -Wno-unknown-pragmas -Wno-unused")
  
  set(CMAKE_C_FLAGS "-pipe -fPIC")
  set(CMAKE_CXX_FLAGS "-pipe -fPIC")
  set(CMAKE_C_FLAGS_DEBUG     "-O0 -Wall ${no_warnings}  -Wformat -Wformat-security -g -D_DEBUG" CACHE STRING "" FORCE)
  set(CMAKE_C_FLAGS_RELEASE   "-O2 -D_FORTIFY_SOURCE=2 -fstack-protector -Wall ${no_warnings} -Wformat -Wformat-security -DNDEBUG"    CACHE STRING "" FORCE)
  set(CMAKE_CXX_FLAGS_DEBUG   "-O0 -Wall ${no_warnings}  -Wformat -Wformat-security -g -D_DEBUG" CACHE STRING "" FORCE)
  set(CMAKE_CXX_FLAGS_RELEASE "-O2 -D_FORTIFY_SOURCE=2 -fstack-protector -Wall ${no_warnings}  -Wformat -Wformat-security -DNDEBUG"    CACHE STRING "" FORCE)

  if ( Darwin )
    if (CMAKE_C_COMPILER MATCHES clang)
       set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -v -std=c++11 -stdlib=libc++")
    endif()
  endif()

  if (DEFINED CMAKE_FIND_ROOT_PATH)
#    append("--sysroot=${CMAKE_FIND_ROOT_PATH} " CMAKE_C_FLAGS)
#    append("--sysroot=${CMAKE_FIND_ROOT_PATH} " CMAKE_CXX_FLAGS)
    append("--sysroot=${CMAKE_FIND_ROOT_PATH} " LINK_FLAGS)
  endif (DEFINED CMAKE_FIND_ROOT_PATH)
  
  if(__ARCH MATCHES ia32)
    append("-m32 -g" CMAKE_C_FLAGS)
    append("-m32 -g" CMAKE_CXX_FLAGS)
    append("-m32 -g" LINK_FLAGS)
  else ( )
    append("-m64 -g" CMAKE_C_FLAGS)
    append("-m64 -g" CMAKE_CXX_FLAGS)
    append("-m64 -g" LINK_FLAGS)
  endif( )

  if(__ARCH MATCHES ia32)
    link_directories(/usr/lib)
    set(MFX_MODULES_DIR ${CMAKE_INSTALL_PREFIX}/lib)
  else ( )
    link_directories(/usr/lib64)
    set(MFX_MODULES_DIR ${CMAKE_INSTALL_PREFIX}/lib64)
  endif( )
endif( )
