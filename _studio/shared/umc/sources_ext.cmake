# Copyright (c) 2017-2019 Intel Corporation
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

if(NOT OPEN_SOURCE)

  ### UMC core umc
  set(sources
    ${CMAKE_CURRENT_SOURCE_DIR}/core/umc/src/umc_audio_codec.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/umc/src/umc_audio_render.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/umc/src/umc_default_frame_allocator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/umc/src/umc_default_memory_allocator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/umc/src/umc_deprecated_splitter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/umc/src/umc_index.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/umc/src/umc_media_buffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/umc/src/umc_media_data_ex.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/umc/src/umc_muxer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/umc/src/umc_par_reader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/umc/src/umc_splitter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/umc/src/umc_utils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/umc/src/umc_video_render.cpp
    )

    make_library( umc_ext none static )
    ### UMC core umc

    ### UMC vm plus
    set(sources
      src/umc_mmap.cpp
      src/umc_sys_info.cpp
      )

    make_library( vm_plus_ext none static )
    ### UMC vm plus

endif() # if(NOT OPEN_SOURCE)
