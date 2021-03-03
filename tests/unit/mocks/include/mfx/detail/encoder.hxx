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

#include "mfxvideo++int.h"

namespace mocks { namespace mfx { namespace api
{
    struct encoder : public VideoENCODE
    {
        mfxStatus Init(mfxVideoParam*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        mfxStatus Reset(mfxVideoParam*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        mfxStatus Close() override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        mfxStatus GetVideoParam(mfxVideoParam*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        mfxStatus GetFrameParam(mfxFrameParam*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        mfxStatus GetEncodeStat(mfxEncodeStat*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        mfxStatus EncodeFrameCheck(mfxEncodeCtrl*, mfxFrameSurface1*, mfxBitstream*, mfxFrameSurface1**, mfxEncodeInternalParams*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        mfxStatus EncodeFrame(mfxEncodeCtrl*, mfxEncodeInternalParams*, mfxFrameSurface1*, mfxBitstream*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        mfxStatus CancelFrame(mfxEncodeCtrl*, mfxEncodeInternalParams*, mfxFrameSurface1*, mfxBitstream*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
    };
} } }