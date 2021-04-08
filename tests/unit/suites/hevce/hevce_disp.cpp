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

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "feature_blocks/mfx_feature_blocks_utils.h"
#include "../hevcehw_disp.h"
#include "hevcehw_base_win.h"
#include "hevcehw_g11lkf.h"
#include "hevcehw_g11lkf_win.h"
#include "hevcehw_g12_win.h"
#include "hevcehw_g12xehp_win.h"
#include "hevcehw_g12dg2_win.h"
#include "hevce_coremock.h"

namespace hevce { namespace tests
{
using Core = CoreMock;
using TBase     = HEVCEHW::Windows::Base::MFXVideoENCODEH265_HW;
using TGen11LKF = HEVCEHW::Windows::Gen11LKF::MFXVideoENCODEH265_HW;
using TGen12    = HEVCEHW::Windows::Gen12::MFXVideoENCODEH265_HW;
using TGen12XEHP = HEVCEHW::Windows::Gen12XEHP::MFXVideoENCODEH265_HW;
using TGen12DG2 = HEVCEHW::Windows::Gen12DG2::MFXVideoENCODEH265_HW;

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

TEST(Disp, Unsupported)
{
    Core vcore;
    mfxStatus sts = MFX_ERR_NONE;
    eMFXHWType hwSet[] = {
        MFX_HW_UNKNOWN
        , MFX_HW_SNB
        , MFX_HW_IVB
        , MFX_HW_HSW
        , MFX_HW_HSW_ULT
        , MFX_HW_VLV
        , MFX_HW_BDW
        , MFX_HW_CHT
    };

    for (auto hw : hwSet)
    {
        vcore.m_hw = hw;

        EXPECT_EQ(std::unique_ptr<VideoENCODE>(HEVCEHW::Create((VideoCORE&)vcore, sts)).get(), (VideoENCODE*)nullptr);
        EXPECT_EQ(sts, MFX_ERR_UNSUPPORTED);
    }
}

TEST(Disp, Base)
{
    Core vcore;
    mfxStatus sts = MFX_ERR_NONE;
    eMFXHWType hwSet[] = {
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
    };

    for (auto hw : hwSet)
    {
        vcore.m_hw = hw;

        std::unique_ptr<VideoENCODE> pHEVC(HEVCEHW::Create((VideoCORE&)vcore, sts));
        EXPECT_NE(pHEVC.get(), (VideoENCODE*)nullptr);
        EXPECT_EQ(sts, MFX_ERR_NONE);
        EXPECT_TRUE(IsGen<TBase>(pHEVC.get()));
    }
}

TEST(Disp, Gen11LKF)
{
    Core vcore;
    mfxStatus sts = MFX_ERR_NONE;
    eMFXHWType hwSet[] = {
        MFX_HW_LKF
        , MFX_HW_JSL
    };

    for (auto hw : hwSet)
    {
        vcore.m_hw = hw;

        std::unique_ptr<VideoENCODE> pHEVC(HEVCEHW::Create((VideoCORE&)vcore, sts));
        EXPECT_NE(pHEVC.get(), (VideoENCODE*)nullptr);
        EXPECT_EQ(sts, MFX_ERR_NONE);
        EXPECT_TRUE(IsGen<TGen11LKF>(pHEVC.get()));
    }
}

TEST(Disp, Gen12)
{
    Core vcore;
    mfxStatus sts = MFX_ERR_NONE;
    eMFXHWType hwSet[] = {
        MFX_HW_TGL_LP
        , MFX_HW_RYF
        , MFX_HW_RKL
        , MFX_HW_DG1
        , MFX_HW_ADL_S
        , MFX_HW_ADL_P
    };

    for (auto hw : hwSet)
    {
        vcore.m_hw = hw;

        std::unique_ptr<VideoENCODE> pHEVC(HEVCEHW::Create((VideoCORE&)vcore, sts));
        EXPECT_NE(pHEVC.get(), (VideoENCODE*)nullptr);
        EXPECT_EQ(sts, MFX_ERR_NONE);
        EXPECT_TRUE(IsGen<TGen12>(pHEVC.get()));
    }
}

TEST(Disp, Gen12XEHP)
{
    Core vcore;
    mfxStatus sts = MFX_ERR_NONE;
    eMFXHWType hwSet[] = {
        MFX_HW_XE_HP_SDV
    };

    for (auto hw : hwSet)
    {
        vcore.m_hw = hw;

        std::unique_ptr<VideoENCODE> pHEVC(HEVCEHW::Create((VideoCORE&)vcore, sts));
        EXPECT_NE(pHEVC.get(), (VideoENCODE*)nullptr);
        EXPECT_EQ(sts, MFX_ERR_NONE);
        EXPECT_TRUE(IsGen<TGen12XEHP>(pHEVC.get()));
    }
}

TEST(Disp, Gen12DG2)
{
    Core vcore;
    mfxStatus sts = MFX_ERR_NONE;
    eMFXHWType hwSet[] = {
        MFX_HW_DG2
        , MFX_HW_PVC
        , eMFXHWType(0xfffffff)
    };

    for (auto hw : hwSet)
    {
        vcore.m_hw = hw;

        std::unique_ptr<VideoENCODE> pHEVC(HEVCEHW::Create((VideoCORE&)vcore, sts));
        EXPECT_NE(pHEVC.get(), (VideoENCODE*)nullptr);
        EXPECT_EQ(sts, MFX_ERR_NONE);
        EXPECT_TRUE(IsGen<TGen12DG2>(pHEVC.get()));
    }
}

}}