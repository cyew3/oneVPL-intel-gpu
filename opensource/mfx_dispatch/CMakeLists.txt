cmake_minimum_required( VERSION 2.6.2 FATAL_ERROR )
project( mfx )

include( ../../builder/FindFunctions.cmake )
include( ../../builder/FindGlobals.cmake )
include( ../../builder/FindPackages.cmake )

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
  ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_plugin_hive_linux.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_load_plugin.cpp  
  )

make_library( mfx none static )

list(APPEND sources.plus
  ${CMAKE_CURRENT_SOURCE_DIR}/src/mfx_dispatcher_log.cpp
  )

set( defs "-DMFX_DISPATCHER_LOG -DDXVA2DEVICE_LOG" )
make_library( dispatch_trace none static )
