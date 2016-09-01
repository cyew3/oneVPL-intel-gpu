##******************************************************************************
##  Copyright(C) 2012-2016 Intel Corporation. All Rights Reserved.
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

set( CMAKE_LIB_DIR ${CMAKE_BINARY_DIR}/__lib )
set( CMAKE_BIN_DIR ${CMAKE_BINARY_DIR}/__bin )
set_property( GLOBAL PROPERTY PROP_PLUGINS_CFG "" )
set_property( GLOBAL PROPERTY PROP_PLUGINS_EVAL_CFG "" )

function( collect_arch )
  if(__ARCH MATCHES ia32)
    set( ia32 true PARENT_SCOPE )
    set( CMAKE_OSX_ARCHITECTURES i386 PARENT_SCOPE )
  else ()
    set( intel64 true PARENT_SCOPE )
    set( CMAKE_OSX_ARCHITECTURES x86_64 PARENT_SCOPE )
  endif()
endfunction()

# .....................................................
function( collect_oses )
  if( ${CMAKE_SYSTEM_NAME} MATCHES Windows )
    set( Windows    true PARENT_SCOPE )
    set( NotLinux   true PARENT_SCOPE )
    set( NotDarwin  true PARENT_SCOPE )

  elseif( ${CMAKE_SYSTEM_NAME} MATCHES Linux )
    set( Linux      true PARENT_SCOPE )
    set( NotDarwin  true PARENT_SCOPE )
    set( NotWindows true PARENT_SCOPE )

  elseif( ${CMAKE_SYSTEM_NAME} MATCHES Darwin )
    set( Darwin     true PARENT_SCOPE )
    set( NotLinux   true PARENT_SCOPE )
    set( NotWindows true PARENT_SCOPE )

  endif()
endfunction()

# .....................................................
function( append what where )
  set(${ARGV1} "${ARGV0} ${${ARGV1}}" PARENT_SCOPE)
endfunction()

# .....................................................
function( create_build )
  file( GLOB_RECURSE components "${CMAKE_SOURCE_DIR}/*/CMakeLists.txt" )
  foreach( component ${components} )
    get_filename_component( path ${component} PATH )
    add_subdirectory( ${path} )
  endforeach()
endfunction()

# .....................................................
function( get_source include sources)
  file( GLOB_RECURSE include "[^.]*.h" )
  file( GLOB_RECURSE sources "[^.]*.c" "[^.]*.cpp" )

  set( ${ARGV0} ${include} PARENT_SCOPE )
  set( ${ARGV1} ${sources} PARENT_SCOPE )
endfunction()

#
# Usage: get_target(target name none|<variant>)
#
function( get_target target name variant)
  if( ARGV1 MATCHES shortname )
    get_filename_component( tname ${CMAKE_CURRENT_SOURCE_DIR} NAME )
  else()
    set( tname ${ARGV1} )
  endif()
  if( ARGV2 MATCHES none OR ARGV2 MATCHES universal OR DEFINED USE_STRICT_NAME)
    set( target ${tname} )
  else()
   set( target ${tname}_${ARGV2} )
  endif()

  set( ${ARGV0} ${target} PARENT_SCOPE )
endfunction()

# .....................................................
function( get_folder folder )
  set( folder ${CMAKE_PROJECT_NAME} )
  set (${ARGV0} ${folder} PARENT_SCOPE)
endfunction()

