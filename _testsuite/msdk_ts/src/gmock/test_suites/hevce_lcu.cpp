/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2018 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

//#define DEBUG_STREAM "hevce_lcu.265"

namespace hevce_lcu
{
    using namespace BS_HEVC2;
/*
    class BitstreamChecker : public tsBitstreamProcessor, public tsParserHEVC2
    {
    public:
        mfxU16  m_saoInit;
        mfxU16  m_slicesWithSAO;
        const mfxU16* m_saoRT;
#ifdef DEBUG_STREAM
        tsBitstreamWriter m_bsw;
#endif
        BitstreamChecker()
            : tsParserHEVC2(PARSE_SSD)
#ifdef DEBUG_STREAM
            , m_bsw(DEBUG_STREAM)
#endif
        {
            set_trace_level(0);
            m_slicesWithSAO = 0;
        }
        mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames);
    };

    mfxStatus BitstreamChecker::ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        mfxU32 checked = 0;

        if (bs.Data)
            SetBuffer0(bs);

#ifdef DEBUG_STREAM
        m_bsw.ProcessBitstream(bs, nFrames);
#endif

        while (checked++ < nFrames)
        {
            auto& hdr = ParseOrDie();

            for (auto pNALU = &hdr; pNALU; pNALU = pNALU->next)
            {
                if (!IsHEVCSlice(pNALU->nal_unit_type))
                    continue;

                auto& slice = *pNALU->slice;
                mfxU16 saoRT = (m_saoRT[slice.POC] && m_saoInit != MFX_SAO_DISABLE) ? m_saoRT[slice.POC] : m_saoInit;

                TS_TRACE(slice.POC);
                TS_TRACE(!!slice.sps->sample_adaptive_offset_enabled_flag);
                TS_TRACE(!!slice.sao_luma_flag);
                TS_TRACE(!!slice.sao_chroma_flag);

                EXPECT_EQ(m_saoInit != MFX_SAO_DISABLE, !!slice.sps->sample_adaptive_offset_enabled_flag);

                if (saoRT != MFX_SAO_UNKNOWN)
                {
                    EXPECT_EQ(!!(saoRT & MFX_SAO_ENABLE_LUMA), !!slice.sao_luma_flag);
                    EXPECT_EQ(!!(saoRT & MFX_SAO_ENABLE_CHROMA), !!slice.sao_chroma_flag);
                }
                
                if (!slice.sao_luma_flag && !slice.sao_chroma_flag)
                    continue;

                m_slicesWithSAO++;

                mfxF64 AppliedSAOPercent[3] = {};
                mfxF64 nLCU = 0;

                for (auto pCTU = slice.ctu; pCTU; pCTU = pCTU->Next)
                {
                    AppliedSAOPercent[0] += (pCTU->sao[0].type_idx != SAO_TYPE::NOT_APPLIED);
                    AppliedSAOPercent[1] += (pCTU->sao[1].type_idx != SAO_TYPE::NOT_APPLIED);
                    AppliedSAOPercent[2] += (pCTU->sao[2].type_idx != SAO_TYPE::NOT_APPLIED);
                    nLCU++;
                }

                AppliedSAOPercent[0] /= nLCU;
                AppliedSAOPercent[1] /= nLCU;
                AppliedSAOPercent[2] /= nLCU;
                AppliedSAOPercent[0] *= 100;
                AppliedSAOPercent[1] *= 100;
                AppliedSAOPercent[2] *= 100;

                TS_TRACE(nLCU);
                TS_TRACE(AppliedSAOPercent[0]);
                TS_TRACE(AppliedSAOPercent[1]);
                TS_TRACE(AppliedSAOPercent[2]);

                if (slice.sao_luma_flag)
                {
                    EXPECT_GT(AppliedSAOPercent[0], 20);
                }
                else
                {
                    EXPECT_EQ(0, AppliedSAOPercent[0]);
                }

                if (slice.sao_chroma_flag)
                {
                    EXPECT_GT(AppliedSAOPercent[1], 10);
                    EXPECT_GT(AppliedSAOPercent[2], 10);
                }
                else
                {
                    EXPECT_EQ(0, AppliedSAOPercent[1]);
                    EXPECT_EQ(0, AppliedSAOPercent[2]);
                }
            }
        }

        bs.DataLength = 0;

        BreakOnFailure();

        return MFX_ERR_NONE;
    }

    class SProc : public tsSurfaceProcessor
    {
    public:
        tsSurfaceProcessor& m_reader;
        tsVideoEncoder& m_enc;
        const TestCase& m_tc;

        SProc(tsSurfaceProcessor& r, const TestCase& tc, tsVideoEncoder& enc)
            : m_reader(r)
            , m_enc(enc)
            , m_tc(tc)
        {
            m_max = m_reader.m_max;
        }

        mfxStatus ProcessSurface(mfxFrameSurface1& s)
        {
            return m_reader.ProcessSurface(s);
        }
    };


    bool isUnsupported(tsExtBufType<mfxVideoParam>& par)
    {
        mfxExtCodingOption2* CO2 = par;
        mfxExtCodingOption3* CO3 = par;

        return (g_tsHWtype < MFX_HW_CNL)
            //|| LCUSize == 16
            || par.mfx.TargetUsage == 7
            || (CO3 && CO3->WeightedPred == MFX_WEIGHTED_PRED_EXPLICIT)
            || (CO3 && CO3->WeightedBiPred == MFX_WEIGHTED_PRED_EXPLICIT)
            || (   (g_tsHWtype == MFX_HW_CNL)
                && (   (CO3 && CO3->TargetBitDepthLuma == 10)
                    || (par.mfx.LowPower == 16 && CO2 && CO2->MaxSliceSize)))
            ;
    }

    const mfxU32 nFrames = 1;
*/

/*

    Description:
    This suite tests AVC encoder\n

    Algorithm:
    - Initializing MSDK lib
    - Set suite params (HEVC encoder params)
    - Set case params
    - Set expected Query() status
    - Call Query()
    - Check returned status
    - Set case params
    - Set expected Init() status
    - Call Init()
    - Check returned status
    - Call GetVideoParam() if Init() returned a warning
    - Check returned status and expected values
    - Set case params
    - Set expected Reset() status
    - Call Reset()
    - Check returned status
    - Call GetVideoParam() if Reset () returned a warning
    - Check returned status and expected values

    */

