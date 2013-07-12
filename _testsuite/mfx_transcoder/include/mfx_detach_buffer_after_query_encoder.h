/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2013 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#pragma once

#include <mfx_ivideo_encode.h>
#include "mfx_extended_buffer.h"

//detach specific buffer prior encoding initialize
template <class TDetachBuffer>
class DetachExtBufferEncode : public InterfaceProxy<IVideoEncode> {
    typedef InterfaceProxy<IVideoEncode> base;
public:
    DetachExtBufferEncode(std::auto_ptr<IVideoEncode>& pEnc)
        : base(pEnc) {
    }
    
    virtual mfxStatus Init(mfxVideoParam *par) {
        auto_ext_buffer auto_buf(*par);
        auto_buf.remove(BufferIdOf<TDetachBuffer>::id);

        return base::Init(par);
    }
    virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request) {
        auto_ext_buffer auto_buf(*par);
        auto_buf.remove(BufferIdOf<TDetachBuffer>::id);

        return base::QueryIOSurf(par, request);
    }
};