function( gen_plugins_cfg plugin_id guid plugin_name type codecID )
  file(STRINGS $ENV{MFX_HOME}/mdp_msdk-api/include/mfxvideo.h MFX_VERSION_MAJOR REGEX "#define MFX_VERSION_MAJOR")
  file(STRINGS $ENV{MFX_HOME}/mdp_msdk-api/include/mfxvideo.h MFX_VERSION_MINOR REGEX "#define MFX_VERSION_MINOR")
  string(REPLACE "#define MFX_VERSION_MAJOR " "" MFX_VERSION_MAJOR ${MFX_VERSION_MAJOR})
  string(REPLACE "#define MFX_VERSION_MINOR " "" MFX_VERSION_MINOR ${MFX_VERSION_MINOR})
  math(EXPR api_version "${MFX_VERSION_MAJOR}*256 + ${MFX_VERSION_MINOR}")

  if((NOT DEFINED ARGV5) OR (ARGV5 STREQUAL "eval"))
    get_property( PLUGINS_EVAL_CFG GLOBAL PROPERTY PROP_PLUGINS_EVAL_CFG )
    set(PLUGINS_EVAL_CFG "${PLUGINS_EVAL_CFG}[${plugin_id}_${guid}]\n")
    set(PLUGINS_EVAL_CFG "${PLUGINS_EVAL_CFG}GUID = ${guid}\n")
    set(PLUGINS_EVAL_CFG "${PLUGINS_EVAL_CFG}PluginVersion = 1\n")
    set(PLUGINS_EVAL_CFG "${PLUGINS_EVAL_CFG}APIVersion = ${api_version}\n")
    set(PLUGINS_EVAL_CFG "${PLUGINS_EVAL_CFG}Path = ${MFX_PLUGINS_DIR}/lib${plugin_name}.so\n")
    set(PLUGINS_EVAL_CFG "${PLUGINS_EVAL_CFG}Type = ${type}\n")
    set(PLUGINS_EVAL_CFG "${PLUGINS_EVAL_CFG}CodecID = ${codecID}\n")
    set(PLUGINS_EVAL_CFG "${PLUGINS_EVAL_CFG}Default = 0\n")
    set_property( GLOBAL PROPERTY PROP_PLUGINS_EVAL_CFG ${PLUGINS_EVAL_CFG} )
  endif()

  if((NOT DEFINED ARGV5) OR (ARGV5 STREQUAL "not_eval"))
    get_property( PLUGINS_CFG GLOBAL PROPERTY PROP_PLUGINS_CFG )
    set(PLUGINS_CFG "${PLUGINS_CFG}[${plugin_id}_${guid}]\n")
    set(PLUGINS_CFG "${PLUGINS_CFG}GUID = ${guid}\n")
    set(PLUGINS_CFG "${PLUGINS_CFG}PluginVersion = 1\n")
    set(PLUGINS_CFG "${PLUGINS_CFG}APIVersion = ${api_version}\n")
    set(PLUGINS_CFG "${PLUGINS_CFG}Path = ${MFX_PLUGINS_DIR}/lib${plugin_name}.so\n")
    set(PLUGINS_CFG "${PLUGINS_CFG}Type = ${type}\n")
    set(PLUGINS_CFG "${PLUGINS_CFG}CodecID = ${codecID}\n")
    set(PLUGINS_CFG "${PLUGINS_CFG}Default = 0\n")
    set_property( GLOBAL PROPERTY PROP_PLUGINS_CFG ${PLUGINS_CFG} )
  endif()

endfunction()

function( create_plugins_cfg directory )
  get_property( PLUGINS_CFG GLOBAL PROPERTY PROP_PLUGINS_CFG )
  get_property( PLUGINS_EVAL_CFG GLOBAL PROPERTY PROP_PLUGINS_EVAL_CFG )
  file(WRITE ${directory}/plugins.cfg ${PLUGINS_CFG})
  file(WRITE ${directory}/plugins_eval.cfg ${PLUGINS_EVAL_CFG})

  install( FILES ${directory}/plugins.cfg ${directory}/plugins_eval.cfg DESTINATION ${MFX_PLUGINS_DIR} )

endfunction()

