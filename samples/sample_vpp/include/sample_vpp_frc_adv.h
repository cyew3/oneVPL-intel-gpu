/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2011-2012 Intel Corporation. All Rights Reserved.
//
//
*/

#ifndef __SAMPLE_VPP_FRC_ADV_H
#define __SAMPLE_VPP_FRC_ADV_H

#include <memory>
#include <list>
#include <stdio.h>

#include "mfxvideo.h"
#include "sample_vpp_frc.h"

class FRCAdvancedChecker : public BaseFRCChecker
{
public:
    FRCAdvancedChecker();
    virtual ~FRCAdvancedChecker(){};

    mfxStatus Init(mfxVideoParam *par, mfxU32 asyncDeep);

    // notify FRCChecker about one more input frame
    bool  PutInputFrameAndCheck(mfxFrameSurface1* pSurface);

    // notify FRCChecker about one more output frame and check result
    bool  PutOutputFrameAndCheck(mfxFrameSurface1* pSurface);

private:

    mfxU64    GetExpectedPTS( mfxU32 frameNumber, mfxU64 timeOffset, mfxU64 timeJump );

    bool IsTimeStampsNear( mfxU64 timeStampRef, mfxU64 timeStampTst,  mfxU64 eps);

    mfxU64 m_minDeltaTime;

    bool   m_bIsSetTimeOffset;

    mfxU64 m_timeOffset;
    mfxU64 m_expectedTimeStamp;
    mfxU64 m_timeStampJump;
    mfxU32 m_numOutputFrames;

    bool   m_bReadyOutput;
    mfxU64 m_defferedInputTimeStamp;

    mfxVideoParam m_videoParam;

    std::list<mfxU64> m_ptsList;
};

#endif /* __SAMPLE_VPP_PTS_ADV_H*/