        enum
        {
            MFX_PAR = 0,
            RESET_PAR = 1,
            QUERY_EXP = 2,
            INIT_EXP = 3,
            RESET_EXP = 4,
            ENC_EXP = 5,
        };

        enum Actions
        {
            DoNothing = 0,
            SetDefault = -1,
            SetMinSupported = -2,
            SetMaxSupported = -3,
        };

        enum TYPE
        {
            QUERY = 1,
            INIT = 2,
            RESET = 4,
            ENC = 8,
            NEED_TWO_LCU_SUPPORTED = 16,
        };

        struct tc_struct {
            mfxU32 type;
            //! Expected Query() status
            mfxStatus q_sts;
            //! Expected Init() status
            mfxStatus i_sts;
            //! Expected Reset() status
            mfxStatus r_sts;
            //! Expected EncodeFrames() status
            mfxStatus enc_sts;
            //! \brief Structure contains params for some fields of encoder
            struct f_pair {
                //! Number of the params set (if there is more than one in a test case)
                mfxU32 ext_type;
                //! Field name
                const tsStruct::Field* f;
                //! Field value
                mfxI32 v;
            }set_par[MAX_NPARS];
        };

        tc_struct test_case[] =
        {
            // VALID PARAMS. Use default value
            {/*00*/ QUERY | INIT | RESET, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
            {
            { MFX_PAR, &tsStruct::mfxExtHEVCParam.LCUSize, 0 },
            { QUERY_EXP, &tsStruct::mfxExtHEVCParam.LCUSize, 0 },
            { INIT_EXP, &tsStruct::mfxExtHEVCParam.LCUSize, SetDefault }, //  Depends on HW....
            { RESET_PAR,  &tsStruct::mfxExtHEVCParam.LCUSize, 0 },
            { RESET_EXP,  &tsStruct::mfxExtHEVCParam.LCUSize, SetDefault } //  Depends on HW....
            } },

            // INVALID PARAMS #2
            {/*01*/ QUERY | INIT , MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_UNKNOWN, MFX_ERR_UNKNOWN,
            {
            { MFX_PAR, &tsStruct::mfxExtHEVCParam.LCUSize, 4 },
            { QUERY_EXP, &tsStruct::mfxExtHEVCParam.LCUSize, 0 },
            }},

            // INVALID PARAMS #3
            {/*02*/ QUERY | INIT , MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_UNKNOWN, MFX_ERR_UNKNOWN,
            {
                { MFX_PAR, &tsStruct::mfxExtHEVCParam.LCUSize, 0xFFFF },
                { QUERY_EXP, &tsStruct::mfxExtHEVCParam.LCUSize, 0 },
            } },

            // INVALID PARAMS #3
            {/*03*/ QUERY | INIT , MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_UNKNOWN, MFX_ERR_UNKNOWN,
            {
                { MFX_PAR, &tsStruct::mfxExtHEVCParam.LCUSize, 256 },// Unsupported
                { QUERY_EXP, &tsStruct::mfxExtHEVCParam.LCUSize, 0 },
            } },

            // VALID PARAMS. Min supported
            {/*04*/ QUERY | INIT | RESET | NEED_TWO_LCU_SUPPORTED, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
            {
                { MFX_PAR, &tsStruct::mfxExtHEVCParam.LCUSize, SetMinSupported },
                { QUERY_EXP, &tsStruct::mfxExtHEVCParam.LCUSize, SetMinSupported }, // not changed
                { INIT_EXP, &tsStruct::mfxExtHEVCParam.LCUSize, SetMinSupported }, //  not changed
            } },

            // VALID PARAMS. Max supported --  really duplicates test #0
            {/*05*/ QUERY | INIT | RESET | NEED_TWO_LCU_SUPPORTED, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
            {
                { MFX_PAR, &tsStruct::mfxExtHEVCParam.LCUSize, SetMaxSupported },
                { QUERY_EXP, &tsStruct::mfxExtHEVCParam.LCUSize, SetMaxSupported }, // not changed
                { INIT_EXP, &tsStruct::mfxExtHEVCParam.LCUSize, SetMaxSupported }, //  not changed
            } },

            // RESET FAIL PARAMS. can't change param
            {/*06*/ INIT | RESET | NEED_TWO_LCU_SUPPORTED, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE,
            {
                { MFX_PAR, &tsStruct::mfxExtHEVCParam.LCUSize, SetMinSupported },
                { INIT_EXP, &tsStruct::mfxExtHEVCParam.LCUSize, SetMinSupported }, //  not changed
                { RESET_PAR, &tsStruct::mfxExtHEVCParam.LCUSize, SetMaxSupported }, // can't be changed
            } },

            // RESET FAIL PARAMS. can't change param
            {/*07*/ INIT | RESET | NEED_TWO_LCU_SUPPORTED, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE,
            {
                { MFX_PAR, &tsStruct::mfxExtHEVCParam.LCUSize, SetMaxSupported },
                { INIT_EXP, &tsStruct::mfxExtHEVCParam.LCUSize, SetMaxSupported }, //  not changed
                { RESET_PAR, &tsStruct::mfxExtHEVCParam.LCUSize, SetMinSupported }, // can't be changed
            } },

            // RESET FAIL PARAMS. invalid value
            {/*08*/ INIT | RESET , MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE,
            {
                { MFX_PAR, &tsStruct::mfxExtHEVCParam.LCUSize, SetMaxSupported },
                { INIT_EXP, &tsStruct::mfxExtHEVCParam.LCUSize, SetMaxSupported }, //  not changed
                { RESET_PAR, &tsStruct::mfxExtHEVCParam.LCUSize, 0xFFFF }, // can't be changed
            } },
        };
        
