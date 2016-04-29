/******************************************************************************\
Copyright (c) 2005-2015, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

#pragma once

#include "mfxstructures.h"
#include "interface.h"
#include "sample_defs.h"

//actions family that are not subject of repeate
class NonRepeatableAction
    : public IAction
{
public:
    virtual bool IsRepeatUntillIDR()
    {
        return false;
    }
    virtual bool IsPermanent()
    {
        return false;
    }
};

class ChangeBitrateAction
    : public NonRepeatableAction
{
public:
    ChangeBitrateAction (double dBitrateScale)
        : m_dBitrateScale(dBitrateScale)
    {
    }
    virtual mfxStatus ApplyFeedBack(IPipeline * pPipeline)
    {
        MSDK_CHECK_POINTER(pPipeline, MFX_ERR_NULL_PTR);
        return pPipeline->ScaleBitrate(m_dBitrateScale);
    }
protected:
    double m_dBitrateScale;
};

class KeyFrameInsertAction
    : public NonRepeatableAction
{
public:
    virtual mfxStatus ApplyFeedBack(IPipeline* pPipeline)
    {
        MSDK_CHECK_POINTER(pPipeline, MFX_ERR_NULL_PTR);
        return pPipeline->InsertKeyFrame();
    }
};

//instructs MediaSDK to put specific frame to specific reference list
class PutFrameIntoRefListAction
    : public IAction
{
public:
    PutFrameIntoRefListAction(RefListType nRefList
        , mfxU32 nFrameOrderToPut
        , bool bActiveUntillIDR)
        : m_nRefList(nRefList)
        , m_nFrameOrder(nFrameOrderToPut)
        , m_bActiveUntillIDR(bActiveUntillIDR)
    {
    }
    virtual mfxStatus ApplyFeedBack(IPipeline* pPipeline)
    {
        MSDK_CHECK_POINTER(pPipeline, MFX_ERR_NULL_PTR);
        return pPipeline->AddFrameToRefList(m_nRefList, m_nFrameOrder);
    }
    virtual bool IsRepeatUntillIDR()
    {
        return m_bActiveUntillIDR;
    }
    virtual bool IsPermanent()
    {
        return false;
    }
protected:
    RefListType m_nRefList;
    mfxU32      m_nFrameOrder;
    bool        m_bActiveUntillIDR;
};

class SetL0SizeAction
    : public NonRepeatableAction
{
public:
    SetL0SizeAction (mfxU16 nL0Active)
        : m_nL0Active(nL0Active)
    {
    }
    virtual mfxStatus ApplyFeedBack(IPipeline * pPipeline)
    {
        MSDK_CHECK_POINTER(pPipeline, MFX_ERR_NULL_PTR);
        return pPipeline->SetNumL0Active(m_nL0Active);
    }

protected:
    mfxU16 m_nL0Active;
};

//reset pipeline by changing source file, resolution, bitrate, etc
class SetSourceAction
    : public NonRepeatableAction
{

public:
    SetSourceAction(int idx)
        : m_idx(idx)
    {
    }
    virtual mfxStatus ApplyFeedBack(IPipeline * pPipeline)
    {
        MSDK_CHECK_POINTER(pPipeline, MFX_ERR_NULL_PTR);
        return pPipeline->ResetPipeline(m_idx);
    }

protected:
      int m_idx;
};