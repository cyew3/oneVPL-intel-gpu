/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2019 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

/*
    Test verifies that application-defined weighted prediction table is written
    in bitstream. There's sanity quality check, but it can't verify that app WP parameters
    applied by encoder.
*/

#include "ts_encoder.h"
#include "ts_decoder.h"
#include "ts_struct.h"
#include "ts_parser.h"
#include <random>

//#define DEBUG

template<class T> inline T Clip3(T min, T max, T x) { return std::min<T>(std::max<T>(min, x), max); }

namespace hevce_explicit_weight_pred
{
    using namespace BS_HEVC;

    const mfxU8 L0 = 0, L1 = 1, Y = 0, Cb = 1, Cr = 2, Weight = 0, Offset = 1;
    const mfxU16 nframes_to_encode = 10;
    const mfxF64 PSNR_THRESHOLD = 30.0;

    enum
    {
        MFX_PAR = 1,
        EXT_COD3,
        MAX_ACT_REF = 4
    };
    enum
    {
        PREDEFINED  = 1,
        RAND        = 1 << 1
    };

    class PSNRVerifier : public tsSurfaceProcessor
    {
        std::unique_ptr<tsRawReader> m_reader;
        mfxFrameSurface1 m_ref_surf;
        mfxU32 m_frames;

    public:
        PSNRVerifier(mfxFrameSurface1& ref_surf, mfxU16 n_frames, const char* sn)
            : tsSurfaceProcessor()
        {
            m_reader.reset(new tsRawReader(sn, ref_surf.Info, n_frames));
            m_max = n_frames;
            m_ref_surf = ref_surf;
            m_frames = 0;
        }

        ~PSNRVerifier()
        {}

        mfxStatus ProcessSurface(mfxFrameSurface1& s)
        {
            m_reader->ProcessSurface(m_ref_surf);

            tsFrame ref_frame(m_ref_surf);
            tsFrame s_frame(s);
            mfxF64 psnr_y = PSNR(ref_frame, s_frame, 0);
            mfxF64 psnr_u = PSNR(ref_frame, s_frame, 1);
            mfxF64 psnr_v = PSNR(ref_frame, s_frame, 2);
            if ((psnr_y < PSNR_THRESHOLD) ||
                (psnr_u < PSNR_THRESHOLD) ||
                (psnr_v < PSNR_THRESHOLD))
            {
                g_tsLog << "ERROR: Low PSNR on frame " << m_frames << ": Y= " << psnr_y << " U=" << psnr_u << " V=" << psnr_v << "\n";
                return MFX_ERR_ABORTED;
            }
            m_frames++;
            return MFX_ERR_NONE;
        }

    };

    class TestSuite : public tsVideoEncoder, tsSurfaceProcessor, tsBitstreamProcessor, tsParserHEVC2
    {
    private:
        struct tc_struct
        {
            mfxU8 mode;
            struct
            {
                mfxU8 luma_denom;
                mfxI16 w_luma;
                mfxI16 o_luma;
            };
            mfxU16 L0_ref;
            mfxU16 L1_ref;
            struct f_pair
            {
                mfxU32 ext_type;
                const  tsStruct::Field* f;
                mfxU32 v;
            } set_par[MAX_NPARS];
        };
        static const tc_struct test_case[];


        mfxU8  m_mode;
        mfxU8  m_def_luma_denom;
        mfxI16 m_def_w_luma;
        mfxI16 m_def_o_luma;

        std::mt19937 m_Gen;
        std::unique_ptr <tsRawReader> m_reader;
        std::vector <tsExtBufType<mfxEncodeCtrl>> m_ctrls;
        mfxBitstream m_bs_copy;
#ifdef DEBUG
        tsBitstreamWriter m_writer;
#endif

    public:
        TestSuite()
            : tsVideoEncoder(MFX_CODEC_HEVC)
            , tsParserHEVC2()
            , m_mode(RAND)
            , m_bs_copy()
#ifdef DEBUG
            , m_writer("debug_wp.bin")
#endif
        {
            m_filler = this;
            m_bs_processor = this;

            m_par.AsyncDepth = 1;
            m_ctrls.resize(2 * nframes_to_encode);

            m_Gen.seed(0);
        }

