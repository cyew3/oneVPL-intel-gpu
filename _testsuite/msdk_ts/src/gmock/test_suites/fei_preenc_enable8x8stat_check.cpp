/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014-2018 Intel Corporation. All Rights Reserved.
//
*/

/******************************************************************************
    Enable8x8Stat for PreENC.
    Check the result are different with Disable/Enable8x8Stat for PreENC.

******************************************************************************/

#include "ts_struct.h"
#include "ts_preenc.h"

#include "vaapi_device.h"


namespace fei_preenc_enable8x8stat_check
{
#define FRAME_TO_ENCODE 6

class TestSuite : tsVideoPreENC
{
public:
    TestSuite() : tsVideoPreENC(MFX_FEI_FUNCTION_PREENC, MFX_CODEC_AVC, true) {}
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    struct tc_par;
    static const mfxU32 n_par = 6;
    enum
    {
        MFXPAR = 1,
        MFXFEIPAR,
    };

    enum
    {
        OUT_PREENC_MV       = 1 << 0,
        OUT_PREENC_MB_STAT  = 1 << 1,
    };

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 mode;
        std::string input_file;
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];
    };
    void randomizeOutBuffer(mfxU8 *out_buf, mfxU16 len);

    static const tc_struct test_case[];
};

/* Caller should make sure the pointer out_buf is not NULL */
void TestSuite::randomizeOutBuffer(mfxU8 *out_buf, mfxU16 len)
{
    for (int i=0; i<len; i++) {
        out_buf[i] = rand() % 256;
    }
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*01*/ MFX_ERR_NONE, OUT_PREENC_MV | OUT_PREENC_MB_STAT, "YUV/foreman_352x288_300.yuv", {}},
    {/*04*/ MFX_ERR_NONE, OUT_PREENC_MB_STAT, "YUV/foreman_352x288_300.yuv", {}},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);
