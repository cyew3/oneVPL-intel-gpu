/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2015-2016 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_struct.h"
#include "mfxplugin.h"
#include "ts_enc.h"


namespace hevce_gaa_processframeasync
{


class TestSuite : tsVideoENC
{
public:
    TestSuite() : tsVideoENC(MFX_CODEC_HEVC, 1) {}
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    struct tc_par;
    static const mfxU32 n_par = 6;
    bool check_output(mfxExtFEIH265Param *in, mfxFEIH265Output *out, mfxU32 funk);

    enum
    {
        NULL_SESSION = 1,
        SET_IN       = 2,
        NOT_INIT     = 3,
        INIT_FAILED  = 4,
        SURF_ZERO    = 8,
    };

    struct tc_struct
    {
        mfxStatus sts;
        std::string file;
        mfxU16 nframes;
        mfxU32 mode;
        struct f_pair 
        {
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[n_par];
        struct input
        {
            const tsStruct::Field* f;
            mfxU32 v;
        } in[3];
       
    };

    static const tc_struct test_case[];
};


const TestSuite::tc_struct TestSuite::test_case[] = 
{
    
    {/*00*/ MFX_ERR_NONE, "forBehaviorTest/foreman_cif.yuv", 1, 0},
    {/*01*/ MFX_ERR_NONE, "forBehaviorTest/foreman_cif.yuv", 1, 0, {}, {&tsStruct::mfxExtFEIH265Input.FEIOp, MFX_FEI_H265_OP_INTRA_MODE}},
    {/*02*/ MFX_ERR_NONE, "forBehaviorTest/foreman_cif.yuv", 1, 0, {}, {&tsStruct::mfxExtFEIH265Input.FEIOp, MFX_FEI_H265_OP_INTRA_DIST}},
    {/*03*/ MFX_ERR_NONE, "forBehaviorTest/foreman_cif.yuv", 1, 0, {}, {&tsStruct::mfxExtFEIH265Input.FEIOp, MFX_FEI_H265_OP_INTER_ME}},
    {/*04*/ MFX_ERR_NONE, "forBehaviorTest/foreman_cif.yuv", 1, 0, {}, {&tsStruct::mfxExtFEIH265Input.FEIOp, MFX_FEI_H265_OP_INTERPOLATE}},
    {/*05*/ MFX_ERR_INVALID_HANDLE, "forBehaviorTest/foreman_cif.yuv", 1, NULL_SESSION, {}, {}},
    {/*06*/ MFX_ERR_NOT_INITIALIZED, "forBehaviorTest/foreman_cif.yuv", 1, NOT_INIT, {}, {}},
    {/*07*/ MFX_ERR_INVALID_VIDEO_PARAM, "forBehaviorTest/foreman_cif.yuv", 1, INIT_FAILED, {}, {}},
    {/*08*/ MFX_ERR_NULL_PTR, "forBehaviorTest/foreman_cif.yuv", 1, SURF_ZERO, {}, {}},
    {/*09*/ MFX_ERR_NONE, "forBehaviorTest/foreman_cif.yuv", 1, 0, {{&tsStruct::mfxVideoParam.mfx.NumSlice, 4},
                                                                         {&tsStruct::mfxVideoParam.mfx.CodecProfile, 1},
                                                                         {&tsStruct::mfxVideoParam.mfx.CodecLevel, 51}},
                                                                        {}
    },
    {/*10*/ MFX_ERR_NONE, "forBehaviorTest/foreman_cif.yuv", 1, 0, {{&tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 60000},
                                                                         {&tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001},
                                                                         {&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioW, 16},
                                                                         {&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioH, 9}},
                                                                        {}
    },
    {/*11*/ MFX_ERR_NONE, "forBehaviorTest/foreman_cif.yuv", 30, 0, {}, {}},
    
    
    

    

   
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

bool TestSuite::check_output(mfxExtFEIH265Param *in, mfxFEIH265Output *out, mfxU32 funk)
{
    if (!m_ENCOutput->ExtParam)
    {
        return 0;
    }
    if ((out->PaddedWidth < m_pPar->mfx.FrameInfo.Width) ||
        (out->PaddedHeight < m_pPar->mfx.FrameInfo.Height))
    {
        return 0;
    }
    else if ((out->PaddedWidth % 16 != 0) || 
             (out->PaddedHeight % 16 != 0))
            {
                return 0;
            }
    if (out->IntraMaxModes > in->NumIntraModes)
    {
        return 0;
    }
    if (out->IntraPitch < (out->PaddedWidth/16))
    {
        return 0;
    }
    
    switch (funk)
    {
    case MFX_FEI_H265_OP_INTRA_MODE:  
        {
            for (mfxU32 i = 0; i < out->IntraMaxModes; i++)
            {
                 if ((!out->IntraModes16x16[i]) ||
                     (!out->IntraModes32x32[i]) ||
                     (!out->IntraModes8x8[i])   ||
                     (!out->IntraModes4x4[i]))
                 {
                     return 0;
                 }
            }
            break;
        }
    case MFX_FEI_H265_OP_INTRA_DIST:  
        {
            if (!out->IntraDist)
            {
                return 0;
            }
            break;
        }
    case MFX_FEI_H265_OP_INTER_ME:
        {
            for (int i = 0; i < m_pPar->mfx.NumRefFrame; i++)
            {
                 if(!out->MV[i])
                 {
                     return 0;
                 }
            }
            break;
        }
    case MFX_FEI_H265_OP_INTERPOLATE: 
        {
            break;
        }
    default:
        break;
    }

    return 1;
}

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];
    mfxU32 encframes = 0;
    mfxStatus sts = MFX_ERR_NONE;
    

