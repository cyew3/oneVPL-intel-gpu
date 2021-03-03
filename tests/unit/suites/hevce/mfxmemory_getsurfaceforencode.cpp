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

#if defined(MFX_ONEVPL)

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "mfxvideo++.h"
#include "mfx_session.h"

#include "mocks/include/mfx/dispatch.h"
#include "mocks/include/mfx/encoder.h"
#include "mocks/include/mfx/plugin.h"
#include "mocks/include/mfx/core.h"
#include "libmfx_core.h"

namespace mfxmemory { namespace tests {

using testing::_;
using testing::Return;
using testing::Eq;
using testing::NotNull;
using testing::IsNull;

struct Cache
    : SurfaceCache
{
    Cache(CommonCORE20& core, mfxU16 type, const mfxFrameInfo& frame_info)
        : SurfaceCache(core, type, frame_info)
    {
    }
    MOCK_METHOD(mfxFrameSurface1*, GetSurface, (bool), (override));

};

struct Core
    : public CommonCORE20
{
    Core() : CommonCORE20(1) {}
    MOCK_METHOD(mfxStatus, CreateSurface, (mfxU16, const mfxFrameInfo&, mfxFrameSurface1*&), (override));
};

struct MFXMemory_GetSurfaceForEncode
    : testing::Test
{
    std::unique_ptr<mocks::registry> registry;
    MFXVideoSession                  session;
    _mfxSession*                     pSession   = nullptr;
    mfxFrameSurface1*                pSurf      = nullptr;
    Core                             core;
    Core*                            pCore      = &core;
    mfxFrameInfo                     fi         = {};
    VideoCORE*                       m_pCoreOld = nullptr;
    mocks::mfx::encoder*             pEncoder   = nullptr; 

    void SetUp() override
    {
        ASSERT_NO_THROW({ registry = mocks::mfx::dispatch_to(0x8086, 42, 1); });
        ASSERT_EQ(session.InitEx(mfxInitParam{ MFX_IMPL_HARDWARE | MFX_IMPL_VIA_D3D11, { 0, 1 } }), MFX_ERR_NONE);
        pSession = static_cast<_mfxSession*>(session);
        pSession->m_pENCODE.reset(pEncoder = new mocks::mfx::encoder);
        m_pCoreOld = pSession->m_pCORE.get();
        pSession->m_pCORE.reset(pCore, false);
    }
    void TearDown() override
    {
        pSession->m_pCORE.reset(m_pCoreOld, false);
        pSession->m_pENCODE.reset();
    }
};

TEST_F(MFXMemory_GetSurfaceForEncode, NoSurf)
{
    EXPECT_EQ(::MFXMemory_GetSurfaceForEncode(pSession, nullptr), MFX_ERR_NULL_PTR);
}

TEST_F(MFXMemory_GetSurfaceForEncode, NoSession)
{
    EXPECT_EQ(::MFXMemory_GetSurfaceForEncode(nullptr, &pSurf), MFX_ERR_INVALID_HANDLE);
}

TEST_F(MFXMemory_GetSurfaceForEncode, NoEncoder)
{
    pSession->m_pENCODE.reset(nullptr);
    EXPECT_EQ(::MFXMemory_GetSurfaceForEncode(pSession, &pSurf), MFX_ERR_NOT_INITIALIZED);
}

TEST_F(MFXMemory_GetSurfaceForEncode, OldCache)
{
    auto& pCache = pSession->m_pENCODE->m_pSurfaceCache;
    using TCachePtr = std::remove_reference<decltype(pCache)>::type;
    Cache cache(*pCore, 0, fi);
    auto pFakeSurf = (mfxFrameSurface1*)(0xfeff);

    pCache = TCachePtr(&cache, [](SurfaceCache*) {});

    EXPECT_CALL(cache, GetSurface(false)).WillOnce(Return(pFakeSurf));

    EXPECT_EQ(::MFXMemory_GetSurfaceForEncode(pSession, &pSurf), MFX_ERR_NONE);
    EXPECT_EQ(pSurf, pFakeSurf);
}

TEST_F(MFXMemory_GetSurfaceForEncode, NewCache)
{
    auto& pCache = pSession->m_pENCODE->m_pSurfaceCache;
    mfxU16 type = 0xeeff & ~MFX_MEMTYPE_EXTERNAL_FRAME;
    ASSERT_TRUE(!pCache);

    EXPECT_CALL(*pEncoder, GetVideoParam(NotNull())).WillOnce(Return(MFX_ERR_NONE));

    //QueryIOSurf is static for native encoders, use plugin path w/a to mock
    auto pPlugin = new mocks::mfx::plugin_codec;
    pSession->m_plgEnc.reset(pPlugin);
    auto qios = [type](VideoCORE*, mfxVideoParam*, mfxFrameAllocRequest* req, mfxFrameAllocRequest*)
    {
        req->Type = type;
        return MFX_ERR_NONE;
    };
    EXPECT_CALL(*pPlugin, QueryIOSurf(Eq(pSession->m_pCORE.get()), NotNull(), NotNull(), IsNull())).WillOnce(qios);

    // real (not mocked) SurfCache will be created, use negative scenario for CreateSurface (positive one verified in OldCache test)
    EXPECT_CALL(*pCore, CreateSurface(Eq(type), _, _)).WillOnce(Return(MFX_ERR_UNSUPPORTED));

    EXPECT_EQ(::MFXMemory_GetSurfaceForEncode(pSession, &pSurf), MFX_ERR_MEMORY_ALLOC);
    EXPECT_TRUE(pSurf == nullptr);
    ASSERT_TRUE(!!pCache);

    EXPECT_CALL(*pCore, CreateSurface(Eq(type), _, _)).WillOnce(Return(MFX_ERR_UNSUPPORTED));
    EXPECT_EQ(pCache->GetSurface(false), nullptr);

    pSession->m_plgEnc.reset();
}

TEST_F(MFXMemory_GetSurfaceForEncode, Core1x)
{
    mocks::mfx::core core1x;
    pSession->m_pCORE.reset(&core1x, false);
    EXPECT_EQ(::MFXMemory_GetSurfaceForEncode(pSession, &pSurf), MFX_ERR_UNSUPPORTED);
}

TEST_F(MFXMemory_GetSurfaceForEncode, FailedGetVideoParam)
{
    EXPECT_CALL(*pEncoder, GetVideoParam(NotNull())).WillOnce(Return(mfxStatus(0xfeff)));
    EXPECT_EQ(::MFXMemory_GetSurfaceForEncode(pSession, &pSurf), mfxStatus(0xfeff));
}

TEST_F(MFXMemory_GetSurfaceForEncode, FailedQueryIOSurf)
{
    EXPECT_CALL(*pEncoder, GetVideoParam(NotNull())).WillOnce(Return(MFX_ERR_NONE));

    //QueryIOSurf is static for native encoders, use plugin path w/a to mock
    auto pPlugin = new mocks::mfx::plugin_codec;
    pSession->m_plgEnc.reset(pPlugin);
    EXPECT_CALL(*pPlugin, QueryIOSurf(Eq(pSession->m_pCORE.get()), NotNull(), NotNull(), IsNull())).WillOnce(Return(mfxStatus(0xfeff)));

    EXPECT_EQ(::MFXMemory_GetSurfaceForEncode(pSession, &pSurf), mfxStatus(0xfeff));
    pSession->m_plgEnc.reset();
}

} }
#endif