        mfxI32 GetMinLCU(mfxU32 LCUSizeSupported)
        {
            if (LCUSizeSupported & 0b001)
                return 16;
            else if (LCUSizeSupported & 0b010)
                return 32;
            else if (LCUSizeSupported & 0b100)
                return 64;

            g_tsLog << "\n\nERROR: Unexpected value in CAPS.LCUSizeSupported\n\n";
            throw tsFAIL;

        }

        mfxI32 GetMaxLCU(mfxU32 LCUSizeSupported)
        {
            if (LCUSizeSupported & 0b100)
                return 64;
            else if (LCUSizeSupported & 0b010)
                return 32;
            else if (LCUSizeSupported & 0b001)
                return 16;

            g_tsLog << "\n\nERROR: Unexpected value in CAPS.LCUSizeSupported\n\n";
            throw tsFAIL;

        }

        mfxI32 GetDefaultLCU(mfxU32 LCUSizeSupported)
        {
            return GetMaxLCU(LCUSizeSupported);
        }

        void PrepareTestCase(tc_struct& tc, const ENCODE_CAPS_HEVC& caps)
        {
            for(mfxU32 i = 0; i < MAX_NPARS; i++)                                                                           
            {
                switch (tc.set_par[i].v)
                {
                case SetDefault:
                    tc.set_par[i].v = GetDefaultLCU(caps.LCUSizeSupported);
                    break;
                case SetMinSupported:
                    tc.set_par[i].v = GetMinLCU(caps.LCUSizeSupported);
                    break;
                case SetMaxSupported:
                    tc.set_par[i].v = GetMaxLCU(caps.LCUSizeSupported);
                    break;
                default:
                    // do nothing
                    break;
                }
            }

            if ((tc.type & NEED_TWO_LCU_SUPPORTED) && GetMinLCU(caps.LCUSizeSupported) == GetMaxLCU(caps.LCUSizeSupported))
            {
                g_tsLog << "\n\nWRN: Test Skipped only one LCU size supported\n\n";
                throw tsSKIP;
            }

        }

