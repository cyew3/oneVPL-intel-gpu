# Copyright (c) 2017-2021 Intel Corporation
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

if(BUILD_MOCK_TESTS)
  add_subdirectory(tests)
endif()

add_library (mocks INTERFACE)
target_include_directories (mocks
    INTERFACE
        ${CMAKE_HOME_DIRECTORY}/tests/unit
)

target_compile_features (mocks
    INTERFACE cxx_std_17
)

target_compile_options (mocks
    INTERFACE
        $<$<PLATFORM_ID:Windows>:/bigobj>
)

target_link_libraries (mocks
    INTERFACE
        $<$<PLATFORM_ID:Windows>:detours>
)

add_library (mocks::mocks ALIAS mocks)
