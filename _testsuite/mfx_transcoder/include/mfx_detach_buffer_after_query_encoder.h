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
        DetachBuffers(par);
        mfxStatus sts = base::Init(par);
        RestoreBuffers(par);
        return sts;
    }
    virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request) {
        DetachBuffers(par);
        mfxStatus sts = base::QueryIOSurf(par, request);
        RestoreBuffers(par);
        return sts;
    }
protected:
    
    mfxExtBuffer ** m_pOld;
    mfxU16 m_numExtOld;
    std::vector<mfxExtBuffer*> buffers;

    void DetachBuffers(mfxVideoParam *par) {
        for (size_t i = 0; i < par->NumExtParam; i++)
        {
            if (par->ExtParam[i]->BufferId == BufferIdOf<TDetachBuffer>::id)
                continue;

            buffers.push_back(par->ExtParam[i]);
        }
        m_pOld = par->ExtParam;
        m_numExtOld = par->NumExtParam;

        par->ExtParam = buffers.empty() ? NULL : &buffers.front();
        par->NumExtParam = (mfxU16)buffers.size();
    }
    void RestoreBuffers(mfxVideoParam *par) {
        par->ExtParam = m_pOld;
        par->NumExtParam = m_numExtOld;
    }
};