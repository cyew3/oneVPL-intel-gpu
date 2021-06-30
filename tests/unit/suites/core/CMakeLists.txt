# Copyright (c) 2021 Intel Corporation
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

# This test hooks dlopen and dlsym calls, i.e. it provides mock mediasdk
# library implementation inside. A key thing for the trick to work is
# avoiding non-default symbol versioning, i.e. -Wl,--default-symver which
# is propagated by mediasdk test system should be avoided.

add_executable (mfx_core_test
    core_alloc.cpp
    core_fast_copy_wrapper.cpp
    core_fourcc.cpp
    core_handle.cpp
    core_main.cpp
    $<$<PLATFORM_ID:Windows>:init_guids.cpp>
    ${MSDK_LIB_ROOT}/shared/src/libmfxsw_async.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/libmfx_allocator.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/cm_mem_copy.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/fast_copy_c_impl.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/libmfx_core_factory.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/fast_copy.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/libmfx_core.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/libmfx_core_hw.cpp
    $<$<PLATFORM_ID:Linux>:${MSDK_STUDIO_ROOT}/shared/src/mfx_vpp_vaapi.cpp>
    $<$<PLATFORM_ID:Linux>:${MSDK_STUDIO_ROOT}/shared/src/libmfx_allocator_vaapi.cpp>
    $<$<PLATFORM_ID:Linux>:${MSDK_STUDIO_ROOT}/shared/src/libmfx_core_vaapi.cpp>
    $<$<PLATFORM_ID:Windows>:${MSDK_STUDIO_ROOT}/shared/src/libmfx_allocator_d3d9.cpp>
    $<$<PLATFORM_ID:Windows>:${MSDK_STUDIO_ROOT}/shared/src/libmfx_allocator_d3d11.cpp>
    $<$<PLATFORM_ID:Windows>:${MSDK_STUDIO_ROOT}/shared/src/libmfx_core_d3d9.cpp>
    $<$<PLATFORM_ID:Windows>:${MSDK_STUDIO_ROOT}/shared/src/libmfx_core_d3d9on11.cpp>
    $<$<PLATFORM_ID:Windows>:${MSDK_STUDIO_ROOT}/shared/src/libmfx_core_d3d11.cpp>
    $<$<PLATFORM_ID:Windows>:${MSDK_STUDIO_ROOT}/shared/src/auxiliary_device.cpp>
    $<$<PLATFORM_ID:Windows>:${MSDK_STUDIO_ROOT}/shared/src/d3d11_decode_accelerator.cpp>
    $<$<PLATFORM_ID:Windows>:${MSDK_STUDIO_ROOT}/shared/src/d3d11_video_processor.cpp>
    $<$<PLATFORM_ID:Windows>:${MSDK_STUDIO_ROOT}/shared/src/mfx_umc_alloc_wrapper.cpp>
    $<$<PLATFORM_ID:Windows>:${MSDK_STUDIO_ROOT}/shared/src/mfx_dxva2_device.cpp>
    $<$<PLATFORM_ID:Windows>:${MSDK_STUDIO_ROOT}/shared/src/fast_compositing_ddi.cpp>
    $<$<PLATFORM_ID:Windows>:${MSDK_STUDIO_ROOT}/shared/src/mfx_umc_mjpeg_vpp.cpp>
    $<$<PLATFORM_ID:Windows>:${MSDK_LIB_ROOT}/shared/src/mfx_check_hardware_support.cpp>
    # $<$<PLATFORM_ID:Windows>:${MSDK_LIB_ROOT}/shared/src/mfx_session.cpp>
)

target_link_libraries (mfx_core_test
    PRIVATE
        ${MFX_DISPATCHER}
        ${MFX_API_TARGET}
        mfx_require_sse4_properties
        $<$<PLATFORM_ID:Windows>:IPP::core>
        $<$<PLATFORM_ID:Windows>:IPP::s>
        $<$<PLATFORM_ID:Windows>:IPP::i>
        $<$<PLATFORM_ID:Windows>:IPP::cc>
        $<$<PLATFORM_ID:Windows>:detours>
        mfx_shared_lib
        gtest_main
        gmock
        mocks::mocks
        mfx_va_properties
        mfx_common_hw
        encode_hw
        genx
        vpp_hw
        fast_copy_sse4
        mfx_trace
)

target_include_directories(mfx_core_test
    PRIVATE
        ${MSDK_STUDIO_ROOT}/mfx_lib/scheduler/include
        ${MSDK_STUDIO_ROOT}/mfx_lib/scheduler/$<IF:$<PLATFORM_ID:Linux>,linux,windows>/include
        ${MSDK_STUDIO_ROOT}/shared/include
)

target_compile_definitions (mfx_core_test
    PRIVATE
        $<$<EQUAL:${CMAKE_SIZEOF_VOID_P},8>:MFX_ENABLE_UNIT_TEST_CORE>
        ENABLE_MFX_INTEL_GUID_DECODE
        ENABLE_MFX_INTEL_GUID_ENCODE
        ENABLE_MFX_INTEL_GUID_PRIVATE
)

set_target_properties (mfx_core_test
    PROPERTIES
        FOLDER "tests"
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BIN_DIR}/${CMAKE_BUILD_TYPE}
)

add_test (NAME mfx_core_test
    COMMAND ./mfx_core_test
    WORKING_DIRECTORY ${CMAKE_BIN_DIR}/${CMAKE_BUILD_TYPE}
)

set_property (TEST mfx_core_test
    PROPERTY
        ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_BIN_DIR}/${CMAKE_BUILD_TYPE}"
)
