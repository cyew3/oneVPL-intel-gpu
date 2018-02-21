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
This behavior test is used to check FEI PREENC downsample control.There are 2
test mode: downsample control for input frame (DS_CTRL_INPUT_FRAME) and
downsample control for refernece control (DS_CTRL_REF_FRAME).


The steps about preenc downsample controlling behavir test for input frame is
as below:
1. Allocate three surfaces, S0, S1, S2, fill in by real YUV data, randomize
   statistics buffer before each PreENC call.
2. Call PreENC for S0, DownsampleInput = true
3. Call PreENC for S0 and S1, DownsampleInput = true, save stat, call it ST1.
4. Call PreENC for S0 and S2, DownsampleInput = true, save stat, call it ST2.
5. Call PreENC for S0 and S1, DownsampleInput = false, compare stat to ST1,
   should be the same except variance and average.
6. Copy S2 to S1
7. Call PreENC for S0 and S1, DownsampleInput = false, compare stat to ST1,
   should be different, don't compare variance and average.
8. Call PreENC for S0 and S1, DownsampleInput = true, compare stat to ST2,
   should be the same including variance and average.


The steps about preenc downsample controlling behavir test for reference frame is
as below:
1. Allocate Six surface, S0-S5, where S0 = S3, S1 = S4, S2 = S5
2. Call preenc with S0, downsampleInput = true, no refs.
3. Call preenc with S1, downsampleInput = true, no refs
4. Call preenc with S2 downsampleInput = true, S0 and S1 are references,
   downsampleReferences = {true, true}, store output data (statistics and MVs)
   as out1
5. Call preenc with S3, downsampleInput = true, S4 and S5 are references,
   downsampleReferences = {true, true}, store output as out2. out1 should be the
   same as out2

******************************************************************************/

#include <string>

#include "ts_struct.h"
#include "ts_preenc.h"

#include "vaapi_device.h"
#include "sample_utils.h"


namespace fei_preenc_downsample
{
#define MAX_NUM_PREENC_CALL 6

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
        // Input Mode
        IN_PREENC_CTRL      = 1 << 0,
        IN_PREENC_MV_PRED   = 1 << 1,
        IN_RREENC_QP        = 1 << 2,
        IN_ALL              = IN_PREENC_CTRL | IN_PREENC_MV_PRED | IN_RREENC_QP,
        // Output Mode
        OUT_PREENC_MV       = 1 << 4,
        OUT_PREENC_MB_STAT  = 1 << 5,
        OUT_ALL             = OUT_PREENC_MV | OUT_PREENC_MB_STAT,
        // Downsampling control for input frame or reference frame
        DS_CTRL_INPUT_FRAME   = 1 << 8,
        DS_CTRL_REF_FRAME_1_ORG     = 1 << 9,
        DS_CTRL_REF_FRAME_1_REORDER = 1 << 10,
        DS_CTRL_REF_FRAME_1         = DS_CTRL_REF_FRAME_1_ORG | DS_CTRL_REF_FRAME_1_REORDER,
        DS_CTRL_REF_FRAME_2_ON      = 1 << 11,
        DS_CTRL_REF_FRAME_2_OFF     = 1 << 12,
    };

   enum
    {
        SAME_MBSTAT_EXCEPT_VAR_AVR  = 1 << 0,
        SAME_VAR_AVR                = 1 << 1,
        SAME_ALL                    = SAME_MBSTAT_EXCEPT_VAR_AVR | SAME_VAR_AVR,
        SAME_OUT_MV                 = 1 << 4,
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
    void SurfaceCopy(mfxFrameSurface1* dst, mfxFrameSurface1* src, mfxFrameAllocator* pfa);
    mfxU16 CmpMBStat(mfxExtFeiPreEncMBStat *out_mbStat_st1, mfxExtFeiPreEncMBStat *out_mbStat_st2, mfxU16 Enable8x8Stat);
    mfxU16 CmpMV(mfxExtFeiPreEncMV *out_mv_st1, mfxExtFeiPreEncMV *out_mv_st2);

    static const tc_struct test_case[];
};

/* Caller should make sure the pointer out_buf is not NULL */
void TestSuite::randomizeOutBuffer(mfxU8 *out_buf, mfxU16 len)
{
    for (int i = 0; i < len; i++) 
    {
        out_buf[i] = rand() % 256;
    }
};

void TestSuite::SurfaceCopy(mfxFrameSurface1* pSurfDst, mfxFrameSurface1* pSurfSrc, mfxFrameAllocator* pfa)
{
    bool useAllocator = pfa && (!pSurfSrc->Data.Y || !pSurfDst->Data.Y);

    g_tsStatus.expect(MFX_ERR_NONE);
    if(useAllocator)
    {
        g_tsStatus.check(pfa->Lock(pfa->pthis, pSurfSrc->Data.MemId, &(pSurfSrc->Data)));
        g_tsStatus.check(pfa->Lock(pfa->pthis, pSurfDst->Data.MemId, &(pSurfDst->Data)));
    }

    tsFrame src(*pSurfSrc);
    tsFrame dst(*pSurfDst);

    dst = src;

    if(useAllocator)
    {
        g_tsStatus.check(pfa->Unlock(pfa->pthis, pSurfSrc->Data.MemId, &(pSurfSrc->Data)));
        g_tsStatus.check(pfa->Unlock(pfa->pthis, pSurfDst->Data.MemId, &(pSurfDst->Data)));
    }

};

