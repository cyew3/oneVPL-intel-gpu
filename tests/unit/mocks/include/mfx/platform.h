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

#include "mocks/include/mfx/detail/platform.hxx"

namespace mocks { namespace mfx
{
    inline
    int family(int type)
    {
        switch (type)
        {
            case HW_HSW:
            case HW_HSW_ULT: return detail::IGFX_HASWELL;
            case HW_IVB:     return detail::IGFX_IVYBRIDGE;
            case HW_VLV:     return detail::IGFX_VALLEYVIEW;
            case HW_SKL:     return detail::IGFX_SKYLAKE;
            case HW_BDW:     return detail::IGFX_BROADWELL;
            case HW_KBL:     return detail::IGFX_KABYLAKE;
            case HW_CFL:     return detail::IGFX_COFFEELAKE;
            case HW_APL:     return detail::IGFX_APOLLOLAKE;
            case HW_GLK:     return detail::IGFX_GEMINILAKE;
            case HW_CNL:     return detail::IGFX_CANNONLAKE;
            case HW_ICL:     return detail::IGFX_ICELAKE;
            case HW_JSL:     return detail::IGFX_JASPERLAKE;
            case HW_TGL:     return detail::IGFX_TIGERLAKE_LP;
            case HW_DG1:     return detail::IGFX_DG1;
            case HW_ATS:     return detail::IGFX_TIGERLAKE_HP;
#ifndef STRIP_EMBARGO
            case HW_LKF:     return detail::IGFX_LAKEFIELD;
            case HW_DG2:     return detail::IGFX_DG2;
            case HW_ADL_S:   return detail::IGFX_ALDERLAKE_S;
            case HW_ADL_UH:  return detail::IGFX_ALDERLAKE_UH;
            case HW_MTL:     return detail::IGFX_METEORLAKE;
#endif
            default:             return detail::IGFX_UNKNOWN;
        }
    }

} }
