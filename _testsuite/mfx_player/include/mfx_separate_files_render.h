/******************************************************************************* *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

File Name: .h

\* ****************************************************************************** */

#pragma once

#include "mfx_rnd_wrapper.h"

//reopened downstream IFile object on reset call
class SeparateFilesRender : public InterfaceProxy<IMFXVideoRender>
{
public:
    SeparateFilesRender(IMFXVideoRender * pDecorated)
        : InterfaceProxy<IMFXVideoRender>(pDecorated)
    {
    
    }
    virtual mfxStatus Init(mfxVideoParam *par, const vm_char *pFilename = NULL)
    {
        IFile * pFile;
        MFX_CHECK_STS(InterfaceProxy<IMFXVideoRender>::GetDownStream(&pFile));
        if (NULL != pFile)
        {
            MFX_CHECK_STS(pFile->Reopen());
        }
        return InterfaceProxy<IMFXVideoRender>::Init(par, pFilename);
    }
    virtual mfxStatus Reset(mfxVideoParam *par)
    {
        IFile * pFile;
        MFX_CHECK_STS(InterfaceProxy<IMFXVideoRender>::GetDownStream(&pFile))
        MFX_CHECK_STS(pFile->Reopen());
        return InterfaceProxy<IMFXVideoRender>::Reset(par);
    }
};
