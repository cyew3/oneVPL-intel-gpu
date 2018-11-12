// Copyright (c) 2003-2018 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

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

