/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2010-2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfx_ivideo_render.h"
#include "test_method.h"

class MockRender 
    : public IMFXVideoRender
{
    IMPLEMENT_CLONE(MockRender);
    mfxFrameAllocRequest m_request;

public:
    
    virtual mfxStatus Query(mfxVideoParam * /*in*/, mfxVideoParam * /*out*/)
    {
        return MFX_ERR_NONE;
    }
    
    virtual mfxStatus Close()
    {
        return MFX_ERR_NONE;
    }
    DECLARE_TEST_METHOD1(mfxStatus, Reset, MAKE_DYNAMIC_TRAIT(mfxVideoParam,  mfxVideoParam*));
    virtual mfxStatus GetVideoParam(mfxVideoParam * /*par*/)
    {
        return MFX_ERR_NONE;
    }
    virtual mfxStatus GetEncodeStat(mfxEncodeStat * /*stat*/)
    {
        return MFX_ERR_NONE;
    }
    
    virtual mfxStatus GetDevice(IHWDevice **  /*ppDevice*/)
    {
        return MFX_ERR_NONE;
    }
    virtual mfxStatus SetOutputFourcc(mfxU32 /*nFourCC*/)
    {
        return MFX_ERR_NONE;
    }
    virtual mfxStatus GetDownStream(IFile ** /*ppDownstream*/)
    {
        return MFX_ERR_NONE;
    }
    virtual mfxStatus SetDownStream(IFile* /*pDownstream*/)
    {
        return MFX_ERR_NONE;
    }

    DECLARE_TEST_METHOD1(mfxStatus, WaitTasks, mfxU32 /*nMilisecconds*/);
    DECLARE_TEST_METHOD2(mfxStatus, QueryIOSurf, mfxVideoParam * /*par*/, mfxFrameAllocRequest * /*request*/);
    DECLARE_TEST_METHOD2(mfxStatus, Init, mfxVideoParam *, const vm_char *);
    DECLARE_TEST_METHOD2(mfxStatus, RenderFrame, mfxFrameSurface1 *, mfxEncodeCtrl * );
    DECLARE_TEST_METHOD1(mfxStatus, SetAutoView, bool );
};