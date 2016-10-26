//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014 Intel Corporation. All Rights Reserved.
//

#pragma once
#include "mfx_h265_encode_hw_ddi.h"

namespace MfxHwH265Encode
{

#ifdef DDI_TRACE
class DDITracer
{
private:
    FILE* m_log;
public:
    DDITracer();
    ~DDITracer();

    template<class T> void Trace(T const &, mfxU32) {};
    template<class T> void TraceArray(T const * b, mfxU32 n) { for (mfxU32 i = 0; i < n; i ++) Trace(b[i], i); };

    void Trace(ENCODE_COMPBUFFERDESC const & b, mfxU32 idx);
    void Trace(ENCODE_CAPS_HEVC const & b, mfxU32 idx);
    void Trace(ENCODE_SET_SEQUENCE_PARAMETERS_HEVC const & b, mfxU32 idx);
    void Trace(ENCODE_SET_PICTURE_PARAMETERS_HEVC const & b, mfxU32 idx);
    void Trace(ENCODE_SET_SLICE_HEADER_HEVC const & b, mfxU32 idx);
    void Trace(ENCODE_PACKEDHEADER_DATA const & b, mfxU32 idx);
    void Trace(ENCODE_EXECUTE_PARAMS const & b, mfxU32 idx);
    void Trace(ENCODE_QUERY_STATUS_PARAMS const & b, mfxU32 idx);
};
#else
class DDITracer
{
public:
    DDITracer(){};
    ~DDITracer(){};
    template<class T> inline void Trace(T const &, mfxU32) {};
    template<class T> inline void TraceArray(T const *, mfxU32) {};
};
#endif

}