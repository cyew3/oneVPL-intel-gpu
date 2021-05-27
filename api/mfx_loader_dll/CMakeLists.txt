if(NOT CMAKE_SYSTEM_NAME MATCHES Windows OR NOT BUILD_EXTRA)
  return()
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(arch_postfix 64)
else()
  set(arch_postfix 32)
endif()

set(loader_dll_name mfx_loader_dll_hw${arch_postfix})

add_library(${loader_dll_name} SHARED)

target_include_directories(${loader_dll_name}
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../mfx_dispatch/windows/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_sources(${loader_dll_name}
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/resource.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mfx_loader_dll_hw.rc
    ${CMAKE_CURRENT_SOURCE_DIR}/../mfx_dispatch/windows/src/mfx_dispatcher_log.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../mfx_dispatch/windows/src/mfx_load_dll.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../mfx_dispatch/windows/src/mfx_dxva2_device.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../_studio/mfx_lib/libmfx.def
)

target_link_libraries(${loader_dll_name}
  PRIVATE
    mfx_static_lib
    mfx_shared_lib
    mfx_sdl_properties
    cfgmgr32
)

target_compile_definitions(${loader_dll_name}
  PRIVATE
    $<$<CONFIG:Debug>:MFX_DISPATCHER_LOG>
)

target_link_options(${loader_dll_name}
  PRIVATE
    $<$<PLATFORM_ID:Windows>:LINKER:/DEF:${CMAKE_CURRENT_SOURCE_DIR}/../../_studio/mfx_lib/libmfx.def>
)