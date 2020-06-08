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

#if defined(MFX_ENABLE_UNIT_TEST_AV1E_DX11_DDI)

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "mfxvideo++.h"
#include "mfx_session.h"

#include "av1ehw_base_general.h"
#include "av1ehw_base_d3d11_win.h"

#include "mocks/include/guid.h"

#include "mocks/include/dxgi/format.h"
#include "mocks/include/dxgi/device/factory.h"

#include "mocks/include/dx9/device/d3d.h"
#include "mocks/include/dx11/device/device.h"

#include "mocks/include/mfx/platform.h"
#include "mocks/include/mfx/traits.h"

#include "mocks/include/mfx/dispatch.h"
#include "mocks/include/mfx/dx11/encoder.h"
#include "mocks/include/mfx/dx11/decoder.h"

#define INITGUID
#include <guiddef.h>
DEFINE_GUID(DXVA_NoEncrypt,   0x1b81beD0, 0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(DXVA2_ModeH264_E, 0x1b81be68, 0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(IID_IDirectXVideoDecoderService,      0xfc51a551,0xd5e7,0x11d9,0xaf,0x55,0x00,0x05,0x4e,0x43,0xff,0x02);
DEFINE_GUID(IID_IDirectXVideoProcessorService,    0xfc51a552,0xd5e7,0x11d9,0xaf,0x55,0x00,0x05,0x4e,0x43,0xff,0x02);

namespace AV1EHW
{
namespace Windows
{
namespace Base
{
    int constexpr INTEL_VENDOR_ID = 0x8086;

    int constexpr DEVICE_ID       = 42; //The value which is returned both as 'DeviceID' by adapter and by mocked registry.
                                        //Actual value doesn't matter here.

    struct FeatureBlocksDDI_D3D11
        : testing::Test
    {
        std::unique_ptr<mocks::registry>      registry;
        std::unique_ptr<mocks::dx11::context> context;
        std::unique_ptr<mocks::dx11::device>  device;

        MFXVideoSession                       session;

        FeatureBlocks                         blocks{};
        General                               general;
        DDI_D3D11                             ddi_d3d11;
        StorageRW                             storage;
        StorageRW                             tasks;

        FeatureBlocksDDI_D3D11()
            : general(FEATURE_GENERAL)
            , ddi_d3d11(FEATURE_DDI)
        {

        }

        void SetUp() override
        {
            ////mock registry that MFX Dispatcher finds right 'libmfxhw'
            //ASSERT_NO_THROW({
            //    registry = mocks::mfx::dispatch_to(INTEL_VENDOR_ID, DEVICE_ID, 1);
            //});

            mfxVideoParam vp{};
            vp.mfx.FrameInfo.Width  = 640;
            vp.mfx.FrameInfo.Height = 480;
            vp = mocks::mfx::make_param(
                mocks::fourcc::tag<MFX_FOURCC_NV12>{},
                mocks::mfx::make_param(mocks::guid<&DXVA2_Intel_LowpowerEncode_AV1_420_8b>{}, vp)
            );

            //'CreateAuxilliaryDevice' queries this decoder's extension
            ENCODE_CAPS_AV1 caps{};
            context = mocks::dx11::make_context(
                D3D11_VIDEO_DECODER_EXTENSION{
                    ENCODE_QUERY_ACCEL_CAPS_ID,
                    nullptr, 0,
                    &caps, sizeof(caps)
                }
            );

            device = mocks::mfx::dx11::make_encoder(nullptr, context.get(),
                std::integral_constant<eMFXHWType, MFX_HW_DG2>{},
                std::make_tuple(mocks::guid<&DXVA2_Intel_LowpowerEncode_AV1_420_8b>{}, vp)
            );

            ASSERT_EQ(
                session.InitEx(
                    mfxInitParam{ MFX_IMPL_HARDWARE | MFX_IMPL_VIA_D3D11, { 0, 1 } }
                ),
                MFX_ERR_NONE
            );

            auto s = static_cast<_mfxSession*>(session);
            storage.Insert(Glob::VideoCore::Key, new StorableRef<VideoCORE>(*(s->m_pCORE.get())));

            //select proper GUID for encoder
            general.Init(AV1EHW::INIT, blocks);
            auto guid = FeatureBlocks::Get(
                FeatureBlocks::BQ<FeatureBlocks::BQ_InitExternal>::Get(blocks),
                { FEATURE_GENERAL, General::BLK_SetGUID }
            );

            ASSERT_EQ(
                guid->Call(vp, storage, storage),
                MFX_ERR_NONE
            );

            //create AUX device
            ddi_d3d11.Init(AV1EHW::INIT | AV1EHW::RUNTIME, blocks);
            auto create = FeatureBlocks::Get(
                FeatureBlocks::BQ<FeatureBlocks::BQ_InitExternal>::Get(blocks),
                { FEATURE_DDI, IDDI::BLK_CreateDevice }
            );

            ASSERT_EQ(
                create->Call(vp, storage, storage),
                MFX_ERR_NONE
            );

            Glob::DDI_SubmitParam::GetOrConstruct(storage);
            Task::Common::GetOrConstruct(tasks);
        };

        void TearDown() override
        {
            storage.Clear();
            tasks.Clear();
        }
    };

    TEST_F(FeatureBlocksDDI_D3D11, QueryWithNoGUID)
    {
        auto& queueQ1WC = FeatureBlocks::BQ<FeatureBlocks::BQ_Query1WithCaps>::Get(blocks);
        auto block = FeatureBlocks::Get(queueQ1WC, { FEATURE_DDI, IDDI::BLK_QueryCaps });

        storage.Erase(Glob::GUID::Key);
        ASSERT_TRUE(
            !storage.Contains(Glob::GUID::Key)
        );

        auto& vp = Glob::VideoParam::GetOrConstruct(storage);
        ASSERT_EQ(
            block->Call(vp, vp, storage),
            MFX_ERR_UNSUPPORTED
        );
    }

    TEST_F(FeatureBlocksDDI_D3D11, QueryWithGUID)
    {
        auto& queueQ1WC = FeatureBlocks::BQ<FeatureBlocks::BQ_Query1WithCaps>::Get(blocks);
        auto block = FeatureBlocks::Get(queueQ1WC, { FEATURE_DDI, IDDI::BLK_QueryCaps });

        ASSERT_TRUE(
            storage.Contains(Glob::GUID::Key)
        );

        auto& vp = Glob::VideoParam::GetOrConstruct(storage);
        ASSERT_EQ(
            block->Call(vp, vp, storage),
            MFX_ERR_NONE
        );
    }

    TEST_F(FeatureBlocksDDI_D3D11, SubmitTaskSkipDriverCall)
    {
        auto& queueST = FeatureBlocks::BQ<FeatureBlocks::BQ_SubmitTask>::Get(blocks);
        auto block = FeatureBlocks::Get(queueST, { FEATURE_DDI, IDDI::BLK_SubmitTask });
        auto& task = Task::Common::Get(tasks);

        task.SkipCMD &= (~SKIPCMD_NeedDriverCall);
        ASSERT_FALSE(
            task.SkipCMD & SKIPCMD_NeedDriverCall
        );

        EXPECT_EQ(
            block->Call(storage, tasks),
            MFX_ERR_NONE
        );
    }

    TEST_F(FeatureBlocksDDI_D3D11, SubmitTaskExecuteEmptyList)
    {
        auto& queueST = FeatureBlocks::BQ<FeatureBlocks::BQ_SubmitTask>::Get(blocks);
        auto block = FeatureBlocks::Get(queueST, { FEATURE_DDI, IDDI::BLK_SubmitTask });
        auto& task = Task::Common::Get(tasks);

        ASSERT_TRUE(
            task.SkipCMD & SKIPCMD_NeedDriverCall
        );

        auto& params = Glob::DDI_SubmitParam::Get(storage);
        ASSERT_TRUE(
            params.empty()
        );

        EXPECT_EQ(
            block->Call(storage, tasks),
            MFX_ERR_NONE
        );
    }

    TEST_F(FeatureBlocksDDI_D3D11, SubmitTaskExecuteUnknownFunction)
    {
        auto& queueST = FeatureBlocks::BQ<FeatureBlocks::BQ_SubmitTask>::Get(blocks);
        auto block = FeatureBlocks::Get(queueST, { FEATURE_DDI, IDDI::BLK_SubmitTask });
        auto& task = Task::Common::Get(tasks);

        ASSERT_TRUE(
            task.SkipCMD & SKIPCMD_NeedDriverCall
        );

        auto& params = Glob::DDI_SubmitParam::Get(storage);
        params.push_back(
            DDIExecParam{}
        );

        ASSERT_EQ(
            params.size(), 1
        );

        ASSERT_EQ(
            params.back().Function, mfxU32(0) //Unknown function
        );

        EXPECT_EQ(
            block->Call(storage, tasks),
            MFX_ERR_DEVICE_FAILED
        );
    }

    // TODO: The test is disabled. Fix the test after merge to master.
    /*TEST_F(FeatureBlocksDDI_D3D11, SubmitTaskExecuteFailure)
    {
        mock_context(*context,
            std::make_tuple(
                D3D11_VIDEO_DECODER_EXTENSION{
                    DXVA2_PRIVATE_SET_GPU_TASK_EVENT_HANDLE
                },
                [](D3D11_VIDEO_DECODER_EXTENSION const*)
                { return E_FAIL; } //return failure for [ID3D11VideoContext::DecoderExtension]
            )
        );

        auto& queueST = FeatureBlocks::BQ<FeatureBlocks::BQ_SubmitTask>::Get(blocks);
        auto block = FeatureBlocks::Get(queueST, { FEATURE_DDI, IDDI::BLK_SubmitTask });
        auto& task = Task::Common::Get(tasks);

        ASSERT_TRUE(
            task.SkipCMD & SKIPCMD_NeedDriverCall
        );

        auto& params = Glob::DDI_SubmitParam::Get(storage);
        params.push_back(
            DDIExecParam{ DXVA2_PRIVATE_SET_GPU_TASK_EVENT_HANDLE }
        );

        EXPECT_EQ(
            block->Call(storage, tasks),
            MFX_ERR_DEVICE_FAILED
        );
    }*/

    // TODO: The test is disabled. Fix the test after merge to master.
    /*TEST_F(FeatureBlocksDDI_D3D11, SubmitTaskExecute)
    {
        //registered extension will return [S_OK] by default
        mock_context(*context,
            D3D11_VIDEO_DECODER_EXTENSION{
                DXVA2_PRIVATE_SET_GPU_TASK_EVENT_HANDLE
            }
        );

        auto& queueST = FeatureBlocks::BQ<FeatureBlocks::BQ_SubmitTask>::Get(blocks);
        auto block = FeatureBlocks::Get(queueST, { FEATURE_DDI, IDDI::BLK_SubmitTask });
        auto& task = Task::Common::Get(tasks);

        ASSERT_TRUE(
            task.SkipCMD & SKIPCMD_NeedDriverCall
        );

        auto& params = Glob::DDI_SubmitParam::Get(storage);
        params.push_back(
            DDIExecParam{ DXVA2_PRIVATE_SET_GPU_TASK_EVENT_HANDLE }
        );

        EXPECT_EQ(
            block->Call(storage, tasks),
            MFX_ERR_NONE
        );
    }*/

    struct FeatureBlocksDDI_D3D11_NoInit
        : testing::Test
    {
        FeatureBlocks                         blocks{};
        DDI_D3D11                             ddi_d3d11;
        StorageRW                             storage;
        StorageRW                             tasks;

        FeatureBlocksDDI_D3D11_NoInit()
            : ddi_d3d11(FEATURE_DDI)
        {

        }

        void SetUp() override
        {
            ddi_d3d11.Init(AV1EHW::INIT | AV1EHW::RUNTIME, blocks);

            Glob::DDI_SubmitParam::GetOrConstruct(storage);
            Task::Common::GetOrConstruct(tasks);
        };

        void TearDown() override
        {
            storage.Clear();
            tasks.Clear();
        }
    };

    TEST_F(FeatureBlocksDDI_D3D11_NoInit, SubmitTaskNoInit)
    {
        auto& queueST = FeatureBlocks::BQ<FeatureBlocks::BQ_SubmitTask>::Get(blocks);
        auto block = FeatureBlocks::Get(queueST, { FEATURE_DDI, IDDI::BLK_SubmitTask });
        auto& task = Task::Common::Get(tasks);

        ASSERT_TRUE(
            task.SkipCMD & SKIPCMD_NeedDriverCall
        );

        auto& params = Glob::DDI_SubmitParam::Get(storage);
        ASSERT_TRUE(
            params.empty()
        );

        EXPECT_EQ(
            block->Call(storage, tasks),
            MFX_ERR_NOT_INITIALIZED
        );
    }

} //Base
} //Windows
} //AV1EHW

#endif // MFX_ENABLE_UNIT_TEST_AV1E_DX11_DDI