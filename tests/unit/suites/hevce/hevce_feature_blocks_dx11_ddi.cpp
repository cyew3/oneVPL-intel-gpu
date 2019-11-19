// Copyright (c) 2019 Intel Corporation
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

#if defined(MFX_ENABLE_UNIT_TEST_HEVCE_DX11_DDI)

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "mfxvideo++.h"
#include "mfx_session.h"

#include "g11/hevcehw_g11_legacy.h"
#include "g11/hevcehw_g11_d3d11_win.h"

#include "mocks/include/guid.h"

#include "mocks/include/dxgi/format.h"

#include "mocks/include/mfx/dispatch.h"
#include "mocks/include/mfx/dx11/encoder.h"
#include "mocks/include/mfx/dx11/decoder.h"

#define INITGUID
#include <guiddef.h>
DEFINE_GUID(DXVA_NoEncrypt,   0x1b81beD0, 0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(DXVA2_ModeH264_E, 0x1b81be68, 0xa0c7,0x11d3,0xb9,0x84,0x00,0xc0,0x4f,0x2e,0x73,0xc5);
DEFINE_GUID(IID_IDirectXVideoDecoderService,      0xfc51a551,0xd5e7,0x11d9,0xaf,0x55,0x00,0x05,0x4e,0x43,0xff,0x02);
DEFINE_GUID(IID_IDirectXVideoProcessorService,    0xfc51a552,0xd5e7,0x11d9,0xaf,0x55,0x00,0x05,0x4e,0x43,0xff,0x02);

namespace hevce { namespace tests
{
    using namespace HEVCEHW;

    int constexpr INTEL_VENDOR_ID = 0x8086;

    int constexpr DEVICE_ID       = 42; //The value which is returned both as 'DeviceID' by adapter and by mocked registry.
                                        //Actual value doesn't matter here.

    struct FeatureBlocksDDI
        : testing::Test
    {
        std::unique_ptr<mocks::registry>      registry;
        std::unique_ptr<mocks::dx11::context> context;
        std::unique_ptr<mocks::dx11::device>  device;

        MFXVideoSession                       session;

        FeatureBlocks                         blocks{};

        HEVCEHW::Gen11::Legacy                legacy;
        Windows::Gen11::DDI_D3D11             ddi;

        StorageRW                             storage;
        StorageRW                             tasks;

        FeatureBlocksDDI()
            : legacy(HEVCEHW::Gen11::FEATURE_LEGACY)
            , ddi(Gen11::FEATURE_DDI)
        {}

        void SetUp() override
        {
            //mock registry that MFX Dispatcher finds right 'libmfxhw'
            ASSERT_NO_THROW({
                registry = mocks::mfx::dispatch_to(INTEL_VENDOR_ID, DEVICE_ID, 1);
            });

            mfxVideoParam vp{};
            vp.mfx.FrameInfo.Width  = 640;
            vp.mfx.FrameInfo.Height = 480;
            vp = mocks::mfx::make_param(
                mocks::fourcc::tag<MFX_FOURCC_NV12>{},
                mocks::mfx::make_param(mocks::guid<&DXVA2_Intel_Encode_HEVC_Main>{}, vp)
            );

            //'CreateAuxilliaryDevice' queries this decoder's extension
            ENCODE_CAPS_HEVC caps{};
            context = mocks::dx11::make_context(
                D3D11_VIDEO_DECODER_EXTENSION{
                    ENCODE_QUERY_ACCEL_CAPS_ID,
                    nullptr, 0,
                    &caps, sizeof(caps)
                }
            );

            device = mocks::mfx::dx11::make_encoder(nullptr, context.get(),
                std::integral_constant<int, mocks::mfx::HW_SKL>{},
                std::make_tuple(mocks::guid<&DXVA2_Intel_Encode_HEVC_Main>{}, vp)
            );

            ASSERT_EQ(
                session.InitEx(
                    mfxInitParam{ MFX_IMPL_HARDWARE | MFX_IMPL_VIA_D3D11, { 0, 1 } }
                ),
                MFX_ERR_NONE
            );

            //populate session's dx11 core for encoder
            auto s = static_cast<_mfxSession*>(session);
            storage.Insert(Gen11::Glob::VideoCore::Key, new StorableRef<VideoCORE>(*(s->m_pCORE.get())));

            legacy.Init(HEVCEHW::INIT, blocks);

            //select proper GUID for encoder
            auto guid = FeatureBlocks::Get(
                FeatureBlocks::BQ<FeatureBlocks::BQ_InitExternal>::Get(blocks),
                { Gen11::FEATURE_LEGACY, Gen11::Legacy::BLK_SetGUID }
            );

            ASSERT_EQ(
                guid->Call(vp, storage, storage),
                MFX_ERR_NONE
            );

            ddi.Init(INIT | RUNTIME, blocks);

            //need to setup [DefaultExecute]
            auto chain = FeatureBlocks::Get(
                FeatureBlocks::BQ<FeatureBlocks::BQ_Query1NoCaps>::Get(blocks),
                { Gen11::FEATURE_DDI, Windows::Gen11::IDDI::BLK_SetCallChains }
            );

            ASSERT_EQ(
                chain->Call(vp, vp, storage),
                MFX_ERR_NONE
            );

            //create AUX device
            auto create = FeatureBlocks::Get(
                FeatureBlocks::BQ<FeatureBlocks::BQ_InitExternal>::Get(blocks),
                { Gen11::FEATURE_DDI, Windows::Gen11::IDDI::BLK_CreateDevice }
            );

            ASSERT_EQ(
                create->Call(vp, storage, storage),
                MFX_ERR_NONE
            );

            Gen11::Glob::DDI_SubmitParam::GetOrConstruct(storage);
            Gen11::Task::Common::GetOrConstruct(tasks);
        }
    };

    TEST_F(FeatureBlocksDDI, SubmitTaskSkipDriverCall)
    {
        auto& queueSubmit = FeatureBlocks::BQ<FeatureBlocks::BQ_SubmitTask>::Get(blocks);
        auto block = FeatureBlocks::Get(queueSubmit, { Gen11::FEATURE_DDI, Windows::Gen11::IDDI::BLK_SubmitTask });

        auto& task =
            Gen11::Task::Common::Get(tasks);

        ASSERT_FALSE(
            task.SkipCMD & Gen11::SKIPCMD_NeedDriverCall
        );

        EXPECT_EQ(
            block->Call(storage, tasks),
            MFX_ERR_NONE
        );
    }

    TEST_F(FeatureBlocksDDI, SubmitTaskExecuteEmptyList)
    {
        auto& queueSubmit = FeatureBlocks::BQ<FeatureBlocks::BQ_SubmitTask>::Get(blocks);
        auto block = FeatureBlocks::Get(queueSubmit, { Gen11::FEATURE_DDI, Windows::Gen11::IDDI::BLK_SubmitTask });

        auto& task =
            Gen11::Task::Common::Get(tasks);
        task.SkipCMD |= Gen11::SKIPCMD_NeedDriverCall;

        auto& params = Gen11::Glob::DDI_SubmitParam::Get(storage);
        ASSERT_TRUE(
            params.empty()
        );

        EXPECT_EQ(
            block->Call(storage, tasks),
            MFX_ERR_NONE
        );
    }

    TEST_F(FeatureBlocksDDI, SubmitTaskExecuteUnknownFunction)
    {
        auto& queueSubmit = FeatureBlocks::BQ<FeatureBlocks::BQ_SubmitTask>::Get(blocks);
        auto block = FeatureBlocks::Get(queueSubmit, { Gen11::FEATURE_DDI, Windows::Gen11::IDDI::BLK_SubmitTask });

        auto& params = Gen11::Glob::DDI_SubmitParam::Get(storage);
        params.push_back(
            Gen11::DDIExecParam{}
        );

        ASSERT_EQ(
            params.size(), 1
        );
        ASSERT_EQ(
            params.back().Function, mfxU32(0) //Unknown function
        );

        auto& task =
            Gen11::Task::Common::Get(tasks);
        task.SkipCMD |= Gen11::SKIPCMD_NeedDriverCall;

        EXPECT_EQ(
            block->Call(storage, tasks),
            MFX_ERR_DEVICE_FAILED
        );
    }

    TEST_F(FeatureBlocksDDI, SubmitTaskExecuteFailure)
    {
        mock_context(*context,
            std::make_tuple(
                D3D11_VIDEO_DECODER_EXTENSION{
                    ENCODE_QUERY_STATUS_ID
                },
                [](D3D11_VIDEO_DECODER_EXTENSION const*)
                { return E_FAIL; } //return failure for [ID3D11VideoContext::DecoderExtension]
            )
        );

        auto& queueSubmit = FeatureBlocks::BQ<FeatureBlocks::BQ_SubmitTask>::Get(blocks);
        auto block = FeatureBlocks::Get(queueSubmit, { Gen11::FEATURE_DDI, Windows::Gen11::IDDI::BLK_SubmitTask });

        auto& params = Gen11::Glob::DDI_SubmitParam::Get(storage);
        params.push_back(
            Gen11::DDIExecParam{ ENCODE_QUERY_STATUS_ID }
        );

        auto& task =
            Gen11::Task::Common::Get(tasks);
        task.SkipCMD |= Gen11::SKIPCMD_NeedDriverCall;

        EXPECT_EQ(
            block->Call(storage, tasks),
            MFX_ERR_DEVICE_FAILED
        );
    }

    TEST_F(FeatureBlocksDDI, SubmitTaskExecute)
    {
        //registered extension will return [S_OK] by default
        mock_context(*context,
            D3D11_VIDEO_DECODER_EXTENSION{
                ENCODE_QUERY_STATUS_ID
            }
        );

        auto& queueSubmit = FeatureBlocks::BQ<FeatureBlocks::BQ_SubmitTask>::Get(blocks);
        auto block = FeatureBlocks::Get(queueSubmit, { Gen11::FEATURE_DDI, Windows::Gen11::IDDI::BLK_SubmitTask });

        auto& params = Gen11::Glob::DDI_SubmitParam::Get(storage);
        params.push_back(
            Gen11::DDIExecParam{ ENCODE_QUERY_STATUS_ID }
        );

        auto& task =
            Gen11::Task::Common::Get(tasks);
        task.SkipCMD |= Gen11::SKIPCMD_NeedDriverCall;

        EXPECT_EQ(
            block->Call(storage, tasks),
            MFX_ERR_NONE
        );
    }

} }

#endif //MFX_ENABLE_UNIT_TEST_HEVCE_DX11_DDI