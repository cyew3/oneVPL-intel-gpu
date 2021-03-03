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

#include "mocks/include/mfx/guids.h"
#include "mocks/include/mfx/dx11/component.h"

namespace mocks { namespace mfx { namespace dx11
{
    struct decoder : component
    {
        using component::component;
    };

} } }

#include "mocks/include/mfx/dx11/h264/decoder.h"
#include "mocks/include/mfx/dx11/h265/decoder.h"
#if defined(MFX_ENABLE_AV1_VIDEO_DECODE)
#include "mocks/include/mfx/dx11/av1/decoder.h"
#endif

namespace mocks { namespace mfx { namespace dx11
{
    template <GUID const* Id>
    inline
    typename std::enable_if<dxva::is_decoder(guid<Id>()), D3D11_VIDEO_DECODER_CONFIG>::type
    make_config(guid<Id>, mfxVideoParam const&)
    {
        D3D11_VIDEO_DECODER_CONFIG config{};
        config.ConfigBitstreamRaw = 3; //both long & short slices

        return config;
    }

    template <int Type, GUID const* Id>
    inline
    typename std::enable_if<dxva::is_decoder(guid<Id>()), D3D11_VIDEO_DECODER_CONFIG>::type
    make_caps(std::integral_constant<int, Type>, guid<Id>, mfxVideoParam const& vp)
    {
        D3D11_VIDEO_DECODER_CONFIG caps{};
        caps.guidConfigBitstreamEncryption = *Id;
        caps.ConfigMBcontrolRasterOrder    = vp.mfx.FrameInfo.Width;
        caps.ConfigResidDiffHost           = vp.mfx.FrameInfo.Height;
        caps.ConfigBitstreamRaw            = mfx::family(Type);

        return caps;
    }

    inline
    std::unique_ptr<decoder>
    make_decoder(int type, IDXGIAdapter* adapter, mfxVideoParam const& vp)
    {
        switch (vp.mfx.CodecId)
        {
            case MFX_CODEC_AVC:  return h264::dx11::make_decoder(type, adapter, vp);
            case MFX_CODEC_HEVC: return h265::dx11::make_decoder(type, adapter, vp);

#if defined(MFX_ENABLE_AV1_VIDEO_DECODE)
            case MFX_CODEC_AV1:  return av1::dx11::make_decoder(type, adapter, vp);
#endif //MFX_ENABLE_AV1_VIDEO_DECODE

            default:
                throw std::logic_error("Codec is not supported");
        }
    }

} } }
