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

add_library(media_buffers STATIC)
set_property(TARGET media_buffers PROPERTY FOLDER "umc")

target_sources(media_buffers
  PRIVATE
    include/umc_cyclic_buffer.h
    include/umc_frame_constructor.h
    include/umc_linear_buffer.h
    include/umc_sample_buffer.h
    include/umc_video_buffer.h

    src/umc_frame_constructor.cpp
    src/umc_linear_buffer.cpp
    src/umc_sample_buffer.cpp
    src/umc_video_buffer.cpp
  )

target_include_directories(media_buffers
  PUBLIC
    include
  )

target_link_libraries(media_buffers PUBLIC vm_plus)
