##******************************************************************************
##  Copyright(C) 2012-2020 Intel Corporation. All Rights Reserved.
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

if (CMAKE_SIZEOF_VOID_P EQUAL 8)
  set( ipp_ts_arch em64t )
else()
  set( ipp_ts_arch 32 )
endif()

set( ipp_ts_root $ENV{MEDIASDK_ROOT}/tools/ts )
if( Windows )
  set( ipp_ts_lib ${ipp_ts_root}/lib/win${ipp_ts_arch} )
  set( lib_suffix ".lib" )
  set( lib_prefix "" )
elseif( Linux )
  set( ipp_ts_lib ${ipp_ts_root}/lib/linux${ipp_ts_arch} )
  set( lib_suffix ".a" )
  set( lib_prefix "lib" )
endif()


if ( Windows )
  set( IPP_TS_LIBS vc2015 vc2015_debug)
else()
  set( IPP_TS_LIBS gcc345 gcc345_debug)
endif()

find_path( IPPTS_INCLUDE ts.h PATHS ${ipp_ts_root}/include )

message( STATUS "Search Intel(R) IPP TS in ${ipp_ts_root}!" )

if(NOT IPPTS_INCLUDE MATCHES NOTFOUND)
  set( IPPTS_FOUND TRUE)
  foreach(ipp_ts_libname ${IPP_TS_LIBS})

    if(${ipp_ts_libname} MATCHES debug)
      set(postfix "_d")
    endif()

    add_library(IPPTS::ts${postfix} STATIC IMPORTED)
    set_target_properties(IPPTS::ts${postfix} PROPERTIES IMPORTED_LOCATION "${ipp_ts_lib}/${lib_prefix}ts_${ipp_ts_libname}${lib_suffix}")
    target_include_directories(IPPTS::ts${postfix} SYSTEM INTERFACE ${IPPTS_INCLUDE})
    target_compile_options(IPPTS::ts${postfix} 
      INTERFACE 
        $<$<CXX_COMPILER_ID:MSVC>:-wd4105>
    )
  endforeach()
endif()

if(NOT DEFINED IPPTS_FOUND)
  message( STATUS "Intel(R) IPP TS was not found (optional)!" )
else()
  message( STATUS "Intel(R) IPP TS was found here ${ipp_ts_lib}" )
endif()
