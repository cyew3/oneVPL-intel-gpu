/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2008-2019 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#ifndef __MFX_IRENDER_H
#define __MFX_IRENDER_H

#include "mfxstructures.h"
#include "mfx_iclonebale.h"
#include "mfx_ifile.h"
#include "mfx_ihw_device.h"

class IMFXVideoRender
    : public ICloneable
{
public:
    virtual mfxStatus Query(mfxVideoParam *in, mfxVideoParam *out) = 0;
    virtual mfxStatus QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request) = 0;
    virtual mfxStatus Init(mfxVideoParam *par, const vm_char *pFilename = NULL) = 0;
    virtual mfxStatus Close() = 0;
    virtual mfxStatus Reset(mfxVideoParam *par) = 0;
    virtual mfxStatus GetVideoParam(mfxVideoParam *par) = 0; 
    virtual mfxStatus GetEncodeStat(mfxEncodeStat *stat) = 0;
    virtual mfxStatus RenderFrame(mfxFrameSurface1 *surface, mfxEncodeCtrl * pCtrl = NULL) = 0;
    
    virtual mfxStatus WaitTasks(mfxU32 nMilisecconds) = 0;

    virtual mfxStatus SetOutputFourcc(mfxU32 nFourCC) = 0;
    ///auto view means render will be a view render if there is a sequence description attached to
    ///initialized parameters, by default render will render in single file
    virtual mfxStatus SetAutoView(bool bIsAutoViewRender) = 0;
    
    //render may have downstream object
    virtual mfxStatus GetDownStream(IFile **ppFile) = 0;
    //set will not delete object if it already existed
    virtual mfxStatus SetDownStream(IFile *ppFile) = 0;

    ///icloneable changing baseinterface
    virtual IMFXVideoRender * Clone()  = 0;

    //render might have this interface
    virtual mfxStatus GetDevice(IHWDevice **pDevice) = 0;
};

#endif//__MFX_IRENDER_H
