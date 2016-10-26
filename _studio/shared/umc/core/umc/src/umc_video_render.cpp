//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2011 Intel Corporation. All Rights Reserved.
//

#include "umc_video_render.h"

namespace UMC
{

VideoRenderParams::VideoRenderParams(void)
{
    color_format = NONE;
    memset(&info, 0, sizeof(info));
    // KW fix
    //memset(&disp, 0, sizeof(disp));
    disp.bottom = 0;
    disp.left = 0;
    disp.right = 0;
    disp.top = 0;
    memset(&range, 0, sizeof(range));
    lFlags = 0;

} // VideoRenderParams::VideoRenderParams(void)

#if defined (_WIN32) || defined (_WIN32_WCE)

void UMCRect2Rect(UMC::sRECT& umcRect, ::RECT& rect)
{
    rect.bottom = (Ipp32s) umcRect.bottom;
    rect.top = (Ipp32s) umcRect.top;
    rect.left = (Ipp32s) umcRect.left;
    rect.right = (Ipp32s) umcRect.right;

} //void UMCRect2Rect(UMC::sRECT& umcRect, ::RECT& rect)

void Rect2UMCRect(::RECT& rect, UMC::sRECT& umcRect)
{
    umcRect.bottom = (Ipp16s) rect.bottom;
    umcRect.top = (Ipp16s) rect.top;
    umcRect.left = (Ipp16s) rect.left;
    umcRect.right = (Ipp16s) rect.right;

} //void Rect2UMCRect(::RECT& rect, UMC::sRECT& umcRect)

#endif //defined (_WIN32) || defined (_WIN32_WCE)

} // namespace UMC

