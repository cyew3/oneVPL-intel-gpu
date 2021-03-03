// Copyright (c) 2020 Intel Corporation
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

#include "mocks/include/common.h"
#include "mocks/include/mfx/detail/encoder.hxx"

namespace mocks { namespace mfx
{ 
    struct encoder : api::encoder
    {
        MOCK_METHOD(mfxStatus, Init, (mfxVideoParam*), (override));
        MOCK_METHOD(mfxStatus, Reset, (mfxVideoParam*), (override));
        MOCK_METHOD(mfxStatus, Close, (), (override));
        MOCK_METHOD(mfxStatus, GetVideoParam, (mfxVideoParam*), (override));
        MOCK_METHOD(mfxStatus, GetFrameParam, (mfxFrameParam*), (override));
        MOCK_METHOD(mfxStatus, GetEncodeStat, (mfxEncodeStat*), (override));
        MOCK_METHOD(mfxStatus, EncodeFrameCheck, (mfxEncodeCtrl*, mfxFrameSurface1*, mfxBitstream*, mfxFrameSurface1**, mfxEncodeInternalParams*), (override));
        MOCK_METHOD(mfxStatus, EncodeFrame, (mfxEncodeCtrl*, mfxEncodeInternalParams*, mfxFrameSurface1*, mfxBitstream*), (override));
        MOCK_METHOD(mfxStatus, CancelFrame, (mfxEncodeCtrl*, mfxEncodeInternalParams*, mfxFrameSurface1*, mfxBitstream*), (override));
    };
} }
