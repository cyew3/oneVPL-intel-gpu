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

add_executable(mfx_hevce_test
    hevce_feature_blocks_g11_interlace.cpp
    hevce_feature_utils_blocks.cpp
    hevce_feature_utils_storage.cpp
    hevce_utils_misc.cpp
    ${CMAKE_HOME_DIRECTORY}/_studio/mfx_lib/shared/src/mfx_common_int.cpp
    ${MSDK_STUDIO_ROOT}/shared/include/fast_copy_c_impl.h
    ${MSDK_STUDIO_ROOT}/shared/include/fast_copy.h
    ${MSDK_STUDIO_ROOT}/shared/src/fast_copy_c_impl.cpp
    ${MSDK_STUDIO_ROOT}/shared/src/fast_copy.cpp
    $<TARGET_OBJECTS:fast_copy_sse4>
)

target_link_libraries(mfx_hevce_test
    PRIVATE
        encode_hw
        mfx_trace
        gtest_main
        gmock
        mfx_va_properties
        fast_copy_sse4
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
