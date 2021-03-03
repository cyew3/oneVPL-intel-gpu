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

project (mfx_dvp_test)

add_executable (${PROJECT_NAME}
    test_dvp_init.cpp
    test_dvp_status.cpp
    test_dvp_corner.cpp
    ${CMAKE_HOME_DIRECTORY}/tutorials/common/common_utils.cpp
    ${CMAKE_HOME_DIRECTORY}/tutorials/common/common_utils_linux.cpp
    ${CMAKE_HOME_DIRECTORY}/tutorials/common/common_vaapi.cpp
)

target_link_libraries (${PROJECT_NAME}
    PRIVATE
        ${MFX_DISPATCHER}
        mfx_common_properties
        mfx_va_properties
        gtest_main
        gmock
)
include(CheckSymbolExists)
check_symbol_exists( clock_gettime "time.h" _gcc_CLOCK_GETTIME )
if (NOT _gcc_CLOCK_GETTIME)
  target_link_libraries( ${PROJECT_NAME} PUBLIC rt)
endif()

target_include_directories (${PROJECT_NAME}
    PRIVATE
        ${CMAKE_HOME_DIRECTORY}/tutorials/common
        ${CMAKE_HOME_DIRECTORY}/contrib/googletest/googletest/include
        ${CMAKE_HOME_DIRECTORY}/contrib/googletest/googlemock/include
        /usr/include/libdrm
)

target_compile_definitions (${PROJECT_NAME}
    PRIVATE
        MFX_ENABLE_UNIT_TEST_DVP
)

set_target_properties (${PROJECT_NAME}
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BIN_DIR}/${CMAKE_BUILD_TYPE}
)

add_test (NAME ${PROJECT_NAME}
    COMMAND ./${PROJECT_NAME}
    WORKING_DIRECTORY ${CMAKE_BIN_DIR}/${CMAKE_BUILD_TYPE}
)

set_property (TEST ${PROJECT_NAME}
    PROPERTY
        ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_BIN_DIR}/${CMAKE_BUILD_TYPE}"
)

