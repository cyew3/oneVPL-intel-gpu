// Copyright (c) 2021 Intel Corporation
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

#pragma once

#include "mfxstructures.h"

inline
bool operator==(mfxFrameId const& i1, mfxFrameId const& i2)
{
    return
           i1.TemporalId   == i2.TemporalId
        && i1.PriorityId   == i2.PriorityId
        && i1.DependencyId == i2.DependencyId
        && i1.QualityId    == i2.QualityId
        ;
}

inline
bool operator==(mfxFrameInfo const& i1, mfxFrameInfo const& i2)
{
    return
           i1.ChannelId      == i2.ChannelId
        && i1.BitDepthLuma   == i1.BitDepthLuma
        && i1.BitDepthChroma == i1.BitDepthChroma
        && i1.Shift          == i1.Shift
        && i1.FrameId        == i1.FrameId
        && i1.FourCC         == i1.FourCC
        && i1.Width          == i1.Width
        && i1.Height         == i1.Height
        && i1.CropX          == i1.CropX
        && i1.CropY          == i1.CropY
        && i1.CropW          == i1.CropW
        && i1.CropH          == i1.CropH
        && i1.FrameRateExtN  == i1.FrameRateExtN
        && i1.FrameRateExtD  == i1.FrameRateExtD
        && i1.AspectRatioW   == i1.AspectRatioW
        && i1.AspectRatioH   == i1.AspectRatioH
        && i1.PicStruct      == i1.PicStruct
        && i1.ChromaFormat   == i1.ChromaFormat
        ;
}

inline
bool operator==(mfxFrameAllocRequest const& r1, mfxFrameAllocRequest const& r2)
{
    return
           r1.AllocId           == r2.AllocId
        && r1.Info              == r2.Info
        && r1.Type              == r2.Type
        && r1.NumFrameMin       == r2.NumFrameMin
        && r1.NumFrameSuggested == r2.NumFrameSuggested
        ;
}

inline
bool operator==(mfxFrameAllocResponse const& r1, mfxFrameAllocResponse const& r2)
{
    return
           r1.AllocId        == r2.AllocId
        && r1.NumFrameActual == r2.NumFrameActual
        && r1.MemType        == r2.MemType
        && std::equal(r1.mids, r1.mids + r1.NumFrameActual, r2.mids)
        ;
}
