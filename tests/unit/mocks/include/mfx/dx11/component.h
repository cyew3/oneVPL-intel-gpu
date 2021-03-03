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

#include "mocks/include/dxgi/format.h"
#include "mocks/include/dx11/device/device.h"
#include "mocks/include/dx11/device/video.h"
#include "mocks/include/dx11/device/resource.h"

#include "mocks/include/dx11/context/context.h"
#include "mocks/include/dx11/context/video.h"

#include "mocks/include/mfx/platform.h"
#include "mocks/include/mfx/dx11/traits.h"

namespace mocks { namespace mfx { namespace dx11
{
    inline
    D3D11_TEXTURE2D_DESC make_texture_desc(mfxVideoParam const& vp, D3D11_USAGE usage, UINT bind, UINT access)
    {
        return D3D11_TEXTURE2D_DESC{
            vp.mfx.FrameInfo.Width, vp.mfx.FrameInfo.Height,
            1, 1,
            dxgi::to_native(vp.mfx.FrameInfo.FourCC),
            { 1, 0 },
            usage,
            bind,
            access
        };
    }

    inline
    D3D11_BUFFER_DESC make_buffer_desc(mfxVideoParam const& vp, D3D11_USAGE usage, UINT bind, UINT access)
    {
        return D3D11_BUFFER_DESC{
            UINT(vp.mfx.FrameInfo.Width * vp.mfx.FrameInfo.Height),
            usage,
            bind,
            access
        };
    }

    namespace detail
    {
        template <int Type, GUID const* Id>
        inline
        void mock_component(std::integral_constant<int, Type>, mocks::dx11::device& d, std::tuple<guid<Id>, mfxVideoParam> params)
        {
            auto&& vp = std::get<1>(params);
            auto vp_check = vp; //most components request support of NV12 as default check
            vp_check.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;

            mock_device(d,
                std::make_tuple(
                    make_desc(DXVADDI_Intel_Decode_PrivateData_Report, vp),
                    make_caps(std::integral_constant<int, Type>{}, guid<Id>{}, vp)
                ),
                std::make_tuple(
                    make_desc( *guid<Id>::value, vp),
                    make_config(guid<Id>{}, vp)
                ),
                std::make_tuple(
                    make_desc( *guid<Id>::value, vp_check),
                    make_config(guid<Id>{}, vp_check)
                )
            );
        }

        template <int Type>
        inline
        void mock_component(std::integral_constant<int, Type>, mocks::dx11::device& d, D3D11_TEXTURE2D_DESC td)
        {
            mock_device(d, td);
        }

        template <int Type>
        inline
        void mock_component(std::integral_constant<int, Type>, mocks::dx11::device& d, D3D11_BUFFER_DESC bd)
        {
            mock_device(d, bd);
        }
    }

    template <int Type, typename ...Args>
    inline
    mocks::dx11::device&
    mock_component(std::integral_constant<int, Type>, mocks::dx11::device& d, Args&&... args)
    {
        mocks::for_each(
            [&](auto&& x)
            {
                detail::mock_component(std::integral_constant<int, Type>{}, d,
                    std::forward<decltype(x)>(x)
                );
            },
            std::forward<Args>(args)...
        );

        return d;
    }

    template <int Type, typename ...Args>
    inline
    std::unique_ptr<mocks::dx11::device>
    make_component(std::integral_constant<int, Type>, IDXGIAdapter* adapter, ID3D11DeviceContext* ctx, Args&&... args)
    {
        auto d = mocks::dx11::make_device(
            std::make_tuple(adapter, adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE, std::make_tuple(D3D_FEATURE_LEVEL_9_3, D3D_FEATURE_LEVEL_11_1), ctx)
        );

        //[MFX core] uses this resolution & GUID while querying private report
        mfxVideoParam vp{};
        vp.mfx.FrameInfo.Width  = 640;
        vp.mfx.FrameInfo.Height = 480;
        vp = make_param(
            fourcc::format<MFX_FOURCC_NV12>{},
            make_param(guid<&DXVA2_ModeH264_VLD_NoFGT>{}, vp)
        );

        detail::mock_component(std::integral_constant<int, Type>{}, *d,
            std::make_tuple(guid<&DXVA2_ModeH264_VLD_NoFGT>{}, vp)
        );

        mock_component(std::integral_constant<int, Type>{}, *d,
            std::forward<Args>(args)...
        );

        return d;
    }

    template <typename ...Args>
    inline
    mocks::dx11::device&
    mock_component(int type, mocks::dx11::device& d, Args&&... args)
    {
        return invoke(type,
            MOCKS_LIFT(mock_component),
            d,
            std::forward<Args>(args)...
        );
    }

    template <typename ...Args>
    inline
    std::unique_ptr<mocks::dx11::device>
    make_component(int type, IDXGIAdapter* adapter, ID3D11DeviceContext* ctx, Args&&... args)
    {
        return invoke(type,
            MOCKS_LIFT(make_component),
            adapter, ctx,
            std::forward<Args>(args)...
        );
    }

    struct component
    {
        CComPtr<mocks::dx11::context>                  context;
        CComPtr<mocks::dx11::device>                   device;

        component(int type, IDXGIAdapter* adapter, mfxVideoParam const&)
        {
            context.Attach(
                mocks::dx11::make_context().release()
            );

            device.Attach(
                make_component(type, adapter, nullptr).release()
            );

            //need DX11 device mimics DXGI device
            //dx11::device will hold reference to DXGI device itself
            CComPtr<IDXGIDevice> d;
            d.Attach(mocks::dxgi::make_device().release());
            mock_device(*device, d.p);
        }

        virtual ~component()
        {}

        virtual void reset(int /*type*/, mfxVideoParam const&)
        {
            assert(device);
        }
    };

} } }