mfxU16 TestSuite::CmpMBStat(mfxExtFeiPreEncMBStat *out_mbStat_st1, mfxExtFeiPreEncMBStat *out_mbStat_st2, mfxU16 Enable8x8Stat)
{
    mfxU16 CmpResStat = 0;
    bool SameMBStatWithoutVA = true;
    bool SameVA = true;
    mfxExtFeiPreEncMBStat::mfxExtFeiPreEncMBStatMB* p_MB1 = out_mbStat_st1->MB;
    mfxExtFeiPreEncMBStat::mfxExtFeiPreEncMBStatMB* p_MB2 = out_mbStat_st2->MB;

    for (size_t i = 0; i < out_mbStat_st1->NumMBAlloc; i++) 
    {
        if(memcmp(p_MB1[i].Inter, p_MB2[i].Inter,sizeof(p_MB1[i].Inter[0])*2) || \
            p_MB1[i].BestIntraDistortion != p_MB2[i].BestIntraDistortion || \
            p_MB1[i].IntraMode!= p_MB2[i].IntraMode || \
            p_MB1[i].NumOfNonZeroCoef != p_MB2[i].NumOfNonZeroCoef || \
            p_MB1[i].SumOfCoef!= p_MB2[i].SumOfCoef) 
        {
            SameMBStatWithoutVA = false;
            break;
        }
    }

    for (size_t i = 0; i < out_mbStat_st1->NumMBAlloc; i++) 
    {
        if (Enable8x8Stat && (memcpy(p_MB1[i].Variance8x8, p_MB2[i].Variance8x8, sizeof(p_MB1[i].Variance8x8[0])*4) || \
            memcpy(p_MB1[i].PixelAverage8x8, p_MB2[i].PixelAverage8x8, sizeof(p_MB1[i].PixelAverage8x8[0])*4))) 
        {
            SameVA = false;
            break;
        }
        if (!Enable8x8Stat && (p_MB1[i].Variance16x16 != p_MB2[i].Variance16x16 || p_MB1[i].PixelAverage16x16!= p_MB2[i].PixelAverage16x16)) 
        {
            SameVA = false;
            break;
        }
    }

    if (SameMBStatWithoutVA)
        CmpResStat |= SAME_MBSTAT_EXCEPT_VAR_AVR;
    if (SameVA)
        CmpResStat |= SAME_VAR_AVR;

    return CmpResStat;
};

mfxU16 TestSuite::CmpMV(mfxExtFeiPreEncMV *out_mv_st1, mfxExtFeiPreEncMV *out_mv_st2)
{
    mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB* p_MB1 = out_mv_st1->MB;
    mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB* p_MB2 = out_mv_st2->MB;

    if ( memcmp(p_MB1, p_MB2, sizeof(p_MB1[0]) * out_mv_st1->NumMBAlloc )) 
    {
        return 0;
    } 
    else 
    {
        return SAME_OUT_MV;
    }
}


const TestSuite::tc_struct TestSuite::test_case[] =
{
    /*** Downsample control for input frame ***/
    {/*00*/ MFX_ERR_NONE, OUT_ALL|DS_CTRL_INPUT_FRAME, "YUV/foreman_352x288_300.yuv", {}},
    {/*01*/ MFX_ERR_NONE, OUT_ALL|DS_CTRL_INPUT_FRAME, "YUV/matrix_1920x1088_250.yuv", {{MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 1920},
                                    {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 1088},
                                    {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 1920},
                                    {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 1088}}},
    {/*02*/ MFX_ERR_NONE, OUT_ALL|DS_CTRL_INPUT_FRAME, "YUV/iceage_720x240_491.yuv", {{MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 720},
                                    {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 240},
                                    {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 720},
                                    {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 240}}},
    /*** Downsample control for reference frames ***/
    {/*03*/ MFX_ERR_NONE, OUT_ALL|DS_CTRL_REF_FRAME_1_ORG, "YUV/foreman_352x288_300.yuv", {}},
    {/*04*/ MFX_ERR_NONE, OUT_ALL|DS_CTRL_REF_FRAME_1_REORDER, "YUV/foreman_352x288_300.yuv", {}},
    {/*05*/ MFX_ERR_NONE, OUT_ALL|DS_CTRL_REF_FRAME_1_REORDER, "YUV/matrix_1920x1088_250.yuv", {{MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 1920},
                                    {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 1088},
                                    {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 1920},
                                    {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 1088}}},
    {/*06*/ MFX_ERR_NONE, OUT_ALL|DS_CTRL_REF_FRAME_1_REORDER, "YUV/iceage_720x240_491.yuv", {{MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 720},
                                    {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 240},
                                    {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 720},
                                    {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 240}}},
    /***  Check whether there is a hang when calling preenc with 2 unprocessed by preenc references and with downsampleReference = false ***/
    {/*07*/ MFX_ERR_NONE, OUT_ALL|DS_CTRL_REF_FRAME_2_ON, "YUV/foreman_352x288_300.yuv", {}},
    {/*08*/ MFX_ERR_NONE, OUT_ALL|DS_CTRL_REF_FRAME_2_OFF, "YUV/foreman_352x288_300.yuv", {}},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);
