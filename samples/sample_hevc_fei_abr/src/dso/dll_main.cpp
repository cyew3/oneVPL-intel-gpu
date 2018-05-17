/******************************************************************************\
Copyright (c) 2018, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

#include <hevc2_parser.h>

extern "C" {

BSErr __STDCALL BS_HEVC2_Init(BS_HEVC2::HDL& hdl, Bs32u mode){
    hdl = NULL;
    hdl = new BS_HEVC2::Parser(mode);
    if (hdl)
        return BS_ERR_NONE;
    return BS_ERR_UNKNOWN;
}

BSErr __STDCALL BS_HEVC2_OpenFile(BS_HEVC2::HDL hdl, const char* file){
    if (!hdl) return BS_ERR_BAD_HANDLE;
    return hdl->open(file);
}

BSErr __STDCALL BS_HEVC2_SetBuffer(BS_HEVC2::HDL hdl, byte* buf, Bs32u buf_size){
    if (!hdl) return BS_ERR_BAD_HANDLE;
    hdl->set_buffer(buf, buf_size);
    return BS_ERR_NONE;
}

BSErr __STDCALL BS_HEVC2_ParseNextAU(BS_HEVC2::HDL hdl, BS_HEVC2::NALU*& header){
    if (!hdl) return BS_ERR_BAD_HANDLE;
    return hdl->parse_next_au(header);
}

BSErr __STDCALL BS_HEVC2_Close(BS_HEVC2::HDL hdl){
    if (!hdl) return BS_ERR_BAD_HANDLE;
    delete (hdl);
    hdl = NULL;
    return BS_ERR_NONE;
}

BSErr __STDCALL BS_HEVC2_SetTraceLevel(BS_HEVC2::HDL hdl, Bs32u level){
    if (!hdl) return BS_ERR_BAD_HANDLE;
    hdl->set_trace_level(level);
    return BS_ERR_NONE;
}

BSErr __STDCALL BS_HEVC2_Lock(BS_HEVC2::HDL hdl, void* p){
    if (!hdl) return BS_ERR_BAD_HANDLE;
    return hdl->Lock(p);
}

BSErr __STDCALL BS_HEVC2_Unlock(BS_HEVC2::HDL hdl, void* p){
    if (!hdl) return BS_ERR_BAD_HANDLE;
    return hdl->Unlock(p);
}

BSErr __STDCALL BS_HEVC2_GetOffset(BS_HEVC2::HDL hdl, Bs64u& offset){
    if (!hdl) return BS_ERR_BAD_HANDLE;
    offset = hdl->get_cur_pos();
    return BS_ERR_NONE;
}

BSErr __STDCALL BS_HEVC2_Sync(BS_HEVC2::HDL hdl, BS_HEVC2::NALU* slice){
    if (!hdl) return BS_ERR_BAD_HANDLE;
    return hdl->sync(slice);
}

Bs16u __STDCALL BS_HEVC2_GetAsyncDepth(BS_HEVC2::HDL hdl){
    if (!hdl) return 0;
    return hdl->get_async_depth();
}

} // extern "C"
