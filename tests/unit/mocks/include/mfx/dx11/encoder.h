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

#include "mfx_common_int.h"

namespace mocks { namespace mfx { namespace dx11
{
    struct encoder : component
    {
        encoder(int type, IDXGIAdapter* adapter, mfxVideoParam const& vp)
            : component(type, adapter, vp)
        {}

        virtual void reset(int /*type*/, mfxVideoParam const&)
        {}
    };

} } }

#include "mocks/include/mfx/dx11/h265/encoder.h"

namespace mocks { namespace mfx { namespace dx11
{
    template <GUID const* Id>
    inline
    typename std::enable_if<dxva::is_encoder(guid<Id>()), D3D11_VIDEO_DECODER_CONFIG>::type
    make_config(guid<Id>, mfxVideoParam const&)
    {
        D3D11_VIDEO_DECODER_CONFIG config{};
        config.ConfigDecoderSpecific = 4;
        config.guidConfigBitstreamEncryption = DXVA_NoEncrypt;

        return config;
    }

    template <GUID const* Id, int Type>
    inline
    typename std::enable_if<dxva::is_encoder(guid<Id>()), D3D11_VIDEO_DECODER_CONFIG>::type
    make_caps(std::integral_constant<int, Type>, guid<Id>, mfxVideoParam const& p)
    {
        D3D11_VIDEO_DECODER_CONFIG caps{};
        caps.guidConfigBitstreamEncryption = *Id;
        caps.ConfigMBcontrolRasterOrder    = p.mfx.FrameInfo.Width;
        caps.ConfigResidDiffHost           = p.mfx.FrameInfo.Height;
        caps.ConfigBitstreamRaw            = mfx::family(Type);

        return caps;
    }

    inline
    D3D11_TEXTURE2D_DESC make_texture_desc(GUID const& id, mfxVideoParam const& vp, D3D11_USAGE usage, UINT access)
    {
        if (!dxva::is_encoder(id))
            throw std::logic_error("Given \'id\' must be encoder's one");

        return make_texture_desc(
            vp, usage, D3D11_BIND_DECODER | D3D11_BIND_VIDEO_ENCODER, access
        );
    }

    template <GUID const* Id>
    inline
    typename std::enable_if<dxva::is_encoder(guid<Id>()), D3D11_TEXTURE2D_DESC>::type
    make_texture_desc(guid<Id>, mfxVideoParam const& vp, D3D11_USAGE usage, UINT access)
    {
        return make_texture_desc(
            *Id, vp, usage, D3D11_BIND_DECODER | D3D11_BIND_VIDEO_ENCODER, access
        );
    }

    inline
    auto make_encoder(int type, IDXGIAdapter* adapter, mfxVideoParam const& vp)
    {
        switch (vp.mfx.CodecId)
        {
            case MFX_CODEC_HEVC: return h265::dx11::make_encoder(type, adapter, vp);

            default:
                throw std::logic_error("Codec is not supported");
        }
    }

} } }
