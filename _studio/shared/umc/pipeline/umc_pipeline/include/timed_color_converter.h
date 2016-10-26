//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2010 Intel Corporation. All Rights Reserved.
//

#ifndef __TIMED_COLOR_CONVERTOR_H__
#define __TIMED_COLOR_CONVERTOR_H__

#include "umc_video_processing.h"

class TimedColorConverter: public UMC::VideoProcessing
{
public:
    TimedColorConverter() :m_dfConversionTime(0.0) {}

    // Convert image(s)
    virtual UMC::Status GetFrame(UMC::MediaData *in, UMC::MediaData *out);

    Ipp64f GetConversionTime() {    return m_dfConversionTime;  }
    void ResetConversionTime() {    m_dfConversionTime = 0.0;   }

protected:
    Ipp64f m_dfConversionTime;
};

#endif // __TIMED_COLOR_CONVERTOR_H__