#
# Usage:
#  make_library(shortname|<name> none|<variant> static|shared)
#    - shortname|<name>: use folder name as library name or <name> specified by user
#    - <variant>|none: build library in specified variant (with drm or x11 or wayland support, etc),
#      universal - special variant which enables compilation flags required for all backends, but
#      moves dependency to runtime instead of linktime
#    or without variant if none was specified
#    - static|shared: build static or shared library
#
function( make_library name variant type )
  if( NOT sources )
   get_source( include sources )
  endif()

  if( sources.plus )
    list( APPEND sources ${sources.plus} )
  endif()

  get_target( target ${ARGV0} ${ARGV1} )
  if( ${ARGV0} MATCHES shortname )
    get_folder( folder )
  else ()
    set( folder ${ARGV0} )
  endif()

  if( ARGV2 MATCHES static )
    add_library( ${target} STATIC ${include} ${sources} )

  elseif( ARGV2 MATCHES shared )
    add_library( ${target} SHARED ${include} ${sources} )

    if( Linux )
      target_link_libraries( ${target} "-Xlinker --start-group" )
    endif()

    foreach( lib ${LIBS_VARIANT} )
      if(ARGV1 MATCHES none OR ARGV1 MATCHES universal)
        add_dependencies( ${target} ${lib} )
        target_link_libraries( ${target} ${lib} )
      else()
        add_dependencies( ${target} ${lib}_${ARGV1} )
        target_link_libraries( ${target} ${lib}_${ARGV1} )
      endif()
    endforeach()

    foreach( lib ${LIBS_NOVARIANT} )
      add_dependencies( ${target} ${lib} )
      target_link_libraries( ${target} ${lib} )
    endforeach()

    foreach( lib ${LIBS} )
      target_link_libraries( ${target} ${lib} )
    endforeach()

    set_target_properties( ${target} PROPERTIES LINK_INTERFACE_LIBRARIES "" )
  endif()

  configure_build_variant( ${target} ${ARGV1} )

  target_link_libraries( ${target} SafeString )

  if( defs )
    append_property( ${target} COMPILE_FLAGS ${defs} )
  endif()
  set_target_properties( ${target} PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BIN_DIR}/${CMAKE_BUILD_TYPE} FOLDER ${folder} )
  set_target_properties( ${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BIN_DIR}/${CMAKE_BUILD_TYPE} FOLDER ${folder} )
  set_target_properties( ${target} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_LIB_DIR}/${CMAKE_BUILD_TYPE} FOLDER ${folder} )

  if( Linux )
    target_link_libraries( ${target} "-Xlinker --end-group" )
  endif()


  set( target ${target} PARENT_SCOPE )
endfunction()

# .....................................................
function( make_executable name variant )
  if( NOT sources )
    get_source( include sources )
  endif()

  if( sources.plus )
    list( APPEND sources ${sources.plus} )
  endif()

  get_target( target ${ARGV0} ${ARGV1} )
  get_folder( folder )

  project( ${target} )

  add_executable( ${target} ${include} ${sources} )

  if( defs )
    append_property( ${target} COMPILE_FLAGS ${defs} )
  endif()
  set_target_properties( ${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BIN_DIR}/${CMAKE_BUILD_TYPE} FOLDER ${folder} )

  if( Linux )
    target_link_libraries( ${target} "-Xlinker --start-group" )
  endif()

  foreach( lib ${LIBS_VARIANT} )
    if(ARGV1 MATCHES none OR ARGV1 MATCHES universal)
      add_dependencies( ${target} ${lib} )
      target_link_libraries( ${target} ${lib} )
    else()
      add_dependencies( ${target} ${lib}_${ARGV1} )
      target_link_libraries( ${target} ${lib}_${ARGV1} )
    endif()
  endforeach()

  foreach( lib ${LIBS_NOVARIANT} )
    add_dependencies( ${target} ${lib} )
    target_link_libraries( ${target} ${lib} )
  endforeach()

  if( ${NEED_DISPATCHER} )
    target_link_libraries( ${target} debug mfx_d )
    target_link_libraries( ${target} optimized mfx )
  endif()

  foreach( lib ${LIBS})
    target_link_libraries( ${target} ${lib} )
  endforeach()

  configure_build_variant( ${target} ${ARGV1} )

  foreach( lib ${LIBS_SUFFIX} )
    target_link_libraries( ${target} ${lib} )
  endforeach()

  target_link_libraries( ${target} SafeString )

  if( Linux )
    target_link_libraries( ${target} "-Xlinker --end-group" )
  endif()

  set( target ${target} PARENT_SCOPE )
endfunction()

function( set_file_and_product_version input_version version_defs )
  if( Linux OR Darwin )
    execute_process(
      COMMAND echo
      COMMAND cut -f 1 -d.
      COMMAND date "+.%-y.%-m.%-d"
      OUTPUT_VARIABLE cur_date
      OUTPUT_STRIP_TRAILING_WHITESPACE
      )
    string( SUBSTRING ${input_version} 0 1 ver )

    set( version_defs " -DMFX_PLUGIN_FILE_VERSION=\"\\\"${ver}${cur_date}\"\\\""
                  " -DMFX_PLUGIN_PRODUCT_VERSION=\"\\\"${input_version}\"\\\""
                  PARENT_SCOPE )
  endif()
endfunction()
