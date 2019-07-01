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

mfx_include_dirs()

include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../include )

set( defs "${API_FLAGS} ${WARNING_FLAGS}" )

### UMC core umc
set(sources
    ${CMAKE_CURRENT_SOURCE_DIR}/core/umc/src/umc_base_codec.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/umc/src/umc_frame_data.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/umc/src/umc_media_data.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/umc/src/umc_va_base.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/umc/src/umc_video_data.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/umc/src/umc_video_decoder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/umc/src/umc_video_encoder.cpp
    )

if(NOT OPEN_SOURCE)
  list(APPEND sources
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
endif()

make_library( umc none static )
### UMC core umc

### UMC vm
set( sources "" )
file( GLOB_RECURSE srcs "${CMAKE_CURRENT_SOURCE_DIR}/core/vm/src/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/core/vm/src/*.cpp" )
list( APPEND sources ${srcs})

make_library( vm none static )
### UMC vm

### UMC vm plus
set(sources
    ${CMAKE_CURRENT_SOURCE_DIR}/core/vm_plus/src/mfx_vm++_pthread.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/vm_plus/src/mfx_vm++_pthread_unix.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/vm_plus/src/mfx_vm++_pthread_windows.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/vm_plus/src/umc_event.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/vm_plus/src/umc_semaphore.cpp
    )

if(NOT OPEN_SOURCE)
  list(APPEND sources
    ${CMAKE_CURRENT_SOURCE_DIR}/core/vm_plus/src/umc_mmap.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/vm_plus/src/umc_sys_info.cpp
    )
endif()

make_library( vm_plus none static )
### UMC vm plus

### UMC VA
set( sources "" )
file( GLOB_RECURSE srcs "${CMAKE_CURRENT_SOURCE_DIR}/io/umc_va/src/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/io/umc_va/src/*.cpp" )
list( APPEND sources ${srcs} )

make_library( umc_va hw static )
### UMC VA

### UMC codec brc
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/codec/h264_enc/include )
include_directories (${CMAKE_CURRENT_SOURCE_DIR}/codec/brc/include)

set(sources
    ${CMAKE_CURRENT_SOURCE_DIR}/codec/brc/src/umc_brc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/codec/brc/src/umc_h264_brc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/codec/brc/src/umc_mpeg2_brc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/codec/brc/src/umc_video_brc.cpp
    )

if(NOT OPEN_SOURCE)
  list(APPEND sources
    ${CMAKE_CURRENT_SOURCE_DIR}/codec/brc/src/umc_svc_brc.cpp
    )
endif()

make_library( bitrate_control none static )
### UMC codec brc
