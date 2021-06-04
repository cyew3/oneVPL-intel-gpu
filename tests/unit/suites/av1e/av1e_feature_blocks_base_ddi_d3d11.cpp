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

#define ENABLE_MFX_INTEL_GUID_DECODE
#define ENABLE_MFX_INTEL_GUID_ENCODE
#define ENABLE_MFX_INTEL_GUID_PRIVATE

#include <initguid.h>
#include "mocks/include/mfx/guids.h"

#include "mocks/include/dxgi/format.h"
#include "mocks/include/dxgi/device/factory.h"

#include "mocks/include/dx9/device/d3d.h"

#include "mocks/include/mfx/dispatch.h"
#include "mocks/include/mfx/dx11/encoder.h"
#include "mocks/include/mfx/dx11/decoder.h"

#include "libmfx_core_factory.h"


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

        VideoCORE*                            core = nullptr;

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
            mfxVideoParam vp{};
            vp.mfx.FrameInfo.Width  = 640;
            vp.mfx.FrameInfo.Height = 480;
            vp = mocks::mfx::make_param(
                mocks::fourcc::format<MFX_FOURCC_NV12>{},
                mocks::mfx::make_param(mocks::guid<&::DXVA2_Intel_LowpowerEncode_AV1_420_8b>{}, vp)
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

            device = mocks::mfx::dx11::make_component(std::integral_constant<int, mocks::mfx::HW_DG2>{},
                nullptr, context.get(),
                std::make_tuple(mocks::guid<&::DXVA2_Intel_LowpowerEncode_AV1_420_8b>{}, vp)
            );

            core = FactoryCORE::CreateCORE(MFX_HW_D3D11, 0, 0, nullptr);

            storage.Insert(Glob::VideoCore::Key, new StorableRef<VideoCORE>(*core));

            //select proper GUID for encoder
            general.Init(AV1EHW::INIT, blocks);
            auto blkQuery1NoCaps = FeatureBlocks::Get(
                FeatureBlocks::BQ<FeatureBlocks::BQ_InitExternal>::Get(blocks),
                { FEATURE_GENERAL, General::BLK_Query1NoCaps }
            );

            EXPECT_EQ(
                blkQuery1NoCaps->Call(vp, storage, storage),
                MFX_ERR_NONE
            );

            //create AUX device
            ddi_d3d11.Init(AV1EHW::INIT | AV1EHW::RUNTIME, blocks);
            auto setCallChains = FeatureBlocks::Get(
                FeatureBlocks::BQ<FeatureBlocks::BQ_Query1NoCaps>::Get(blocks),
                { FEATURE_DDI, IDDI::BLK_SetCallChains }
            );

            EXPECT_EQ(
                setCallChains->Call(vp, vp, storage),
                MFX_ERR_NONE
            );

            auto create = FeatureBlocks::Get(
                FeatureBlocks::BQ<FeatureBlocks::BQ_InitExternal>::Get(blocks),
                { FEATURE_DDI, IDDI::BLK_CreateDevice }
            );

            EXPECT_EQ(
                create->Call(vp, storage, storage),
                MFX_ERR_NONE
            );

            Glob::DDI_SubmitParam::GetOrConstruct(storage);
            Task::Common::GetOrConstruct(tasks);
        };

        void TearDown() override
        {
        }
    };

    TEST_F(FeatureBlocksDDI_D3D11, QueryCaps)
    {
        auto& queueQ1WC = FeatureBlocks::BQ<FeatureBlocks::BQ_Query1WithCaps>::Get(blocks);
        auto block = FeatureBlocks::Get(queueQ1WC, { FEATURE_DDI, IDDI::BLK_QueryCaps });

        ASSERT_TRUE(
            storage.Contains(Glob::GUID::Key)
        );

        auto& vp = Glob::VideoParam::GetOrConstruct(storage);
        Glob::DDI_Execute::GetOrConstruct(storage);
        EXPECT_EQ(
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

    TEST_F(FeatureBlocksDDI_D3D11, SubmitTaskExecuteFailure)
    {
        mock_context(*context,
            std::make_tuple(
                D3D11_VIDEO_DECODER_EXTENSION{
                    DXVA2_SET_GPU_TASK_EVENT_HANDLE
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
            DDIExecParam{ DXVA2_SET_GPU_TASK_EVENT_HANDLE }
        );

        EXPECT_EQ(
            block->Call(storage, tasks),
            MFX_ERR_DEVICE_FAILED
        );
    }

    TEST_F(FeatureBlocksDDI_D3D11, SubmitTaskExecute)
    {
        //registered extension will return [S_OK] by default
        mock_context(*context,
            D3D11_VIDEO_DECODER_EXTENSION{
                DXVA2_SET_GPU_TASK_EVENT_HANDLE
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
            DDIExecParam{ DXVA2_SET_GPU_TASK_EVENT_HANDLE }
        );

        EXPECT_EQ(
            block->Call(storage, tasks),
            MFX_ERR_NONE
        );
    }

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
        Glob::DDI_Execute::GetOrConstruct(storage);
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
