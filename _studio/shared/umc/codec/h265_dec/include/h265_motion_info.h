/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2013 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#include "umc_h265_dec_defs_dec.h"

#ifndef __H265_MOTION_INFO_H
#define __H265_MOTION_INFO_H
#define NOT_VALID -1

#include <memory.h>

namespace UMC_HEVC_DECODER
{

// ML: OPT: significant overhead if not inlined (ICC does not honor implied 'inline' with -Qipo)
// ML: OPT: TODO: Make sure compiler recognizes saturation idiom for vectorization when used
#if 1
#define Clip3( m_Min, m_Max, m_Value) ( (m_Value) < (m_Min) ? \
                                                  (m_Min) : \
                                                ( (m_Value) > (m_Max) ? \
                                                              (m_Max) : \
                                                              (m_Value) ) )
#else
// ML: OPT: TODO: Not sure why the below template is not as good (fast) as the macro above
template <class T> 
H265_FORCEINLINE 
T Clip3(const T& Min, const T& Max, T Value)
{
    Value = (Value < Min) ? Min : Value;
    Value = (Value > Max) ? Max : Value;
    return ( Value );
    // return ( Value < Min ? Min : (Value > Max) ? Max : Value ); 
#endif

//structure vector class
struct H265MotionVector
{
public:

    Ipp16s Horizontal;
    Ipp16s Vertical;

    H265MotionVector()
    {
        Horizontal = 0;
        Vertical = 0;
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

    //get
    H265_FORCEINLINE Ipp32s getAbsHor() const { return abs(Horizontal); }
    H265_FORCEINLINE Ipp32s getAbsVer() const { return abs(Vertical);   }

    //const H265MotionVector& operator += (const H265MotionVector& MV)
    //{
    //    Horizontal += MV.Horizontal;
    //    Vertical += MV.Vertical;
    //    return  *this;
    //}

    /*const H265MotionVector& operator -= (const H265MotionVector& MV)
    {
        Horizontal -= MV.Horizontal;
        Vertical -= MV.Vertical;
        return  *this;
    }*/

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

//parameters for AMVP
struct AMVPInfo
{
    H265MotionVector MVCandidate[AMVP_MAX_NUM_CAND + 1]; //array of motion vector predictor candidates
    Ipp32s NumbOfCands;
};

typedef Ipp8s RefIndexType;
//structure for motion vector with reference index
struct MVBuffer
{
public:
    H265MotionVector    MV;
    RefIndexType        RefIdx;

    MVBuffer()
    {
        RefIdx = -1; //not valid
    };

    void setMVBuffer(H265MotionVector const &cMV, RefIndexType iRefIdx)
    {
        MV = cMV;
        RefIdx = iRefIdx;
    }
};
} // end namespace UMC_HEVC_DECODER

#endif //__H265_MOTION_INFO_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