        ~TestSuite()
        {
            if (m_bs_copy.Data != nullptr)
                delete[] m_bs_copy.Data;
        }

        static const unsigned int n_cases;
        template<mfxU32 fourcc>
        int RunTest(unsigned int id);

        mfxStatus ProcessSurface(mfxFrameSurface1& s);
        mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames);
        mfxI32    GetRandomNumber(mfxI32 min, mfxI32 max);
        void      GeneratePredTable(mfxExtPredWeightTable& pwt, mfxU8 num_l0, mfxU8 num_l1, bool gpb);
    };

    mfxI32 TestSuite::GetRandomNumber(mfxI32 min, mfxI32 max)
    {
        std::uniform_int_distribution<mfxI32> distr(min, max);
        return distr(m_Gen);
    }

    void TestSuite::GeneratePredTable(mfxExtPredWeightTable& pwt, mfxU8 num_l0, mfxU8 num_l1, bool gpb)
    {
        if (m_mode & PREDEFINED)
        {
            pwt.LumaLog2WeightDenom = m_def_luma_denom;
            for (mfxU8 ref = 0; ref < num_l0; ref++)
            {
                pwt.LumaWeightFlag[L0][ref] = 1;
                pwt.Weights[L0][ref][Y][Weight] = m_def_w_luma;
                pwt.Weights[L0][ref][Y][Offset] = m_def_o_luma;
            }
            for (mfxU8 ref = 0; ref < num_l1; ref++)
            {
                if (gpb)
                {
                    pwt.LumaWeightFlag[L1][ref] = pwt.LumaWeightFlag[L0][ref];
                    pwt.Weights[L1][ref][Y][Weight] = pwt.Weights[L0][ref][Y][Weight];
                    pwt.Weights[L1][ref][Y][Offset] = pwt.Weights[L0][ref][Y][Offset];
                }
                else
                {
                    pwt.LumaWeightFlag[L1][ref] = 1;
                    pwt.Weights[L1][ref][Y][Weight] = m_def_w_luma;
                    pwt.Weights[L1][ref][Y][Offset] = m_def_o_luma;
                }
            }
            return;
        }

        // driver's limitation: kernel has hard-coded luma denom=6, so others are not allowed
        pwt.LumaLog2WeightDenom = 6; // GetRandomNumber(0, 7);
        mfxI16 wY = 1 << pwt.LumaLog2WeightDenom;

        // HEVC encoder writes not weights itself, but delta = weight - (1 << luma_denom) and delta
        // should be in range [-128, 127]. See HEVC specification for details
        mfxI16 minYWeight = -128 + wY;
        mfxI16 maxYWeight = 127 + wY;
        // MSDK doesn't set high_precision_offsets_enabled_flag so 8-bit precision for offsets
        // for any formats. TODO: change 8 on BitDepth if will be enabled.
        mfxI16 WpOffsetHalfRangeY = 1 << (8 - 1);
        mfxI16 minOffsetY = -1 * WpOffsetHalfRangeY;
        mfxI16 maxOffsetY = WpOffsetHalfRangeY - 1;

        for (mfxU8 ref = 0; ref < num_l0; ref++)
        {
            pwt.LumaWeightFlag[L0][ref] = GetRandomNumber(0, 1);
            if (pwt.LumaWeightFlag[L0][ref] == 1)
            {
                pwt.Weights[L0][ref][Y][Weight] = GetRandomNumber(minYWeight, maxYWeight);
                pwt.Weights[L0][ref][Y][Offset] = GetRandomNumber(minOffsetY, maxOffsetY);
            }
            else
            {
                pwt.Weights[L0][ref][Y][Weight] = 0;
                pwt.Weights[L0][ref][Y][Offset] = 0;
            }
        }
        for (mfxU8 ref = 0; ref < num_l1; ref++)
        {
            if (gpb)
            {
                pwt.LumaWeightFlag[L1][ref] = pwt.LumaWeightFlag[L0][ref];
                pwt.Weights[L1][ref][Y][Weight] = pwt.Weights[L0][ref][Y][Weight];
                pwt.Weights[L1][ref][Y][Offset] = pwt.Weights[L0][ref][Y][Offset];
            }
            else
            {
                pwt.LumaWeightFlag[L1][ref] = GetRandomNumber(0, 1);
                if (pwt.LumaWeightFlag[L1][ref] == 1)
                {
                    pwt.Weights[L1][ref][Y][Weight] = GetRandomNumber(minYWeight, maxYWeight);
                    pwt.Weights[L1][ref][Y][Offset] = GetRandomNumber(minOffsetY, maxOffsetY);
                }
                else
                {
                    pwt.Weights[L1][ref][Y][Weight] = 0;
                    pwt.Weights[L1][ref][Y][Offset] = 0;
                }
            }
        }
    }

    mfxStatus TestSuite::ProcessSurface(mfxFrameSurface1& s)
    {
        mfxStatus sts = m_reader->ProcessSurface(s);

        s.Data.TimeStamp = s.Data.FrameOrder;


        if (s.Data.FrameOrder % m_par.mfx.GopPicSize != 0)
        {
            mfxExtPredWeightTable& pwt = m_ctrls[s.Data.FrameOrder];
            mfxExtCodingOption3& cod3 = m_par;
            mfxU8 nref_l0 = s.Data.FrameOrder < cod3.NumRefActiveP[0] ? s.Data.FrameOrder : cod3.NumRefActiveP[0];
            mfxU8 nref_l1 = s.Data.FrameOrder < cod3.NumRefActiveBL1[0] ? s.Data.FrameOrder : cod3.NumRefActiveBL1[0];
            bool pframe = !!(s.Data.FrameOrder % m_par.mfx.GopRefDist == 0);
            bool gpb = !!(pframe && cod3.GPB != MFX_CODINGOPTION_OFF);
            GeneratePredTable(pwt, nref_l0, ((!pframe || gpb) ? nref_l1 : 0), gpb);

 #ifdef DEBUG
             printf("Frame #%d\n", s.Data.FrameOrder);
             printf("LumaLog2Denom %d\n", pwt.LumaLog2WeightDenom);
             printf("ChromaLog2Denom %d\n", pwt.ChromaLog2WeightDenom);
             printf("L0:\n");
             for (mfxU8 ref = 0; ref < 4; ref++)
             {
                 printf("  %d(%d): WY=%d, OY=%d, WCb=%d, OCb=%d, WCr=%d, OCr=%d\n", ref, pwt.LumaWeightFlag[L0][ref],
                         pwt.Weights[L0][ref][Y][Weight], pwt.Weights[L0][ref][Y][Offset],
                         pwt.Weights[L0][ref][Cb][Weight], pwt.Weights[L0][ref][Cb][Offset],
                         pwt.Weights[L0][ref][Cr][Weight], pwt.Weights[L0][ref][Cr][Offset]);
             }
             printf("L1:\n");
             for (mfxU8 ref = 0; ref < 2; ref++)
             {
                 printf("  %d(%d): WY=%d, OY=%d, WCb=%d, OCb=%d, WCr=%d, OCr=%d\n", ref, pwt.LumaWeightFlag[L1][ref],
                         pwt.Weights[L1][ref][Y][Weight], pwt.Weights[L1][ref][Y][Offset],
                         pwt.Weights[L1][ref][Cb][Weight], pwt.Weights[L1][ref][Cb][Offset],
                         pwt.Weights[L1][ref][Cr][Weight], pwt.Weights[L1][ref][Cr][Offset]);
             }
 #endif
         }

        m_pCtrl = &m_ctrls[s.Data.FrameOrder];

        return sts;
    }


    mfxStatus TestSuite::ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        mfxStatus sts = MFX_ERR_NONE;
        SetBuffer0(bs);

        if (m_bs_copy.MaxLength >= m_bs_copy.DataLength + bs.DataLength)
        {
            memcpy(&m_bs_copy.Data[m_bs_copy.DataLength], &bs.Data[bs.DataOffset], bs.DataLength);
            m_bs_copy.DataLength += bs.DataLength;
        }
        else
            return MFX_ERR_ABORTED;

