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

#
# Usage: check_variant( <variant> )
#  Terminates configuration process if specified build variant is not supported.
#
function( check_variant variant configured )
  if( NOT ${ARGV0} MATCHES none|sw|hw|drm|x11 )
    message( FATAL_ERROR "bug: unknown build variant (${ARGV0}) specified" )
  endif( )

  set( configured 0 )

  if( ${ARGV0} MATCHES none )
    set( configured 1 )
  elseif( ${ARGV0} MATCHES sw )
    if( Linux )
      set( configured 0 )
    else( )
      set( configured 1 )
    endif( )
  elseif( ${ARGV0} MATCHES hw AND Linux AND PKG_LIBVA_FOUND )
    set( configured 1 )
  elseif( ${ARGV0} MATCHES hw AND Darwin )
    set( configured 1 )
  elseif( ${ARGV0} MATCHES drm AND PKG_LIBDRM_FOUND AND PKG_LIBVA_FOUND AND PKG_LIBVA_DRM_FOUND )
    set( configured 1 )
  elseif( ${ARGV0} MATCHES x11 AND PKG_X11_FOUND AND PKG_LIBVA_FOUND AND PKG_LIBVA_X11_FOUND )
    set( configured 1 )
  elseif( ${ARGV0} MATCHES universal AND PKG_X11_FOUND AND PKG_LIBVA_FOUND AND PKG_LIBVA_X11_FOUND AND PKG_LIBDRM_FOUND AND PKG_LIBVA_DRM_FOUND )
    set( configured 1 )
  endif( )

  set( ${ARGV1} ${configured} PARENT_SCOPE )
endfunction( )

#
# Usage: append_property( <target> <property_name> <property>)
#  Appends settings to the target property.
#
function( append_property target property_name property )
  get_target_property( property ${ARGV0} ${ARGV1} )
  if( property MATCHES NOTFOUND)
    set( property "" )
  endif( )
  string( REPLACE ";" " " property "${ARGV2} ${property}" )
  set_target_properties( ${ARGV0} PROPERTIES ${ARGV1} "${property}" )
endfunction( )

#
# Usage: configure_build_variant( <target> <variant> )
#   Sets compilation and link flags for the specified target according to the
# specified variant.
#
function( configure_build_variant target variant )
  if( Linux )
    configure_build_variant_linux( ${ARGV0} ${ARGV1} )
  elseif( Darwin )
    configure_build_variant_darwin( ${ARGV0} ${ARGV1} )
  endif( )
endfunction( )

function( configure_build_variant_linux target variant )
  if( NOT Linux )
    return( ) # should not occur; just in case
  endif( )
  set( link_flags_list "-Wl,--no-undefined,-z,relro,-z,now,-z,noexecstack")
  append_property( ${ARGV0} LINK_FLAGS "${link_flags_list} ${MFX_LDFLAGS} -fstack-protector" )
