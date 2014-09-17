/******************************************************************************* *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfx_renders.h"

class VideoDataChecker_MFX_Base;

class MFXOutlineRender : public MFXFileWriteRender
{
    IMPLEMENT_CLONE(MFXOutlineRender);
public:
    MFXOutlineRender(const FileWriterRenderInputParams & params, IVideoSession *core, mfxStatus *status);
    ~MFXOutlineRender();

    virtual mfxStatus Init(mfxVideoParam *par, const vm_char *pFilename = NULL);
    virtual mfxStatus RenderFrame(mfxFrameSurface1 * pSurface = NULL, mfxEncodeCtrl * pCtrl = NULL);
    virtual mfxStatus Reset(mfxVideoParam *par);
    virtual mfxStatus Close();

    void SetDecoderVideoParamsPtr(mfxVideoParam *par);
    void SetFiles( const vm_char *pOutputFilename
        , const vm_char *pRefFilename);

    void IsNeedToCheckFramesNumber(bool check); // true by default

private:
    VideoDataChecker_MFX_Base* m_checker;

    mfxVideoParam * m_decoderParams;
    bool m_wasSequenceProcessed;
    bool m_wasInitialized;
    bool m_isNeedToCheckFramesNumber;

    const vm_char *m_refOutline;
    const vm_char *m_FWRenderFile;

    mfxStatus ProcessSequence(mfxVideoParam *par);
    mfxStatus InternalInit(mfxVideoParam *par, const vm_char *pFilename = NULL);
    mfxStatus InternalClose();
};
