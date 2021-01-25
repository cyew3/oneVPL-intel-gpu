// Copyright (c) 2018-2020 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef __BS_PARSER_H
#define __BS_PARSER_H

#include <av1_struct.h>

extern "C"{
    BSErr __STDCALL BS_AV1_Init(BS_AV1_hdl& hdl, Bs32u mode);
    BSErr __STDCALL BS_AV1_OpenFile(BS_AV1_hdl hdl, const char* file);
    BSErr __STDCALL BS_AV1_SetBuffer(BS_AV1_hdl hdl, byte* buf, Bs32u buf_size);
    BSErr __STDCALL BS_AV1_ParseNextUnit(BS_AV1_hdl hdl, BS_AV1::Frame*& header);
    BSErr __STDCALL BS_AV1_Close(BS_AV1_hdl hdl);
    BSErr __STDCALL BS_AV1_GetOffset(BS_AV1_hdl hdl, Bs64u& offset);
    BSErr __STDCALL BS_AV1_SetTraceLevel(BS_AV1_hdl hdl, Bs32u level);
}


#endif //__BS_PARSER_H
