cmake_minimum_required( VERSION 3.16 FATAL_ERROR )

project(dispatcher_uwp LANGUAGES CXX)

if(NOT CMAKE_SYSTEM_NAME MATCHES WindowsStore)
  return()
endif()

if( NOT MFX_API_FOLDER )
  set( MFX_API_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}/../../include )
endif()

add_library(libmfx_uwp STATIC)

target_sources(libmfx_uwp
  PRIVATE
    ../../mfx_dispatch/windows/src/mfx_dispatcher.cpp
    ../../mfx_dispatch/windows/src/mfx_dispatcher_uwp.cpp
    ../../mfx_dispatch/windows/src/mfx_dispatcher_log.cpp
    ../../mfx_dispatch/windows/src/mfx_function_table.cpp
    ../../mfx_dispatch/windows/src/main.cpp
    ../../mfx_dispatch/windows/src/mfx_load_dll.cpp
    ../../mfx_dispatch/windows/src/mfx_driver_store_loader.cpp
    ../../mfx_dispatch/windows/src/mfx_dxva2_device.cpp
    ../../mfx_dispatch/windows/include/mfxaudio_exposed_functions_list.h
    ../../mfx_dispatch/windows/include/mfx_dispatcher.h
    ../../mfx_dispatch/windows/include/mfx_dispatcher_uwp.h
    ../../mfx_dispatch/windows/include/mfx_dispatcher_defs.h
    ../../mfx_dispatch/windows/include/mfx_dispatcher_log.h
    ../../mfx_dispatch/windows/include/mfx_exposed_functions_list.h
    ../../mfx_dispatch/windows/include/mfx_vector.h
    ../../mfx_dispatch/windows/include/mfx_load_dll.h
    ../../mfx_dispatch/windows/include/mfx_driver_store_loader.h
    ../../mfx_dispatch/windows/include/mfx_dxva2_device.h
)

target_link_libraries(libmfx_uwp PRIVATE ${CMAKE_DL_LIBS})
target_link_options(libmfx_uwp
  PRIVATE
    /DEBUG
    /PDB:$<TARGET_PDB_FILE_DIR:libmfx_uwp>/libmfx_uwp_full.pdb
    /PDBSTRIPPED:$<TARGET_PDB_FILE_DIR:libmfx_uwp>/libmfx_uwp.pdb    
)

target_compile_features(libmfx_uwp PRIVATE cxx_std_11)

target_compile_options(libmfx_uwp
  PRIVATE
    -W4
)

target_compile_options(libmfx_uwp
  PRIVATE
    /bigobj
    /wd4453
    /wd28204
    /ZW:nostdlib
    ### WA SINCE CMAKE EXPLICITLY INHERITED SOME SPECIFIC DEFAULT UWP VALUES.
    /UWINAPI_FAMILY=WINAPI_FAMILY_APP
)

target_compile_definitions(libmfx_uwp
  PRIVATE
    _LIB
    MEDIASDK_UWP_DISPATCHER
    MFX_D3D11_ENABLED
    _ALLOW_MSC_VER_MISMATCH
    _ALLOW_ITERATOR_DEBUG_LEVEL_MISMATCH
    _ALLOW_RUNTIME_LIBRARY_MISMATCH
    __WRL_NO_DEFAULT_LIB__
    PRI
    WINAPI_FAMILY=WINAPI_FAMILY_DESKTOP_APP
  PUBLIC
    _UNICODE
    UNICODE
)


target_include_directories(libmfx_uwp
  PUBLIC
    $ENV{MINIDDK_ROOT}/Include/um
    $ENV{MINIDDK_ROOT}/Include/shared
    $<BUILD_INTERFACE:${MFX_API_FOLDER}>
    $<INSTALL_INTERFACE:../../mfx_dispatch/windows/include>
  PRIVATE
    ../../mfx_dispatch/windows/include
)

target_link_directories(libmfx_uwp
  PUBLIC
    $ENV{MINIDDK_ROOT}/Lib/win8/um/x86
)

set_target_properties(libmfx_uwp PROPERTIES
    VS_WINDOWS_TARGET_PLATFORM_MIN_VERSION "${VS_WINDOWS_TARGET_PLATFORM_MIN_VERSION}"
    PDB_OUTPUT_DIR ${CMAKE_BINARY_DIR}
)
