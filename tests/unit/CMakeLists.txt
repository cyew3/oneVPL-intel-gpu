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

if (BUILD_DISPATCHER AND BUILD_EXTRA)
  if(CMAKE_SYSTEM_NAME MATCHES Windows)
    add_subdirectory(suites/mfx_dispatch/windows)
  else()
    add_subdirectory(suites/mfx_dispatch/linux)
  endif()
endif()

if (BUILD_TOOLS)
  if(NOT CMAKE_SYSTEM_NAME MATCHES Windows)
    add_subdirectory(suites/tracer/linux)
  endif()
endif()

add_subdirectory(suites/compare_struct)
add_subdirectory(mocks)
add_subdirectory(suites/allocator)

add_subdirectory(suites/utils)
add_subdirectory(suites/hevce)
add_subdirectory(suites/av1e)

#TEMP
if(NOT CMAKE_SYSTEM_NAME MATCHES Windows)
  add_subdirectory(suites/core)
  add_subdirectory(suites/memory)
  add_subdirectory(suites/dvp)
endif()
