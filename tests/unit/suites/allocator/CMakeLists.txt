# Copyright (c) 2020-2021 Intel Corporation
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

set (SOURCES
    ${CMAKE_HOME_DIRECTORY}/_studio/shared/src/libmfx_allocator.cpp
    ${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/shared/src/libmfxsw_async.cpp
    ${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/shared/src/mfx_common_int.cpp
    $<$<PLATFORM_ID:Linux>:${CMAKE_HOME_DIRECTORY}/_studio/shared/src/libmfx_allocator_vaapi.cpp>
    $<$<PLATFORM_ID:Windows>:${CMAKE_HOME_DIRECTORY}/_studio/shared/src/libmfx_allocator_d3d11.cpp>
)

add_executable (mfx_allocator_test
    flexible_allocator.cpp
    flexible_allocator_lock_unlock.cpp
    flexible_allocator_exceptions.cpp
    flexible_allocator_realloc.cpp
    flexible_allocator_type_flags.cpp
    flexible_allocator_lock_fourcc.cpp
    test_surface_array.cpp
    $<$<PLATFORM_ID:Linux>:scoped_lock.cpp>
    allocator_wrapper.cpp
    ${SOURCES}
)

target_link_libraries (mfx_allocator_test
    PRIVATE
        ${MFX_DISPATCHER}
        $<$<PLATFORM_ID:Windows>:detours>
        mfx_shared_lib
        mfx_trace
        gtest_main
        gmock
        mocks::mocks
        mfx_va_properties
)

target_compile_definitions (mfx_allocator_test
    PRIVATE
        MFX_ENABLE_UNIT_TEST_ALLOCATOR
        ENABLE_MFX_INTEL_GUID_DECODE
        ENABLE_MFX_INTEL_GUID_PRIVATE
)

set_target_properties (mfx_allocator_test
    PROPERTIES
        FOLDER "tests"
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BIN_DIR}/${CMAKE_BUILD_TYPE}
)

add_test (NAME mfx_allocator_test
    COMMAND ./mfx_allocator_test
    WORKING_DIRECTORY ${CMAKE_BIN_DIR}/${CMAKE_BUILD_TYPE}
)

set_property (TEST mfx_allocator_test
    PROPERTY
        ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_BIN_DIR}/${CMAKE_BUILD_TYPE}"
)
