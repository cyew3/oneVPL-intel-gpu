// Copyright (c) 2020-2021 Intel Corporation
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

namespace mocks { namespace va
{
    inline
    void fill_offsets(VAImage& i)
    {
        switch(i.format.fourcc)
        {
        case VA_FOURCC_NV12:
            i.num_planes = 2;
            i.pitches[0] = i.width;
            i.offsets[0] = 0;
            i.offsets[1] = i.width * i.height;
            break;

        case VA_FOURCC_P010:
            i.num_planes = 2;
            i.pitches[0] = i.width * 2;
            i.offsets[0] = 0;
            i.offsets[1] = i.width * i.height * 2;
            break;

        case VA_FOURCC_YUY2:
            i.num_planes = 1;
            i.pitches[0] = i.width * 2;
            break;

        case VA_FOURCC_RGBP:
            i.num_planes = 3;
            i.pitches[0] = i.width;
            i.offsets[0] = 0;
            i.offsets[1] = i.width * i.height;
            i.offsets[2] = i.width * i.height * 2;
            break;

        case VA_FOURCC_ARGB:
            i.num_planes = 1;
            i.pitches[0] = i.width * 4;
            i.offsets[0] = 0;
            break;

        case VA_FOURCC_ABGR:
            i.num_planes = 1;
            i.pitches[0] = i.width * 4;
            i.offsets[0] = 0;
            break;

        case VA_FOURCC_P208:
            i.num_planes = 1;
            i.pitches[0] = i.width * i.height;
            i.offsets[0] = 0;
            break;

        case VA_FOURCC_AYUV:
            i.num_planes = 1;
            i.pitches[0] = i.width * 4;
            i.offsets[0] = 0;
            break;
        case VA_FOURCC_Y210:
            i.num_planes = 1;
            i.pitches[0] = i.width * 4;
            i.offsets[0] = 0;
            break;

        case VA_FOURCC_Y410:
            i.num_planes = 1;
            i.pitches[0] = i.width * 4;
            i.offsets[0] = 0;
            break;

        case VA_FOURCC_P016:
            i.num_planes = 2;
            i.pitches[0] = i.width * 2;
            i.offsets[0] = 0;
            i.offsets[1] = i.width * i.height * 2;
            break;

        case VA_FOURCC_Y216:
            i.num_planes = 1;
            i.pitches[0] = i.width * 4;
            i.offsets[0] = 0;
            break;

        case VA_FOURCC_Y416:
            i.num_planes = 1;
            i.pitches[0] = i.width * 8;
            i.offsets[0] = 0;
            break;

        case VA_FOURCC_A2R10G10B10:
            i.num_planes = 1;
            i.pitches[0] = i.width * 4;
            i.offsets[0] = 0;
            break;

        case VA_FOURCC_YV12:
            i.num_planes = 2;
            i.pitches[0] = i.width;
            i.offsets[0] = 0;
            i.offsets[1] = i.width * i.height;
            i.offsets[2] = i.width * i.height * 5 / 4;
            break;

        case VA_FOURCC_RGB565:
            i.num_planes = 1;
            i.pitches[0] = i.width * 2;
            i.offsets[0] = 0;
            break;

        case VA_FOURCC_UYVY:
            i.num_planes = 1;
            i.pitches[0] = i.width * 2;
            i.offsets[0] = 0;
            break;

        default:
            throw std::invalid_argument("unsupported fourcc");
        }
    }
} }