#  message( STATUS "Libva located at: ${PKG_LIBVA_LIBRARY_DIRS}" )

  if( ARGV1 MATCHES hw AND Linux )
    append_property( ${ARGV0} COMPILE_FLAGS "-DMFX_VA" )
    append_property( ${ARGV0} COMPILE_FLAGS "${PKG_LIBVA_CFLAGS}" )
    foreach(libpath ${PKG_LIBVA_LIBRARY_DIRS} )
     append_property( ${ARGV0} LINK_FLAGS "-L${libpath}" )
    endforeach()
    #append_property( ${ARGV0} LINK_FLAGS "${PKG_LIBVA_LDFLAGS_OTHER}" )

    target_link_libraries( ${ARGV0} va ${MDF_LIBS} )

  elseif( ARGV1 MATCHES universal )

    append_property( ${ARGV0} COMPILE_FLAGS "-DMFX_VA -DLIBVA_SUPPORT -DLIBVA_DRM_SUPPORT -DLIBVA_X11_SUPPORT" )
    append_property( ${ARGV0} COMPILE_FLAGS "${PKG_LIBDRM_CFLAGS} ${PKG_LIBVA_CFLAGS} ${PKG_LIBVA_DRM_CFLAGS} ${PKG_LIBVA_X11_CFLAGS} ${PKG_X11_CFLAGS}" )

  elseif( ARGV1 MATCHES drm )
    append_property( ${ARGV0} COMPILE_FLAGS "-DLIBVA_SUPPORT -DLIBVA_DRM_SUPPORT" )
    append_property( ${ARGV0} COMPILE_FLAGS "${PKG_LIBDRM_CFLAGS} ${PKG_LIBVA_CFLAGS} ${PKG_LIBVA_DRM_CFLAGS}" )
    foreach(libpath ${PKG_LIBDRM_LIBRARY_DIRS} ${PKG_LIBVA_LIBRARY_DIRS} ${PKG_LIBVA_DRM_LIBRARY_DIRS})
      append_property( ${ARGV0} LINK_FLAGS "-L${libpath}" )
    endforeach()
    #append_property( ${ARGV0} LINK_FLAGS "${PKG_LIBDRM_LDFLAGS_OTHER} ${PKG_LIBVA_LDFLAGS_OTHER} ${PKG_LIBVA_DRM_LDFLAGS_OTHER}" )

    target_link_libraries( ${ARGV0} va drm va-drm )

  elseif( ARGV1 MATCHES x11 )
    append_property( ${ARGV0} COMPILE_FLAGS "-DLIBVA_SUPPORT -DLIBVA_X11_SUPPORT" )
    append_property( ${ARGV0} COMPILE_FLAGS "${PKG_LIBVA_CFLAGS} ${PKG_LIBVA_X11_CFLAGS} ${PKG_X11_CFLAGS}" )
    foreach(libpath ${PKG_LIBVA_LIBRARY_DIRS} ${PKG_LIBVA_X11_LIBRARY_DIRS} ${PKG_X11_LIBRARY_DIRS} )
      append_property( ${ARGV0} LINK_FLAGS "-L${libpath}" )
    endforeach()
    #append_property( ${ARGV0} LINK_FLAGS "${PKG_LIBVA_LDFLAGS_OTHER} ${PKG_LIBVA_X11_LDFLAGS_OTHER} ${PKG_X11_LDFLAGS_OTHER}" )

    target_link_libraries( ${ARGV0} va-x11 va X11 )

  elseif( ARGV1 MATCHES none OR ARGV1 MATCHES sw)
    # Multiple 'none' and 'sw' variants include cm_rt_linux.h. And
    # cm_rt_linux.h includes LIBVA headers unconditionally. We need to tell the
    # compiler where to find the LIBVA headers, especially if LIBVA is installed
    # in a custom location.
    append_property( ${ARGV0} COMPILE_FLAGS "${PKG_LIBVA_CFLAGS}" )
    foreach(libpath ${PKG_LIBVA_LIBRARY_DIRS} )
     append_property( ${ARGV0} LINK_FLAGS "-L${libpath}" )
    endforeach()

  endif( )
endfunction( )

function( configure_build_variant_darwin target variant )
  if( NOT Darwin)
    return( ) # should not occur; just in case
  endif( )

  if( ARGV1 MATCHES hw)
    list( APPEND darwin_includes
      /System/Library/Frameworks/CoreVideo.framework/Headers
      /System/Library/Frameworks/VideoToolbox.framework/Headers
      )
    list( APPEND darwin_frameworks
      CoreFoundation
      CoreMedia
      CoreVideo
      )

    append_property( ${ARGV0} COMPILE_FLAGS "-DMFX_VA" )
    
    foreach( item ${darwin_includes} )
      append_property( ${ARGV0} COMPILE_FLAGS "-I${item}" )
    endforeach()
    foreach( item ${darwin_frameworks} )
      append_property( ${ARGV0} LINK_FLAGS "-framework ${item}" )
    endforeach()
  endif( )

endfunction( )

if( Linux )
  find_package(PkgConfig REQUIRED)

  # required:
  pkg_check_modules(PKG_LIBDRM libdrm)
  pkg_check_modules(PKG_LIBVA libva>=0.33)
  pkg_check_modules(PKG_LIBVA_X11 libva-x11>=0.33)
  pkg_check_modules(PKG_LIBVA_DRM libva-drm>=0.33)
  pkg_check_modules(PKG_X11 x11)

endif( )