        const unsigned int n_cases = sizeof(test_case) / sizeof(test_case[0]);

        int test(unsigned int id, tsVideoEncoder& enc, const char* stream) {
            auto& tc = test_case[id];
            
            enc.MFXInit();
            enc.Load();

            ENCODE_CAPS_HEVC caps = {};
            mfxU32 capSize = sizeof(ENCODE_CAPS_HEVC);

            g_tsStatus.expect(MFX_ERR_NONE);
            g_tsStatus.check(enc.GetCaps(&caps, &capSize));

            if (g_tsHWtype < MFX_HW_CNL)
            {
                caps.LCUSizeSupported = 0b10;   // 32x32 lcu is only supported
            }

            //m_par = initParams();
            PrepareTestCase(tc, caps);

            
            if (tc.type & QUERY)
            {
                tsExtBufType<mfxVideoParam> out_par;
                SETPARS(enc.m_par, MFX_PAR);
                out_par = enc.m_par;
                enc.m_pParOut = &out_par;
                g_tsStatus.expect(tc.q_sts);
                enc.Query();
                EXPECT_TRUE(COMPAREPARS(out_par, QUERY_EXP)) << "Error! Parameters mismatch.";
            }

            if (tc.type & INIT)
            {
                tsExtBufType<mfxVideoParam> out_par;
                SETPARS(&enc.m_par, MFX_PAR);
                out_par = enc.m_par;
                enc.m_pParOut = &out_par;
                g_tsStatus.expect(tc.i_sts);
                enc.Init();
                if (tc.i_sts >= MFX_ERR_NONE)
                {
                    g_tsStatus.expect(MFX_ERR_NONE);
                    enc.GetVideoParam(enc.m_session, &out_par);
                    EXPECT_TRUE(COMPAREPARS(out_par, INIT_EXP)) << "Error! Parameters mismatch.";
                }
            }

            if (tc.type & ENC)
            {
                g_tsStatus.expect(tc.enc_sts);
                enc.EncodeFrames(1);
            }

            if (tc.type & RESET)
            {
                tsExtBufType<mfxVideoParam> out_par;
                SETPARS(&enc.m_par, RESET_PAR);
                out_par = enc.m_par;
                enc.m_pParOut = &out_par;
                g_tsStatus.expect(tc.r_sts);
                enc.Reset();
                if (tc.r_sts >= MFX_ERR_NONE)
                {
                    g_tsStatus.expect(MFX_ERR_NONE);
                    enc.GetVideoParam(enc.m_session, &out_par);
                    EXPECT_TRUE(COMPAREPARS(out_par, RESET_EXP)) << "Error! Parameters mismatch.";
                }
            }

            return 0;
        }


