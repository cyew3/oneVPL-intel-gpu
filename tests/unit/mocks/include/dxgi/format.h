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

#include "mocks/include/fourcc.h"

#include <dxgiformat.h>

namespace mocks { namespace dxgi
{
    inline
    DXGI_FORMAT to_native(unsigned f)
    {
        switch (f)
        {
            case fourcc::NV12:    return DXGI_FORMAT_NV12;
            case fourcc::P010:    return DXGI_FORMAT_P010;
            case fourcc::YUY2:    return DXGI_FORMAT_YUY2;

            case fourcc::RGB4:    return DXGI_FORMAT_B8G8R8A8_UNORM;
            case fourcc::BGR4:    return DXGI_FORMAT_R8G8B8A8_UNORM;
                
            case fourcc::P8:
                return DXGI_FORMAT_P8;

            case fourcc::AYUV:    return DXGI_FORMAT_AYUV;
            
            case fourcc::R16:
                return DXGI_FORMAT_R16_TYPELESS;
                
            case fourcc::ARGB16:
            case fourcc::ABGR16:
                return DXGI_FORMAT_R16G16B16A16_UNORM;
            case fourcc::A2RGB10:
                return DXGI_FORMAT_R10G10B10A2_UNORM;

            case fourcc::Y210:    return DXGI_FORMAT_Y210;
            case fourcc::Y410:    return DXGI_FORMAT_Y410;

            case fourcc::P016:    return DXGI_FORMAT_P016;
            case fourcc::Y216:    return DXGI_FORMAT_Y216;
            case fourcc::Y416:    return DXGI_FORMAT_Y416;
        }

        return DXGI_FORMAT_UNKNOWN;
    }

    template <unsigned F>
    inline
    DXGI_FORMAT to_native(fourcc::tag<F> const& f)
    { return to_native(f.value); }

} }
