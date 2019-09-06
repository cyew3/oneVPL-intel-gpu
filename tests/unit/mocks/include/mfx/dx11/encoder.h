// Copyright (c) 2019 Intel Corporation
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

#include "mocks/include/mfx/dx11/component.h"

namespace mocks { namespace mfx { namespace dx11
{
    template <GUID const* Id, int Type>
    inline
    typename std::enable_if<mocks::dxva::is_encoder<Id>::value, D3D11_VIDEO_DECODER_CONFIG>::type
    make_caps(mocks::guid<Id>, std::integral_constant<int, Type>, mfxVideoParam const& p)
    {
        D3D11_VIDEO_DECODER_CONFIG caps{};
        caps.guidConfigBitstreamEncryption = *Id;
        caps.ConfigMBcontrolRasterOrder    = p.mfx.FrameInfo.Width;
        caps.ConfigResidDiffHost           = p.mfx.FrameInfo.Height;
        caps.ConfigBitstreamRaw            = mocks::mfx::family(Type);

        return caps;
    }

    template <GUID const* Id>
    inline
    typename std::enable_if<mocks::dxva::is_encoder<Id>::value, D3D11_VIDEO_DECODER_CONFIG>::type
    make_config(mocks::guid<Id>, mfxVideoParam const&)
    {
        D3D11_VIDEO_DECODER_CONFIG config{};
        config.ConfigDecoderSpecific = 4;
        config.guidConfigBitstreamEncryption = DXVA_NoEncrypt;

        return config;
    }

    template <int Type, typename ...Args>
    inline
    std::unique_ptr<mocks::dx11::device>
    make_encoder(IDXGIAdapter* adapter, ID3D11DeviceContext* context, std::integral_constant<int, Type>, Args&&... args)
    {
        return make_component(adapter, context,
            std::integral_constant<int, Type>{},
            std::forward<Args>(args)...
        );
    }

    template <typename ...Args>
    inline
    std::unique_ptr<mocks::dx11::device>
    make_encoder(IDXGIAdapter* adapter, ID3D11DeviceContext* context, int type, Args&&... args)
    {
        return make_component(adapter, context,
            type,
            std::forward<Args>(args)...
        );
    }

} } }
