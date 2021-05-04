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

#if defined(MFX_ONEVPL)
#if defined (MFX_ENABLE_UNIT_TEST_ALLOCATOR)

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "mfxvideo++.h"
#include "mfx_common.h"

#include "libmfx_allocator.h"

#if defined(MFX_VA_WIN)
#include "libmfx_allocator_d3d11.h"

#include <initguid.h>
#include "mocks/include/guid.h"

#include "mocks/include/dxgi/format.h"
#include "mocks/include/mfx/dispatch.h"
#include "mocks/include/mfx/dx11/decoder.h"

#elif defined(MFX_VA_LINUX)
#include "libmfx_allocator_vaapi.h"

#include "mocks/include/va/display/display.h"
#include "mocks/include/va/display/surface.h"
#endif

#include "mocks/include/mfx/allocator.h"

namespace test
{
    struct AllocatorWrapperBase
        : testing::Test
    {

#if defined(MFX_VA_WIN)
        std::unique_ptr<mocks::dx11::context>        context;
        std::unique_ptr<mocks::mfx::dx11::component> component;
#elif defined(MFX_VA_LINUX)
        std::shared_ptr<mocks::va::display>          display;
#endif
        mfxFrameAllocRequest                         req{};
        mfxFrameAllocResponse                        res{};

        std::shared_ptr<mocks::mfx::allocator>       allocator;
        std::unique_ptr<FrameAllocatorWrapper>       allocator_wrapper;

        std::vector<int>                             buffer;

        void SetUp() override
        {
            allocator_wrapper.reset(
                new FrameAllocatorWrapper()
            );

            req.AllocId = 1;
            req.Info.FourCC = MFX_FOURCC_NV12;
            req.Info.Width = 640;
            req.Info.Height = 480;
            req.NumFrameMin = req.NumFrameSuggested = 1;
            req.Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_FROM_DECODE;

            allocator = mocks::mfx::make_allocator(
                req,
                std::make_tuple(mfxMemId(0), mfxFrameData{})
            );

            ASSERT_EQ(
                allocator_wrapper->GetExtAllocator(),
                nullptr
            );

            ASSERT_EQ(
                allocator_wrapper->SetFrameAllocator(*allocator),
                MFX_ERR_NONE
            );

            allocator_wrapper->allocator_sw.reset(
                new FlexibleFrameAllocatorSW{ nullptr }
            );
        }
    };

#if defined(MFX_VA_WIN)
    struct AllocatorWrapperExt
        : AllocatorWrapperBase
    {

        mfxVideoParam                                vp{};

        void SetUp() override
        {
            AllocatorWrapperBase::SetUp();

            /*factory = mocks::dxgi::make_factory(
                DXGI_ADAPTER_DESC1{ L"iGFX", INTEL_VENDOR_ID, DEVICE_ID },
                __uuidof(IDXGIFactory), __uuidof(IDXGIFactory1)
            );*/

            vp.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
            vp.mfx.FrameInfo.Width = 640;
            vp.mfx.FrameInfo.Height = 480;

            vp.mfx.CodecProfile = MFX_PROFILE_HEVC_MAIN;
            vp.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;

            buffer.resize(vp.mfx.FrameInfo.Width * vp.mfx.FrameInfo.Height * 8);
            context = mocks::dx11::make_context(
                std::make_tuple(D3D11_MAP_READ, buffer.data(), vp.mfx.FrameInfo.Width),
                std::make_tuple(D3D11_MAP_WRITE, buffer.data(), vp.mfx.FrameInfo.Width),
                std::make_tuple(D3D11_MAP_READ_WRITE, buffer.data(), vp.mfx.FrameInfo.Width)
            );

            auto constexpr type = mocks::mfx::HW_SCL;
            component.reset(
                new mocks::mfx::dx11::component(type, nullptr, vp)
            );
            mocks::dx11::mock_device(*(component->device.p), context.get(),
                D3D11_TEXTURE2D_DESC{ UINT(vp.mfx.FrameInfo.Width), UINT(vp.mfx.FrameInfo.Height), 1, 1, DXGI_FORMAT_NV12, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_DECODER | D3D11_BIND_SHADER_RESOURCE, 0, D3D11_RESOURCE_MISC_SHARED },
                D3D11_TEXTURE2D_DESC{ UINT(vp.mfx.FrameInfo.Width), UINT(vp.mfx.FrameInfo.Height), 1, 1, DXGI_FORMAT_NV12, {1, 0}, D3D11_USAGE_STAGING, 0, D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE, D3D11_RESOURCE_MISC_SHARED }
            );

            allocator_wrapper->allocator_hw.reset(
                new FlexibleFrameAllocatorHW_D3D11{ component->device.p }
            );

        }
    };
#elif defined(MFX_VA_LINUX)
    struct AllocatorWrapperExt
        : AllocatorWrapperBase
    {
        void SetUp() override
        {
            AllocatorWrapperBase::SetUp();

            buffer.resize(640 * 480 * 8);
            auto surface_attributes = VASurfaceAttrib{
                VASurfaceAttribPixelFormat,
                VA_SURFACE_ATTRIB_SETTABLE,
                { VAGenericValueTypeInteger,.value = {.i = VA_FOURCC_NV12} }
            };

            display = mocks::va::make_display(
                std::make_tuple(0, 0),  // version

                std::make_tuple(
                    VA_RT_FORMAT_YUV420,
                    640, 480,
                    std::make_tuple(surface_attributes),
                    [&](unsigned int /*format*/, unsigned int /*fourcc*/, size_t /*width*/, size_t /*height*/)
                    { return buffer.data(); }
                )
            );

            allocator_wrapper->allocator_hw.reset(
                new FlexibleFrameAllocatorHW_VAAPI{ display.get() }
            );
        }
    };
#endif

