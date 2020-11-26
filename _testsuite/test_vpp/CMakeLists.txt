if(MFX_BUNDLED_IPP)
  return()
endif()

add_executable(test_vpp)

target_include_directories(test_vpp
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/include
    ${CMAKE_CURRENT_SOURCE_DIR}/common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/common/include/vm
    $<$<PLATFORM_ID:Windows>:
      ${MSDK_TSUITE_ROOT}/../igfx_s3dcontrol/include/
      ${MSDK_TSUITE_ROOT}/../igfx_s3dcontrol/include/API/
    >
)

target_sources(test_vpp
  PRIVATE
    include/test_vpp_config.h
    include/test_vpp_frc.h
    include/test_vpp_frc_adv.h
    include/test_vpp_pts.h
    include/test_vpp_roi.h
    include/test_vpp_utils.h

    src/test_vpp.cpp
    src/test_vpp_config.cpp
    src/test_vpp_frc.cpp
    src/test_vpp_frc_adv.cpp
    src/test_vpp_parser.cpp
    src/test_vpp_pts.cpp
    src/test_vpp_roi.cpp
    src/test_vpp_utils.cpp

    common/include/base_allocator.h
    common/include/current_date.h
    common/include/d3d_allocator.h
    common/include/d3d_device.h
    common/include/d3d11_allocator.h
    common/include/d3d11_device.h
    common/include/hw_device.h
    common/include/mfx_decode_buffering.h
    common/include/mfx_plugin_uids.h
    common/include/sample_defs.h
    common/include/sample_utils.h
    common/include/shared_ptr.h
    common/include/surface_auto_lock.h
    common/include/sysmem_allocator.h
    common/include/vaapi_allocator.h
    common/include/vaapi_device.h
    common/include/vaapi_utils.h
    common/include/vaapi_utils_android.h
    common/include/vaapi_utils_drm.h
    common/include/vaapi_utils_x11.h
    common/include/vpp_ex.h

    common/src/base_allocator.cpp
    common/src/d3d_allocator.cpp
    common/src/d3d_device.cpp
    common/src/d3d11_allocator.cpp
    common/src/d3d11_device.cpp
    common/src/mfx_decode_buffering.cpp
    common/src/mfx_plugin_uids.cpp
    common/src/sample_utils.cpp
    common/src/sysmem_allocator.cpp
    common/src/vaapi_allocator.cpp
    common/src/vaapi_device.cpp
    common/src/vaapi_utils.cpp
    common/src/vaapi_utils_android.cpp
    common/src/vaapi_utils_drm.cpp
    common/src/vaapi_utils_x11.cpp
    common/src/vpp_ex.cpp

    common/src/vm/atomic.cpp
    common/src/vm/atomic_linux.cpp
    common/src/vm/shared_object.cpp
    common/src/vm/shared_object_linux.cpp
    common/src/vm/thread.cpp
    common/src/vm/thread_linux.cpp
    common/src/vm/time.cpp
    common/src/vm/time_linux.cpp
)

target_link_libraries(test_vpp
  PUBLIC
    mfx_shared_lib
    shared_utils
    mfx
    umc
    vm
    vm_plus
    IPP::dc IPP::vc IPP::cc IPP::i IPP::s IPP::core
  PRIVATE
    mfx_va_properties
    mfx_require_sse4_properties
)

target_compile_definitions(test_vpp 
  PRIVATE 
    MFX_VERSION_USE_LATEST
  )

set_property(TARGET test_vpp PROPERTY FOLDER "testsuite")