    int testNV12(unsigned int id)
    {
        TS_START;
        auto stream = g_tsStreamPool.Get("forBehaviorTest/foreman_cif.nv12");
        g_tsStreamPool.Reg();

        tsVideoEncoder enc(MFX_CODEC_HEVC);
        enc.m_par.mfx.FrameInfo.BitDepthLuma = 8;
        enc.m_par.mfx.FrameInfo.BitDepthChroma = 8;
        enc.m_par.mfx.FrameInfo.Shift = 0;
        enc.m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        enc.m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
        enc.m_par.mfx.FrameInfo.CropW = enc.m_par.mfx.FrameInfo.Width = 352;
        enc.m_par.mfx.FrameInfo.CropH = enc.m_par.mfx.FrameInfo.Height = 288;
        enc.m_par.mfx.FrameInfo.CropX = enc.m_par.mfx.FrameInfo.CropY = 0;

        return test(id, enc, stream);
        TS_END;

        return 0;
    }

    int testYUY2(unsigned int id)
    {
        TS_START;
        auto stream = g_tsStreamPool.Get("YUV8bit422/Kimono1_352x288_24_yuy2.yuv");
        g_tsStreamPool.Reg();

        tsVideoEncoder enc(MFX_CODEC_HEVC);
        enc.m_par.mfx.FrameInfo.BitDepthLuma = 8;
        enc.m_par.mfx.FrameInfo.BitDepthChroma = 8;
        enc.m_par.mfx.FrameInfo.Shift = 0;
        enc.m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
        enc.m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_YUY2;
        enc.m_par.mfx.FrameInfo.CropW = enc.m_par.mfx.FrameInfo.Width = 352;
        enc.m_par.mfx.FrameInfo.CropH = enc.m_par.mfx.FrameInfo.Height = 288;
        enc.m_par.mfx.FrameInfo.CropX = enc.m_par.mfx.FrameInfo.CropY = 0;

        return test(id, enc, stream);
        TS_END;

        return 0;
    }

    int testAYUV(unsigned int id)
    {
        TS_START;
        auto stream = g_tsStreamPool.Get("YUV8bit444/Kimono1_352x288_24_ayuv.yuv");
        g_tsStreamPool.Reg();

        tsVideoEncoder enc(MFX_CODEC_HEVC);
        enc.m_par.mfx.FrameInfo.BitDepthLuma = 8;
        enc.m_par.mfx.FrameInfo.BitDepthChroma = 8;
        enc.m_par.mfx.FrameInfo.Shift = 0;
        enc.m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
        enc.m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_AYUV;
        enc.m_par.mfx.FrameInfo.CropW = enc.m_par.mfx.FrameInfo.Width = 352;
        enc.m_par.mfx.FrameInfo.CropH = enc.m_par.mfx.FrameInfo.Height = 288;
        enc.m_par.mfx.FrameInfo.CropX = enc.m_par.mfx.FrameInfo.CropY = 0;

        return test(id, enc, stream);
        TS_END;

        return 0;
    }

