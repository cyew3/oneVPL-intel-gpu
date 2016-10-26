//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2013-2016 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#include "umc_h265_dec_defs.h"

#ifndef __H265_MOTION_INFO_H
#define __H265_MOTION_INFO_H
#define NOT_VALID -1

#include <memory.h>

namespace UMC_HEVC_DECODER
{
#pragma pack(1)

// Motion vector class
struct H265MotionVector
{
public:

    Ipp16s Horizontal;
    Ipp16s Vertical;

    H265MotionVector()
    {
        Vertical = 0;
        Horizontal = 0;
    }

    H265MotionVector(Ipp32s horizontal, Ipp32s vertical)
    {
        Horizontal = (Ipp16s)horizontal;
        Vertical = (Ipp16s)vertical;
    }

    //set
    H265_FORCEINLINE void  setZero()
    {
        Horizontal = 0;
        Vertical = 0;
    }

    const H265MotionVector operator + (const H265MotionVector& MV) const
    {
        return H265MotionVector(Horizontal + MV.Horizontal, Vertical + MV.Vertical);
    }

    const H265MotionVector operator - (const H265MotionVector& MV) const
    {
        return H265MotionVector(Horizontal - MV.Horizontal, Vertical - MV.Vertical);
    }

    bool operator == (const H265MotionVector& MV) const
    {
        return (Horizontal == MV.Horizontal && Vertical == MV.Vertical);
    }

    bool operator != (const H265MotionVector& MV) const
    {
        return (Horizontal != MV.Horizontal || Vertical != MV.Vertical);
    }

    const H265MotionVector scaleMV(Ipp32s scale) const
    {
        Ipp32s mvx = Clip3(-32768, 32767, (scale * Horizontal + 127 + (scale * Horizontal < 0)) >> 8);
        Ipp32s mvy = Clip3(-32768, 32767, (scale * Vertical + 127 + (scale * Vertical < 0)) >> 8);
        return H265MotionVector((Ipp16s)mvx, (Ipp16s)mvy);
    }
};

// Parameters for AMVP
struct AMVPInfo
{
    H265MotionVector MVCandidate[AMVP_MAX_NUM_CAND + 1]; //array of motion vector predictor candidates
    Ipp32s NumbOfCands;
};

typedef Ipp8s RefIndexType;
// Structure used for motion info. Contains both ref index and POC since both are used in different places of code
struct H265MVInfo
{
public:
    H265MotionVector    m_mv[2];
    RefIndexType        m_refIdx[2];
    Ipp8s               m_index[2];

    void setMVInfo(Ipp32s refList, RefIndexType iRefIdx, H265MotionVector const &cMV)
    {
        m_refIdx[refList] = iRefIdx;
        m_mv[refList] = cMV;
    }
};

#pragma pack()

} // end namespace UMC_HEVC_DECODER

#endif //__H265_MOTION_INFO_H
#endif // UMC_ENABLE_H265_VIDEO_DECODER
