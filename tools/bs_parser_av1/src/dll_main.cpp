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

#include <av1_parser.h>

extern "C" {

// For Perl compatibility only:
Bs32u BS_GetVal( void* hdr, Bs32u* element_idx ){
    if( !hdr         ) return (Bs32u)BS_ERR_WRONG_UNITS_ORDER;
    if( !element_idx ) return (Bs32u)BS_ERR_INVALID_PARAMS;

    Bs32u val = 0;
    void* addr = hdr;
    Bs32s nesting_level = *element_idx;
    while( nesting_level-- ){
        addr = *(void**)((Bs64s)addr + *(++element_idx));
    }
    addr = (void*)((Bs64s)addr + *(++element_idx));
    for( Bs32u i = 0; i<(*(element_idx+1));i++ ){
        val += ((*((byte*)addr+i))<<(i<<3));
    }
    return val;
}

BSErr __STDCALL BS_AV1_Init(BS_AV1_hdl& hdl, Bs32u mode) {
    hdl = NULL;
    hdl = new AV1_BitStream(mode);
    if (hdl) return (hdl)->get_last_err();
    return BS_ERR_UNKNOWN;
}

BSErr __STDCALL BS_AV1_OpenFile(BS_AV1_hdl hdl, const char* file) {
    if (!hdl) return BS_ERR_BAD_HANDLE;
    return hdl->open(file);
}

BSErr __STDCALL BS_AV1_Close(BS_AV1_hdl hdl) {
    if (!hdl) return BS_ERR_BAD_HANDLE;
    delete (hdl);
    hdl = NULL;
    return BS_ERR_NONE;
}

BSErr __STDCALL BS_AV1_SetBuffer(BS_AV1_hdl hdl, byte* buf, Bs32u buf_size) {
    if (!hdl) return BS_ERR_BAD_HANDLE;
    hdl->set_buffer(buf, buf_size);
    return hdl->get_last_err();
}

BSErr __STDCALL BS_AV1_ParseNextUnit(BS_AV1_hdl hdl, BS_AV1::Frame*& header) {
    if (!hdl) return BS_ERR_BAD_HANDLE;
    hdl->parse_next_unit();
    header = hdl->get_hdr();
    return hdl->get_last_err();
}

BSErr __STDCALL BS_AV1_GetOffset(BS_AV1_hdl hdl, Bs64u& offset) {
    if (!hdl) return BS_ERR_BAD_HANDLE;
    offset = (Bs64u)hdl->get_cur_pos();
    return BS_ERR_NONE;
}

BSErr __STDCALL BS_AV1_SetTraceLevel(BS_AV1_hdl hdl, Bs32u level) {
    if (!hdl) return BS_ERR_BAD_HANDLE;
    hdl->set_trace_level(level);
    return BS_ERR_NONE;
}
} // extern "C"
