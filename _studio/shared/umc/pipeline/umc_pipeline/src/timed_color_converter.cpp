//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2010 Intel Corporation. All Rights Reserved.
//

#include "timed_color_converter.h"
#include "vm_time.h"

UMC::Status
TimedColorConverter::GetFrame(UMC::MediaData *in, UMC::MediaData *out)
{
    vm_tick tStartTime = vm_time_get_tick();
    UMC::Status umcRes = VideoProcessing::GetFrame(in, out);
    vm_tick tStopTime = vm_time_get_tick();

    m_dfConversionTime += ((Ipp64f)(Ipp64s)(tStopTime - tStartTime)) /
        (Ipp64s)vm_time_get_frequency();
    return umcRes;
} // TimedColorConverter::ConvertFrame()