int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];
    mfxStatus sts;

    mfxU16 NumOfAlloc = 24;
    mfxU16 NumOfFrameUsed = tc.mode & DS_CTRL_REF_FRAME_1 ? 6 : 3;
    mfxI16 IndexPreencCall = 0;
    mfxU16 CmpResStat = 0;
    mfxU16 CmpResMV = 0;

    m_impl = MFX_IMPL_HARDWARE;
    m_par.IOPattern =  MFX_IOPATTERN_IN_VIDEO_MEMORY;

    // Set default parameters
    m_pPar->mfx.FrameInfo.CropW = m_pPar->mfx.FrameInfo.Width = 352;
    m_pPar->mfx.FrameInfo.CropH = m_pPar->mfx.FrameInfo.Height = 288;
    m_pPar->mfx.FrameInfo.CropX = 0;
    m_pPar->mfx.FrameInfo.CropY = 0;
    m_pPar->mfx.FrameInfo.BitDepthLuma = 8;
    m_pPar->mfx.FrameInfo.BitDepthChroma = 8;
    m_pPar->mfx.NumRefFrame = 0;
    m_pPar->mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
    m_pPar->AsyncDepth = 1;
    m_pPar->AllocId = 1;
    m_pPar->mfx.GopRefDist = 5;
    m_pPar->mfx.GopPicSize = 10;
    m_pPar->mfx.IdrInterval = 65535;
    m_pPar->mfx.NumSlice = 1;
    m_pPar->mfx.NumRefFrame = 2;

    // Set up parameters for case
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
    m_request.NumFrameSuggested= NumOfAlloc;
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
        // Only video memory in this test.
        surfacesPool[i].Data.MemId = response.mids[i];
    }

    g_tsStatus.check(Init(m_session, m_pPar));

    // Fill in surfaces by real YUV data.
    tsRawReader stream(g_tsStreamPool.Get(tc.input_file), m_pPar->mfx.FrameInfo);
    g_tsStreamPool.Reg();
    m_filler = &stream;

    //Calc the number of MB in one frame
    mfxU16 widthMBpreenc = MSDK_ALIGN16(m_par.mfx.FrameInfo.Width);
    mfxU16 heightMBpreenc = m_par.mfx.FrameInfo.PicStruct == \
        MFX_PICSTRUCT_PROGRESSIVE ? MSDK_ALIGN16(m_par.mfx.FrameInfo.Height) : \
        MSDK_ALIGN16(m_par.mfx.FrameInfo.Height);
    mfxU32 numMBPreENC = (widthMBpreenc * heightMBpreenc) >> 8;

    // Attach mfxExtFeiPreEncMV and mfxExtFeiPreEncMBStat to m_pPreENCOutput.
    mfxENCInput* p_PreENCInput[MAX_NUM_PREENC_CALL];
    mfxENCOutput* p_PreENCOutput[MAX_NUM_PREENC_CALL];
    std::vector<mfxExtBuffer*> in_buffs[MAX_NUM_PREENC_CALL];
    std::vector<mfxExtBuffer*> out_buffs[MAX_NUM_PREENC_CALL];
    mfxExtFeiPreEncCtrl FEIPreEncCtrl[MAX_NUM_PREENC_CALL];
    mfxExtFeiPreEncMV out_mvs[MAX_NUM_PREENC_CALL];
    mfxExtFeiPreEncMBStat out_mbStat[MAX_NUM_PREENC_CALL];

    // Init in and out parameter for preenc call.
    for (int i =0; i < MAX_NUM_PREENC_CALL; i++) 
    {
        p_PreENCInput[i] = new mfxENCInput();
        p_PreENCOutput[i] = new mfxENCOutput();

        FEIPreEncCtrl[i].Header.BufferId = MFX_EXTBUFF_FEI_PREENC_CTRL;
        FEIPreEncCtrl[i].Header.BufferSz = sizeof(mfxExtFeiPreEncCtrl);
        FEIPreEncCtrl[i].PictureType             = MFX_PICTYPE_FRAME;
        FEIPreEncCtrl[i].RefPictureType[0]       = FEIPreEncCtrl[i].PictureType;
        FEIPreEncCtrl[i].RefPictureType[1]       = FEIPreEncCtrl[i].PictureType;
        FEIPreEncCtrl[i].DownsampleReference[0]  = MFX_CODINGOPTION_OFF;
        FEIPreEncCtrl[i].DownsampleReference[1]  = MFX_CODINGOPTION_OFF;
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

        // Attach mfxExtFEIPreEncCtrl[i] to p_PreENCInput[i]
        if (!in_buffs[i].empty()) 
        {
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
            randomizeOutBuffer((mfxU8*)out_mvs[i].MB, sizeof(*(out_mvs[i].MB))*numMBPreENC);
            out_buffs[i].push_back((mfxExtBuffer*)&out_mvs[i]);
        }

        if (!FEIPreEncCtrl[i].DisableStatisticsOutput)
        {
            out_mbStat[i].Header.BufferId = MFX_EXTBUFF_FEI_PREENC_MB;
            out_mbStat[i].Header.BufferSz = sizeof(mfxExtFeiPreEncMBStat);
            out_mbStat[i].NumMBAlloc = numMBPreENC;
            out_mbStat[i].MB = new mfxExtFeiPreEncMBStat::mfxExtFeiPreEncMBStatMB[numMBPreENC];
            MSDK_CHECK_POINTER(out_mbStat[i].MB, MFX_ERR_MEMORY_ALLOC);
            randomizeOutBuffer((mfxU8*)out_mbStat[i].MB, sizeof(*(out_mbStat[i].MB))*numMBPreENC);
            out_buffs[i].push_back((mfxExtBuffer*)&out_mbStat[i]);
        }

        if (!out_buffs[i].empty()) 
        {
            p_PreENCOutput[i]->NumExtParam = (mfxU16)out_buffs[i].size();
            p_PreENCOutput[i]->ExtParam = &out_buffs[i][0];
        }
        FEIPreEncCtrl[i].RefFrame[0] = NULL;
        FEIPreEncCtrl[i].RefFrame[1] = NULL;
    }

    if ( tc.mode & DS_CTRL_INPUT_FRAME)
    {
        /*** Step1. Fill in surfaces by real YUV data. ***/
        g_tsLog << "Step1. Fill in surfaces by real YUV data.\n";
        if(m_filler)
        {
            for (int SurfIdx = 0; SurfIdx < NumOfFrameUsed; SurfIdx++) 
            {
                surfacesPool[SurfIdx].Data.FrameOrder = SurfIdx;
                m_filler->ProcessSurface(&surfacesPool[SurfIdx], m_pFrameAllocator);
            }
        }

        /*** Step2. Call ProcessFrameAsync for S0, DownsampleInput is true; ***/
        g_tsLog << "step2. Call ProcessFrameAsync for S0, DownsampleInput is true;\n";
        
        IndexPreencCall =0;
        p_PreENCInput[IndexPreencCall]->InSurface = &surfacesPool[0];
        msdk_atomic_inc16(&p_PreENCInput[IndexPreencCall]->InSurface->Data.Locked);
        FEIPreEncCtrl[IndexPreencCall].DownsampleInput = MFX_CODINGOPTION_ON;
        FEIPreEncCtrl[IndexPreencCall].RefFrame[0] = NULL;
        FEIPreEncCtrl[IndexPreencCall].RefFrame[1] = NULL;
        p_PreENCInput[IndexPreencCall]->NumFrameL0 = 0;
        p_PreENCInput[IndexPreencCall]->NumFrameL1 = 0;
        g_tsStatus.expect(MFX_ERR_NONE);
        sts = ProcessFrameAsync(m_session, p_PreENCInput[IndexPreencCall], p_PreENCOutput[IndexPreencCall], m_pSyncPoint);
        g_tsStatus.expect(tc.sts);
        sts = SyncOperation();
        mfxU16 S0Index = IndexPreencCall;

        /*** Step3. Call ProcessFrameAsync for S0 and S1, DownsampleInput is true; ***/
        g_tsLog << "step3. Call ProcessFrameAsync for S0 and S1, DownsampleInput is true;\n";
        IndexPreencCall++;
        p_PreENCInput[IndexPreencCall]->InSurface = &surfacesPool[1];
        msdk_atomic_inc16(&p_PreENCInput[IndexPreencCall]->InSurface->Data.Locked);
        FEIPreEncCtrl[IndexPreencCall].DownsampleInput = MFX_CODINGOPTION_ON;
        FEIPreEncCtrl[IndexPreencCall].RefFrame[0] = p_PreENCInput[S0Index]->InSurface;
        FEIPreEncCtrl[IndexPreencCall].RefFrame[1] = NULL;
        p_PreENCInput[IndexPreencCall]->NumFrameL0 = 1;
        p_PreENCInput[IndexPreencCall]->NumFrameL1 = 0;
        g_tsStatus.expect(MFX_ERR_NONE);
        sts = ProcessFrameAsync(m_session, p_PreENCInput[IndexPreencCall], p_PreENCOutput[IndexPreencCall], m_pSyncPoint);
        g_tsStatus.expect(tc.sts);
        sts = SyncOperation();
        mfxExtFeiPreEncMBStat *out_mbStat_st1 = &out_mbStat[IndexPreencCall];
        mfxExtFeiPreEncMV *out_mv_st1 = &out_mvs[IndexPreencCall];

        /*** Step4. Call ProcessFrameAsync for S0 and S2, DownsampleInput is true; ***/
        g_tsLog << "step4. Call ProcessFrameAsync for S0 and S2, DownsampleInput is true;\n";
        IndexPreencCall++;
        p_PreENCInput[IndexPreencCall]->InSurface = &surfacesPool[2];
        msdk_atomic_inc16(&p_PreENCInput[IndexPreencCall]->InSurface->Data.Locked);
        FEIPreEncCtrl[IndexPreencCall].DownsampleInput = MFX_CODINGOPTION_ON;
        FEIPreEncCtrl[IndexPreencCall].RefFrame[0] = p_PreENCInput[S0Index]->InSurface;
        FEIPreEncCtrl[IndexPreencCall].RefFrame[1] = NULL;
        p_PreENCInput[IndexPreencCall]->NumFrameL0 = 1;
        p_PreENCInput[IndexPreencCall]->NumFrameL1 = 0;
        g_tsStatus.expect(MFX_ERR_NONE);
        sts = ProcessFrameAsync(m_session, p_PreENCInput[IndexPreencCall], p_PreENCOutput[IndexPreencCall], m_pSyncPoint);
        g_tsStatus.expect(tc.sts);
        sts = SyncOperation();
        mfxExtFeiPreEncMBStat *out_mbStat_st2 = &out_mbStat[IndexPreencCall];
        mfxExtFeiPreEncMV *out_mv_st2 = &out_mvs[IndexPreencCall];

        /*** Step5. Call ProcessFrameAsync for S0 and S1, DownsampleInput is false; ***/
        g_tsLog << "step5. Call ProcessFrameAsync for S0 and S1, DownsampleInput is false;\n";
        IndexPreencCall++;
        p_PreENCInput[IndexPreencCall]->InSurface = &surfacesPool[1];
        msdk_atomic_inc16(&p_PreENCInput[IndexPreencCall]->InSurface->Data.Locked);
        FEIPreEncCtrl[IndexPreencCall].DownsampleInput = MFX_CODINGOPTION_OFF;
        FEIPreEncCtrl[IndexPreencCall].RefFrame[0] = p_PreENCInput[S0Index]->InSurface;
        FEIPreEncCtrl[IndexPreencCall].RefFrame[1] = NULL;
        p_PreENCInput[IndexPreencCall]->NumFrameL0 = 1;
        p_PreENCInput[IndexPreencCall]->NumFrameL1 = 0;
        g_tsStatus.expect(MFX_ERR_NONE);
        sts = ProcessFrameAsync(m_session, p_PreENCInput[IndexPreencCall], p_PreENCOutput[IndexPreencCall], m_pSyncPoint);
        g_tsStatus.expect(tc.sts);
        sts = SyncOperation();
        mfxExtFeiPreEncMBStat *out_mbStat_st1_5 = &out_mbStat[IndexPreencCall];
        mfxExtFeiPreEncMV *out_mv_st1_5 = &out_mvs[IndexPreencCall];
        // Compare stat to ST1, should be the same except variance and average
        CmpResStat = CmpMBStat(out_mbStat_st1, out_mbStat_st1_5, FEIPreEncCtrl[IndexPreencCall].Enable8x8Stat);
        CmpResMV = CmpMV(out_mv_st1, out_mv_st1_5);
        g_tsLog << "in step5, CmpResStat is " << CmpResStat << "\tCmpResMV is " << CmpResMV << "\n";
        if (!(CmpResStat & SAME_MBSTAT_EXCEPT_VAR_AVR))
        {
            g_tsLog << "ERROR: CmpResStat is not expected\n";
            return -1;
        }

        /*** Step6. Copy S2 to S1 ***/
        g_tsLog << "step6. Copy S2 to S1\n";
        surfacesPool[1].Data.Locked = 0;
        surfacesPool[2].Data.Locked = 0;
        SurfaceCopy(&surfacesPool[1], &surfacesPool[2], m_pFrameAllocator);

        /*** Step7. Call ProcessFrameAsync for S0 and S1, DownsampleInput is false; ***/
        g_tsLog << "step7. Call ProcessFrameAsync for S0 and S1, DownsampleInput is false;\n";
        IndexPreencCall++;
        p_PreENCInput[IndexPreencCall]->InSurface = &surfacesPool[1];
        msdk_atomic_inc16(&p_PreENCInput[IndexPreencCall]->InSurface->Data.Locked);
        FEIPreEncCtrl[IndexPreencCall].DownsampleInput = MFX_CODINGOPTION_OFF;
        FEIPreEncCtrl[IndexPreencCall].RefFrame[0] = p_PreENCInput[S0Index]->InSurface;
        FEIPreEncCtrl[IndexPreencCall].RefFrame[1] = NULL;
        p_PreENCInput[IndexPreencCall]->NumFrameL0 = 1;
        p_PreENCInput[IndexPreencCall]->NumFrameL1 = 0;
        g_tsStatus.expect(MFX_ERR_NONE);
        sts = ProcessFrameAsync(m_session, p_PreENCInput[IndexPreencCall], p_PreENCOutput[IndexPreencCall], m_pSyncPoint);
        g_tsStatus.expect(tc.sts);
        sts = SyncOperation();
        mfxExtFeiPreEncMBStat *out_mbStat_st1_7 = &out_mbStat[IndexPreencCall];
        mfxExtFeiPreEncMV *out_mv_st1_7 = &out_mvs[IndexPreencCall];

        //compare stat to ST1, should be different, don't compare variance and average
        CmpResStat = CmpMBStat(out_mbStat_st1, out_mbStat_st1_7, FEIPreEncCtrl[IndexPreencCall].Enable8x8Stat);
        CmpResMV = CmpMV(out_mv_st1, out_mv_st1_7);
        g_tsLog << "in step7, CmpResStat is " << CmpResStat << "\tCmpResMV is " << CmpResMV << "\n";
        if (CmpResStat & SAME_MBSTAT_EXCEPT_VAR_AVR)
        {
            g_tsLog << "ERROR: CmpResStat is not expected\n";
            return -1;
        }

        /*** Step8. Call ProcessFrameAsync for S0 and S1, DownsampleInput is true; ***/
        g_tsLog << "step8. Call ProcessFrameAsync for S0 and S1, DownsampleInput is true;\n";
        IndexPreencCall++;
        p_PreENCInput[IndexPreencCall]->InSurface = &surfacesPool[1];
        msdk_atomic_inc16(&p_PreENCInput[IndexPreencCall]->InSurface->Data.Locked);
        FEIPreEncCtrl[IndexPreencCall].DownsampleInput = MFX_CODINGOPTION_ON;
        FEIPreEncCtrl[IndexPreencCall].RefFrame[0] = p_PreENCInput[S0Index]->InSurface;
        FEIPreEncCtrl[IndexPreencCall].RefFrame[1] = NULL;
        p_PreENCInput[IndexPreencCall]->NumFrameL0 = 1;
        p_PreENCInput[IndexPreencCall]->NumFrameL1 = 0;
        g_tsStatus.expect(MFX_ERR_NONE);
        sts = ProcessFrameAsync(m_session, p_PreENCInput[IndexPreencCall], p_PreENCOutput[IndexPreencCall], m_pSyncPoint);
        g_tsStatus.expect(tc.sts);
        sts = SyncOperation();
        mfxExtFeiPreEncMBStat *out_mbStat_st2_8 = &out_mbStat[IndexPreencCall];
        mfxExtFeiPreEncMV *out_mv_st2_8 = &out_mvs[IndexPreencCall];
        
        // Compare stat to ST2, should be the same including variance and average
        CmpResStat = CmpMBStat(out_mbStat_st2, out_mbStat_st2_8, FEIPreEncCtrl[IndexPreencCall].Enable8x8Stat);
        CmpResMV = CmpMV(out_mv_st2, out_mv_st2_8);
        g_tsLog << "in step8, CmpResStat is " << CmpResStat << "\tCmpResMV is " << CmpResMV << "\n";
        if (!((CmpResStat & SAME_MBSTAT_EXCEPT_VAR_AVR) && (CmpResStat & SAME_VAR_AVR)))
        {
            g_tsLog << "ERROR: CmpResStat is not expected\n";
            return -1;
        }

        /*** Finish ***/
        g_tsLog << "step finish.\n";
    } 
    else if ( tc.mode & (DS_CTRL_REF_FRAME_1_REORDER | DS_CTRL_REF_FRAME_1_ORG)) 
    {

        /*** Step1. Fill in surfaces by real YUV data, Where S0 = S3, S1 = S4, S2 = S5***/
        g_tsLog << "Step1. Fill in surfaces by real YUV data.\n";
        if(m_filler)
        {
            for (int SurfIdx = 0; SurfIdx < NumOfFrameUsed; SurfIdx++) 
            {
                surfacesPool[SurfIdx].Data.FrameOrder = SurfIdx;
                m_filler->ProcessSurface(&surfacesPool[SurfIdx], m_pFrameAllocator);
            }
        }
        // Fill with the identical content for surface 4,5,6 with surface 1,2,3.
        SurfaceCopy(&surfacesPool[3], &surfacesPool[0], m_pFrameAllocator);
        SurfaceCopy(&surfacesPool[4], &surfacesPool[1], m_pFrameAllocator);
        SurfaceCopy(&surfacesPool[5], &surfacesPool[2], m_pFrameAllocator);

        mfxExtFeiPreEncMV *out_mv_st1 = NULL, *out_mv_st2  = NULL;
        mfxExtFeiPreEncMBStat *out_mbStat_st1 = NULL, *out_mbStat_st2 = NULL;

        for (mfxU16 i =0; i < 2; i++) 
        {
            //set the run order
            mfxU16 idxRun = tc.mode & DS_CTRL_REF_FRAME_1_ORG ? i : 1-i;
            if (idxRun == 0) 
            {
                /*** Run 1 ***/
                g_tsLog << "Stage 1:\n";
                /*** Step2. Call preenc with S0, downsampleInput = true, no refs. ***/
                g_tsLog << "step2. Call preenc with S0, downsampleInput = true, no refs.\n";
                IndexPreencCall = 0;
                p_PreENCInput[IndexPreencCall]->InSurface = &surfacesPool[0];
                msdk_atomic_inc16(&p_PreENCInput[IndexPreencCall]->InSurface->Data.Locked);
                FEIPreEncCtrl[IndexPreencCall].DownsampleInput = MFX_CODINGOPTION_ON;
                FEIPreEncCtrl[IndexPreencCall].RefFrame[0] = NULL;
                FEIPreEncCtrl[IndexPreencCall].RefFrame[1] = NULL;
                p_PreENCInput[IndexPreencCall]->NumFrameL0 = 0;
                p_PreENCInput[IndexPreencCall]->NumFrameL1 = 0;
                g_tsStatus.expect(MFX_ERR_NONE);
                sts = ProcessFrameAsync(m_session, p_PreENCInput[IndexPreencCall], p_PreENCOutput[IndexPreencCall], m_pSyncPoint);
                g_tsStatus.expect(tc.sts);
                sts = SyncOperation();

                /*** Step3. Call preenc with S1, downsampleInput = true, no refs ***/
                g_tsLog << "step3. Call preenc with S1, downsampleInput = true, no refs\n";
                IndexPreencCall++;
                p_PreENCInput[IndexPreencCall]->InSurface = &surfacesPool[1];
                msdk_atomic_inc16(&p_PreENCInput[IndexPreencCall]->InSurface->Data.Locked);
                FEIPreEncCtrl[IndexPreencCall].DownsampleInput = MFX_CODINGOPTION_ON;
                FEIPreEncCtrl[IndexPreencCall].RefFrame[0] = NULL;
                FEIPreEncCtrl[IndexPreencCall].RefFrame[1] = NULL;
                p_PreENCInput[IndexPreencCall]->NumFrameL0 = 0;
                p_PreENCInput[IndexPreencCall]->NumFrameL1 = 0;
                g_tsStatus.expect(MFX_ERR_NONE);
                sts = ProcessFrameAsync(m_session, p_PreENCInput[IndexPreencCall], p_PreENCOutput[IndexPreencCall], m_pSyncPoint);
                g_tsStatus.expect(tc.sts);
                sts = SyncOperation();

                /*** Step4. Call preenc with S2 downsampleInput = true, S0 and S1 are references, downsampleReferences = {false, false} ***/
                g_tsLog << "Step4. Call preenc with S2 downsampleInput = true, S0 and S1 are references, downsampleReferences = {false, false}; \n";
                IndexPreencCall++;
                p_PreENCInput[IndexPreencCall]->InSurface = &surfacesPool[2];
                msdk_atomic_inc16(&p_PreENCInput[IndexPreencCall]->InSurface->Data.Locked);
                FEIPreEncCtrl[IndexPreencCall].DownsampleInput = MFX_CODINGOPTION_ON;
                FEIPreEncCtrl[IndexPreencCall].RefFrame[0] = &surfacesPool[0];
                FEIPreEncCtrl[IndexPreencCall].RefFrame[1] = &surfacesPool[1];
                FEIPreEncCtrl[IndexPreencCall].DownsampleReference[0] = MFX_CODINGOPTION_OFF;
                FEIPreEncCtrl[IndexPreencCall].DownsampleReference[1] = MFX_CODINGOPTION_OFF;
                p_PreENCInput[IndexPreencCall]->NumFrameL0 = 1;
                p_PreENCInput[IndexPreencCall]->NumFrameL1 = 1;
                g_tsStatus.expect(MFX_ERR_NONE);
                sts = ProcessFrameAsync(m_session, p_PreENCInput[IndexPreencCall], p_PreENCOutput[IndexPreencCall], m_pSyncPoint);
                g_tsStatus.expect(tc.sts);
                sts = SyncOperation();
                out_mv_st1 = &out_mvs[IndexPreencCall];
                out_mbStat_st1 = &out_mbStat[IndexPreencCall];
            } 
            else if (idxRun == 1) 
            {
                /*** Run 2 ***/
                g_tsLog << "Stage 2:\n";
                /*** Step5. Call preenc with S5, downsampleInput = true, S3 and S4 are references, downsampleReferences = {true, true} ***/
                g_tsLog << "Step5. Call preenc with S3, downsampleInput = true, S4 and S5 are references, downsampleReferences = {true, true};\n";
                IndexPreencCall = 5;
                p_PreENCInput[IndexPreencCall]->InSurface = &surfacesPool[5];
                msdk_atomic_inc16(&p_PreENCInput[IndexPreencCall]->InSurface->Data.Locked);
                FEIPreEncCtrl[IndexPreencCall].DownsampleInput = MFX_CODINGOPTION_ON;
                FEIPreEncCtrl[IndexPreencCall].RefFrame[0] = &surfacesPool[3];
                FEIPreEncCtrl[IndexPreencCall].RefFrame[1] = &surfacesPool[4];
                FEIPreEncCtrl[IndexPreencCall].DownsampleReference[0] = MFX_CODINGOPTION_ON;
                FEIPreEncCtrl[IndexPreencCall].DownsampleReference[1] = MFX_CODINGOPTION_ON;
                p_PreENCInput[IndexPreencCall]->NumFrameL0 = 1;
                p_PreENCInput[IndexPreencCall]->NumFrameL1 = 1;
                g_tsStatus.expect(MFX_ERR_NONE);
                sts = ProcessFrameAsync(m_session, p_PreENCInput[IndexPreencCall], p_PreENCOutput[IndexPreencCall], m_pSyncPoint);
                g_tsStatus.expect(tc.sts);
                sts = SyncOperation();
                out_mv_st2 = &out_mvs[IndexPreencCall];
                out_mbStat_st2 = &out_mbStat[IndexPreencCall];
            }
        }
        CmpResStat = CmpMBStat(out_mbStat_st1, out_mbStat_st2, FEIPreEncCtrl[IndexPreencCall].Enable8x8Stat);
        CmpResMV = CmpMV(out_mv_st1, out_mv_st2);
        g_tsLog << "in step5, CmpResStat is " << CmpResStat << "\tCmpResMV is " << CmpResMV << "\n";
        if (CmpResStat != SAME_ALL || CmpResMV != SAME_OUT_MV)
        {
            g_tsLog << "ERROR: CmpResStat or CmpResMV is not expected\n";
            return -1;
        }
        g_tsLog << "finish\n";
    } 
    else if ( tc.mode & (DS_CTRL_REF_FRAME_2_ON | DS_CTRL_REF_FRAME_2_OFF )) 
    {
        /*** Step1. Fill in surfaces by real YUV data, Where S0,S1,S2***/
        g_tsLog << "Step1. Fill in surfaces by real YUV data.\n";
        if(m_filler)
        {
            for (int SurfIdx = 0; SurfIdx < NumOfFrameUsed; SurfIdx++) 
            {
                surfacesPool[SurfIdx].Data.FrameOrder = SurfIdx;
                m_filler->ProcessSurface(&surfacesPool[SurfIdx], m_pFrameAllocator);
            }
        }

        /*** Step2. Call preenc with S2 downsampleInput = true, S0 and S1 are references, downsampleReferences = {false, false} ***/
        g_tsLog << "Step2. Call preenc with S2 downsampleInput = true, S0 and S1 are references. \n";
        if (tc.mode & DS_CTRL_REF_FRAME_2_ON)
        {
             g_tsLog << " downsampleReferences = {true, true}; \n";        
        }
        else
        {
            g_tsLog << " downsampleReferences = {false, false}; \n";        
        }

        IndexPreencCall = 2;
        p_PreENCInput[IndexPreencCall]->InSurface = &surfacesPool[2];
        msdk_atomic_inc16(&p_PreENCInput[IndexPreencCall]->InSurface->Data.Locked);
        FEIPreEncCtrl[IndexPreencCall].DownsampleInput = MFX_CODINGOPTION_ON;
        FEIPreEncCtrl[IndexPreencCall].RefFrame[0] = &surfacesPool[0];
        FEIPreEncCtrl[IndexPreencCall].RefFrame[1] = &surfacesPool[1];
        FEIPreEncCtrl[IndexPreencCall].DownsampleReference[0] = (tc.mode & DS_CTRL_REF_FRAME_2_ON) ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF;
        FEIPreEncCtrl[IndexPreencCall].DownsampleReference[1] = (tc.mode & DS_CTRL_REF_FRAME_2_ON) ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF;
        p_PreENCInput[IndexPreencCall]->NumFrameL0 = 1;
        p_PreENCInput[IndexPreencCall]->NumFrameL1 = 1;
        g_tsStatus.expect(MFX_ERR_NONE);
        sts = ProcessFrameAsync(m_session, p_PreENCInput[IndexPreencCall], p_PreENCOutput[IndexPreencCall], m_pSyncPoint);
        g_tsStatus.expect(tc.sts);
        sts = SyncOperation();
        mfxExtFeiPreEncMV *out_mv_st1 = &out_mvs[IndexPreencCall];
        mfxExtFeiPreEncMBStat *out_mbStat_st1 = &out_mbStat[IndexPreencCall];
        g_tsLog << "Steps finish.\n";
    }

    Close();

    // Free memory allocated dynamically
    if (NULL != surfacesPool) 
    {
        delete surfacesPool;
        surfacesPool = NULL;
    }

    for (int i = 0; i < MAX_NUM_PREENC_CALL; i++) 
    {
        if (NULL != out_mvs[i].MB) 
        {
            delete out_mvs[i].MB;
            out_mvs[i].MB = NULL;
        }
        if (NULL != out_mbStat[i].MB) 
        {
            delete out_mbStat[i].MB;
            out_mbStat[i].MB = NULL;
        }
        if (NULL != out_mbStat[i].MB) 
        {
            delete out_mbStat[i].MB;
            out_mbStat[i].MB = NULL;
        }
        if (NULL != p_PreENCInput[i]) 
        {
            delete p_PreENCInput[i];
            p_PreENCInput[i] = NULL;
        }
        if (NULL != p_PreENCOutput[i]) 
        {
            delete p_PreENCOutput[i];
            p_PreENCOutput[i] = NULL;
        }
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(fei_preenc_downsample);
};
