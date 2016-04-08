/* ////////////////////////////////////////////////////////////////////////////// */
/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2010 - 2012 Intel Corporation. All Rights Reserved.
//
//
*/

#ifndef __SAMPLE_VPP_ROI_H
#define __SAMPLE_VPP_ROI_H

#include "mfxvideo.h"

typedef enum
{
    ROI_FIX_TO_FIX = 0x0001,
    ROI_VAR_TO_FIX = 0x0002,
    ROI_FIX_TO_VAR = 0x0003,
    ROI_VAR_TO_VAR = 0x0004

} eROIMode;

typedef struct
{
    eROIMode mode;

    int srcSeed;

    int dstSeed;

} sROICheckParam;

/* ************************************************************************* */

class ROIGenerator
{
public:
    ROIGenerator( void );
    ~ROIGenerator( void );

    mfxStatus Init(  mfxU16  width, mfxU16  height, int seed);
    mfxStatus Close( void );

    // need to set specific ROI from usage model of generator
    mfxStatus  SetROI(mfxFrameInfo* pInfo);

    // return seed for external application
    mfxI32     GetSeed( void );

protected:

    mfxU16  m_width;
    mfxU16  m_height;

    mfxI32  m_seed;

};

#endif /* __SAMPLE_VPP_PTS_H*/