disable_werror()

###################################################################################################
# scheduler_tests
add_executable(scheduler_tests)

# Includes
target_include_directories(scheduler_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${MSDK_STUDIO_ROOT}/mfx_lib/scheduler/include
  ${MSDK_STUDIO_ROOT}/mfx_lib/scheduler/$<IF:$<PLATFORM_ID:Linux>,linux,windows>/include
  # gtest/gmock
  ${CMAKE_HOME_DIRECTORY}/mdp_msdk-contrib/gmock/gtest/include
  ${CMAKE_HOME_DIRECTORY}/mdp_msdk-contrib/gmock/include
)

target_sources(scheduler_tests
  PRIVATE
    #scheduler sources
    ${MSDK_LIB_ROOT}/scheduler/$<IF:$<PLATFORM_ID:Linux>,linux,windows>/src/mfx_scheduler_core.cpp
    ${MSDK_LIB_ROOT}/scheduler/$<IF:$<PLATFORM_ID:Linux>,linux,windows>/src/mfx_scheduler_core_iunknown.cpp
    ${MSDK_LIB_ROOT}/scheduler/$<IF:$<PLATFORM_ID:Linux>,linux,windows>/src/mfx_scheduler_core_ischeduler.cpp
    ${MSDK_LIB_ROOT}/scheduler/$<IF:$<PLATFORM_ID:Linux>,linux,windows>/src/mfx_scheduler_core_task.cpp
    ${MSDK_LIB_ROOT}/scheduler/$<IF:$<PLATFORM_ID:Linux>,linux,windows>/src/mfx_scheduler_core_task_management.cpp
    ${MSDK_LIB_ROOT}/scheduler/$<IF:$<PLATFORM_ID:Linux>,linux,windows>/src/mfx_scheduler_core_thread.cpp
    ${MSDK_LIB_ROOT}/shared/src/mfx_session.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/libmfx_core.cpp
    $<$<PLATFORM_ID:Windows>:${MSDK_STUDIO_ROOT}/shared/src/d3d11_video_processor.cpp>
    $<$<PLATFORM_ID:Windows>:${MSDK_STUDIO_ROOT}/shared/src/fast_compositing_ddi.cpp>
    $<$<PLATFORM_ID:Windows>:${MSDK_STUDIO_ROOT}/shared/src/auxiliary_device.cpp>
    $<$<PLATFORM_ID:Windows>:${MSDK_STUDIO_ROOT}/shared/src/d3d11_decode_accelerator.cpp>
    $<$<PLATFORM_ID:Windows>:${MSDK_STUDIO_ROOT}/shared/src/mfx_umc_alloc_wrapper.cpp>
    $<$<PLATFORM_ID:Windows>:${MSDK_STUDIO_ROOT}/shared/src/mfx_dxva2_device.cpp>
    $<$<PLATFORM_ID:Windows>:${MSDK_STUDIO_ROOT}/shared/src/libmfx_allocator.cpp>
    $<$<PLATFORM_ID:Windows>:${MSDK_STUDIO_ROOT}/shared/src/libmfx_allocator_d3d11.cpp>
    $<$<PLATFORM_ID:Windows>:${MSDK_STUDIO_ROOT}/shared/src/libmfx_allocator_d3d9.cpp>
    $<$<PLATFORM_ID:Windows>:${MSDK_STUDIO_ROOT}/shared/src/libmfx_core.cpp>
    $<$<PLATFORM_ID:Windows>:${MSDK_STUDIO_ROOT}/shared/src/libmfx_core_hw.cpp>
    $<$<PLATFORM_ID:Windows>:${MSDK_STUDIO_ROOT}/shared/src/libmfx_core_d3d9on11.cpp>
    $<$<PLATFORM_ID:Windows>:${MSDK_STUDIO_ROOT}/shared/src/libmfx_core_d3d9.cpp>
    $<$<PLATFORM_ID:Windows>:${MSDK_STUDIO_ROOT}/shared/src/libmfx_core_d3d11.cpp>
    $<$<PLATFORM_ID:Windows>:${MSDK_STUDIO_ROOT}/shared/src/cm_mem_copy.cpp>
    $<$<PLATFORM_ID:Windows>:${MSDK_LIB_ROOT}/shared/src/mfx_common_int.cpp>
    $<$<PLATFORM_ID:Windows>:${MSDK_LIB_ROOT}/shared/src/mfx_common_decode_int.cpp>
    $<$<PLATFORM_ID:Windows>:${MSDK_LIB_ROOT}/shared/src/mfx_check_hardware_support.cpp>
    $<$<PLATFORM_ID:Windows>:${MSDK_LIB_ROOT}/shared/src/mfx_check_hardware_support.cpp>
    $<$<PLATFORM_ID:Windows>:${MSDK_LIB_ROOT}/vpp/src/mfx_vpp_factory.cpp>

    #test sources
    scheduler_api.cpp
    scheduler_event.cpp
    scheduler_tasks.cpp
    test_main.cpp
)

target_compile_definitions(scheduler_tests
  PRIVATE
    MFX_VA
)

target_compile_features(scheduler_tests PUBLIC cxx_std_11)

target_link_libraries(scheduler_tests
  PRIVATE
    vm
    vm_plus
    umc_va_hw
    mfx_trace
    gmock
    gmock_main
    ${ITT_LIBRARIES}
    mfx_static_lib
    mfx_shared_lib
    mfx_require_sse4_properties
    $<$<PLATFORM_ID:Windows>:cmrt_cross_platform_hw>
    $<$<PLATFORM_ID:Windows>:vc1_dec>
    $<$<PLATFORM_ID:Windows>:genx>
    $<$<PLATFORM_ID:Windows>:IPP::core>
    $<$<PLATFORM_ID:Windows>:IPP::s>
    $<$<PLATFORM_ID:Windows>:IPP::i>
    $<$<PLATFORM_ID:Windows>:IPP::cc>
    ${CMAKE_THREAD_LIBS_INIT}
    ${CMAKE_DL_LIBS}
)
