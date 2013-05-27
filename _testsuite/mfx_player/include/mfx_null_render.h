/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once


class MFXLockUnlockRender : public MFXVideoRender
{
    IMPLEMENT_CLONE(MFXLockUnlockRender);
public:
    MFXLockUnlockRender(IVideoSession *core, mfxStatus *status):MFXVideoRender(core, status)
    {
    }

    virtual mfxStatus RenderFrame(mfxFrameSurface1 *surface, mfxEncodeCtrl * /*pCtrl = NULL*/)
    {
        MFX_CHECK_STS(LockFrame(surface));
        MFX_CHECK_STS(UnlockFrame(surface));
        return MFX_ERR_NONE;
    }
};


class MFXNullRender : public MFXVideoRender
{
    IMPLEMENT_CLONE(MFXNullRender);
public:
    MFXNullRender(IVideoSession *core, mfxStatus *status):MFXVideoRender(core, status)
    {
    }
    //that is why it is NULL
    virtual mfxStatus RenderFrame(mfxFrameSurface1 * surface, mfxEncodeCtrl * pCtrl = NULL)
    {
        //for debugging, it it useful to see when frame actually gets rendered
        PipelineTraceEncFrame(0);
        return MFXVideoRender::RenderFrame(surface, pCtrl);
    }
};
