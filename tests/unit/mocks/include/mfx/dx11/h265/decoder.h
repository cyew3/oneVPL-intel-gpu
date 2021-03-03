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

namespace mocks { namespace mfx { namespace h265 { namespace dx11
{
    namespace detail
    {
        template <int Type>
        inline
        mocks::dx11::device&
        mock_decoder(gen0, std::integral_constant<int, Type>, mocks::dx11::device& d, mfxVideoParam const&)
        {
            assert(!"H.265 is not supported by this Gen");
            return d;
        }

        template <int Type>
        inline
        mocks::dx11::device&
        mock_decoder(gen9, std::integral_constant<int, Type>, mocks::dx11::device& d, mfxVideoParam const& vp)
        {
            return mfx::dx11::mock_component(std::integral_constant<int, Type>{}, d
                , std::make_tuple(mocks::guid<&DXVA_ModeHEVC_VLD_Main>{}, vp)
                , std::make_tuple(mocks::guid<&DXVA_ModeHEVC_VLD_Main10>{}, vp)
            );
        }

        template <int Type>
        inline
        mocks::dx11::device&
        mock_decoder(gen11, std::integral_constant<int, Type>, mocks::dx11::device& d, mfxVideoParam const& vp)
        {
            mock_decoder(gen10{}, std::integral_constant<int, Type>{}, d, vp);

            return mfx::dx11::mock_component(std::integral_constant<int, Type>{}, d
#if defined(MFX_INTEL_GUID_DECODE)
                , std::make_tuple(mocks::guid<&DXVA_Intel_ModeHEVC_VLD_Main422_10Profile>{}, vp)
                , std::make_tuple(mocks::guid<&DXVA_Intel_ModeHEVC_VLD_Main444_10Profile>{}, vp)
                , std::make_tuple(mocks::guid<&DXVA_Intel_ModeHEVC_VLD_Main12Profile>{}, vp)
                , std::make_tuple(mocks::guid<&DXVA_Intel_ModeHEVC_VLD_Main422_12Profile>{}, vp)
                , std::make_tuple(mocks::guid<&DXVA_Intel_ModeHEVC_VLD_Main444_12Profile>{}, vp)
#endif
            );
        }

        template <int Type>
        inline
        mocks::dx11::device&
        mock_decoder(gen12, std::integral_constant<int, Type>, mocks::dx11::device& d, mfxVideoParam const& vp)
        {
            mock_decoder(gen11{}, std::integral_constant<int, Type>{}, d, vp);

            return mfx::dx11::mock_component(std::integral_constant<int, Type>{}, d
#if defined(MFX_INTEL_GUID_DECODE)
                , std::make_tuple(mocks::guid<&DXVA_Intel_ModeHEVC_VLD_SCC_Main_Profile>{}, vp)
                , std::make_tuple(mocks::guid<&DXVA_Intel_ModeHEVC_VLD_SCC_Main_10Profile>{}, vp)
                , std::make_tuple(mocks::guid<&DXVA_Intel_ModeHEVC_VLD_SCC_Main444_Profile>{}, vp)
                , std::make_tuple(mocks::guid<&DXVA_Intel_ModeHEVC_VLD_SCC_Main444_10Profile>{}, vp)
#endif
            );
        };
    } //detail

    template <int Type>
    inline
    mocks::dx11::device&
    mock_decoder(std::integral_constant<int, Type>, mocks::dx11::device& d, mfxVideoParam const& vp)
    {
        detail::mock_decoder(gen_of<Type>::type{},
            std::integral_constant<int, Type>{},
            d, vp
        );

        auto td = mfx::dx11::make_texture_desc(vp, D3D11_USAGE_DEFAULT, D3D11_BIND_DECODER, 0/*no CPU access*/);
        if (vp.IOPattern & MFX_IOPATTERN_OUT_SYSTEM_MEMORY)
        {
            td.BindFlags |= D3D11_BIND_VIDEO_ENCODER | D3D11_BIND_SHADER_RESOURCE;
            td.MiscFlags |= D3D11_RESOURCE_MISC_SHARED;
        }

        return mfx::dx11::mock_component(std::integral_constant<int, Type>{}, d,
            td,
            mfx::dx11::make_texture_desc(vp, D3D11_USAGE_STAGING, 0, D3D11_CPU_ACCESS_READ)
        );
    }

    inline
    mocks::dx11::device&
    mock_decoder(int type, mocks::dx11::device& d, mfxVideoParam const& vp)
    {
        return invoke(type,
            MOCKS_LIFT(mock_decoder),
            d, vp
        );
    }

    namespace detail
    {
        struct decoder : mfx::dx11::decoder
        {
            decoder(int type, IDXGIAdapter* adapter, mfxVideoParam const& vp)
                : mfx::dx11::decoder(type, adapter, vp)
            {
                reset(type, vp);
            }

            void reset(int type, mfxVideoParam const& vp) override
            {
                assert(device);

                h265::dx11::mock_decoder(
                    type, *device, vp
                );
            }
        };
    }

    inline
    std::unique_ptr<mfx::dx11::decoder>
    make_decoder(int type, IDXGIAdapter* adapter, mfxVideoParam const& vp)
    { return std::unique_ptr<mfx::dx11::decoder>(new detail::decoder(type, adapter, vp)); }

} } } }

