/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2007-2018 Intel Corporation. All Rights Reserved.

File Name: hevce_ref_list1_checks.cpp

\* ****************************************************************************** */

/*!
\file hevce_ref_list1_checks.cpp
\brief Gmock test hevce_ref_list1_checks

Description:
Beh test checks components MV L1. If each components of MV L1 is equal zero than test is not pass. 
It's important that even MV L1 is exist then its components of some frame some 
PU must be is not equal of zero.\n

Algorithm:
- Set stream param
- Set suite params
- Check plugin and Supported platform
- Create a session
- Load the plugin in advance.
- Get allocator for surface
- Set handle for session
- Init raw reader
- Call Query()
- Get video param
- Encode stream. Checking MV L1 of every PU of every frame in all stream.If each component of MV L1
is not zero in AMVP or Merge block then test is pass. 
- Check required flag
- Close session and free the surfaces in pool.
*/
#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

namespace hevce_ref_list1_checks
{
    using namespace BS_HEVC2;

    class TestSuite
        : public tsVideoEncoder
        , public tsParserHEVC2
        , public tsBitstreamProcessor
        , public tsSurfaceProcessor
    {
    private:
        bool inter_pred_l1_exist;
    public:
        static const unsigned int n_cases;

        TestSuite()
            : tsVideoEncoder(MFX_CODEC_HEVC)
            , tsParserHEVC2(PARSE_SSD)
#ifdef DEBUG_STREAM
            , m_bsw(DEBUG_STREAM)
#endif
        {
            m_bs_processor = this;
            m_filler = this;
            inter_pred_l1_exist = false;
        }
#ifdef DEBUG_STREAM
        tsBitstreamWriter m_bsw;
#endif
        enum
        {
            MFX_PAR
        };

        struct tc_struct
        {
            mfxStatus sts;
            mfxU32 mode;
            struct f_pair
            {
                mfxU32 ext_type;
                const  tsStruct::Field* f;
                mfxU32 v;
            } set_par[MAX_NPARS];
        };

        static const tc_struct test_case[];

        ~TestSuite() {}

        template<mfxU32 fourcc>
        int RunTest_Subtype(const unsigned int id);

        int RunTest(tc_struct tc, unsigned int fourcc_id);

        mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
        {
            g_tsLog << "ProcessBitstream " << "\n";
            mfxU32 checked = 0;

            set_buffer(bs.Data + bs.DataOffset, bs.DataLength);

#ifdef DEBUG_STREAM
            m_bsw.ProcessBitstream(bs, nFrames);
#endif
            /*Every CTU divided by CU, and CU divided by PU.
            Checking MV L1 of every PU of every frame in all stream. If each component of MV L1 is 
            not zero in AMVP or Merge block then test is pass. If mode CQP is set and 1 
            repeated frame due to all CU will be coded as skipped 
            then test will be fail but this is not required on original problem.*/
            while (checked++ < nFrames)
            {
                UnitType& hdr = ParseOrDie();
                if (inter_pred_l1_exist)
                    break;
                for (auto pNALU = &hdr; pNALU; pNALU = pNALU->next)
                {
                    if (!IsHEVCSlice(pNALU->nal_unit_type))
                        continue;

                    Slice* slice = pNALU->slice;
                    if(!slice)
                        continue;
                    if(slice->num_ref_idx_l1_active != 0)
                    {
                       for (auto pCTU = slice->ctu; pCTU; pCTU = pCTU->Next)
                       {
                          for (auto pCU = pCTU->Cu; pCU; pCU = pCU->Next)
                          {
                              for (auto pPU = pCU->Pu; pPU; pPU = pPU->Next)
                              {
                                  if (PRED_L1 == pPU->inter_pred_idc ||
                                      PRED_BI == pPU->inter_pred_idc)
                                  {
                                      if (pPU->MvLX[1][0] != 0 ||
                                          pPU->MvLX[1][1] != 0)
                                      {
                                          inter_pred_l1_exist = true;
                                      }
                                  }
                              }
                          }//pCU
                       }//pCTU
                    }//0 != slice->num_ref_idx_l1_active
                }
            }

            bs.DataLength = 0;

            return MFX_ERR_NONE;
        }
    };

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        { MFX_ERR_NONE, MFX_RATECONTROL_VBR,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 1 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR }
            }
        },
        { MFX_ERR_NONE, MFX_RATECONTROL_VBR,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 2 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR }
            }
        },
        { MFX_ERR_NONE, MFX_RATECONTROL_CQP,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 1 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP }
            }
        },
        { MFX_ERR_NONE, MFX_RATECONTROL_CQP,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 2 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP }
            }
        },
        { MFX_ERR_NONE, MFX_RATECONTROL_CBR,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 1 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR }
            }
        },
        { MFX_ERR_NONE, MFX_RATECONTROL_CBR,
            {
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 2 },
                { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR }
            }
        }
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    template<mfxU32 fourcc>
    int TestSuite::RunTest_Subtype(const unsigned int id)
    {
        const tc_struct& tc = test_case[id];
        return RunTest(tc, fourcc);
    }

    int TestSuite::RunTest(tc_struct tc, unsigned int fourcc_id)
    {
        TS_START;
        const char* stream = nullptr;

        //set default param
       if (fourcc_id == MFX_FOURCC_NV12)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;

            if (g_tsConfig.sim)
            {
                stream = g_tsStreamPool.Get("YUV/salesman_176x144_449.yuv");
                m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 176;
                m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 144;
            }
            else
            {
                stream = g_tsStreamPool.Get("YUV/cust/Fairytale_1280x720p.yuv");
                m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 1280;
                m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 720;
            }
        }
        else if (fourcc_id == MFX_FOURCC_P010)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_P010;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.Shift = 1;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;

            if (g_tsConfig.sim)
            {
                stream = g_tsStreamPool.Get("YUV10bit420_ms/Kimono1_176x144_24_p010_shifted.yuv");
                m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 176;
                m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 144;
            }
            else 
            {
                stream = g_tsStreamPool.Get("YUV10bit/Cactus_ProRes_1280x720_50f.p010.yuv");
                m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 1280;
                m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 720;
            }
        }
        else if (fourcc_id == GMOCK_FOURCC_P012)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_P016;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.Shift = 1;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 12;

            if (g_tsConfig.sim)
            {
                stream = g_tsStreamPool.Get("YUV16bit420/FruitStall_176x144_240_p016.yuv");
                m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 176;
                m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 144;
            }
            else
            {
                stream = g_tsStreamPool.Get("YUV16bit420/FruitStall_1280x720_240_p016.yuv");
                m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 1280;
                m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 720;
            }
        }
        else if (fourcc_id == MFX_FOURCC_YUY2)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_YUY2;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
            m_par.mfx.FrameInfo.Shift = 0;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;

            if (g_tsConfig.sim)
            {
                stream = g_tsStreamPool.Get("YUV8bit422/Kimono1_176x144_24_yuy2.yuv");
                m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 176;
                m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 144;
            }
            else
            {
                //
                stream = g_tsStreamPool.Get("AMT/422/CrowdRun_422_1280x720_50_nv16.yuv");
                m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 1280;
                m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 720;
            }
        }
        else if (fourcc_id == MFX_FOURCC_Y210)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y210;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
            m_par.mfx.FrameInfo.Shift = 1;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;

            if (g_tsConfig.sim)
            {
                stream = g_tsStreamPool.Get("YUV10bit422/Kimono1_176x144_24_y210.yuv");
                m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 176;
                m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 144;
            }
            else
            {
                stream = g_tsStreamPool.Get("YUV10bit422/Fast_walk_1920x1080p50_300.p210.yuv");
                m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 1920;
                m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 1080;
            }
        }
        else if (fourcc_id == GMOCK_FOURCC_Y212)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y216;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
            m_par.mfx.FrameInfo.Shift = 1;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 12;

            if (g_tsConfig.sim)
            {
                stream = g_tsStreamPool.Get("YUV16bit422/FruitStall_176x144_240_y216.yuv");
                m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 176;
                m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 144;
            }
            else
            {
                stream = g_tsStreamPool.Get("YUV16bit422/FruitStall_1280x720_240_y216.yuv");
                m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 1280;
                m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 720;
            }
        }
        else if (fourcc_id == MFX_FOURCC_AYUV)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_AYUV;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            m_par.mfx.FrameInfo.Shift = 0;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;

            if (g_tsConfig.sim)
            {
                stream = g_tsStreamPool.Get("YUV8bit444/Kimono1_176x144_24_ayuv.yuv");
                m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 176;
                m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 144;
            }
            else
            {
                stream = g_tsStreamPool.Get("YUV8bit444/Kimono1_1920x1080_24_ayuv.yuv");
                m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 1920;
                m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 1080;
            }
        }
        else if (fourcc_id == MFX_FOURCC_Y410)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y410;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            m_par.mfx.FrameInfo.Shift = 0;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;

            if (g_tsConfig.sim)
            {
                stream = g_tsStreamPool.Get("YUV10bit444/Kimono1_176x144_24_y410.yuv");
                m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 176;
                m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 144;
            }
            else
            {
                stream = g_tsStreamPool.Get("forBehaviorTest/Kimono1_1280x720_24_y410_30.yuv");
                m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 1280;
                m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 720;
            }
        }
        else if (fourcc_id == GMOCK_FOURCC_Y412)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y416;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            m_par.mfx.FrameInfo.Shift = 1;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 12;

            if (g_tsConfig.sim)
            {
                stream = g_tsStreamPool.Get("YUV16bit444/FruitStall_176x144_240_y416.yuv");
                m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 176;
                m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 144;
            }
            else
            {
                stream = g_tsStreamPool.Get("YUV16bit444/FruitStall_1280x720_240_y416.yuv");
                m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 1280;
                m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 720;
            }
        }
        else
        {
            g_tsLog << "ERROR: invalid fourcc_id parameter: " << fourcc_id << "\n";
            return 0;
        }
        g_tsStreamPool.Reg();

        g_tsStatus.expect(tc.sts);

        mfxHDL hdl;
        mfxHandleType type;

        //Create a session.
        MFXInit();
        //load the plugin in advance.
        Load();

        //get allocator
        if (!GetAllocator())
        {
            UseDefaultAllocator(
                (m_par.IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY)
                || (m_request.Type & MFX_MEMTYPE_SYSTEM_MEMORY)
            );
        }

        //set handle
        if (!((m_par.IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY)
            || (m_request.Type & MFX_MEMTYPE_SYSTEM_MEMORY))
            && (!m_pVAHandle))
        {
            m_pFrameAllocator = GetAllocator();
            SetFrameAllocator();
            m_pVAHandle = m_pFrameAllocator;
            m_pVAHandle->get_hdl(type, hdl);
            SetHandle(m_session, type, hdl);
            m_is_handle_set = (g_tsStatus.get() >= 0);
        }

        tsSurfaceProcessor *reader;
        reader = new tsRawReader(stream, m_pPar->mfx.FrameInfo);
        m_filler = reader;

        Query();

        Init();

        //Get video param.
        AllocBitstream(); TS_CHECK_MFX;
        AllocSurfaces(); TS_CHECK_MFX;
        m_pSurf = GetSurface(); TS_CHECK_MFX;
        if (m_filler)
        {
            m_pSurf = m_filler->ProcessSurface(m_pSurf, m_pFrameAllocator);
        }

        int nFrames = 10;

        if (tc.sts >= MFX_ERR_NONE)
        {
            int encoded = 0;
            while (encoded < nFrames)
            {
                if (MFX_ERR_MORE_DATA == EncodeFrameAsync())
                {
                    continue;
                }

                g_tsStatus.check(); TS_CHECK_MFX;
                SyncOperation(); TS_CHECK_MFX;
                encoded++;
            }
        }

        if(!inter_pred_l1_exist)
        {
            g_tsLog << "THE ENCODING IS PERFORMED B-FRAME PREDICTION(LDB) AND EXPECTED"
              "AT LEAST ONE COMPONENT MV L1 IS NOT EQUAL ZERO. THE SUSPECT DYNAMIC CONTENT STREAM";
        }
        EXPECT_TRUE(inter_pred_l1_exist);
        g_tsStatus.expect(tc.sts);

        Close();
        delete reader;

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_8b_420_nv12_ref_list1_checks, RunTest_Subtype<MFX_FOURCC_NV12>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_420_p010_ref_list1_checks, RunTest_Subtype<MFX_FOURCC_P010>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_12b_420_p016_ref_list1_checks, RunTest_Subtype<GMOCK_FOURCC_P012>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_8b_422_yuy2_ref_list1_checks, RunTest_Subtype<MFX_FOURCC_YUY2>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_422_y210_ref_list1_checks, RunTest_Subtype<MFX_FOURCC_Y210>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_12b_422_y216_ref_list1_checks, RunTest_Subtype<GMOCK_FOURCC_Y212>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_8b_444_ayuv_ref_list1_checks, RunTest_Subtype<MFX_FOURCC_AYUV>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_444_y410_ref_list1_checks, RunTest_Subtype<MFX_FOURCC_Y410>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_12b_444_y416_ref_list1_checks, RunTest_Subtype<GMOCK_FOURCC_Y412>, n_cases);
};