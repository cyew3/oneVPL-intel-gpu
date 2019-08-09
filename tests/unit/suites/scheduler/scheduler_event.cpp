/*********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2019 Intel Corporation. All Rights Reserved.

**********************************************************************************/

#include "mfx_scheduler_core.h"
#include "libmfx_core_d3d11.h"

#include "gmock/gmock.h"

using ::testing::AtLeast;
using ::testing::Mock;
using ::testing::Return;
using ::testing::_;

#if defined (MFX_D3D11_ENABLED)

class MockSchedulerCore : public mfxSchedulerCore
{
public:
    void ** GetHwEvent()
    {
        return &m_hwTaskDone.handle;
    }
};

class MockCommonCORE : public CommonCORE
{
    friend class MockAdapter;
    class MockAdapter : public D3D11Interface
    {
    public:
        MockAdapter(MockCommonCORE *pD3D11Core) : m_pMockCommonCORE(pD3D11Core) {};

        ID3D11Device* GetD3D11Device(bool isTemporal = false)
        {
            m_pMockCommonCORE->InitializeDevice();
            return m_pMockCommonCORE->m_pD11Device;
        }
        ID3D11VideoContext* GetD3D11VideoContext(bool isTemporal = false)
        {
            m_pMockCommonCORE->InitializeDevice();
            return m_pMockCommonCORE->m_pD11VideoContext;
        }
        ID3D11VideoDevice* GetD3D11VideoDevice(bool isTemporal = false)
        {
            m_pMockCommonCORE->InitializeDevice();
            return m_pMockCommonCORE->m_pD11VideoDevice;
        }
        ID3D11DeviceContext* GetD3D11DeviceContext(bool isTemporal = false)
        {
            m_pMockCommonCORE->InitializeDevice();
            return m_pMockCommonCORE->m_pD11Context;
        }
    protected:
        MockCommonCORE *m_pMockCommonCORE;
    };
public:
    MockCommonCORE(const mfxU32 numThreadsAvailable, const mfxSession session) : CommonCORE(numThreadsAvailable, session) {}

    MOCK_METHOD4(CreateVA, mfxStatus(mfxVideoParam*, mfxFrameAllocRequest*, mfxFrameAllocResponse*, UMC::FrameAllocator*));
    MOCK_METHOD3(AllocFrames, mfxStatus(mfxFrameAllocRequest*, mfxFrameAllocResponse*, bool));
    MOCK_CONST_METHOD0(GetVAType, eMFXVAType());
    MOCK_METHOD0(GetHWType, eMFXHWType());
    MOCK_METHOD3(IsGuidSupported, mfxStatus(const GUID, mfxVideoParam*, bool));
    MOCK_METHOD2(SetHandle, mfxStatus(mfxHandleType, mfxHDL));
    MOCK_METHOD0(GetAutoAsyncDepth, mfxU16());
    MOCK_METHOD0(IsCompatibleForOpaq, bool());

    eMFXPlatform GetPlatformType() override { return  MFX_PLATFORM_HARDWARE; }

    void* QueryCoreInterface(const MFX_GUID &guid) override
    {
        if (MFXID3D11DECODER_GUID == guid)
        {
            return (void*)&m_comptr;
        }
        if (MFXICORED3D11_GUID == guid)
        {
            if (!m_mockAdapter.get())
            {
                m_mockAdapter.reset(new MockAdapter(this));
            }
            return (void*)m_mockAdapter.get();
        }
        return CommonCORE::QueryCoreInterface(guid);
    }

    mfxStatus CreateVideoProcessing(mfxVideoParam * param) override
    {
        mfxStatus sts = MFX_ERR_NONE;
#if defined (MFX_ENABLE_VPP) && !defined(MFX_RT)
        if (!vpp_hw_resmng.GetDevice()) {
            MfxHwVideoProcessing::D3D11VideoProcessor *ddi = new MfxHwVideoProcessing::D3D11VideoProcessor();
            EXPECT_NE(nullptr, ddi);

            mfxVideoParam par = {};
            par.vpp.In.Width = 352;
            par.vpp.In.Height = 288;
            par.vpp.In.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
            par.vpp.In.FourCC = MFX_FOURCC_NV12;
            par.vpp.Out = par.vpp.In;

            sts = ddi->CreateDevice(this, &par, true);
            MFX_CHECK_STS(sts);

            sts = vpp_hw_resmng.SetDevice(ddi);
        }
#else
        sts = MFX_ERR_UNSUPPORTED;
#endif
        return sts;
    }

#if defined (MFX_ENABLE_VPP) && !defined(MFX_RT)
    void  GetVideoProcessing(mfxHDL* phdl) override
    {
        *phdl = &vpp_hw_resmng;
    }
#endif

