/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2019 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#pragma once

#include <mfx_ivideo_encode.h>
#include "mfx_extended_buffer.h"

//enables mode3 query for mfxExtEncoderCapability
class QueryMode4Encode : public InterfaceProxy<IVideoEncode> {
    typedef InterfaceProxy<IVideoEncode> base;

    enum {
        idToRemove = BufferIdOf<mfxExtEncoderCapability>::id
    };

public:
    QueryMode4Encode(std::unique_ptr<IVideoEncode> &&pEnc)
        : base(std::move(pEnc)) {
    }
    virtual mfxStatus Init(mfxVideoParam *par) {
        auto_ext_buffer_remove auto_buf(*par, idToRemove);
        return base::Init(par);
    }

    virtual mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out) {
        {
            //query mod2
            auto_ext_buffer_remove auto_buf(*in, idToRemove);
            auto_ext_buffer_remove auto_buf2(*out, idToRemove);

            MFX_CHECK_STS(base::Query(in, out));
        }
        //query mod3
        return base::Query(in, out);
    }
    virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request) {
        auto_ext_buffer_remove auto_buf(*par, idToRemove);
        return base::QueryIOSurf(par, request);
    }
    virtual mfxStatus GetVideoParam(mfxVideoParam *par) {
        auto_ext_buffer_remove auto_buf(*par, idToRemove);
        return base::GetVideoParam(par);
    }
};
