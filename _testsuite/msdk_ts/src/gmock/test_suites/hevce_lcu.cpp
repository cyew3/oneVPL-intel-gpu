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
//#define DEBUG_STREAM

namespace hevce_lcu
{
    using namespace BS_HEVC2;

    class BitstreamChecker : public tsBitstreamProcessor, public tsParserHEVC2
    {
    public:
        mfxU16 log2_min_luma_coding_block_size_minus3;
        mfxU16 log2_diff_max_min_luma_coding_block_size;
        mfxU16 MinCbLog2SizeY;
        mfxU16 CtbLog2SizeY;
        mfxU16 CtbSizeY;
        mfxU16 TrueLCUSize;
#ifdef DEBUG_STREAM
        tsBitstreamWriter m_bsw;
#endif
        BitstreamChecker()
            : tsParserHEVC2(PARSE_SSD)
#ifdef DEBUG_STREAM
            , m_bsw("hevce_lcu.265")
#endif
        {
            set_trace_level(0);
        }
        mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames);
    };
    mfxStatus BitstreamChecker::ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        mfxU32 checked = 0;

        if (bs.Data)
            SetBuffer0(bs);
        mfxStatus sts = MFX_ERR_NONE;
#ifdef DEBUG_STREAM
        m_bsw.ProcessBitstream(bs, nFrames);
