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
##  Content: Intel(R) Media SDK projects creation and build
##******************************************************************************

if (CMAKE_SIZEOF_VOID_P EQUAL 8)
  set( ipp_arch em64t )
else()
  set( ipp_arch ia32)
endif()

if (NOT MFX_BUNDLED_IPP)

  set( ipp_root $ENV{MEDIASDK_ROOT}/ipp )
  if( Windows )
    set( ipp_root ${ipp_root}/ipp81goldGuard/windows/${ipp_arch} )
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
      set( lib_suffix "_s_y8.lib" )
    else()
      set( lib_suffix "_s_p8.lib" )
    endif()
    set( lib_prefix "" )
  elseif( Linux )
    set( ipp_root ${ipp_root}/linux/${ipp_arch} )
    set( lib_suffix "_l.a" )
    set( lib_prefix "lib" )

  endif()

  message( STATUS "Search IPP in ${ipp_root}" )

  find_path( IPP_INCLUDE ippcore.h PATHS ${ipp_root}/include )
  if( Windows )
    find_library( IPP_LIBRARY ippcoremt PATHS ${ipp_root}/lib )
  else()
    find_library( IPP_LIBRARY ippcore_l PATHS ${ipp_root}/lib )
  endif()

  if ( Windows )
    set( IPP_LIBRRARY_LIST s vc i cc cv j msdk ) # core/dc/vm use different suffix 'mt'
  else()
    set( IPP_LIBRRARY_LIST dc core vm s cp vc i cc cv j msdk )
  endif()

  if(NOT IPP_INCLUDE MATCHES NOTFOUND)
    if(NOT IPP_LIBRARY MATCHES NOTFOUND)
      set( IPP_FOUND TRUE )

      get_filename_component( IPP_LIBRARY_PATH ${IPP_LIBRARY} PATH )
    
      foreach (lib_name ${IPP_LIBRRARY_LIST})
        add_library(IPP::${lib_name} STATIC IMPORTED)
        set_target_properties(IPP::${lib_name} PROPERTIES IMPORTED_LOCATION "${ipp_root}/lib/${lib_prefix}ipp${lib_name}${lib_suffix}")
        target_include_directories(IPP::${lib_name} SYSTEM INTERFACE ${ipp_root}/include)
        target_compile_definitions(IPP::${lib_name} INTERFACE MSDK_USE_EXTERNAL_IPP)

        if ( __IPP AND Linux )
          target_sources( IPP::${lib_name} INTERFACE "${ipp_root}/tools/staticlib/ipp_${__IPP}.h" )
        endif()
      endforeach()

      if ( Windows )
        foreach (lib_name vm core)
          add_library(IPP::${lib_name} STATIC IMPORTED)
          set_target_properties(IPP::${lib_name} PROPERTIES IMPORTED_LOCATION "${ipp_root}/lib/ipp${lib_name}mt.lib")
          target_include_directories(IPP::${lib_name} SYSTEM INTERFACE ${ipp_root}/include)
          target_compile_definitions(IPP::${lib_name} INTERFACE MSDK_USE_EXTERNAL_IPP)
        endforeach()

        foreach (lib_name dc cp)
          add_library(IPP::${lib_name} STATIC IMPORTED)
          set_target_properties(IPP::${lib_name} PROPERTIES IMPORTED_LOCATION $ENV{MEDIASDK_ROOT}/ipp/ipp81gold/windows/${ipp_arch}/lib/ipp${lib_name}mt.lib)
          target_include_directories(IPP::${lib_name} SYSTEM INTERFACE $ENV{MEDIASDK_ROOT}/ipp/ipp81gold/windows/${ipp_arch}/include)
          target_compile_definitions(IPP::${lib_name} INTERFACE MSDK_USE_EXTERNAL_IPP)
        endforeach()
        link_directories( ${IPP_LIBRARY_PATH} )  # w/a for erroneus dependency on svml_disp.lib in IPP 8.1
      
      endif()
    endif()
  endif()

  if(NOT DEFINED IPP_FOUND)
    message( FATAL_ERROR "Intel(R) IPP was not found (required)! Set/check MEDIASDK_ROOT environment variable!" )
  else ()
    message( STATUS "Intel(R) IPP was found here $ENV{MEDIASDK_ROOT}" )
  endif()


else()
  set( IPP_FOUND TRUE )
  # set( IPP_INCLUDE ${CMAKE_HOME_DIRECTORY}/contrib/ipp/include )

  message( STATUS "Using built-in subset of Intel(R) IPP" )

  #include_directories( ${IPP_INCLUDE} )

  add_subdirectory(${CMAKE_HOME_DIRECTORY}/contrib/ipp)

  foreach(lib_name dc core vm s cp vc i cc cv j msdk)
    add_library(IPP::${lib_name} ALIAS ipp)
  endforeach()
  
endif()

