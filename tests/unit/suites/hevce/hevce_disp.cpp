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
#include "hevcehw_g9_win.h"
#include "hevcehw_g11lkf.h"
#include "hevcehw_g11lkf_win.h"
#include "hevcehw_g12_win.h"
#include "hevcehw_g12ats_win.h"
#include "hevcehw_g12dg2_win.h"

namespace hevce { namespace tests
{

using TGen9     = HEVCEHW::Windows::Gen9::MFXVideoENCODEH265_HW;
using TGen11LKF = HEVCEHW::Windows::Gen11LKF::MFXVideoENCODEH265_HW;
using TGen12    = HEVCEHW::Windows::Gen12::MFXVideoENCODEH265_HW;
using TGen12ATS = HEVCEHW::Windows::Gen12ATS::MFXVideoENCODEH265_HW;
using TGen12DG2 = HEVCEHW::Windows::Gen12DG2::MFXVideoENCODEH265_HW;

template<typename T>
inline bool IsGen(VideoENCODE*) { return false; }

template<>
inline bool IsGen<TGen9>(VideoENCODE* p)
{
    return dynamic_cast<TGen9*>(p)
        && !dynamic_cast<TGen11LKF*>(p)
        && !dynamic_cast<TGen12*>(p)
        && !dynamic_cast<TGen12ATS*>(p)
        && !dynamic_cast<TGen12DG2*>(p);
}

template<>
inline bool IsGen<TGen11LKF>(VideoENCODE* p)
{
    return dynamic_cast<TGen11LKF*>(p)
        && !dynamic_cast<TGen12*>(p)
        && !dynamic_cast<TGen12ATS*>(p)
        && !dynamic_cast<TGen12DG2*>(p);
}

template<>
inline bool IsGen<TGen12>(VideoENCODE* p)
{
    return dynamic_cast<TGen12*>(p)
        && !dynamic_cast<TGen12ATS*>(p)
        && !dynamic_cast<TGen12DG2*>(p);
}

template<>
inline bool IsGen<TGen12ATS>(VideoENCODE* p)
{
    return dynamic_cast<TGen12ATS*>(p)
        && !dynamic_cast<TGen12DG2*>(p);
}

template<>
inline bool IsGen<TGen12DG2>(VideoENCODE* p)
{
    return dynamic_cast<TGen12DG2*>(p);
}

class Core
    : public VideoCORE
{
public:
    Core(eMFXHWType hw = MFX_HW_UNKNOWN) : m_hw(hw) {}

    eMFXHWType m_hw;

    virtual mfxStatus GetHandle(mfxHandleType, mfxHDL*) override { return MFX_ERR_UNSUPPORTED; }
    virtual mfxStatus SetHandle(mfxHandleType , mfxHDL ) override { return MFX_ERR_UNSUPPORTED; };
    virtual mfxStatus SetBufferAllocator(mfxBufferAllocator *) override { return MFX_ERR_UNSUPPORTED; };
    virtual mfxStatus SetFrameAllocator(mfxFrameAllocator *) override { return MFX_ERR_UNSUPPORTED; };
    virtual mfxStatus AllocBuffer(mfxU32, mfxU16, mfxMemId *) override { return MFX_ERR_UNSUPPORTED; };
    virtual mfxStatus LockBuffer(mfxMemId, mfxU8 **) override { return MFX_ERR_UNSUPPORTED; };
    virtual mfxStatus UnlockBuffer(mfxMemId ) override { return MFX_ERR_UNSUPPORTED; };
    virtual mfxStatus FreeBuffer(mfxMemId ) override { return MFX_ERR_UNSUPPORTED; };
    virtual mfxStatus CheckHandle() override { return MFX_ERR_UNSUPPORTED; };
    virtual mfxStatus GetFrameHDL(mfxMemId, mfxHDL *, bool) override { return MFX_ERR_UNSUPPORTED; };
    virtual mfxStatus AllocFrames(mfxFrameAllocRequest *, mfxFrameAllocResponse *, bool) override { return MFX_ERR_UNSUPPORTED; }
    virtual mfxStatus  AllocFrames(mfxFrameAllocRequest *, mfxFrameAllocResponse *, mfxFrameSurface1 **, mfxU32) override { return MFX_ERR_UNSUPPORTED; }
    virtual mfxStatus  LockFrame(mfxMemId, mfxFrameData *) override { return MFX_ERR_UNSUPPORTED; }
    virtual mfxStatus  UnlockFrame(mfxMemId, mfxFrameData *) override { return MFX_ERR_UNSUPPORTED; }
    virtual mfxStatus  FreeFrames(mfxFrameAllocResponse *, bool) override { return MFX_ERR_UNSUPPORTED; }
    virtual mfxStatus  LockExternalFrame(mfxMemId, mfxFrameData *, bool) override { return MFX_ERR_UNSUPPORTED; }
    virtual mfxStatus  GetExternalFrameHDL(mfxMemId, mfxHDL *, bool) override { return MFX_ERR_UNSUPPORTED; }
    virtual mfxStatus  UnlockExternalFrame(mfxMemId, mfxFrameData *, bool) override { return MFX_ERR_UNSUPPORTED; }
    virtual mfxMemId MapIdx(mfxMemId) override { return nullptr; }
    virtual mfxFrameSurface1* GetNativeSurface(mfxFrameSurface1 *, bool  = true) override { return nullptr; }
    virtual mfxFrameSurface1* GetOpaqSurface(mfxMemId, bool  = true) override { return nullptr; }
    virtual mfxStatus  IncreaseReference(mfxFrameData *, bool) override { return MFX_ERR_UNSUPPORTED; }
    virtual mfxStatus  DecreaseReference(mfxFrameData *, bool) override { return MFX_ERR_UNSUPPORTED; }
    virtual mfxStatus IncreasePureReference(mfxU16 &) override { return MFX_ERR_UNSUPPORTED; }
    virtual mfxStatus DecreasePureReference(mfxU16 &) override { return MFX_ERR_UNSUPPORTED; }
    virtual void  GetVA(mfxHDL*, mfxU16) override {  }
    virtual mfxStatus CreateVA(mfxVideoParam *, mfxFrameAllocRequest *, mfxFrameAllocResponse *, UMC::FrameAllocator *) override { return MFX_ERR_UNSUPPORTED; }
    virtual mfxU32 GetAdapterNumber(void) override { return 0; }
    virtual void  GetVideoProcessing(mfxHDL*) {}
    virtual mfxStatus CreateVideoProcessing(mfxVideoParam *) override { return MFX_ERR_UNSUPPORTED; }
    virtual eMFXPlatform GetPlatformType() override { return MFX_PLATFORM_HARDWARE; }
    virtual mfxU32 GetNumWorkingThreads(void) override { return 1; }
    virtual void INeedMoreThreadsInside(const void *) {}
    virtual mfxStatus DoFastCopy(mfxFrameSurface1 *, mfxFrameSurface1 *) override { return MFX_ERR_UNSUPPORTED; }
    virtual mfxStatus DoFastCopyExtended(mfxFrameSurface1 *, mfxFrameSurface1 *) override { return MFX_ERR_UNSUPPORTED; }
    virtual mfxStatus DoFastCopyWrapper(mfxFrameSurface1 *, mfxU16, mfxFrameSurface1 *, mfxU16) override { return MFX_ERR_UNSUPPORTED; }
    virtual bool IsFastCopyEnabled(void) override { return false; }
    virtual bool IsExternalFrameAllocator(void) const override { return false; }

    virtual eMFXHWType   GetHWType() { return m_hw; };

    virtual bool         SetCoreId(mfxU32) override { return false; }
    virtual eMFXVAType   GetVAType() const override { return MFX_HW_D3D11; }
    virtual mfxStatus CopyFrame(mfxFrameSurface1 *, mfxFrameSurface1 *) override { return MFX_ERR_UNSUPPORTED; }
    virtual mfxStatus CopyBuffer(mfxU8 *, mfxU32 , mfxFrameSurface1 *) override { return MFX_ERR_UNSUPPORTED; }
    virtual mfxStatus CopyFrameEx(mfxFrameSurface1 *, mfxU16 , mfxFrameSurface1 *, mfxU16 ) override { return MFX_ERR_UNSUPPORTED; }
    virtual mfxStatus IsGuidSupported(const GUID, mfxVideoParam *, bool) override { return MFX_ERR_UNSUPPORTED; }
    virtual bool CheckOpaqueRequest(mfxFrameAllocRequest *, mfxFrameSurface1 **, mfxU32, bool) override { return false; }
    virtual bool IsOpaqSurfacesAlreadyMapped(mfxFrameSurface1 **, mfxU32, mfxFrameAllocResponse *, bool) override { return false; }
    virtual void* QueryCoreInterface(const MFX_GUID &) override { return nullptr; };
    virtual mfxSession GetSession() override { return nullptr; }
    virtual void SetWrapper(void*) override {}
    virtual mfxU16 GetAutoAsyncDepth() override { return 0; }
    virtual bool IsCompatibleForOpaq() override { return false; }
};

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

TEST(Disp, Gen9)
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
        EXPECT_TRUE(IsGen<TGen9>(pHEVC.get()));
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

TEST(Disp, Gen12ATS)
{
    Core vcore;
    mfxStatus sts = MFX_ERR_NONE;
    eMFXHWType hwSet[] = {
        MFX_HW_ATS
    };

    for (auto hw : hwSet)
    {
        vcore.m_hw = hw;

        std::unique_ptr<VideoENCODE> pHEVC(HEVCEHW::Create((VideoCORE&)vcore, sts));
        EXPECT_NE(pHEVC.get(), (VideoENCODE*)nullptr);
        EXPECT_EQ(sts, MFX_ERR_NONE);
        EXPECT_TRUE(IsGen<TGen12ATS>(pHEVC.get()));
    }
}

TEST(Disp, Gen12DG2)
{
    Core vcore;
    mfxStatus sts = MFX_ERR_NONE;
    eMFXHWType hwSet[] = {
        MFX_HW_DG2
        , MFX_HW_ADL_S
        , MFX_HW_ADL_UH
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