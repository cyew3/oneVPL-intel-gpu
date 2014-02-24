/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

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