    TEST_F(AllocatorWrapperExt, Alloc)
    {
        EXPECT_EQ(
            allocator_wrapper->Alloc(req, res),
            MFX_ERR_NONE
        );
    }

    TEST_F(AllocatorWrapperExt, BadAlloc)
    {
        //'Alloc' returns no error but no frames actually allocated
        EXPECT_CALL(*allocator, Alloc)
            .WillRepeatedly(testing::WithoutArgs(testing::Invoke(
                []() { return MFX_ERR_NONE; }
        )));
        EXPECT_CALL(*allocator, Free).Times(1);

        EXPECT_EQ(
            allocator_wrapper->Alloc(req, res),
            MFX_ERR_MEMORY_ALLOC
        );
    }

    TEST_F(AllocatorWrapperExt, AllocNonDecodeMemType)
    {
        req.Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_FROM_ENCODE;
        EXPECT_CALL(*allocator, Alloc).Times(0);

        EXPECT_EQ(
            allocator_wrapper->Alloc(req, res),
            MFX_ERR_MEMORY_ALLOC
        );
    }

    TEST_F(AllocatorWrapperExt, AllocThrow)
    {
        EXPECT_CALL(*allocator, Alloc).WillRepeatedly(testing::WithoutArgs(testing::Invoke(
            []() -> mfxStatus
            { throw std::system_error(std::make_error_code(MFX_ERR_MEMORY_ALLOC)); }
        )));

        EXPECT_EQ(
            allocator_wrapper->Alloc(req, res, true),
            MFX_ERR_MEMORY_ALLOC
        );
    }

    TEST_F(AllocatorWrapperExt, AllocExtHint)
    {
        // Put req Type w/ which we can't use external allocator w/o ext_alloc_hint
        req.Type = MFX_MEMTYPE_EXTERNAL_FRAME | MFX_MEMTYPE_FROM_ENCODE;
        mock_allocator(*allocator, req);

        EXPECT_EQ(
            allocator_wrapper->Alloc(req, res, true),
            MFX_ERR_NONE
        );
    }

    TEST_F(AllocatorWrapperExt, AllocInternalHWVideoMemType)
    {
        req.Type = MFX_MEMTYPE_INTERNAL_FRAME | MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET;
        EXPECT_CALL(*allocator, Alloc).Times(0);

        EXPECT_EQ(
            allocator_wrapper->Alloc(req, res),
            MFX_ERR_NONE
        );
    }

    TEST_F(AllocatorWrapperExt, AllocInternalSWSystemMemType)
    {
        EXPECT_CALL(*allocator, Alloc).Times(0);

        req.Type = MFX_MEMTYPE_SYSTEM_MEMORY | MFX_MEMTYPE_INTERNAL_FRAME;
        ASSERT_EQ(
            allocator_wrapper->Alloc(req, res),
            MFX_ERR_NONE
        );
    }

    TEST_F(AllocatorWrapperExt, AllocInternalSWVideoMemType)
    {
        req.Type = MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET | MFX_MEMTYPE_INTERNAL_FRAME;
        EXPECT_CALL(*allocator, Alloc).Times(0);

        allocator_wrapper->allocator_hw.reset(nullptr);

        ASSERT_EQ(
            allocator_wrapper->Alloc(req, res),
            MFX_ERR_UNSUPPORTED
        );
    }

    TEST_F(AllocatorWrapperExt, Lock)
    {
        ASSERT_EQ(
            allocator_wrapper->Alloc(req, res),
            MFX_ERR_NONE
        );

        mfxFrameData data{};
        EXPECT_EQ(
            allocator_wrapper->Lock(res.mids[0], &data),
            MFX_ERR_NONE
        );
    }

    TEST_F(AllocatorWrapperExt, LockExternalMemId)
    {
        mfxFrameData data{};
        EXPECT_EQ(
            allocator_wrapper->Lock(mfxMemId(0), &data),
            MFX_ERR_NONE
        );
    }

    TEST_F(AllocatorWrapperExt, LockExternalMemIdNoExtAllocator)
    {
        EXPECT_CALL(*allocator, Lock).Times(0);
        allocator_wrapper->allocator_ext.reset(nullptr);

        mfxFrameData data{};
        EXPECT_EQ(
            allocator_wrapper->Lock(mfxMemId(0), &data),
            MFX_ERR_LOCK_MEMORY
        );
    }

