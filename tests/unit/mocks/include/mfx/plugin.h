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
#include "mocks/include/mfx/detail/plugin.hxx"

namespace mocks { namespace mfx
{ 
    struct plugin_codec : api::plugin_codec
    {
        MOCK_METHOD(mfxStatus, PluginInit, (const mfxPlugin*, mfxSession, mfxU32), (override));
        MOCK_METHOD(mfxStatus, PluginClose, (), (override));
        MOCK_METHOD(mfxStatus, Check, (const mfxHDL*, mfxU32, const mfxHDL*, mfxU32, MFX_ENTRY_POINT*), (override));
        MOCK_METHOD(mfxStatus, Init, (mfxVideoParam*), (override));
        MOCK_METHOD(mfxStatus, QueryIOSurf, (VideoCORE*, mfxVideoParam*, mfxFrameAllocRequest*, mfxFrameAllocRequest*), (override));
        MOCK_METHOD(mfxStatus, Query, (VideoCORE*, mfxVideoParam*, mfxVideoParam*), (override));
        MOCK_METHOD(mfxStatus, DecodeHeader, (VideoCORE*, mfxBitstream*, mfxVideoParam*), (override));
        MOCK_METHOD(mfxStatus, VPPFrameCheck, (mfxFrameSurface1*, mfxFrameSurface1*, mfxExtVppAuxData*, MFX_ENTRY_POINT*), (override));
        MOCK_METHOD(mfxStatus, VPPFrameCheckEx, (mfxFrameSurface1*, mfxFrameSurface1*, mfxFrameSurface1**, MFX_ENTRY_POINT*), (override));
        MOCK_METHOD(VideoENCODE*, GetEncodePtr, (), (override));
        MOCK_METHOD(VideoDECODE*, GetDecodePtr, (), (override));
        MOCK_METHOD(VideoVPP*, GetVPPPtr, (), (override));
        MOCK_METHOD(VideoENC*, GetEncPtr, (), (override));
        MOCK_METHOD(void, GetPlugin, (mfxPlugin&), (override));
    };
} }