    int testP010(unsigned int id)
    {
        TS_START;
        auto stream = g_tsStreamPool.Get("YUV10bit420/Kimono1_352x288_24_p010_shifted.yuv");
        g_tsStreamPool.Reg();

        tsVideoEncoder enc(MFX_CODEC_HEVC);
        enc.m_par.mfx.FrameInfo.BitDepthLuma = 10;
        enc.m_par.mfx.FrameInfo.BitDepthChroma = 10;
        enc.m_par.mfx.FrameInfo.Shift = 1;
        enc.m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        enc.m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_P010;
        enc.m_par.mfx.FrameInfo.CropW = enc.m_par.mfx.FrameInfo.Width = 352;
        enc.m_par.mfx.FrameInfo.CropH = enc.m_par.mfx.FrameInfo.Height = 288;
        enc.m_par.mfx.FrameInfo.CropX = enc.m_par.mfx.FrameInfo.CropY = 0;
        enc.m_par.mfx.QPI = enc.m_par.mfx.QPP = enc.m_par.mfx.QPB = 26 + 12;

        return test(id, enc, stream);
        TS_END;

        return 0;
    }


    int testY210(unsigned int id)
    {
        TS_START;
        auto stream = g_tsStreamPool.Get("YUV10bit422/Kimono1_352x288_24_y210.yuv");
        g_tsStreamPool.Reg();

        tsVideoEncoder enc(MFX_CODEC_HEVC);
        enc.m_par.mfx.FrameInfo.BitDepthLuma = 10;
        enc.m_par.mfx.FrameInfo.BitDepthChroma = 10;
        enc.m_par.mfx.FrameInfo.Shift = 1;
        enc.m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
        enc.m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y210;
        enc.m_par.mfx.FrameInfo.CropW = enc.m_par.mfx.FrameInfo.Width = 352;
        enc.m_par.mfx.FrameInfo.CropH = enc.m_par.mfx.FrameInfo.Height = 288;
        enc.m_par.mfx.FrameInfo.CropX = enc.m_par.mfx.FrameInfo.CropY = 0;
        enc.m_par.mfx.QPI = enc.m_par.mfx.QPP = enc.m_par.mfx.QPB = 26 + 12;

        return test(id, enc, stream);
        TS_END;

        return 0;
    }

    int testY410(unsigned int id)
    {
        TS_START;
        auto stream = g_tsStreamPool.Get("YUV10bit444/Kimono1_352x288_24_y410.yuv");
        g_tsStreamPool.Reg();

        tsVideoEncoder enc(MFX_CODEC_HEVC);
        enc.m_par.mfx.FrameInfo.BitDepthLuma = 10;
        enc.m_par.mfx.FrameInfo.BitDepthChroma = 10;
        enc.m_par.mfx.FrameInfo.Shift = 0;
        enc.m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
        enc.m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y410;
        enc.m_par.mfx.FrameInfo.CropW = enc.m_par.mfx.FrameInfo.Width = 352;
        enc.m_par.mfx.FrameInfo.CropH = enc.m_par.mfx.FrameInfo.Height = 288;
        enc.m_par.mfx.FrameInfo.CropX = enc.m_par.mfx.FrameInfo.CropY = 0;
        enc.m_par.mfx.QPI = enc.m_par.mfx.QPP = enc.m_par.mfx.QPB = 26 + 12;

        return test(id, enc, stream);
        TS_END;

        return 0;
    }

    int testP012(unsigned int id)
    {
        TS_START;
        auto stream = g_tsStreamPool.Get("YUV16bit420/FruitStall_176x144_240_p016.yuv");
        g_tsStreamPool.Reg();

        tsVideoEncoder enc(MFX_CODEC_HEVC);
        enc.m_par.mfx.FrameInfo.BitDepthLuma = 12;
        enc.m_par.mfx.FrameInfo.BitDepthChroma = 12;
        enc.m_par.mfx.FrameInfo.Shift = 1;
        enc.m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        enc.m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_P016;
        enc.m_par.mfx.FrameInfo.CropW = enc.m_par.mfx.FrameInfo.Width = 176;
        enc.m_par.mfx.FrameInfo.CropH = enc.m_par.mfx.FrameInfo.Height = 144;
        enc.m_par.mfx.FrameInfo.CropX = enc.m_par.mfx.FrameInfo.CropY = 0;
        enc.m_par.mfx.QPI = enc.m_par.mfx.QPP = enc.m_par.mfx.QPB = 26 + 24;

        return test(id, enc, stream);
        TS_END;

        return 0;
    }


