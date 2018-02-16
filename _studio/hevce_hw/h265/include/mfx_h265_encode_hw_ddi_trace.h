//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2014-2018 Intel Corporation. All Rights Reserved.
#include "mfx_common.h"
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE)

#pragma once
#include "mfx_h265_encode_hw_ddi.h"

namespace MfxHwH265Encode
{

#ifdef DDI_TRACE
class DDITracer
{
private:
    FILE* m_log;
    ENCODER_TYPE m_type;
public:
    DDITracer(ENCODER_TYPE type = ENCODER_DEFAULT);
    ~DDITracer();

    template<class T> void Trace(T const &, mfxU32) {};
    template<class T> void TraceArray(T const * b, mfxU32 n) {
        if (b)
        for (mfxU32 i = 0; i < n; i ++) Trace(b[i], i);
    };
    inline void TraceArray(void const *, mfxU32) {};

    void Trace(ENCODE_COMPBUFFERDESC const & b, mfxU32 idx);
    void Trace(ENCODE_CAPS_HEVC const & b, mfxU32 idx);
    void Trace(ENCODE_SET_SEQUENCE_PARAMETERS_HEVC const & b, mfxU32 idx);
    void Trace(ENCODE_SET_PICTURE_PARAMETERS_HEVC const & b, mfxU32 idx);
    void Trace(ENCODE_SET_SLICE_HEADER_HEVC const & b, mfxU32 idx);
    void Trace(ENCODE_PACKEDHEADER_DATA const & b, mfxU32 idx);
    void Trace(ENCODE_EXECUTE_PARAMS const & b, mfxU32 idx);
    void Trace(ENCODE_QUERY_STATUS_PARAMS const & b, mfxU32 idx);
    void Trace(ENCODE_QUERY_STATUS_SLICE_PARAMS const & b, mfxU32 idx);

    void Trace(D3D11_VIDEO_DECODER_EXTENSION const & b, mfxU32 idx);

    void Trace(ENCODE_SET_SEQUENCE_PARAMETERS_HEVC_REXT const & b, mfxU32 idx);
    void Trace(ENCODE_SET_PICTURE_PARAMETERS_HEVC_REXT const & b, mfxU32 idx);
    void Trace(ENCODE_SET_SLICE_HEADER_HEVC_REXT const & b, mfxU32 idx);

    void Trace(ENCODE_SET_SEQUENCE_PARAMETERS_HEVC_SCC const & b, mfxU32 idx);
    void Trace(ENCODE_SET_PICTURE_PARAMETERS_HEVC_SCC const & b, mfxU32 idx);

    inline void Trace(GUID const & guid, mfxU32) { TraceGUID(guid, m_log); };
    void Trace(const char* name, mfxU32 value);
    template<mfxU32 N> inline void Trace(const char name[N], mfxU32 value) { Trace((const char*) name, value);  }

    static void TraceGUID(GUID const & guid, FILE*);
};
#else
class DDITracer
{
public:
    DDITracer(ENCODER_TYPE type = ENCODER_DEFAULT) { type; };
    ~DDITracer(){};
    template<class T> inline void Trace(T const &, mfxU32) {};
    template<class T> inline void TraceArray(T const *, mfxU32) {};
    static void TraceGUID(GUID const &, FILE*) {};
};
#endif

}
#endif