    TEST_F(AllocatorWrapperExt, Unlock)
    {
        ASSERT_EQ(
            allocator_wrapper->Alloc(req, res),
            MFX_ERR_NONE
        );

        mfxFrameData data{};

        ASSERT_EQ(
            allocator_wrapper->Lock(res.mids[0], &data),
            MFX_ERR_NONE
        );

        EXPECT_EQ(
            allocator_wrapper->Unlock(res.mids[0], &data),
            MFX_ERR_NONE
        );
    }

    TEST_F(AllocatorWrapperExt, UnlockNoExtAllocator)
    {
        EXPECT_CALL(*allocator, Unlock).Times(0);
        allocator_wrapper->allocator_ext.reset(nullptr);

        mfxFrameData data{};
        EXPECT_EQ(
            allocator_wrapper->Unlock(mfxMemId(0), &data),
            MFX_ERR_UNKNOWN
        );
    }

    TEST_F(AllocatorWrapperExt, GetHDL)
    {
        ASSERT_EQ(
            allocator_wrapper->Alloc(req, res),
            MFX_ERR_NONE
        );

        mfxHDL hdl{};

        EXPECT_EQ(
            allocator_wrapper->GetHDL(res.mids[0], hdl),
            MFX_ERR_NONE
        );
    }

    TEST_F(AllocatorWrapperExt, GetHDLNoExtAllocator)
    {
        EXPECT_CALL(*allocator, GetHDL).Times(0);
        allocator_wrapper->allocator_ext.reset(nullptr);

        mfxHDL hdl{};
        EXPECT_EQ(
            allocator_wrapper->GetHDL(mfxMemId(0), hdl),
            MFX_ERR_UNDEFINED_BEHAVIOR
        );
    }

    TEST_F(AllocatorWrapperExt, Realloc)
    {
        ASSERT_EQ(
            allocator_wrapper->Alloc(req, res),
            MFX_ERR_NONE
        );

        ASSERT_EQ(
            allocator_wrapper->ReallocSurface(req.Info, res.mids[0]),
            MFX_ERR_UNSUPPORTED
        );
    }

    TEST_F(AllocatorWrapperExt, ReallocExternalMemId)
    {
        EXPECT_CALL(*allocator, Alloc).Times(0);
        EXPECT_CALL(*allocator, Free).Times(0);

        EXPECT_EQ(
            allocator_wrapper->ReallocSurface(req.Info, mfxMemId(0)),
            MFX_ERR_UNKNOWN
        );
    }

    TEST_F(AllocatorWrapperExt, CreateSufaceNoHWAllocator)
    {
        EXPECT_CALL(*allocator, Alloc).Times(0);

        allocator_wrapper->allocator_hw.reset(nullptr);

        mfxFrameSurface1* surface{};
        EXPECT_EQ(
            allocator_wrapper->CreateSurface(req.Type, req.Info, surface),
            MFX_ERR_UNSUPPORTED
        );
    }

    TEST_F(AllocatorWrapperExt, CreateSufaceNoHWAllocatorVideoMemory)
    {
        req.Type |= MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET;
        EXPECT_CALL(*allocator, Alloc).Times(0);

        allocator_wrapper->allocator_hw.reset(nullptr);

        mfxFrameSurface1* surface{};
        EXPECT_EQ(
            allocator_wrapper->CreateSurface(req.Type, req.Info, surface),
            MFX_ERR_UNSUPPORTED
        );
    }

    TEST_F(AllocatorWrapperExt, CreateSurfaceNoAllocator)
    {
        EXPECT_CALL(*allocator, Alloc).Times(0);
        allocator_wrapper->allocator_sw.reset(nullptr);
        allocator_wrapper->allocator_hw.reset(nullptr);

        mfxFrameSurface1* surface{};
        EXPECT_EQ(
            allocator_wrapper->CreateSurface(req.Type, req.Info, surface),
            MFX_ERR_INVALID_HANDLE
        );
    }

    TEST_F(AllocatorWrapperExt, GetExtAllocator)
    {
        auto* ext_alloc = allocator_wrapper->GetExtAllocator();

        EXPECT_NE(
            ext_alloc,
            nullptr
        );
    }

    TEST_F(AllocatorWrapperExt, Free)
    {
        ASSERT_EQ(
            allocator_wrapper->Alloc(req, res),
            MFX_ERR_NONE
        );

        ASSERT_EQ(
            allocator_wrapper->Free(res),
            MFX_ERR_NONE
        );
    }

    TEST_F(AllocatorWrapperExt, FreeNullMids)
    {
        ASSERT_EQ(
            allocator_wrapper->Free(res),
            MFX_ERR_NULL_PTR
        );
    }

    TEST_F(AllocatorWrapperExt, FreeNoExtAllocator)
    {
        auto mid = mfxMemId(0);
        res.mids = &mid;

        ASSERT_EQ(
            allocator_wrapper->Free(res),
            MFX_ERR_UNKNOWN
        );
    }

}

#endif // MFX_ENABLE_UNIT_TEST_ALLOCATOR
#endif
