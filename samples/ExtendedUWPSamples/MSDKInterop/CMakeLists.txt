if(NOT CMAKE_SYSTEM_NAME MATCHES WindowsStore)
  return()
endif()

add_library(MSDKInterop SHARED)

target_include_directories(MSDKInterop
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/vm
)

target_sources(MSDKInterop
  PRIVATE
    base_allocator.h
    d3d11_allocator.h
    d3d11_device.h
    DecodingPipeline.h
    EncoderInterop.h
    EncodingPipeline.h
    extras/FFMPEGBitstreamWriter.h
    hw_device.h
    MfxFrameSurfaceExt.h
    mfx_itt_trace.h
    MSDKHandle.h
    DecoderInterop.h
    MSDKTime.h
    pch.h
    sample_defs.h
    sample_types.h
    simplebitstreamreader.h
    SimpleYUVReader.h
    StreamWriter.h
    surfacespool.h
    SysMemAllocator.h
    targetver.h
    Thread11.h
    vm/atomic_defs.h
    vm/file_defs.h
    vm/so_defs.h
    vm/strings_defs.h
    vm/thread_defs.h
    vm/time_defs.h

    base_allocator.cpp
    d3d11_allocator.cpp
    d3d11_device.cpp
    DecodingPipeline.cpp
    dllmain.cpp
    DecoderInterop.cpp
    EncoderInterop.cpp
    EncodingPipeline.cpp
    extras/FFMPEGBitstreamWriter.cpp
    pch.cpp
    SimpleBitstreamReader.cpp
    SimpleYUVReader.cpp
    StreamWriter.cpp
    SurfacesPool.cpp
    SysMemAllocator.cpp
    Thread11.cpp
)


set_source_files_properties(pch.cpp PROPERTIES COMPILE_FLAGS /Yc)
target_compile_options(MSDKInterop PRIVATE /Yu /ZW /EHsc)

set_target_properties(MSDKInterop PROPERTIES VS_WINDOWS_TARGET_PLATFORM_MIN_VERSION "${VS_WINDOWS_TARGET_PLATFORM_MIN_VERSION}")

target_compile_definitions(MSDKInterop PRIVATE _WINRT_DLL)

target_link_libraries(MSDKInterop PRIVATE libmfx_uwp WindowsApp)
target_link_options(MSDKInterop PRIVATE /WINMD /SUBSYSTEM:WINDOWS)