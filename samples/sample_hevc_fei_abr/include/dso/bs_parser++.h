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

#ifndef __BS_PARSER_PP_H
#define __BS_PARSER_PP_H

#include <bs_parser.h>
#include <memory.h>

class BS_parser{
public:
    BS_parser(){};
    virtual BSErr open(const char* file) = 0;
    virtual BSErr set_buffer(byte* buf, Bs32u buf_size) = 0;
    virtual BSErr parse_next_unit() = 0;
    virtual BSErr reset() = 0;
    virtual BSErr lock  (void* p) = 0;
    virtual BSErr unlock(void* p) = 0;
    virtual void* get_header() = 0;
    virtual void* get_handle() = 0;
    virtual Bs64u get_offset() = 0;
    virtual void set_trace_level(Bs32u level) = 0;
    virtual BSErr sync(void* p) { return BS_ERR_NONE; };
    virtual Bs16u async_depth() { return 0; }
    virtual ~BS_parser(){};
};

class BS_HEVC2_parser : public BS_parser{
public:
    typedef BS_HEVC2::NALU UnitType;

    BS_HEVC2_parser(Bs32u mode = 0)
    {
        BS_HEVC2_Init(hdl, mode);
        hdr = 0;
    };
    ~BS_HEVC2_parser() { BS_HEVC2_Close(hdl); };

    BSErr parse_next_au(UnitType*& pAU)         { return BS_HEVC2_ParseNextAU(hdl, pAU); };
    BSErr parse_next_unit()                     { return BS_HEVC2_ParseNextAU(hdl, hdr); };
    BSErr open(const char* file)                { return BS_HEVC2_OpenFile(hdl, file); };
    BSErr set_buffer(byte* buf, Bs32u buf_size) { return BS_HEVC2_SetBuffer(hdl, buf, buf_size); };
    BSErr lock(void* p)                         { return BS_HEVC2_Lock(hdl, p); };
    BSErr unlock(void* p)                       { return BS_HEVC2_Unlock(hdl, p); };
    BSErr sync(void* p)                         { return BS_HEVC2_Sync(hdl, (BS_HEVC2::NALU*)p); };
    Bs16u async_depth()                         { return BS_HEVC2_GetAsyncDepth(hdl); }

    void set_trace_level(Bs32u level)           { BS_HEVC2_SetTraceLevel(hdl, level); };
    void* get_header() { return hdr; };
    void* get_handle() { return hdl; };
    Bs64u get_offset() {
        Bs64u offset = -1;
        BS_HEVC2_GetOffset(hdl, offset);
        return offset;
    };

    BSErr reset()
    {
        BS_HEVC2_Close(hdl);
        hdr = 0;
        return BS_HEVC2_Init(hdl, 0);
    };
private:
    BS_HEVC2::HDL hdl;
    UnitType* hdr;
};


#endif //__BS_PARSER_PP_H