    MFXInit();
    
    g_tsPlugin.Reg(MFX_PLUGINTYPE_VIDEO_ENC, MFX_CODEC_HEVC, MFX_PLUGINID_HEVCE_FEI_HW);
    m_uid = g_tsPlugin.UID(MFX_PLUGINTYPE_VIDEO_ENC, MFX_CODEC_HEVC);
    m_loaded = false;
    Load();
    
    // set default parameters
    m_pPar->mfx.FrameInfo.Width = 352;
    m_pPar->mfx.FrameInfo.Height = 288;
    m_pPar->mfx.FrameInfo.CropW = 0;
    m_pPar->mfx.FrameInfo.CropH = 0;
    m_pPar->mfx.NumRefFrame = 4;
    m_pPar->mfx.FrameInfo.FourCC = MFX_FOURCC_YV12;
   
    m_ENCInput = new mfxENCInput();
    m_ENCOutput = new mfxENCOutput();
    
    mfxExtFEIH265Param& FEIParam = m_par;
    FEIParam.MaxCUSize = 16;
    FEIParam.MPMode = 1;
    FEIParam.NumIntraModes = 1;
    
    mfxExtFEIH265Input In = {0};
    In.Header.BufferId = MFX_EXTBUFF_FEI_H265_INPUT;
    In.FEIOp = MFX_FEI_H265_OP_NOP;
    In.FrameType = MFX_FRAMETYPE_I;
    In.RefIdx = m_pPar->mfx.NumRefFrame - 1;

    mfxExtFEIH265Output Out = {0};
    Out.Header.BufferId = MFX_EXTBUFF_FEI_H265_OUTPUT;
    mfxFEIH265Output tmp_out = {0};
    Out.feiOut = &tmp_out;
    

    m_ENCInput->NumExtParam = 1;
    m_ENCInput->ExtParam = new mfxExtBuffer*[m_ENCInput->NumExtParam];
    m_ENCInput->ExtParam[0] = new mfxExtBuffer();
    m_ENCInput->ExtParam[0] = (mfxExtBuffer*)&In;

    m_ENCOutput->NumExtParam = 1;
    m_ENCOutput->ExtParam = new mfxExtBuffer*[m_ENCOutput->NumExtParam];
    m_ENCOutput->ExtParam[0] = new mfxExtBuffer();
    m_ENCOutput->ExtParam[0] = (mfxExtBuffer*)&Out;
    
    // set up parameters for case
    for(mfxU32 i = 0; i < n_par; i++)
    {
        if(tc.set_par[i].f)
        {
            tsStruct::set(m_pPar, *tc.set_par[i].f, tc.set_par[i].v);
        }
        if((i < 3) && (tc.in[i].f))
        {
            tsStruct::set(&In, *tc.in[i].f, tc.in[i].v);
        }
    }
    
   
    

    //set surfce processor
    std::string file_name = g_tsStreamPool.Get(tc.file);
    tsRawReader SurfProc = tsRawReader(file_name.c_str(), m_pPar->mfx.FrameInfo, tc.nframes);
    g_tsStreamPool.Reg();
    m_filler = &SurfProc;
    
    for(;;)
    {
        sts = MFX_ERR_NONE;
        if (tc.mode == INIT_FAILED)
        {
            m_pPar->NumExtParam = 0;
            m_pPar->ExtParam = NULL;
            sts = MFX_ERR_NULL_PTR;
            g_tsStatus.expect(sts);
        }
        else if (tc.mode != NOT_INIT)
        {
            g_tsStatus.expect(sts);
            sts = MFX_ERR_NONE;
        }
        else
        {
            sts = MFX_ERR_NOT_INITIALIZED;
        }
        
        if (sts != MFX_ERR_NOT_INITIALIZED)
        {
            Init(m_session, m_pPar);
            if (sts == MFX_ERR_NONE)
            {
                g_tsStatus.expect(MFX_ERR_NONE);
                AllocSurfaces();TS_CHECK_MFX;
                m_ENCInput->InSurface = GetSurface();TS_CHECK_MFX;
                if(m_filler)
                {
                    m_ENCInput->InSurface = m_filler->ProcessSurface(m_ENCInput->InSurface, m_pFrameAllocator);
                }
            }
            sts = MFX_ERR_NONE;
        }
                
        if (tc.mode == SURF_ZERO)
        {
            m_ENCInput->InSurface->Data.Y = 0;
        }
        g_tsStatus.expect(tc.sts);
        if (tc.mode != NULL_SESSION)
            g_tsStatus.check(ProcessFrameAsync(m_session, m_ENCInput, m_ENCOutput, m_pSyncPoint));
        else
            g_tsStatus.check(ProcessFrameAsync(NULL, m_ENCInput, m_ENCOutput, m_pSyncPoint));
        if (g_tsStatus.get() == MFX_ERR_NONE)
        {
            SyncOperation();
            if (!check_output(&FEIParam, Out.feiOut, In.FEIOp))
            {
                g_tsLog<<"Error: wrong out on "<<encframes<<"frame!\n";
                throw tsFAIL;
            }
        }
        encframes++;
        if (encframes >= tc.nframes)
            break;
    }
    

    g_tsStatus.expect(sts);
    Close();
   

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(hevce_gaa_processframeasync);
};