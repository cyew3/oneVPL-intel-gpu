//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2013-2018 Intel Corporation. All Rights Reserved.
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

    int16_t Horizontal;
    int16_t Vertical;

    H265MotionVector()
    {
        Vertical = 0;
        Horizontal = 0;
    }

    H265MotionVector(int32_t horizontal, int32_t vertical)
    {
        Horizontal = (int16_t)horizontal;
        Vertical = (int16_t)vertical;
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

    const H265MotionVector scaleMV(int32_t scale) const
    {
        int32_t mvx = Clip3(-32768, 32767, (scale * Horizontal + 127 + (scale * Horizontal < 0)) >> 8);
        int32_t mvy = Clip3(-32768, 32767, (scale * Vertical + 127 + (scale * Vertical < 0)) >> 8);
        return H265MotionVector((int16_t)mvx, (int16_t)mvy);
    }
};

// Parameters for AMVP
struct AMVPInfo
{
    H265MotionVector MVCandidate[AMVP_MAX_NUM_CAND + 1]; //array of motion vector predictor candidates
    int32_t NumbOfCands;
};

typedef int8_t RefIndexType;
// Structure used for motion info. Contains both ref index and POC since both are used in different places of code
struct H265MVInfo
{
public:
    H265MotionVector    m_mv[2];
    RefIndexType        m_refIdx[2];
    int8_t               m_index[2];

    void setMVInfo(int32_t refList, RefIndexType iRefIdx, H265MotionVector const &cMV)
    {
        m_refIdx[refList] = iRefIdx;
        m_mv[refList] = cMV;
    }
};

#pragma pack()

} // end namespace UMC_HEVC_DECODER

#endif //__H265_MOTION_INFO_H
#endif // UMC_ENABLE_H265_VIDEO_DECODER