    int testY212(unsigned int id)
    {
        TS_START;
        auto stream = g_tsStreamPool.Get("YUV16bit422/FruitStall_176x144_240_y216.yuv");
        g_tsStreamPool.Reg();

        tsVideoEncoder enc(MFX_CODEC_HEVC);
        enc.m_par.mfx.FrameInfo.BitDepthLuma = 12;
        enc.m_par.mfx.FrameInfo.BitDepthChroma = 12;
        enc.m_par.mfx.FrameInfo.Shift = 1;
        enc.m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
        enc.m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y216;
        enc.m_par.mfx.FrameInfo.CropW = enc.m_par.mfx.FrameInfo.Width = 176;
        enc.m_par.mfx.FrameInfo.CropH = enc.m_par.mfx.FrameInfo.Height = 144;
        enc.m_par.mfx.FrameInfo.CropX = enc.m_par.mfx.FrameInfo.CropY = 0;
        enc.m_par.mfx.QPI = enc.m_par.mfx.QPP = enc.m_par.mfx.QPB = 26 + 24;

        return test(id, enc, stream);
        TS_END;

        return 0;
    }

    int testY412(unsigned int id)
    {
        TS_START;
        auto stream = g_tsStreamPool.Get("YUV16bit444/FruitStall_176x144_240_y416.yuv");
        g_tsStreamPool.Reg();

        tsVideoEncoder enc(MFX_CODEC_HEVC);
        enc.m_par.mfx.FrameInfo.BitDepthLuma = 12;
        enc.m_par.mfx.FrameInfo.BitDepthChroma = 12;
        enc.m_par.mfx.FrameInfo.Shift = 1;
        enc.m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
        enc.m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y416;
        enc.m_par.mfx.FrameInfo.CropW = enc.m_par.mfx.FrameInfo.Width = 176;
        enc.m_par.mfx.FrameInfo.CropH = enc.m_par.mfx.FrameInfo.Height = 144;
        enc.m_par.mfx.FrameInfo.CropX = enc.m_par.mfx.FrameInfo.CropY = 0;
        enc.m_par.mfx.QPI = enc.m_par.mfx.QPP = enc.m_par.mfx.QPB = 26 + 24;

        return test(id, enc, stream);
        TS_END;

        return 0;
    }

    TS_REG_TEST_SUITE(hevce_8b_420_nv12_lcu, testNV12, sizeof(test_case) / sizeof(test_case[0]));
    //TS_REG_TEST_SUITE(hevce_8b_422_yuy2_lcu, testYUY2, sizeof(test_case) / sizeof(test_case[0]));
    //TS_REG_TEST_SUITE(hevce_8b_444_ayuv_lcu, testAYUV, sizeof(test_case) / sizeof(test_case[0]));
    //TS_REG_TEST_SUITE(hevce_10b_420_p010_lcu, testP010, sizeof(test_case) / sizeof(test_case[0]));
    //TS_REG_TEST_SUITE(hevce_10b_422_y210_lcu, testY210, sizeof(test_case) / sizeof(test_case[0]));
    //TS_REG_TEST_SUITE(hevce_10b_444_y410_lcu, testY410, sizeof(test_case) / sizeof(test_case[0]));
    //TS_REG_TEST_SUITE(hevce_12b_420_p016_lcu, testP012, sizeof(test_case) / sizeof(test_case[0]));
    //TS_REG_TEST_SUITE(hevce_12b_422_y216_lcu, testY212, sizeof(test_case) / sizeof(test_case[0]));
    //TS_REG_TEST_SUITE(hevce_12b_444_y416_lcu, testY412, sizeof(test_case) / sizeof(test_case[0]));
};
