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
    struct plugin : public VideoUSER
    {
        mfxStatus PluginInit(const mfxPlugin*, mfxSession, mfxU32) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        mfxStatus PluginClose() override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        mfxStatus Check(const mfxHDL*, mfxU32, const mfxHDL*, mfxU32, MFX_ENTRY_POINT*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
    };
    
    struct plugin_codec : public VideoCodecUSER
    {
        mfxStatus PluginInit(const mfxPlugin*, mfxSession, mfxU32) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        mfxStatus PluginClose() override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        mfxStatus Check(const mfxHDL*, mfxU32, const mfxHDL*, mfxU32, MFX_ENTRY_POINT*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        mfxStatus Init(mfxVideoParam*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        mfxStatus QueryIOSurf(VideoCORE*, mfxVideoParam*, mfxFrameAllocRequest*, mfxFrameAllocRequest*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        mfxStatus Query(VideoCORE*, mfxVideoParam*, mfxVideoParam*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        mfxStatus DecodeHeader(VideoCORE*, mfxBitstream*, mfxVideoParam*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        mfxStatus VPPFrameCheck(mfxFrameSurface1*, mfxFrameSurface1*, mfxExtVppAuxData*, MFX_ENTRY_POINT*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        mfxStatus VPPFrameCheckEx(mfxFrameSurface1*, mfxFrameSurface1*, mfxFrameSurface1**, MFX_ENTRY_POINT*) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        VideoENCODE* GetEncodePtr() override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        VideoDECODE* GetDecodePtr() override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        VideoVPP* GetVPPPtr() override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        VideoENC* GetEncPtr() override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
        void GetPlugin(mfxPlugin&) override
        { throw std::system_error(E_NOTIMPL, std::system_category()); }
    };
} } }