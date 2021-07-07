// Copyright (c) 2019-2021 Intel Corporation
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
#if defined(MFX_ENABLE_H265_VIDEO_ENCODE)

#include "hevcehw_disp.h"
#include "hevcehw_base.h"

#if defined(MFX_VA_LINUX)
#if defined(STRIP_EMBARGO)
#include "hevcehw_base_lin.h"
namespace HEVCEHWDisp
{
    namespace SKL { using namespace HEVCEHW::Linux::Base; };
    namespace ICL { using namespace HEVCEHW::Linux::Base; };
};
#else
#include "hevcehw_base_embargo_lin.h"
namespace HEVCEHWDisp
{
    namespace SKL { using namespace HEVCEHW::Linux::Base_Embargo; };
    namespace ICL { using namespace HEVCEHW::Linux::Base_Embargo; };
};
#endif //defined(STRIP_EMBARGO)
#else
#include "hevcehw_base_win.h"
namespace HEVCEHWDisp
{
    namespace SKL { using namespace HEVCEHW::Windows::Base; };
    namespace ICL { using namespace HEVCEHW::Windows::Base; };
};
#endif

#ifndef STRIP_EMBARGO
#if defined(MFX_VA_LINUX)
#include "hevcehw_g11lkf_lin.h"
namespace HEVCEHWDisp
{
    namespace LKF { using namespace HEVCEHW::Linux::Gen11LKF; };
};
#else
#include "hevcehw_g11lkf_win.h"
namespace HEVCEHWDisp
{
    namespace LKF { using namespace HEVCEHW::Windows::Gen11LKF; };
};
#endif
#endif

#if defined(MFX_VA_LINUX)
#include "hevcehw_g12_lin.h"
namespace HEVCEHWDisp
{
    namespace TGL { using namespace HEVCEHW::Linux::Gen12; };
    namespace DG1 { using namespace HEVCEHW::Linux::Gen12; };
};
#else
#include "hevcehw_g12_win.h"
namespace HEVCEHWDisp
{
    namespace TGL { using namespace HEVCEHW::Windows::Gen12; };
    namespace DG1 { using namespace HEVCEHW::Windows::Gen12; };
};
#endif


#if !defined(STRIP_EMBARGO)

#if defined(MFX_VA_LINUX)
#include "hevcehw_g12xehp_lin.h"
#include "hevcehw_g12dg2_lin.h"
namespace HEVCEHWDisp
{
    namespace ATS { using namespace HEVCEHW::Linux::Gen12XEHP; };
    namespace DG2 { using namespace HEVCEHW::Linux::Gen12DG2; };
};
#else
#include "hevcehw_g12xehp_win.h"
#include "hevcehw_g12dg2_win.h"
namespace HEVCEHWDisp
{
    namespace ATS { using namespace HEVCEHW::Windows::Gen12XEHP; };
    namespace DG2 { using namespace HEVCEHW::Windows::Gen12DG2; };
};
#endif
#endif // !(STRIP_EMBARGO)

namespace HEVCEHW
{

static ImplBase* CreateSpecific(
    eMFXHWType HW
    , VideoCORE& core
    , mfxStatus& status
    , eFeatureMode mode)
{
#ifndef STRIP_EMBARGO
    if (HW >= MFX_HW_DG2)
        return new HEVCEHWDisp::DG2::MFXVideoENCODEH265_HW(core, status, mode);
    if (HW >= MFX_HW_XE_HP_SDV)
        return new HEVCEHWDisp::ATS::MFXVideoENCODEH265_HW(core, status, mode);
#endif
    if (HW == MFX_HW_DG1)
        return new HEVCEHWDisp::DG1::MFXVideoENCODEH265_HW(core, status, mode);
    if (HW >= MFX_HW_TGL_LP)
        return new HEVCEHWDisp::TGL::MFXVideoENCODEH265_HW(core, status, mode);
#ifndef STRIP_EMBARGO
    if (HW == MFX_HW_LKF || HW == MFX_HW_JSL)
        return new HEVCEHWDisp::LKF::MFXVideoENCODEH265_HW(core, status, mode);
#endif //STRIP_EMBARGO
    if (HW >= MFX_HW_ICL)
        return new HEVCEHWDisp::ICL::MFXVideoENCODEH265_HW(core, status, mode);
    return new HEVCEHWDisp::SKL::MFXVideoENCODEH265_HW(core, status, mode);
}

VideoENCODE* Create(
    VideoCORE& core
    , mfxStatus& status
    , bool /*bFEI*/)
{
    auto hw = core.GetHWType();

    if (hw < MFX_HW_SCL)
    {
        status = MFX_ERR_UNSUPPORTED;
        return nullptr;
    }

    auto p = CreateSpecific(hw, core, status, eFeatureMode::INIT);

    return p;
}

mfxStatus QueryIOSurf(
    VideoCORE *core
    , mfxVideoParam *par
    , mfxFrameAllocRequest *request
    , bool bFEI)
{
    MFX_CHECK_NULL_PTR3(core, par, request);

    auto hw = core->GetHWType();

    if (hw < MFX_HW_SCL)
        return MFX_ERR_UNSUPPORTED;

    mfxStatus sts = MFX_ERR_NONE;
    std::unique_ptr<ImplBase> impl(CreateSpecific(hw, *core, sts, eFeatureMode::QUERY_IO_SURF));

    MFX_CHECK_STS(sts);
    MFX_CHECK(impl, MFX_ERR_UNKNOWN);

    impl.reset(impl.release()->ApplyMode(bFEI * IMPL_MODE_FEI));
    MFX_CHECK(impl, MFX_ERR_UNKNOWN);

    return impl->InternalQueryIOSurf(*core, *par, *request);
}

mfxStatus Query(
    VideoCORE *core
    , mfxVideoParam *in
    , mfxVideoParam *out
    , bool bFEI)
{
    MFX_CHECK_NULL_PTR2(core, out);

    auto hw = core->GetHWType();

    if (hw < MFX_HW_SCL)
        return MFX_ERR_UNSUPPORTED;

    mfxStatus sts = MFX_ERR_NONE;
    std::unique_ptr<ImplBase> impl(CreateSpecific(hw, *core, sts, in ? eFeatureMode::QUERY1 : eFeatureMode::QUERY0));

    MFX_CHECK_STS(sts);
    MFX_CHECK(impl, MFX_ERR_UNKNOWN);

    impl.reset(impl.release()->ApplyMode(bFEI * IMPL_MODE_FEI));
    MFX_CHECK(impl, MFX_ERR_UNKNOWN);

    return impl->InternalQuery(*core, in, *out);
}

mfxStatus QueryImplsDescription(
    VideoCORE& core
    , mfxEncoderDescription::encoder& caps
    , mfx::PODArraysHolder& ah)
{
    auto hw = core.GetHWType();

    if (hw < MFX_HW_SCL)
        return MFX_ERR_UNSUPPORTED;

    mfxStatus sts = MFX_ERR_NONE;
    std::unique_ptr<ImplBase> impl(CreateSpecific(hw, core, sts, eFeatureMode::QUERY_IMPLS_DESCRIPTION));

    MFX_CHECK_STS(sts);
    MFX_CHECK(impl, MFX_ERR_UNKNOWN);

    return impl->QueryImplsDescription(core, caps, ah);
}

} //namespace HEVCEHW

#endif //defined(MFX_ENABLE_H265_VIDEO_ENCODE)
