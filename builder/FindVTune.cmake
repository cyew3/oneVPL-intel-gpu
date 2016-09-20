##******************************************************************************
##  Copyright(C) 2015 Intel Corporation. All Rights Reserved.
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
##  Content: Intel(R) Media SDK Global Configuration of Targets Cmake module
##******************************************************************************

if(__ITT)
  if( Linux )
    if( __ARCH MATCHES intel64 )
      set( arch "64" )
    elseif()
      set( arch "32" )
    endif()

    if(0)
      # NOTE: that's a placeholder. Currently VTune installation can't be used directly.
      find_path( VTUNE_INCLUDE ittnotify.h PATHS $ENV{MFX_VTUNE_PATH}/include )
      find_library( VTUNE_LIBRARY libittnotify.a PATHS $ENV{MFX_VTUNE_PATH}/lib${arch}/ )

      if(NOT VTUNE_INCLUDE MATCHES NOTFOUND)
        if(NOT VTUNE_LIBRARY MATCHES NOTFOUND)
          set( VTUNE_FOUND TRUE )
          include_directories( ${VTUNE_INCLUDE} )

          get_filename_component( VTUNE_LIBRARY_PATH ${VTUNE_LIBRARY} PATH )

          link_directories( ${VTUNE_LIBRARY_PATH} )

        endif()
      endif()

      if(NOT DEFINED VTUNE_FOUND)
        message( FATAL_ERROR "ITT was not found! Set/check MFX_VTUNE_PATH environment variable!" )
      else ()
        message( STATUS "ITT was found here $ENV{MFX_VTUNE_PATH}" )
      endif()
    else()
      set( VTUNE_FOUND TRUE )
      message( STATUS "ITT was found here $ENV{MFX_HOME}/mdp_msdk-contrib/itt" )

      include_directories($ENV{MFX_HOME}/mdp_msdk-contrib/itt/include)
      link_directories( $ENV{MFX_HOME}/mdp_msdk-contrib/itt/lib${arch} )
    endif()

    append("-DMFX_TRACE_ENABLE_ITT" CMAKE_C_FLAGS)
    append("-DMFX_TRACE_ENABLE_ITT" CMAKE_CXX_FLAGS)

    set( ITT_LIBS "" )
    list( APPEND ITT_LIBS
      mfx_trace
      ittnotify
      dl
    )

  elseif( Darwin )
    message( STATUS "MFX tracing is unsupported on Darwin.")
  else()
    message( FATAL_ERROR "MFX tracing is supported only for linux!")
  endif()

endif()
