// Copyright (c) 2021 Intel Corporation
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

#include <va/va.h>

namespace mocks { namespace va
{
    inline
    unsigned int to_native(unsigned f)
    {
        switch (f)
        {
            case fourcc::NV12:    return VA_FOURCC_NV12;
            case fourcc::P010:    return VA_FOURCC_P010;
            case fourcc::YUY2:    return VA_FOURCC_YUY2;

            case fourcc::RGB4:    return VA_FOURCC_ARGB;
            case fourcc::BGR4:    return VA_FOURCC_ABGR;

            case fourcc::P8:
            case fourcc::P8MB:
                return VA_FOURCC_P208;

            case fourcc::AYUV:    return VA_FOURCC_AYUV;

            case fourcc::Y210:    return VA_FOURCC_Y210;
            case fourcc::Y410:    return VA_FOURCC_Y410;

            case fourcc::P016:    return VA_FOURCC_P016;
            case fourcc::Y216:    return VA_FOURCC_Y216;
            case fourcc::Y416:    return VA_FOURCC_Y416;
            case fourcc::YV12:    return VA_FOURCC_YV12;
            case fourcc::RGBP:    return VA_FOURCC_RGBP;
            case fourcc::A2RGB10: return VA_FOURCC_ARGB;
            case fourcc::UYVY:    return VA_FOURCC_UYVY;
            case fourcc::RGB565:  return VA_FOURCC_RGB565;
        }

        return 0;
    }

    template <unsigned F>
    inline
    unsigned int to_native(fourcc::format<F> const& f)
    { return to_native(f.value); }

    inline
    unsigned int to_native_rt(unsigned f)
    {
        switch (f)
        {
            case fourcc::NV12:    return VA_RT_FORMAT_YUV420;

            case fourcc::UYVY:
            case fourcc::YUY2:
                return VA_RT_FORMAT_YUV422;

            case fourcc::A2RGB10: return VA_RT_FORMAT_RGB32_10BPP;
            case fourcc::RGBP:    return VA_RT_FORMAT_RGBP;

            case fourcc::RGB4:
            case fourcc::BGR4:
                return VA_RT_FORMAT_RGB32;
         }

         return to_native(f);
    }

    template <unsigned F>
    inline
    unsigned int to_native_rt(fourcc::format<F> const& f)
    { return to_native_rt(f.value); }
} }
