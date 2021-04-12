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

cmake_minimum_required(VERSION 3.11)

if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    set (_detours_TARGET_PROCESSOR X64)
else()
    set (_detours_TARGET_PROCESSOR X86)
endif()

set (_detours_LIB_DIR lib.${_detours_TARGET_PROCESSOR})
set (_detours_LIB_PATH
    ${CMAKE_CURRENT_SOURCE_DIR}/../../detours/${_detours_LIB_DIR}/detours.lib
)

add_custom_command (
    OUTPUT ${_detours_LIB_PATH}
    COMMAND nmake DETOURS_TARGET_PROCESSOR=${_detours_TARGET_PROCESSOR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../detours
    VERBATIM
)

add_custom_target (detours-make
    DEPENDS ${_detours_LIB_PATH}
)
add_library (detours STATIC IMPORTED GLOBAL)
add_dependencies (detours detours-make)

set_target_properties (detours
    PROPERTIES
        IMPORTED_LOCATION ${_detours_LIB_PATH}
        INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/../../detours/src
        FOLDER dependencies
)

add_library (detours::detours ALIAS detours)
