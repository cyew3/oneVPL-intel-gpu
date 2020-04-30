// Copyright (c) 2019-2020 Intel Corporation
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

#include "mocks/include/dxva/traits.h"

#include "mfxstructures.h"

namespace mocks { namespace mfx
{
    template <GUID const*, typename Enable = void>
    struct codecof;

    template <GUID const* id>
    struct codecof<id, typename std::enable_if<dxva::is_h264_codec<id>::value>::type>
        : std::integral_constant<unsigned, MFX_CODEC_AVC>
    {};

    template <GUID const* id>
    struct codecof<id, typename std::enable_if<dxva::is_h265_codec<id>::value>::type>
        : std::integral_constant<unsigned, MFX_CODEC_HEVC>
    {};

#if (MFX_VERSION >= MFX_VERSION_NEXT) && !defined(STRIP_EMBARGO)
    template <GUID const* id>
    struct codecof<id, typename std::enable_if<dxva::is_av1_codec<id>::value>::type>
        : std::integral_constant<unsigned, MFX_CODEC_AV1>
    {};
#endif

    template <GUID const* id>
    inline
    mfxVideoParam make_param(guid<id>, mfxVideoParam const& p)
    {
        mfxVideoParam r = p;
        r.mfx.CodecId = codecof<id>::value;
        return r;
    }

    inline
    mfxVideoParam make_param(fourcc::tag<MFX_FOURCC_NV12>, mfxVideoParam const& p)
    {
        mfxVideoParam r = p;
        r.mfx.FrameInfo.BitDepthLuma = 8;
        r.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        r.mfx.FrameInfo.FourCC       = MFX_FOURCC_NV12;
        return r;
    }

    inline
    mfxVideoParam make_param(fourcc::tag<MFX_FOURCC_P010>, mfxVideoParam const& p)
    {
        mfxVideoParam r = p;
        r.mfx.FrameInfo.BitDepthLuma = 10;
        r.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        r.mfx.FrameInfo.FourCC       = MFX_FOURCC_P010;
        return r;
    }

    inline
    mfxVideoParam make_param(fourcc::tag<MFX_FOURCC_P016>, mfxVideoParam const& p)
    {
        mfxVideoParam r = p;
        r.mfx.FrameInfo.BitDepthLuma = 12;
        r.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        r.mfx.FrameInfo.FourCC       = MFX_FOURCC_P016;
        return r;
    }

} }
