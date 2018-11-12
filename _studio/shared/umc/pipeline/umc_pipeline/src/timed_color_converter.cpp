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