#endif

        while (checked++ < nFrames)
        {
            auto& hdr = ParseOrDie();

            for (auto pNALU = &hdr; pNALU; pNALU = pNALU->next)
            {
                if (SPS_NUT != pNALU->nal_unit_type)
                    continue;

                if (!pNALU->sps)
                {
                    g_tsLog << "\n\nERROR:Failed to get the values of pNALU->sps\n\n";
                    throw tsFAIL;
                }

                auto& sps = *pNALU->sps;
                MinCbLog2SizeY = sps.log2_min_luma_coding_block_size_minus3 + 3;
                CtbLog2SizeY = MinCbLog2SizeY + sps.log2_diff_max_min_luma_coding_block_size;
                CtbSizeY = 1 << CtbLog2SizeY;

                if (TrueLCUSize != CtbSizeY)
                {
                    g_tsLog << "\n\nERROR:LCUSize at initialization does not coincide with the value at encoding\n\n";
                    throw tsFAIL;
                }

            }
        }

        bs.DataLength = 0;
        return MFX_ERR_NONE;
    };

    /*

    Description:
    This suite tests HEVC encoder\n

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
        INIT_EXP_SW = 6,
        RESET_EXP_SW = 7,
        QUERY_EXP_SW = 8,
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
        //! Expected Query() status with SW plugin
        mfxStatus q_sw_sts;
        //! Expected Init() status with SW plugin
        mfxStatus i_sw_sts;
        //! Expected Reset() status with SW plugin
        mfxStatus r_sw_sts;
        //! Expected EncodeFrames() status with SW plugin
        mfxStatus enc_sw_sts;
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
        {/*00*/ QUERY | INIT | RESET | ENC, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxExtHEVCParam.LCUSize, 0 },
            { QUERY_EXP, &tsStruct::mfxExtHEVCParam.LCUSize, 0 },
            { INIT_EXP, &tsStruct::mfxExtHEVCParam.LCUSize, SetDefault }, //  Depends on HW....
            { ENC_EXP, &tsStruct::mfxExtHEVCParam.LCUSize },
            { RESET_PAR,  &tsStruct::mfxExtHEVCParam.LCUSize, 0 },
            { RESET_EXP,  &tsStruct::mfxExtHEVCParam.LCUSize, SetDefault }, //  Depends on HW....
            { INIT_EXP_SW, &tsStruct::mfxExtHEVCParam.LCUSize, 0 },
            { RESET_EXP_SW, &tsStruct::mfxExtHEVCParam.LCUSize, 0 },
        } },

        // INVALID PARAMS #1
        {/*01*/ QUERY | INIT , MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_UNKNOWN, MFX_ERR_UNKNOWN, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxExtHEVCParam.LCUSize, 7 },
            { QUERY_EXP, &tsStruct::mfxExtHEVCParam.LCUSize, 0 },
            { QUERY_EXP_SW, &tsStruct::mfxExtHEVCParam.LCUSize, 0 },
        } },

        // INVALID PARAMS #2
        {/*02*/ QUERY | INIT , MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_UNKNOWN, MFX_ERR_UNKNOWN, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxExtHEVCParam.LCUSize, 0xFFFF },
            { QUERY_EXP, &tsStruct::mfxExtHEVCParam.LCUSize, 0 },
        } },

        // INVALID PARAMS #3
        {/*03*/ QUERY | INIT , MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_UNKNOWN, MFX_ERR_UNKNOWN, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxExtHEVCParam.LCUSize, 256 },// Unsupported
            { QUERY_EXP, &tsStruct::mfxExtHEVCParam.LCUSize, 0 },
        } },

        // VALID PARAMS. Min supported
        {/*04*/ QUERY | INIT | ENC | NEED_TWO_LCU_SUPPORTED, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxExtHEVCParam.LCUSize, SetMinSupported },
            { QUERY_EXP, &tsStruct::mfxExtHEVCParam.LCUSize, SetMinSupported }, // not changed
            { INIT_EXP, &tsStruct::mfxExtHEVCParam.LCUSize, SetMinSupported }, //  not changed
            { ENC_EXP, &tsStruct::mfxExtHEVCParam.LCUSize },
            { INIT_EXP_SW, &tsStruct::mfxExtHEVCParam.LCUSize, 0 },
        } },


        // VALID PARAMS. Max supported --  really duplicates test #0
        {/*05*/ QUERY | INIT | ENC | NEED_TWO_LCU_SUPPORTED, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxExtHEVCParam.LCUSize, SetMaxSupported },
            { QUERY_EXP, &tsStruct::mfxExtHEVCParam.LCUSize, SetMaxSupported }, // not changed
            { INIT_EXP, &tsStruct::mfxExtHEVCParam.LCUSize, SetMaxSupported }, //  not changed
            { ENC_EXP, &tsStruct::mfxExtHEVCParam.LCUSize },
            { INIT_EXP_SW, &tsStruct::mfxExtHEVCParam.LCUSize, 0 },
            { QUERY_EXP_SW, &tsStruct::mfxExtHEVCParam.LCUSize, 0 },
        } },

        // RESET FAIL PARAMS. can't change param
        {/*06*/ INIT | RESET | NEED_TWO_LCU_SUPPORTED, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxExtHEVCParam.LCUSize, SetMinSupported },
            { INIT_EXP, &tsStruct::mfxExtHEVCParam.LCUSize, SetMinSupported }, //  not changed
            { RESET_PAR, &tsStruct::mfxExtHEVCParam.LCUSize, SetMaxSupported }, // can't be changed
            { ENC_EXP, &tsStruct::mfxExtHEVCParam.LCUSize },
            { INIT_EXP_SW, &tsStruct::mfxExtHEVCParam.LCUSize, 0 },
        } },

        // RESET FAIL PARAMS. can't change param
        {/*07*/ INIT | RESET | ENC | NEED_TWO_LCU_SUPPORTED, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxExtHEVCParam.LCUSize, SetMaxSupported },
            { INIT_EXP, &tsStruct::mfxExtHEVCParam.LCUSize, SetMaxSupported }, //  not changed
            { RESET_PAR, &tsStruct::mfxExtHEVCParam.LCUSize, SetMinSupported }, // can't be changed
            { ENC_EXP, &tsStruct::mfxExtHEVCParam.LCUSize },
            { INIT_EXP_SW, &tsStruct::mfxExtHEVCParam.LCUSize, 0 },
        } },

        // RESET FAIL PARAMS. invalid value
        {/*08*/ INIT | RESET , MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE,
        {
            { MFX_PAR, &tsStruct::mfxExtHEVCParam.LCUSize, SetMaxSupported },
            { INIT_EXP, &tsStruct::mfxExtHEVCParam.LCUSize, SetMaxSupported }, //  not changed
            { RESET_PAR, &tsStruct::mfxExtHEVCParam.LCUSize, 0xFFFF }, // can't be changed
            { RESET_EXP, &tsStruct::mfxExtHEVCParam.LCUSize, SetDefault },
            { RESET_EXP_SW, &tsStruct::mfxExtHEVCParam.LCUSize, 0xFFFF },
        } },
    };

    class SProc : public tsSurfaceProcessor
    {
    public:
        tsSurfaceProcessor& m_reader;
        tsVideoEncoder& m_enc;
        const tc_struct& m_tc;

        SProc(tsSurfaceProcessor& r, const tc_struct& tc, tsVideoEncoder& enc)
            : m_reader(r)
            , m_enc(enc)
            , m_tc(tc)
        {
            m_max = m_reader.m_max;
        }

        mfxStatus ProcessSurface(mfxFrameSurface1& s)
        {
            auto& ctrl = *m_enc.m_ctrl_next.New();
            mfxExtHEVCParam& hp = ctrl;
            hp = (mfxExtHEVCParam&)m_enc.m_par;
            return m_reader.ProcessSurface(s);
        }
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
        for (mfxU32 i = 0; i < MAX_NPARS; i++)
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
    const mfxU32 nFrames = 30;
    int test(unsigned int id, tsVideoEncoder& enc, const char* stream) {

        auto& tc = test_case[id];
        mfxExtHEVCParam& hp = enc.m_par;
        BitstreamChecker checker;
        tsRawReader reader(stream, enc.m_par.mfx.FrameInfo, nFrames);
        SProc sproc(reader, tc, enc);
        reader.m_disable_shift_hack = true; // w/a stupid hack in tsRawReader for P010
        enc.m_filler = &sproc;
        enc.MFXInit();
        enc.Load();
        ENCODE_CAPS_HEVC caps = {};
        mfxU32 capSize = sizeof(ENCODE_CAPS_HEVC);
        g_tsStatus.expect(0 == memcmp(enc.m_uid->Data, MFX_PLUGINID_HEVCE_HW.Data, sizeof(MFX_PLUGINID_HEVCE_HW.Data)) ? MFX_ERR_NONE : MFX_ERR_UNSUPPORTED);
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
            g_tsStatus.expect(0 == memcmp(enc.m_uid->Data, MFX_PLUGINID_HEVCE_HW.Data, sizeof(MFX_PLUGINID_HEVCE_HW.Data)) ? tc.q_sts : tc.q_sw_sts);
            enc.Query();
            if (0 == memcmp(enc.m_uid->Data, MFX_PLUGINID_HEVCE_HW.Data, sizeof(MFX_PLUGINID_HEVCE_HW.Data))) //In the case of SW, the exbuffer is not used and does not change. COMPAREPARS does not make sense.
                EXPECT_TRUE(COMPAREPARS(out_par, QUERY_EXP)) << "Error! Parameters mismatch.";
        }

        if (tc.type & INIT)
        {
            tsExtBufType<mfxVideoParam> out_par;
            SETPARS(&enc.m_par, MFX_PAR);
            out_par = enc.m_par;
            enc.m_pParOut = &out_par;
            g_tsStatus.expect(0 == memcmp(enc.m_uid->Data, MFX_PLUGINID_HEVCE_HW.Data, sizeof(MFX_PLUGINID_HEVCE_HW.Data)) ? tc.i_sts : tc.i_sw_sts);
            enc.Init();
            if ((0 == memcmp(enc.m_uid->Data, MFX_PLUGINID_HEVCE_HW.Data, sizeof(MFX_PLUGINID_HEVCE_HW.Data)) ? tc.i_sts : tc.i_sw_sts) >= MFX_ERR_NONE)
            {
                g_tsStatus.expect(MFX_ERR_NONE);
                enc.GetVideoParam(enc.m_session, &out_par);
                EXPECT_TRUE(COMPAREPARS(out_par, 0 == memcmp(enc.m_uid->Data, MFX_PLUGINID_HEVCE_HW.Data, sizeof(MFX_PLUGINID_HEVCE_HW.Data)) ? INIT_EXP : INIT_EXP_SW)) << "Error! Parameters mismatch.";

                if (tc.type & ENC)
                {
                    checker.TrueLCUSize = hp.LCUSize;
                    if (!checker.TrueLCUSize)
                    {
                        checker.TrueLCUSize = 0 == memcmp(enc.m_uid->Data, MFX_PLUGINID_HEVCE_HW.Data, sizeof(MFX_PLUGINID_HEVCE_HW.Data)) ? GetDefaultLCU(caps.LCUSizeSupported) : 64;
                    }
                    enc.m_bs_processor = &checker;
                    g_tsStatus.expect(0 == memcmp(enc.m_uid->Data, MFX_PLUGINID_HEVCE_HW.Data, sizeof(MFX_PLUGINID_HEVCE_HW.Data)) ? tc.enc_sts : tc.enc_sw_sts);
                    enc.EncodeFrames(nFrames);
                }
            }
        }

        if (tc.type & RESET)
        {
            tsExtBufType<mfxVideoParam> out_par;
            SETPARS(&enc.m_par, RESET_PAR);
            out_par = enc.m_par;
            enc.m_pParOut = &out_par;
            g_tsStatus.expect(0 == memcmp(enc.m_uid->Data, MFX_PLUGINID_HEVCE_HW.Data, sizeof(MFX_PLUGINID_HEVCE_HW.Data)) ? tc.r_sts : tc.r_sw_sts);
            enc.Reset();
            if (tc.r_sw_sts >= MFX_ERR_NONE)
            {
                g_tsStatus.expect(MFX_ERR_NONE);
                enc.GetVideoParam(enc.m_session, &out_par);
                EXPECT_TRUE(COMPAREPARS(out_par, 0 == memcmp(enc.m_uid->Data, MFX_PLUGINID_HEVCE_HW.Data, sizeof(MFX_PLUGINID_HEVCE_HW.Data)) ? RESET_EXP : RESET_EXP_SW)) << "Error! Parameters mismatch.";
                //g_tsStatus.check(enc.DrainEncodedBitstream());
                if (tc.type & ENC)
                {
                    checker.TrueLCUSize = hp.LCUSize;
                    if (!checker.TrueLCUSize)
                    {
                        checker.TrueLCUSize = (0 == memcmp(enc.m_uid->Data, MFX_PLUGINID_HEVCE_HW.Data, sizeof(MFX_PLUGINID_HEVCE_HW.Data)) ? GetDefaultLCU(caps.LCUSizeSupported) : 64);
                    }
                    enc.m_bs_processor = &checker;
                    g_tsStatus.expect(0 == memcmp(enc.m_uid->Data, MFX_PLUGINID_HEVCE_HW.Data, sizeof(MFX_PLUGINID_HEVCE_HW.Data)) ? tc.enc_sts : tc.enc_sw_sts);
                    enc.EncodeFrames(nFrames);
                }
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
        auto stream = g_tsStreamPool.Get("YUV10bit420_ms/Kimono1_352x288_24_p010_shifted.yuv");
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