#ifdef DEBUG
        sts = m_writer.ProcessBitstream(bs, nFrames);
#endif

        g_tsLog << "Check frame #" << bs.TimeStamp << "\n";
        mfxExtPredWeightTable& exp_pwt = m_ctrls[bs.TimeStamp];
        mfxU32 checked = 0;
        while (checked++ < nFrames)
        {
            auto& hdr = ParseOrDie();

            for (auto pNALU = &hdr; pNALU; pNALU = pNALU->next)
            {
                if (!IsHEVCSlice(pNALU->nal_unit_type))
                    continue;

                if (pNALU->slice == nullptr)
                {
                    g_tsLog << "ERROR: pNALU->slice is null in BS parsing\n";
                    g_tsStatus.check(MFX_ERR_ABORTED);
                }

                auto& slice = *pNALU->slice;

                if (slice.type == I)
                    continue;

                if (slice.pps != nullptr)
                {
                    EXPECT_EQ((mfxU8)1, slice.pps->weighted_pred_flag);
                    EXPECT_EQ((mfxU8)1, slice.pps->weighted_bipred_flag);
                }

                if (slice.pwt == nullptr)
                {
                    g_tsLog << "ERROR: predWeightTable is null in BS parsing\n";
                    g_tsStatus.check(MFX_ERR_ABORTED);
                }

                auto pwt  = slice.pwt;

                EXPECT_EQ(exp_pwt.LumaLog2WeightDenom, slice.luma_log2_weight_denom);
                EXPECT_EQ(exp_pwt.ChromaLog2WeightDenom, slice.chroma_log2_weight_denom);

                for (mfxU8 lx = 0; lx < (slice.type == B ? 2 : 1); lx++) // L0, L1
                {
                    mfxU8 check_lx = lx;
                    if (lx == 1 && slice.L0 != nullptr && slice.L1 != nullptr &&
                        slice.L0[0].POC == slice.L1[0].POC) // GPB frame
                        check_lx = 0;
                    for (mfxU8 ref = 0; ref < (lx ? slice.num_ref_idx_l1_active : slice.num_ref_idx_l0_active); ref++)
                    {
                        g_tsLog << "Check List" << lx << " for reference #" << ref << "\n";
                        if (exp_pwt.LumaWeightFlag[check_lx][ref])
                        {
                            EXPECT_EQ(exp_pwt.Weights[check_lx][ref][Y][Weight], pwt[lx][ref][Y][Weight]);
                            EXPECT_EQ(exp_pwt.Weights[check_lx][ref][Y][Offset], pwt[lx][ref][Y][Offset]);
                        }
                        else
                        {
                            EXPECT_EQ(1 << slice.luma_log2_weight_denom, pwt[lx][ref][Y][Weight]);
                            EXPECT_EQ(0, pwt[lx][ref][Y][Offset]);
                        }
                        if (exp_pwt.ChromaWeightFlag[check_lx][ref])
                        {
                            EXPECT_EQ(exp_pwt.Weights[check_lx][ref][Cb][Weight], pwt[lx][ref][Cb][Weight]);
                            EXPECT_EQ(exp_pwt.Weights[check_lx][ref][Cb][Offset], pwt[lx][ref][Cb][Offset]);
                            EXPECT_EQ(exp_pwt.Weights[check_lx][ref][Cr][Weight], pwt[lx][ref][Cr][Weight]);
                            EXPECT_EQ(exp_pwt.Weights[check_lx][ref][Cr][Offset], pwt[lx][ref][Cr][Offset]);
                        }
                        else
                        {
                            EXPECT_EQ(1 << slice.chroma_log2_weight_denom, pwt[lx][ref][Cb][Weight]);
                            EXPECT_EQ(0, pwt[lx][ref][Cb][Offset]);
                            EXPECT_EQ(1 << slice.chroma_log2_weight_denom, pwt[lx][ref][Cr][Weight]);
                            EXPECT_EQ(0, pwt[lx][ref][Cr][Offset]);
                        }
                    } // for ref
                } // for lx

            }
        }

        bs.DataLength = 0;
        return sts;
    }

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        {/*00*/ PREDEFINED, {6, 60, -128}, MAX_ACT_REF, MAX_ACT_REF, { {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
                                                                       {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 10}}
        },
        {/*00*/ PREDEFINED, {6, -34, 127}, 2, 1, { {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 3},
                                                   {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 10}}
        },
        {/*00*/ PREDEFINED, {6, 191, 0}, 2, 1, { {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 3},
                                                 {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 10},
                                                 {EXT_COD3, &tsStruct::mfxExtCodingOption3.GPB, MFX_CODINGOPTION_OFF} }
        },
        {/*00*/ RAND, {0, 0, 0},  2, 1, { {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 3},
                                          {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 10}}
        }
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    struct StreamDescr
    {
        mfxU16 w;
        mfxU16 h;
        const char *name;
        mfxU16 chroma_format;
        mfxU16 bd;
        mfxU16 shift;
    };

    const StreamDescr streams[] = {
        { 352, 288, "forBehaviorTest/foreman_cif.nv12",                   MFX_CHROMAFORMAT_YUV420, 8,  0},
        { 352, 288, "YUV8bit422/Kimono1_352x288_24_yuy2.yuv",             MFX_CHROMAFORMAT_YUV422, 8,  0},
        { 352, 288, "YUV10bit420_ms/Kimono1_352x288_24_p010_shifted.yuv", MFX_CHROMAFORMAT_YUV420, 10, 1},
        { 352, 288, "YUV10bit422/Kimono1_352x288_24_y210.yuv",            MFX_CHROMAFORMAT_YUV422, 10, 1},
        { 176, 144, "YUV16bit420/FruitStall_176x144_240_p016.yuv",        MFX_CHROMAFORMAT_YUV420, 12, 1},
        { 176, 144, "YUV16bit422/FruitStall_176x144_240_y216.yuv",        MFX_CHROMAFORMAT_YUV422, 12, 1},
    };

    const StreamDescr& get_stream_descr(const mfxU32& fourcc)
    {
        switch(fourcc)
        {
            case MFX_FOURCC_NV12:   return streams[0];
            case MFX_FOURCC_YUY2:   return streams[1];
            case MFX_FOURCC_P010:   return streams[2];
            case MFX_FOURCC_Y210:   return streams[3];
            case GMOCK_FOURCC_P012: return streams[4];
            case GMOCK_FOURCC_Y212: return streams[5];

            default: assert(0); return streams[0];
        }
    }

    template<mfxU32 fourcc>
    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;
        auto& tc = test_case[id];

        m_mode = tc.mode;
        m_def_luma_denom = tc.luma_denom;
        m_def_w_luma = tc.w_luma;
        m_def_o_luma = tc.o_luma;

        SETPARS(&m_par, MFX_PAR);
        mfxExtCodingOption3& cod3 = m_par;
        SETPARS(&cod3, EXT_COD3);

        if (tc.L0_ref == MAX_ACT_REF && tc.L1_ref == MAX_ACT_REF)
        {
            ENCODE_CAPS_HEVC caps = {};
            mfxU32 caps_size = sizeof(ENCODE_CAPS_HEVC);
            g_tsStatus.check(GetCaps(&caps, &caps_size));
            std::fill(std::begin(cod3.NumRefActiveP),   std::end(cod3.NumRefActiveP),   caps.MaxNum_Reference0);
            std::fill(std::begin(cod3.NumRefActiveBL0), std::end(cod3.NumRefActiveBL0), caps.MaxNum_Reference0);
            std::fill(std::begin(cod3.NumRefActiveBL1), std::end(cod3.NumRefActiveBL1), caps.MaxNum_Reference1);
        }
        else
        {
            std::fill(std::begin(cod3.NumRefActiveP),   std::end(cod3.NumRefActiveP),   tc.L0_ref);
            std::fill(std::begin(cod3.NumRefActiveBL0), std::end(cod3.NumRefActiveBL0), tc.L0_ref);
            std::fill(std::begin(cod3.NumRefActiveBL1), std::end(cod3.NumRefActiveBL1), tc.L1_ref);
        }

        if (cod3.GPB == MFX_CODINGOPTION_OFF && (g_tsOSFamily == MFX_OS_FAMILY_WINDOWS || m_par.mfx.LowPower == MFX_CODINGOPTION_ON))
        {
            g_tsLog << "\n\nWARNING: HEVCe does not support P-slices on VDEnc and on Windows!\n\n\n";
            throw tsSKIP;
        }

        const StreamDescr& yuv_descr = get_stream_descr(fourcc);
        m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = yuv_descr.w;
        m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = yuv_descr.h;
        m_par.mfx.FrameInfo.ChromaFormat = yuv_descr.chroma_format;
        m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = yuv_descr.bd;
        m_par.mfx.FrameInfo.Shift = yuv_descr.shift;
        m_par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
        m_par.mfx.QPI = m_par.mfx.QPP = m_par.mfx.QPB = 26 + (m_par.mfx.FrameInfo.BitDepthLuma - 8) * 6;
        switch(fourcc)
        {
            case GMOCK_FOURCC_P012: m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_P016; break;
            case GMOCK_FOURCC_Y212: m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y216; break;
            case GMOCK_FOURCC_Y412: m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y416; break;

            default: m_par.mfx.FrameInfo.FourCC = fourcc;
        }

        auto stream = g_tsStreamPool.Get(yuv_descr.name);
        m_reader.reset(new tsRawReader(stream, m_par.mfx.FrameInfo));
        m_reader->m_disable_shift_hack = true; // disable shift for 10b+ streams
        g_tsStreamPool.Reg();

        // save bs for PSNR check below
        m_bs_copy.MaxLength = m_par.mfx.FrameInfo.Width * m_par.mfx.FrameInfo.Height * nframes_to_encode * 3; // 3 - magic number :)
        m_bs_copy.Data = new mfxU8[m_bs_copy.MaxLength];

        MFXInit();

        // call Query to let MSDK correct NumRefActiveP/BL0/BL1 according to TU, VDEnc/VME, etc.
        g_tsStatus.disable_next_check();
        Query(m_session, m_pPar, m_pPar);

        cod3.WeightedPred = MFX_WEIGHTED_PRED_EXPLICIT;
        cod3.WeightedBiPred = MFX_WEIGHTED_PRED_EXPLICIT;

        Init();
        EncodeFrames(nframes_to_encode);
        DrainEncodedBitstream();

        // sanity check of the encoded stream's quality
        tsVideoDecoder dec(MFX_CODEC_HEVC);
        tsBitstreamReader reader(m_bs_copy, m_bs_copy.MaxLength);
        dec.m_bs_processor = &reader;

        // Borrow one surface from encoder which is already done execution.
        // This surface will be used in verifier only.
        mfxFrameSurface1 *ref = GetSurface();
        PSNRVerifier psnr_ver(*ref, nframes_to_encode, stream);
        dec.m_surf_processor = &psnr_ver;

        dec.Init();
        dec.AllocSurfaces();
        dec.DecodeFrames(nframes_to_encode);
        dec.Close();

        Close(); // encode

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_8b_420_nv12_explicit_weight_pred,  RunTest<MFX_FOURCC_NV12>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_420_p010_explicit_weight_pred, RunTest<MFX_FOURCC_P010>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_12b_420_p016_explicit_weight_pred, RunTest<GMOCK_FOURCC_P012>, n_cases);

    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_8b_422_yuy2_explicit_weight_pred,  RunTest<MFX_FOURCC_YUY2>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_422_y210_explicit_weight_pred, RunTest<MFX_FOURCC_Y210>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_12b_422_y216_explicit_weight_pred, RunTest<GMOCK_FOURCC_Y212>, n_cases);
};
