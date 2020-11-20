# Copyright (c) 2017-2020 Intel Corporation
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

set(HEVC_FEI_Encoder_HW_GUID "5418a70666f94d5cb4f7b1caee86339b")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set( plugin_name mfx_hevc_fei_hw64 )
else()
  set( plugin_name mfx_hevc_fei_hw32 )
endif()

add_library(${plugin_name} SHARED)

# Plugin version info
set(version_defs "")
set_file_and_product_version( ${MEDIA_VERSION_STR} version_defs )

target_include_directories(${plugin_name}
  PRIVATE
    h265_fei/include
    ${MSDK_LIB_ROOT}/shared/include
  )

target_sources(${plugin_name}
  PRIVATE
    h265_fei/include/mfx_h265_fei_encode_plugin_hw.h
    h265_fei/src/export.cpp
  )

target_link_libraries(${plugin_name} PUBLIC mfx_shared_lib mfx_plugin_properties)

gen_plugins_cfg("HEVC_Encoder_HW" ${HEVC_FEI_Encoder_HW_GUID} ${plugin_name} "02" "HEVC")

target_compile_definitions(${plugin_name}
  PUBLIC
    ${version_defs}
  )

install( TARGETS ${plugin_name} LIBRARY DESTINATION ${MFX_PLUGINS_DIR} )

include(sources_ext.cmake)
