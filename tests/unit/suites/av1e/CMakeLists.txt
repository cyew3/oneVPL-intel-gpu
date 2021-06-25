# Copyright (c) 2020 Intel Corporation
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

if(NOT (${CMAKE_SIZEOF_VOID_P} EQUAL 8))
    return()
endif()

add_executable (mfx_av1e_test
    $<$<PLATFORM_ID:Windows>:av1e_feature_blocks_base_ddi_d3d11.cpp>
    av1e_feature_blocks_base_general.cpp
    av1e_feature_blocks_base_packer.cpp
    av1e_feature_blocks_base_superres.cpp
    av1e_feature_blocks_base_task.cpp
    av1e_feature_blocks_base_tile.cpp
)

target_link_libraries (mfx_av1e_test
    PRIVATE
        mfxcore
        ${MFX_DISPATCHER}
        $<$<PLATFORM_ID:Windows>:detours>
        gtest_main
        gmock
        mfx_shared_lib
        bs_parser_av1_static
        mocks::mocks
        fast_copy_sse4
)

target_compile_definitions (mfx_av1e_test
    PRIVATE
        MFX_ENABLE_UNIT_TEST_AV1E_DX11_DDI
)

set_target_properties (mfx_av1e_test
    PROPERTIES
        FOLDER "tests"
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BIN_DIR}/${CMAKE_BUILD_TYPE}
)

add_test (NAME mfx_av1e_test
    COMMAND ./mfx_av1e_test
    WORKING_DIRECTORY ${CMAKE_BIN_DIR}/${CMAKE_BUILD_TYPE}
)

set_property (TEST mfx_av1e_test
    PROPERTY
        ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_BIN_DIR}/${CMAKE_BUILD_TYPE}"
)