    void GetVA(mfxHDL* phdl, mfxU16 type) override
    {
#if defined (MFX_ENABLE_VPP) && !defined(MFX_RT)
        if ((type & MFX_MEMTYPE_FROM_VPPIN))
        {
            (*phdl = &vpp_hw_resmng);
        }
        else
#endif
            (*phdl = 0);
    }

    void InitializeDevice()
    {
        if (m_pD11Device)
            return;

        HRESULT hres = S_OK;
        static D3D_FEATURE_LEVEL FeatureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
        D3D_FEATURE_LEVEL pFeatureLevelsOut;
        D3D_DRIVER_TYPE type = D3D_DRIVER_TYPE_HARDWARE;

        hres = D3D11CreateDevice(NULL,
            type,
            NULL,
            0,
            FeatureLevels,
            sizeof(FeatureLevels) / sizeof(D3D_FEATURE_LEVEL),
            D3D11_SDK_VERSION,
            &m_pD11Device,
            &pFeatureLevelsOut,
            &m_pD11Context);
        if (FAILED(hres))
        {
            static D3D_FEATURE_LEVEL FeatureLevels9[] = {
                D3D_FEATURE_LEVEL_9_1,
                D3D_FEATURE_LEVEL_9_2,
                D3D_FEATURE_LEVEL_9_3 };

            hres = D3D11CreateDevice(NULL,
                D3D_DRIVER_TYPE_HARDWARE,
                NULL,
                0,
                FeatureLevels9,
                sizeof(FeatureLevels9) / sizeof(D3D_FEATURE_LEVEL),
                D3D11_SDK_VERSION,
                &m_pD11Device,
                &pFeatureLevelsOut,
                &m_pD11Context);

            if (FAILED(hres))
                return;
        }

        m_pD11VideoDevice = m_pD11Device;
        m_pD11VideoContext = m_pD11Context;
    }
private:
    VPPHWResMng                      vpp_hw_resmng;
    std::auto_ptr<MockAdapter>       m_mockAdapter;
    ComPtrCore<ID3D11VideoDecoder>   m_comptr;

    CComPtr<ID3D11Device>            m_pD11Device;
    CComQIPtr<ID3D11VideoDevice>     m_pD11VideoDevice;
    CComPtr<ID3D11DeviceContext>     m_pD11Context;
    CComPtr<ID3D11VideoContext>      m_pD11VideoContext;
};

class EventTest : public ::testing::Test {
protected:
    EventTest() {
        scheduler_ = new MockSchedulerCore;
    }

    ~EventTest() {
        scheduler_->Release();
    }

    MockSchedulerCore* scheduler_;
};

TEST_F(EventTest, CheckVPPEventHandle)
{
    mfxStatus sts;
    mfxSyncPoint syncp;

    MFX_TASK task;
    memset(&task, 0, sizeof(task));

    MockCommonCORE videoCore(0, nullptr);
    sts = videoCore.CreateVideoProcessing(nullptr);
    EXPECT_EQ(MFX_ERR_NONE, sts);

    MFX_SCHEDULER_PARAM sched_params = { MFX_SCHEDULER_DEFAULT, 0, &videoCore, };

    sts = scheduler_->Initialize(&sched_params);
    EXPECT_EQ(MFX_ERR_NONE, sts);

    sts = scheduler_->AddTask(task, &syncp);
    EXPECT_EQ(MFX_ERR_NULL_PTR, sts);

    HANDLE handle = scheduler_->GetHwEvent();
    EXPECT_NE(handle, nullptr);

    // TODO: Need to add check on creation UMD event
    // EXPECT_NE(handle, noUMDEventHandle);

    sts = scheduler_->Reset();
    EXPECT_EQ(MFX_ERR_NONE, sts);
}
#endif