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

#include "mocks/include/dx11/device/device.h"
#include "mocks/include/dx11/device/video.h"
#include "mocks/include/dx11/context/video.h"

#include "mocks/include/mfx/platform.h"
#include "mocks/include/mfx/dx11/traits.h"

namespace mocks { namespace mfx { namespace dx11
{
    template <int Type, GUID const* Id>
    inline
    void mock_component(mocks::dx11::device& d, std::integral_constant<int, Type>, guid<Id>, mfxVideoParam const& vp)
    {
        mock_device(d,
            std::make_tuple(
                make_desc(DXVADDI_Intel_Decode_PrivateData_Report, vp),
                make_caps(guid<Id>{}, std::integral_constant<int, Type>{}, vp)
            ),
            std::make_tuple(
                make_desc( *guid<Id>::value, vp),
                make_config(guid<Id>{}, vp)
            )
        );
    }

    template <int Type, typename ...Args>
    inline
    std::unique_ptr<mocks::dx11::device>
    make_component(IDXGIAdapter* adapter, ID3D11DeviceContext* ctx, std::integral_constant<int, Type>, Args&&... args)
    {
        auto d = mocks::dx11::make_device(
            std::make_tuple(adapter, D3D_DRIVER_TYPE_HARDWARE, std::make_tuple(D3D_FEATURE_LEVEL_9_3, D3D_FEATURE_LEVEL_11_1), ctx)
        );

        //[MFX core] uses this resolution & GUID while querying private report
        mfxVideoParam vp{};
        vp.mfx.FrameInfo.Width  = 640;
        vp.mfx.FrameInfo.Height = 480;
        vp = make_param(
            fourcc::tag<MFX_FOURCC_NV12>{},
            make_param(mocks::guid<&DXVA2_ModeH264_VLD_NoFGT>{}, vp)
        );

        mocks::for_each(
            [&](auto&& x)
            {
                mock_component(*d,
                    std::integral_constant<int, Type>{},
                    std::get<0>(std::forward<decltype(x)>(x)), std::get<1>(std::forward<decltype(x)>(x))
                );
            },
            std::make_tuple(guid<&DXVA2_ModeH264_VLD_NoFGT>{}, vp), std::forward<Args>(args)...
        );

        return d;
    }

    template <typename ...Args>
    inline
    std::unique_ptr<mocks::dx11::device>
    make_component(IDXGIAdapter* adapter, ID3D11DeviceContext* ctx, int type, Args&&... args)
    {
        switch (type)
        {
            case HW_HSW:
            case HW_HSW_ULT: return make_component(adapter, ctx, std::integral_constant<int, HW_HSW>{},     std::forward<Args>(args)...);
            case HW_IVB:     return make_component(adapter, ctx, std::integral_constant<int, HW_IVB>{},     std::forward<Args>(args)...);
            case HW_VLV:     return make_component(adapter, ctx, std::integral_constant<int, HW_VLV>{},     std::forward<Args>(args)...);
            case HW_SKL:     return make_component(adapter, ctx, std::integral_constant<int, HW_SKL>{},     std::forward<Args>(args)...);
            case HW_BDW:     return make_component(adapter, ctx, std::integral_constant<int, HW_BDW>{},     std::forward<Args>(args)...);
            case HW_KBL:     return make_component(adapter, ctx, std::integral_constant<int, HW_KBL>{},     std::forward<Args>(args)...);
            case HW_CFL:     return make_component(adapter, ctx, std::integral_constant<int, HW_CFL>{},     std::forward<Args>(args)...);
            case HW_APL:     return make_component(adapter, ctx, std::integral_constant<int, HW_APL>{},     std::forward<Args>(args)...);
            case HW_GLK:     return make_component(adapter, ctx, std::integral_constant<int, HW_GLK>{},     std::forward<Args>(args)...);
            case HW_CNL:     return make_component(adapter, ctx, std::integral_constant<int, HW_CNL>{},     std::forward<Args>(args)...);
            case HW_ICL:     return make_component(adapter, ctx, std::integral_constant<int, HW_ICL>{},     std::forward<Args>(args)...);
            case HW_JSL:     return make_component(adapter, ctx, std::integral_constant<int, HW_JSL>{},     std::forward<Args>(args)...);
            case HW_TGL:     return make_component(adapter, ctx, std::integral_constant<int, HW_TGL>{},     std::forward<Args>(args)...);
            case HW_DG1:     return make_component(adapter, ctx, std::integral_constant<int, HW_DG1>{},     std::forward<Args>(args)...);
#ifndef STRIP_EMBARGO
            case HW_LKF:     return make_component(adapter, ctx, std::integral_constant<int, HW_LKF>{},     std::forward<Args>(args)...);
            case HW_ATS:     return make_component(adapter, ctx, std::integral_constant<int, HW_ATS>{},     std::forward<Args>(args)...);
            case HW_DG2:     return make_component(adapter, ctx, std::integral_constant<int, HW_DG2>{},     std::forward<Args>(args)...);
#endif
            default:         return make_component(adapter, ctx, std::integral_constant<int, HW_UNKNOWN>{}, std::forward<Args>(args)...);
        }
    }

} } }
