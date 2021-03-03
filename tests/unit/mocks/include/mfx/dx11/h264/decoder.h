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

namespace mocks { namespace mfx { namespace h264 { namespace dx11
{
    namespace detail
    {
        template <int Type>
        inline
        mocks::dx11::device&
        mock_decoder(gen0, std::integral_constant<int, Type>, mocks::dx11::device& d, mfxVideoParam const&)
        {
            assert(!"H.264 is not supported by this Gen");
            return d;
        }

        template <int Type>
        inline
        mocks::dx11::device&
        mock_decoder(gen9, std::integral_constant<int, Type>, mocks::dx11::device& d, mfxVideoParam const& vp)
        {
            return mfx::dx11::mock_component(std::integral_constant<int, Type>{}, d
                , std::make_tuple(mocks::guid<&DXVA2_ModeH264_VLD_NoFGT>{}, vp)
                , std::make_tuple(mocks::guid<&DXVA_ModeH264_VLD_Multiview_NoFGT>{}, vp)
                , std::make_tuple(mocks::guid<&DXVA_ModeH264_VLD_Stereo_NoFGT>{}, vp)
                , std::make_tuple(mocks::guid<&DXVA_ModeH264_VLD_Stereo_Progressive_NoFGT>{}, vp)
#if defined(ENABLE_MFX_INTEL_GUID_DECODE)
                , std::make_tuple(mocks::guid<&DXVA_Intel_ModeH264_VLD_MVC>{}, vp)
#endif
            );
        }
    }

    template <int Type>
    inline
    mocks::dx11::device&
    mock_decoder(std::integral_constant<int, Type>, mocks::dx11::device& d, mfxVideoParam const& vp)
    {
        return detail::mock_decoder(gen_of<Type>::type{},
            std::integral_constant<int, Type>{},
            d, vp
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

    inline
    std::unique_ptr<mfx::dx11::decoder>
    make_decoder(int type, IDXGIAdapter* adapter, mfxVideoParam const& vp)
    { return std::unique_ptr<mfx::dx11::decoder>(new mfx::dx11::decoder(type, adapter, vp)); }

} } } }
