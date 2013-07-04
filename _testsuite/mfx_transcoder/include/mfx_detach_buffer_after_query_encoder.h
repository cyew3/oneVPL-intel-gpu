/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2013 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#pragma once

#include <mfx_ivideo_encode.h>

//detach specific buffer prior encoding initialze
template <class TDetachBuffer>
class DetachExtBufferEncode : public InterfaceProxy<IVideoEncode> {
    typedef InterfaceProxy<IVideoEncode> base;
public:
    DetachExtBufferEncode(std::auto_ptr<IVideoEncode>& pEnc)
        : base(pEnc) {
    }
    
    virtual mfxStatus Init(mfxVideoParam *par) {
        std::vector<mfxExtBuffer*> buffers;
        for (size_t i = 0; i < par->NumExtParam; i++)
        {
            if (par->ExtParam[i]->BufferId == BufferIdOf<TDetachBuffer>::id)
                continue;

            buffers.push_back(par->ExtParam[i]);
        }
        mfxExtBuffer ** pOld = par->ExtParam;
        mfxU16 numExtOld = par->NumExtParam;
        par->ExtParam = &buffers.front();
        par->NumExtParam = (mfxU16)buffers.size();
        mfxStatus sts = base::Init(par);
        par->ExtParam = pOld;
        par->NumExtParam = numExtOld;
        return sts;
    }
};