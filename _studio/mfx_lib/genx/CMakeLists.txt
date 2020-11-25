# Copyright (c) 2018-2020 Intel Corporation
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

if(MFX_ENABLE_KERNELS)

  list( APPEND sources
    asc/isa/asc_gen8_isa.cpp
    asc/isa/asc_gen9_isa.cpp
    asc/isa/asc_gen11_isa.cpp
    asc/isa/asc_gen11lp_isa.cpp
    asc/isa/asc_gen12_isa.cpp
    asc/isa/asc_gen12lp_isa.cpp
  )

  list( APPEND sources
    copy_kernels/src/genx_cht_copy_isa.cpp
    copy_kernels/src/genx_skl_copy_isa.cpp
    copy_kernels/src/genx_icl_copy_isa.cpp
    copy_kernels/src/genx_icllp_copy_isa.cpp
    copy_kernels/src/genx_cnl_copy_isa.cpp
    copy_kernels/src/genx_tgl_copy_isa.cpp
    copy_kernels/src/genx_tgllp_copy_isa.cpp
    copy_kernels/src/genx_dg2_copy_isa.cpp
  )

  list( APPEND sources
    field_copy/src/genx_fcopy_gen8_isa.cpp
    field_copy/src/genx_fcopy_gen9_isa.cpp
    field_copy/src/genx_fcopy_gen10_isa.cpp
    field_copy/src/genx_fcopy_gen11_isa.cpp
    field_copy/src/genx_fcopy_gen11lp_isa.cpp
    field_copy/src/genx_fcopy_gen12lp_isa.cpp
    field_copy/src/genx_fcopy_gen12_isa.cpp
  )

  list( APPEND sources
    h264_encode/src/genx_bdw_simple_me_isa.cpp
    h264_encode/src/genx_cnl_histogram_isa.cpp
    h264_encode/src/genx_cnl_simple_me_isa.cpp
    h264_encode/src/genx_hsw_simple_me_isa.cpp
    h264_encode/src/genx_hsw_simple_me_proto.cpp
    h264_encode/src/genx_icl_histogram_isa.cpp
    h264_encode/src/genx_icl_simple_me_isa.cpp
    h264_encode/src/genx_icllp_histogram_isa.cpp
    h264_encode/src/genx_icllp_simple_me_isa.cpp
    h264_encode/src/genx_skl_histogram_isa.cpp
    h264_encode/src/genx_skl_simple_me_isa.cpp
    h264_encode/src/genx_tgllp_histogram_isa.cpp
    h264_encode/src/genx_tgllp_simple_me_isa.cpp
  )

  list( APPEND sources
    mctf/isa/mctf_mc_gen8_isa.cpp
    mctf/isa/mctf_me_gen8_isa.cpp
    mctf/isa/mctf_sd_gen8_isa.cpp
    mctf/isa/mctf_mc_gen9_isa.cpp
    mctf/isa/mctf_me_gen9_isa.cpp
    mctf/isa/mctf_sd_gen9_isa.cpp
    mctf/isa/mctf_mc_gen11_isa.cpp
    mctf/isa/mctf_me_gen11_isa.cpp
    mctf/isa/mctf_sd_gen11_isa.cpp
    mctf/isa/mctf_mc_gen11lp_isa.cpp
    mctf/isa/mctf_me_gen11lp_isa.cpp
    mctf/isa/mctf_sd_gen11lp_isa.cpp
    mctf/isa/mctf_mc_gen12_isa.cpp
    mctf/isa/mctf_me_gen12_isa.cpp
    mctf/isa/mctf_sd_gen12_isa.cpp
    mctf/isa/mctf_mc_gen12lp_isa.cpp
    mctf/isa/mctf_me_gen12lp_isa.cpp
    mctf/isa/mctf_sd_gen12lp_isa.cpp
  )

  add_library(genx STATIC ${sources})
  set_property(TARGET genx PROPERTY FOLDER "kernels")

  target_include_directories(genx
    PUBLIC
      asc/isa
      copy_kernels/include
      field_copy/include
      h264_encode/include
      mctf/isa
    )

  target_link_libraries(genx PUBLIC mfx_static_lib)
endif()

include(sources_ext.cmake OPTIONAL)