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


target_sources(mfx_common_hw
  PRIVATE
    include/encoder_ddi.hpp
    include/encoding_ddi.h
    include/mfx_check_hardware_support.h
    include/mfx_h264_encode_d3d11.h
    include/mfx_h264_encode_d3d9.h
    include/mfx_h264_encode_d3d_common.h
    include/mfx_mpeg2_encode_d3d11.h
    include/mfx_mpeg2_encode_d3d9.h
    include/mfx_mpeg2_encode_d3d_common.h
    include/mfx_win_event_cache.h
    src/mfx_check_hardware_support.cpp
    src/mfx_h264_encode_d3d11.cpp
    src/mfx_h264_encode_d3d9.cpp
    src/mfx_h264_encode_d3d_common.cpp
    src/mfx_mpeg2_encode_d3d11.cpp
    src/mfx_mpeg2_encode_d3d9.cpp
    src/mfx_mpeg2_encode_d3d_common.cpp
    src/mfx_win_event_cache.cpp
  )

