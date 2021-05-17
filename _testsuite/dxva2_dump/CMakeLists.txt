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

if(NOT CMAKE_SYSTEM_NAME MATCHES Windows)
  return()
endif()

add_library(dxva2_dump STATIC)
set_property(TARGET dxva2_dump PROPERTY FOLDER "testsuite")

target_sources(dxva2_dump
  PRIVATE
    include/datatypes.h
    include/dx11_spy.h
    include/dxva2_buffers_ext.h
    include/dxva2_log.h
    include/dxva2_spy.h
    include/lrb_vme_api.h
    ${MSDK_STUDIO_ROOT}/mfx_lib/shared/include/mfx_ddi_enc_dump.h

    src/dxva_spy.cpp
    ${MSDK_STUDIO_ROOT}/mfx_lib/shared/src/mfx_ddi_enc_dump.cpp

  )

target_include_directories(dxva2_dump
  PUBLIC
    include
  )

target_compile_definitions(dxva2_dump
  PUBLIC
    MFX_VA
    $<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_DEPRECATE>
  )

target_link_libraries(dxva2_dump PUBLIC vm_plus mfx_common_properties PRIVATE umc_va_hw)