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

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "feature_blocks/mfx_feature_blocks_utils.h"
#include "../hevcehw_disp.h"
#include "hevcehw_base_win.h"
#include "hevcehw_g11lkf.h"
#if defined(MFX_VA_WIN)
#include "hevcehw_g11lkf_win.h"
#include "hevcehw_g12_win.h"
#include "hevcehw_g12xehp_win.h"
#include "hevcehw_g12dg2_win.h"
#elif defined(MFX_VA_LINUX)
#include "hevcehw_g11lkf_lin.h"
#include "hevcehw_g12_lin.h"
#include "hevcehw_g12xehp_lin.h"
#include "hevcehw_g12dg2_lin.h"
#endif

#include "mocks/include/mfx/core.h"

namespace hevce { namespace tests
{
#if defined(MFX_VA_WIN)
namespace HEVCERoot = HEVCEHW::Windows;
#else
#if defined(MFX_VA_LINUX)
namespace HEVCERoot = HEVCEHW::Linux;
#endif
#endif

using TBase      = HEVCERoot::Base::MFXVideoENCODEH265_HW;
using TGen11LKF  = HEVCERoot::Gen11LKF::MFXVideoENCODEH265_HW;
using TGen12     = HEVCERoot::Gen12::MFXVideoENCODEH265_HW;
using TGen12XEHP = HEVCERoot::Gen12XEHP::MFXVideoENCODEH265_HW;
using TGen12DG2  = HEVCERoot::Gen12DG2::MFXVideoENCODEH265_HW;

template<typename T>
inline bool IsGen(VideoENCODE*) { return false; }

template<>
inline bool IsGen<TBase>(VideoENCODE* p)
{
    return dynamic_cast<TBase*>(p)
        && !dynamic_cast<TGen11LKF*>(p)
        && !dynamic_cast<TGen12*>(p)
        && !dynamic_cast<TGen12XEHP*>(p)
        && !dynamic_cast<TGen12DG2*>(p);
}

template<>
inline bool IsGen<TGen11LKF>(VideoENCODE* p)
{
    return dynamic_cast<TGen11LKF*>(p)
        && !dynamic_cast<TGen12*>(p)
        && !dynamic_cast<TGen12XEHP*>(p)
        && !dynamic_cast<TGen12DG2*>(p);
}

template<>
inline bool IsGen<TGen12>(VideoENCODE* p)
{
    return dynamic_cast<TGen12*>(p)
        && !dynamic_cast<TGen12XEHP*>(p)
        && !dynamic_cast<TGen12DG2*>(p);
}

template<>
inline bool IsGen<TGen12XEHP>(VideoENCODE* p)
{
    return dynamic_cast<TGen12XEHP*>(p)
        && !dynamic_cast<TGen12DG2*>(p);
}

template<>
inline bool IsGen<TGen12DG2>(VideoENCODE* p)
{
    return dynamic_cast<TGen12DG2*>(p)
        && !dynamic_cast<TGen12XEHP*>(p);
}

struct Disp
    : testing::TestWithParam<eMFXHWType>
{
    std::unique_ptr<mocks::mfx::core> core;

    void SetUp() override
    {
        core = mocks::mfx::make_core(
            GetParam(),
#if defined(MFX_VA_WIN)
            MFX_HW_D3D11
#elif defined(MFX_VA_LINUX)
            MFX_HW_VAAPI
#endif
        );
    }
};

INSTANTIATE_TEST_SUITE_P(Unsupported, Disp,
    testing::Values(
          MFX_HW_UNKNOWN
        , MFX_HW_SNB
        , MFX_HW_IVB
        , MFX_HW_HSW
        , MFX_HW_HSW_ULT
        , MFX_HW_VLV
        , MFX_HW_BDW
        , MFX_HW_CHT
    )
);

struct BaseDisp
    : Disp
    , testing::WithParamInterface<eMFXHWType>
{};

INSTANTIATE_TEST_SUITE_P(Supported, BaseDisp,
    testing::Values(
          MFX_HW_SCL
        , MFX_HW_APL
        , MFX_HW_KBL
        , MFX_HW_GLK
        , MFX_HW_CFL
        , MFX_HW_CNL
        , MFX_HW_ICL
        , MFX_HW_ICL_LP
        , MFX_HW_CNX_G
        , MFX_HW_EHL
    )
);

TEST_P(Disp, Create)
{
    mfxStatus sts = MFX_ERR_NONE;
    EXPECT_EQ(std::unique_ptr<VideoENCODE>(HEVCEHW::Create(*core, sts)).get(), nullptr);
    EXPECT_EQ(sts, MFX_ERR_UNSUPPORTED);
}

TEST_P(BaseDisp, Create)
{
    mfxStatus sts = MFX_ERR_NONE;

    std::unique_ptr<VideoENCODE> pHEVC(HEVCEHW::Create(*core, sts));
    EXPECT_NE(pHEVC.get(), nullptr);
    EXPECT_EQ(sts, MFX_ERR_NONE);
    EXPECT_TRUE(IsGen<TBase>(pHEVC.get()));
}

struct Gen11LKF
    : Disp
    , testing::WithParamInterface<eMFXHWType>
{};

INSTANTIATE_TEST_SUITE_P(Supported, Gen11LKF,
    testing::Values(
          MFX_HW_LKF
        , MFX_HW_JSL
    )
);

TEST_P(Gen11LKF, Create)
{
    mfxStatus sts = MFX_ERR_NONE;

    std::unique_ptr<VideoENCODE> pHEVC(HEVCEHW::Create(*core, sts));
    EXPECT_NE(pHEVC.get(), nullptr);
    EXPECT_EQ(sts, MFX_ERR_NONE);
    EXPECT_TRUE(IsGen<TGen11LKF>(pHEVC.get()));
}

struct Gen12
    : Disp
    , testing::WithParamInterface<eMFXHWType>
{};

INSTANTIATE_TEST_SUITE_P(Supported, Gen12,
    testing::Values(
          MFX_HW_TGL_LP
        , MFX_HW_RYF
        , MFX_HW_RKL
        , MFX_HW_DG1
        , MFX_HW_ADL_S
        , MFX_HW_ADL_P
    )
);

TEST_P(Gen12, Create)
{
    mfxStatus sts = MFX_ERR_NONE;

    std::unique_ptr<VideoENCODE> pHEVC(HEVCEHW::Create(*core, sts));
    EXPECT_NE(pHEVC.get(), nullptr);
    EXPECT_EQ(sts, MFX_ERR_NONE);
    EXPECT_TRUE(IsGen<TGen12>(pHEVC.get()));
}

struct Gen12XEHP
    : Disp
    , testing::WithParamInterface<eMFXHWType>
{};

INSTANTIATE_TEST_SUITE_P(Supported, Gen12XEHP,
    testing::Values(
          MFX_HW_XE_HP_SDV
    )
);

TEST_P(Gen12XEHP, Create)
{
    mfxStatus sts = MFX_ERR_NONE;

    std::unique_ptr<VideoENCODE> pHEVC(HEVCEHW::Create(*core, sts));
    EXPECT_NE(pHEVC.get(), nullptr);
    EXPECT_EQ(sts, MFX_ERR_NONE);
    EXPECT_TRUE(IsGen<TGen12XEHP>(pHEVC.get()));
}

struct Gen12DG2
    : Disp
    , testing::WithParamInterface<eMFXHWType>
{};

INSTANTIATE_TEST_SUITE_P(Supported, Gen12DG2,
    testing::Values(
          MFX_HW_DG2
        , MFX_HW_PVC
        , eMFXHWType(0xfffffff)
    )
);

TEST_P(Gen12DG2, Create)
{
    mfxStatus sts = MFX_ERR_NONE;

    std::unique_ptr<VideoENCODE> pHEVC(HEVCEHW::Create(*core, sts));
    EXPECT_NE(pHEVC.get(), nullptr);
    EXPECT_EQ(sts, MFX_ERR_NONE);
    EXPECT_TRUE(IsGen<TGen12DG2>(pHEVC.get()));
}

}}