int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];
    mfxStatus sts;

    mfxU16 NumOfAlloc = 44;
    mfxU16 NumOfFrameUsed = FRAME_TO_ENCODE;

    m_impl = MFX_IMPL_HARDWARE;
    m_par.IOPattern =  MFX_IOPATTERN_IN_VIDEO_MEMORY;

    // Set default parameters
    m_pPar->mfx.FrameInfo.CropW = m_pPar->mfx.FrameInfo.Width = 352;
    m_pPar->mfx.FrameInfo.CropH = m_pPar->mfx.FrameInfo.Height = 288;
    m_pPar->mfx.FrameInfo.CropX = 0;
    m_pPar->mfx.FrameInfo.CropY = 0;
    m_pPar->mfx.NumRefFrame = 0;
    m_pPar->mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
    m_pPar->AsyncDepth = 1;
    m_pPar->AllocId = 1;
    m_pPar->mfx.GopRefDist = 20;
    m_pPar->mfx.GopPicSize = 50;
    m_pPar->mfx.IdrInterval = 65535;
    m_pPar->mfx.NumSlice = 1;
    m_pPar->mfx.NumRefFrame = 1;

    SETPARS(m_pPar, MFXPAR);

    g_tsStatus.expect(MFX_ERR_NONE);
    TRACE_FUNC3( MFXInit, m_impl, m_pVersion, m_pSession );
    g_tsStatus.check(::MFXInit(m_impl, m_pVersion, m_pSession));
    TS_TRACE(m_pVersion);
    TS_TRACE(m_pSession);

    m_initialized = (g_tsStatus.get() >= 0);

    // Create HWDevice
    CHWDevice* m_hwdev = NULL;
    m_hwdev = CreateVAAPIDevice();
    if (NULL == m_hwdev)
    {
        return MFX_ERR_MEMORY_ALLOC;
    }
    g_tsStatus.check(m_hwdev->Init(NULL, 0, MSDKAdapter::GetNumber(*m_pSession)));

    // Set handle
    mfxHDL hdl = NULL;
    g_tsStatus.check(m_hwdev->GetHandle(MFX_HANDLE_VA_DISPLAY, &hdl));

    // Provide device manager to MediaSDK
    g_tsStatus.check(MFXVideoCORE_SetHandle(m_session, MFX_HANDLE_VA_DISPLAY, hdl));

    // Create VAAPI allocator
    MFXFrameAllocator *m_pFrameAllocator = new vaapiFrameAllocator;
    MSDK_CHECK_POINTER(m_pFrameAllocator, MFX_ERR_MEMORY_ALLOC);
    vaapiAllocatorParams *p_vaapiAllocParams = new vaapiAllocatorParams;
    MSDK_CHECK_POINTER(p_vaapiAllocParams, MFX_ERR_MEMORY_ALLOC);
    p_vaapiAllocParams->m_dpy = (VADisplay)hdl;
    mfxAllocatorParams* m_pmfxAllocatorParams = p_vaapiAllocParams;
    g_tsStatus.check(MFXVideoCORE_SetFrameAllocator(m_session, m_pFrameAllocator));

    // Initialize memory allocator
    g_tsStatus.check(m_pFrameAllocator->Init(m_pmfxAllocatorParams));

    mfxU32 BaseAllocID = (mfxU64)&m_session & 0xffffffff;
    m_par.AllocId = BaseAllocID;

    m_request.NumFrameMin = NumOfAlloc;
    m_request.NumFrameSuggested = NumOfAlloc;
    m_request.Type = MFX_MEMTYPE_FROM_ENC | MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET | MFX_MEMTYPE_EXTERNAL_FRAME;
    m_request.Info = m_par.mfx.FrameInfo;
    m_request.AllocId = m_par.AllocId;

    // Alloc frames for encoder
    mfxFrameAllocResponse response = {};
    sts = m_pFrameAllocator->Alloc(m_pFrameAllocator->pthis, &m_request, &response);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    mfxFrameSurface1 *surfacesPool = new mfxFrameSurface1[response.NumFrameActual];
    MSDK_CHECK_POINTER(surfacesPool, MFX_ERR_MEMORY_ALLOC);
    memset(surfacesPool, 0, sizeof(*surfacesPool)*response.NumFrameActual);

    for (int i = 0; i < response.NumFrameActual; i++)
    {
        MSDK_MEMCPY_VAR(surfacesPool[i].Info,&(m_pPar->mfx.FrameInfo), sizeof(m_pPar->mfx.FrameInfo));
        surfacesPool[i].Data.MemId = response.mids[i];
    }

    g_tsStatus.check(Init(m_session, m_pPar));

    // Fill in surfaces by real YUV data.
    tsRawReader stream(g_tsStreamPool.Get(tc.input_file), m_pPar->mfx.FrameInfo);
    g_tsStreamPool.Reg();
    m_filler = &stream;

    if(m_filler)
    {
        for (int SurfIdx = 0; SurfIdx < NumOfFrameUsed; SurfIdx++) {
            surfacesPool[SurfIdx].Data.FrameOrder = SurfIdx;
            m_filler->ProcessSurface(&surfacesPool[SurfIdx], m_pFrameAllocator);
        }
    }

    //Calc the number of MB in one frame
    mfxU16 widthMBpreenc = MSDK_ALIGN16(m_par.mfx.FrameInfo.Width);
    mfxU16 heightMBpreenc = m_par.mfx.FrameInfo.PicStruct == \
        MFX_PICSTRUCT_PROGRESSIVE ? MSDK_ALIGN16(m_par.mfx.FrameInfo.Height) : \
        MSDK_ALIGN32(m_par.mfx.FrameInfo.Height); // in case of
    mfxU32 numMBPreENC = (widthMBpreenc * heightMBpreenc) >> 8;

    // Attach mfxExtFeiPreEncMV and mfxExtFeiPreEncMBStat to m_pPreENCOutput.
    mfxENCInput* p_PreENCInput[FRAME_TO_ENCODE];
    mfxENCOutput* p_PreENCOutput[FRAME_TO_ENCODE];
    std::vector<mfxExtBuffer*> in_buffs[FRAME_TO_ENCODE];
    std::vector<mfxExtBuffer*> out_buffs[FRAME_TO_ENCODE];
    mfxExtFeiPreEncCtrl FEIPreEncCtrl[FRAME_TO_ENCODE];
    mfxExtFeiPreEncMV out_mvs[FRAME_TO_ENCODE];
    mfxExtFeiPreEncMBStat out_mbStat[FRAME_TO_ENCODE];
    mfxExtFeiPreEncMBStat tmp_mbStat[FRAME_TO_ENCODE];

    // Init in and out parameter for preenc call.
    for (int i =0; i < FRAME_TO_ENCODE; i++) {
        p_PreENCInput[i] = new mfxENCInput();
        p_PreENCOutput[i] = new mfxENCOutput();

        FEIPreEncCtrl[i].Header.BufferId = MFX_EXTBUFF_FEI_PREENC_CTRL;
        FEIPreEncCtrl[i].Header.BufferSz = sizeof(mfxExtFeiPreEncCtrl);
        FEIPreEncCtrl[i].PictureType             = MFX_PICTYPE_FRAME;
        FEIPreEncCtrl[i].RefPictureType[0]       = FEIPreEncCtrl[i].PictureType;
        FEIPreEncCtrl[i].RefPictureType[1]       = FEIPreEncCtrl[i].PictureType;
        FEIPreEncCtrl[i].DownsampleReference[0]  = MFX_CODINGOPTION_ON;
        FEIPreEncCtrl[i].DownsampleReference[1]  = MFX_CODINGOPTION_ON;
        FEIPreEncCtrl[i].DisableMVOutput         = tc.mode & OUT_PREENC_MV ? false : true;
        FEIPreEncCtrl[i].DisableStatisticsOutput = tc.mode & OUT_PREENC_MB_STAT ? false : true;
        FEIPreEncCtrl[i].FTEnable                = false;
        FEIPreEncCtrl[i].AdaptiveSearch          = 0;
        FEIPreEncCtrl[i].LenSP                   = 48;
        FEIPreEncCtrl[i].MBQp                    = 0;
        FEIPreEncCtrl[i].MVPredictor             = 0;
        FEIPreEncCtrl[i].RefHeight               = 32;
        FEIPreEncCtrl[i].RefWidth                = 32;
        FEIPreEncCtrl[i].SubPelMode              = 0x03;
        FEIPreEncCtrl[i].SearchWindow            = 5;
        FEIPreEncCtrl[i].SearchPath              = 0;
        FEIPreEncCtrl[i].Qp                      = 26;
        FEIPreEncCtrl[i].IntraSAD                = 0x00;
        FEIPreEncCtrl[i].InterSAD                = 0;
        FEIPreEncCtrl[i].SubMBPartMask           = 0;
        FEIPreEncCtrl[i].IntraPartMask           = 0;
        FEIPreEncCtrl[i].Enable8x8Stat           = 0;
        FEIPreEncCtrl[i].DownsampleInput = MFX_CODINGOPTION_ON;

        in_buffs[i].push_back((mfxExtBuffer*)&FEIPreEncCtrl[i]);

        if (!in_buffs[i].empty()) {
            p_PreENCInput[i]->NumExtParam = (mfxU16)in_buffs[i].size();
            p_PreENCInput[i]->ExtParam = &in_buffs[i][0];
        }

        if (!FEIPreEncCtrl[i].DisableMVOutput)
        {
            out_mvs[i].Header.BufferId = MFX_EXTBUFF_FEI_PREENC_MV;
            out_mvs[i].Header.BufferSz = sizeof(mfxExtFeiPreEncMV);
            out_mvs[i].NumMBAlloc = numMBPreENC;
            out_mvs[i].MB = new mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB[numMBPreENC];
            MSDK_CHECK_POINTER(out_mvs[i].MB, MFX_ERR_MEMORY_ALLOC);
            memset((mfxU8*)out_mvs[i].MB, 0, sizeof(*(out_mvs[i].MB))*numMBPreENC);
            out_buffs[i].push_back((mfxExtBuffer*)&out_mvs[i]);
        }

        if (!FEIPreEncCtrl[i].DisableStatisticsOutput)
        {
            out_mbStat[i].Header.BufferId = MFX_EXTBUFF_FEI_PREENC_MB;
            out_mbStat[i].Header.BufferSz = sizeof(mfxExtFeiPreEncMBStat);
            out_mbStat[i].NumMBAlloc = numMBPreENC;
            out_mbStat[i].MB = new mfxExtFeiPreEncMBStat::mfxExtFeiPreEncMBStatMB[numMBPreENC];
            tmp_mbStat[i].MB = new mfxExtFeiPreEncMBStat::mfxExtFeiPreEncMBStatMB[numMBPreENC];
            MSDK_CHECK_POINTER(out_mbStat[i].MB, MFX_ERR_MEMORY_ALLOC);
            memset((mfxU8*)out_mbStat[i].MB, 0, sizeof(*(out_mbStat[i].MB))*numMBPreENC);
            out_buffs[i].push_back((mfxExtBuffer*)&out_mbStat[i]);
        }

        if (!out_buffs[i].empty()) {
            p_PreENCOutput[i]->NumExtParam = (mfxU16)out_buffs[i].size();
            p_PreENCOutput[i]->ExtParam = &out_buffs[i][0];
        }
        FEIPreEncCtrl[i].RefFrame[0] = NULL;
        FEIPreEncCtrl[i].RefFrame[1] = NULL;
    }

    /*** Starting preenc ***/
    for (int i =0; i < FRAME_TO_ENCODE; i++) {
        p_PreENCInput[i]->InSurface = &surfacesPool[i];
        msdk_atomic_inc16(&p_PreENCInput[i]->InSurface->Data.Locked);
        FEIPreEncCtrl[i].RefFrame[0] = (i == 0) ? NULL : p_PreENCInput[i - 1]->InSurface;
        FEIPreEncCtrl[i].RefFrame[1] = NULL;
        p_PreENCInput[i]->NumFrameL0 = (i == 0) ? 0 : 1;
        p_PreENCInput[i]->NumFrameL1 = 0;
        sts = ProcessFrameAsync(m_session, p_PreENCInput[i], p_PreENCOutput[i], m_pSyncPoint);
        sts = SyncOperation();
        g_tsStatus.expect(tc.sts);

        memcpy((mfxU8*)tmp_mbStat[i].MB, (mfxU8*)out_mbStat[i].MB, sizeof(*(out_mbStat[i].MB))*numMBPreENC);
        memset((mfxU8*)out_mbStat[i].MB, 0, sizeof(*(out_mbStat[i].MB))*numMBPreENC);
        FEIPreEncCtrl[i].Enable8x8Stat = 1;
        sts = ProcessFrameAsync(m_session, p_PreENCInput[i], p_PreENCOutput[i], m_pSyncPoint);
        sts = SyncOperation();
        g_tsStatus.expect(tc.sts);

        /*** Check the statistic are different ***/
        if(sts == MFX_ERR_NONE) {
            if(tc.mode & OUT_PREENC_MB_STAT) {
                EXPECT_NE(0, memcmp((mfxU8*)tmp_mbStat[i].MB, (mfxU8*)out_mbStat[i].MB, out_mbStat[i].NumMBAlloc *sizeof(mfxExtFeiPreEncMBStat)))
                    << "ERROR: mfxExtFeiEncMBStat of disable/enable8x8stat output should not be same.";
            }
        }
    }

    /*** Finish ***/
    Close();

    // Free memory allocated dynamically
    if (NULL != surfacesPool) {
        delete surfacesPool;
        surfacesPool = NULL;
    }

    for (int i = 0; i < FRAME_TO_ENCODE; i++) {
        if (NULL != out_mvs[i].MB) {
            delete out_mvs[i].MB;
            out_mvs[i].MB = NULL;
        }
        if (NULL != out_mbStat[i].MB) {
            delete out_mbStat[i].MB;
            out_mbStat[i].MB = NULL;
        }
        if (NULL != tmp_mbStat[i].MB) {
            delete tmp_mbStat[i].MB;
            tmp_mbStat[i].MB = NULL;
        }
        if (NULL != p_PreENCInput[i]) {
            delete p_PreENCInput[i];
            p_PreENCInput[i] = NULL;
        }
        if (NULL != p_PreENCOutput[i]) {
            delete p_PreENCOutput[i];
            p_PreENCOutput[i] = NULL;
        }
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(fei_preenc_enable8x8stat_check);
};

