//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2018 Intel Corporation. All Rights Reserved.
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
    rect.bottom = (int32_t) umcRect.bottom;
    rect.top = (int32_t) umcRect.top;
    rect.left = (int32_t) umcRect.left;
    rect.right = (int32_t) umcRect.right;

} //void UMCRect2Rect(UMC::sRECT& umcRect, ::RECT& rect)

void Rect2UMCRect(::RECT& rect, UMC::sRECT& umcRect)
{
    umcRect.bottom = (int16_t) rect.bottom;
    umcRect.top = (int16_t) rect.top;
    umcRect.left = (int16_t) rect.left;
    umcRect.right = (int16_t) rect.right;

} //void Rect2UMCRect(::RECT& rect, UMC::sRECT& umcRect)

#endif //defined (_WIN32) || defined (_WIN32_WCE)

} // namespace UMC

