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

if(NOT (${CMAKE_SIZEOF_VOID_P} EQUAL 8))
    return()
endif()

add_executable(mfx_hevce_test
    hevce_disp.cpp
    hevce_feature_blocks_g11_interlace.cpp
    hevce_feature_utils_blocks.cpp
    hevce_feature_utils_storage.cpp
    hevce_utils_misc.cpp
    $<$<PLATFORM_ID:Windows>:hevce_feature_blocks_dx11_ddi.cpp>
    $<$<PLATFORM_ID:Windows>:hevce_feature_blocks_g11_win_qmatrix.cpp>
)

target_link_libraries(mfx_hevce_test
    PRIVATE
        ${MFX_DISPATCHER}
        mfxcore
        $<$<PLATFORM_ID:Windows>:detours>
        mfx_shared_lib
        gtest_main
        gmock
        fast_copy_sse4
        mocks::mocks
)

target_compile_definitions (mfx_hevce_test
    PRIVATE
        MFX_ENABLE_UNIT_TEST_HEVCE_DX11_DDI
        ENABLE_MFX_INTEL_GUID_DECODE
        ENABLE_MFX_INTEL_GUID_ENCODE
        ENABLE_MFX_INTEL_GUID_PRIVATE
)

set_target_properties (mfx_hevce_test
    PROPERTIES
        FOLDER "tests"
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BIN_DIR}/${CMAKE_BUILD_TYPE}
)

add_test (NAME mfx_hevce_test
    COMMAND ./mfx_hevce_test
    WORKING_DIRECTORY ${CMAKE_BIN_DIR}/${CMAKE_BUILD_TYPE}
)

set_property (TEST mfx_hevce_test
    PROPERTY
        ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_BIN_DIR}/${CMAKE_BUILD_TYPE}"
)
