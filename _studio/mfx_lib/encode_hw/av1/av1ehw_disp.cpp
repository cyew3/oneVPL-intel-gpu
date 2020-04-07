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

#include "mfx_common.h"
#if defined(MFX_ENABLE_AV1_VIDEO_ENCODE)

#include "av1ehw_disp.h"

#if !defined(MFX_VA_LINUX)
    #define GEN_FUNC(GEN, FUNC, ...) Windows::GEN::FUNC(__VA_ARGS__)
    #include "av1ehw_g12_win.h"

    #define GEN_SPECIFIC(HW, OP, FUNC, ...)\
        (\
            OP GEN_FUNC(Gen12,    FUNC, __VA_ARGS__)\
        )
#endif

namespace AV1EHW
{

VideoENCODE* Create(
    VideoCORE& core
    , mfxStatus& status)
{
    auto hw = core.GetHWType();

    if (hw < MFX_HW_SCL)
    {
        status = MFX_ERR_UNSUPPORTED;
        return nullptr;
    }

    return GEN_SPECIFIC(hw, (VideoENCODE*)new, MFXVideoENCODEAV1_HW, core, status);
}

mfxStatus QueryIOSurf(
    VideoCORE *core
    , mfxVideoParam *par
    , mfxFrameAllocRequest *request)
{
    MFX_CHECK_NULL_PTR3(core, par, request);

    auto hw = core->GetHWType();

    if (hw < MFX_HW_SCL)
        return MFX_ERR_UNSUPPORTED;

    return GEN_SPECIFIC(hw, , MFXVideoENCODEAV1_HW::QueryIOSurf, *core, *par, *request);
}

mfxStatus Query(
    VideoCORE *core
    , mfxVideoParam *in
    , mfxVideoParam *out)
{
    MFX_CHECK_NULL_PTR2(core, out);

    auto hw = core->GetHWType();

    if (hw < MFX_HW_SCL)
        return MFX_ERR_UNSUPPORTED;

    return GEN_SPECIFIC(hw, , MFXVideoENCODEAV1_HW::Query, *core, in, *out);
}

} //namespace AV1EHW

#endif //defined(MFX_ENABLE_AV1_VIDEO_ENCODE)