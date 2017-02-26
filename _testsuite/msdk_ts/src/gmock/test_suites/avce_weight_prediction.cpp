/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_decoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

//#define DEBUG

using namespace BS_AVC2;


namespace avce_weight_prediction
{

    enum
    {
        MFX_PAR = 1,
    };
    enum
    {
        EXP_WEIGHT_PEREDICTION = 2, // init by ExtCodingOption3
        IMP_WEIGHT_PREDICTION = 3,
        DEFAULT_WEIGHT_PREDICTION_B = 4, // init by mfx paramerers
        NONE = 0,
    };
    const mfxU16 NumFrames = 30;
    const mfxF64 PSNR_THRESHOLD = 30;
    const mfxF64 SSIM_THRESHOLD = 0.8;

    mfxI32 logWC_luma = 6;
    mfxI32 logWC_chroma = 0;
    mfxI32 w_luma = 64;
    mfxI32 w_chroma = 1;
    mfxI32 o_luma = 7;
    mfxI32 o_chroma = 0;

    const char* stream;

    void CorruptSurface(mfxFrameSurface1& _s)
    {
        tsFrameNV12 frame(_s.Data);
        for (mfxU32 i = 0; i < _s.Info.Height; i++)
            for (mfxU32 j = 0; j < _s.Info.Width; j++)
            {
                if (logWC_luma >= 1)
                    frame.Y(j, i) = ((frame.Y(j, i) * w_luma + (1 << (logWC_luma - 1))) >> logWC_luma) + o_luma;
                else
                    frame.Y(j, i) = frame.Y(j, i) * w_luma + o_luma;
            }
        for (mfxU32 i = 0; i < _s.Info.Height / 2; i++)
            for (mfxU32 j = 0; j < _s.Info.Width; j++)
            {
                if (logWC_chroma >= 1)
                {
                    frame.Y(j, i) = ((frame.Y(j, i) * w_chroma + (1 << (logWC_chroma - 1))) >> logWC_chroma) + o_chroma;
                }
                else
                {
                    frame.Y(j, i) = frame.Y(j, i) * w_chroma + o_chroma;
                }

            }
    }

    class Verifier : public tsSurfaceProcessor
    {
    public:
        tsRawReader* m_reader;
        Verifier(mfxFrameSurface1& _ref):
            tsSurfaceProcessor()
            ,m_reader(0)
        {
            m_reader = new tsRawReader(stream, _ref.Info, NumFrames);
            ref = _ref;
            frames = 0;
        }
        ~Verifier()
        {
            if (m_reader)
                delete m_reader;
        }

        mfxFrameSurface1 ref;
        mfxU32 frames;

        mfxStatus ProcessSurface(mfxFrameSurface1& s)
        {
            if (m_reader)
                delete m_reader;
            m_reader = new tsRawReader(stream, ref.Info, NumFrames);
            m_reader->ProcessSurface(ref);
            if ((frames > 0) && ((frames % 2) != 0))
                CorruptSurface(ref);

            tsFrame ref_frame(ref);
            tsFrame s_frame(s);
            mfxF64 psnr_y = PSNR(ref_frame, s_frame, 0);
            mfxF64 psnr_u = PSNR(ref_frame, s_frame, 1);
            mfxF64 psnr_v = PSNR(ref_frame, s_frame, 2);
            if ((psnr_y < PSNR_THRESHOLD) ||
                (psnr_u < PSNR_THRESHOLD) ||
                (psnr_v < PSNR_THRESHOLD))
            {
                g_tsLog << "ERROR: Low PSNR on frame " << frames << "\n";
                return MFX_ERR_ABORTED;
            }
            frames++;
            return MFX_ERR_NONE;
        }
    };


    class TestSuite
        : public tsVideoEncoder
        , public tsSurfaceProcessor
        , public tsBitstreamProcessor
        , public tsParserAVC2
    {
    private:
        struct tc_struct
        {
            mfxStatus sts;
            mfxU16 WeightPredP;
            mfxU16 WeightPredB;
            mfxStatus Resetsts;
            mfxU16 ResetWeightPredP;
            mfxU16 ResetWeightPredB;
            struct
            {
                mfxI32 DenomLuma;
                mfxI32 DenomChroma;
                mfxI32 WLuma;
                mfxI32 WChroma;
                mfxI32 OLuma;
                mfxI32 OChroma;
            };
        };
        static const tc_struct test_case[];

        tsRawReader* m_reader;
        mfxU16 WeightPredP;
        mfxU16 WeightPredB;
        std::vector<mfxExtPredWeightTable*> pwt;
        mfxExtBuffer* tmp_buf;
#ifdef DEBUG
        tsSurfaceWriter m_swriter;
#endif
        mfxU32 fn;
        mfxU8 *buffer;
        mfxU32 len;

    public:
        TestSuite()
            : tsVideoEncoder(MFX_CODEC_AVC)
            , tsParserAVC2(INIT_MODE_DEFAULT)
            , m_reader(0)
#ifdef DEBUG
            , m_swriter("out.yuv")
#endif
        {
            set_trace_level(0);

            m_filler = this;
            m_bs_processor = this;

            fn = 0;
            len = 0;
            buffer = new mfxU8[1000000];
            tmp_buf = NULL;

            for (mfxU16 i = 0; i <= (NumFrames + 1); i++)
            {
                pwt.push_back(new mfxExtPredWeightTable);
                memset(pwt[i], 0, sizeof(mfxExtPredWeightTable));
                pwt[i]->Header.BufferId = MFX_EXTBUFF_PRED_WEIGHT_TABLE;
                pwt[i]->Header.BufferSz = sizeof(mfxExtPredWeightTable);
            }
        }

        ~TestSuite()
        {
            if (m_reader)
                delete m_reader;
            if (buffer)
                delete buffer;
            for (mfxU16 i = 0; i < pwt.size(); i++)
            {
                if (pwt[i])
                    delete pwt[i];
            }
            pwt.clear();
        }

        static const unsigned int n_cases;
        int RunTest(unsigned int id);

        mfxStatus ProcessSurface(mfxFrameSurface1& s);
        mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames);
    };

    mfxStatus TestSuite::ProcessSurface(mfxFrameSurface1& s)
    {
        if (m_reader)
            delete m_reader;
        m_reader = new tsRawReader(stream, m_par.mfx.FrameInfo, 30);
        m_reader->ProcessSurface(s);

        if ((fn % 2) != 0)
        {
            CorruptSurface(s);
            pwt[fn]->LumaLog2WeightDenom = logWC_luma;
            pwt[fn]->ChromaLog2WeightDenom = logWC_chroma;
            if (WeightPredB == EXP_WEIGHT_PEREDICTION)//IBPBP
            {
                pwt[fn]->LumaWeightFlag[1][0] = 1;
                pwt[fn]->ChromaWeightFlag[1][0] = 1;
                pwt[fn]->Weights[1][0][0][0] = w_luma;
                pwt[fn]->Weights[1][0][0][1] = o_luma;
                pwt[fn]->Weights[1][0][1][0] = w_chroma;
                pwt[fn]->Weights[1][0][1][1] = o_chroma;
                pwt[fn]->Weights[1][0][2][0] = w_chroma;
                pwt[fn]->Weights[1][0][2][0] = w_chroma;
                pwt[fn]->Weights[1][0][2][0] = w_chroma;
                pwt[fn]->Weights[1][0][2][0] = w_chroma;
                pwt[fn]->Weights[1][0][2][0] = w_chroma;
                pwt[fn]->Weights[1][0][2][1] = o_chroma;
            }
            pwt[fn]->LumaWeightFlag[0][0] = 1;
            pwt[fn]->ChromaWeightFlag[0][0] = 1;
            pwt[fn]->Weights[0][0][0][0] = w_luma;
            pwt[fn]->Weights[0][0][0][1] = o_luma;
            pwt[fn]->Weights[0][0][1][0] = w_chroma;
            pwt[fn]->Weights[0][0][1][1] = o_chroma;
            pwt[fn]->Weights[0][0][2][0] = w_chroma;
            pwt[fn]->Weights[0][0][2][1] = o_chroma;
        }
        else if (fn > 0)
        {
            if (WeightPredB == EXP_WEIGHT_PEREDICTION)// predict from non corruptes frame
            {
                pwt[fn]->LumaLog2WeightDenom = logWC_luma;
                pwt[fn]->ChromaLog2WeightDenom = logWC_chroma;
                pwt[fn]->LumaWeightFlag[0][0] = 1;
                pwt[fn]->ChromaWeightFlag[0][0] = 1;
                pwt[fn]->Weights[0][0][0][0] = w_luma;
                pwt[fn]->Weights[0][0][0][1] = 0;
                pwt[fn]->Weights[0][0][1][0] = w_chroma;
                pwt[fn]->Weights[0][0][1][1] = 0;
                pwt[fn]->Weights[0][0][2][0] = w_chroma;
                pwt[fn]->Weights[0][0][2][1] = 0;
            }
            else// predirct from corruptes frame
            {
                pwt[fn]->LumaLog2WeightDenom = logWC_luma;
                pwt[fn]->ChromaLog2WeightDenom = logWC_chroma;
                pwt[fn]->LumaWeightFlag[0][0] = 1;
                pwt[fn]->ChromaWeightFlag[0][0] = 1;
                pwt[fn]->Weights[0][0][0][0] = w_luma;
                pwt[fn]->Weights[0][0][0][1] = -o_luma;
                pwt[fn]->Weights[0][0][1][0] = w_chroma;
                pwt[fn]->Weights[0][0][1][1] = -o_chroma;
                pwt[fn]->Weights[0][0][2][0] = w_chroma;
                pwt[fn]->Weights[0][0][2][1] = -o_chroma;
            }
        }
        if ((WeightPredP == MFX_WEIGHTED_PRED_EXPLICIT) || (WeightPredB == EXP_WEIGHT_PEREDICTION))
        {
            tmp_buf = (mfxExtBuffer*)(pwt[fn]);
            m_ctrl.ExtParam = &tmp_buf;
            m_ctrl.NumExtParam = 1;
        }
        fn++;
#ifdef DEBUG
        m_swriter.ProcessSurface(s);
#endif
        return MFX_ERR_NONE;
    }


    mfxStatus TestSuite::ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        SetBuffer0(bs);
        UnitType& hdr = ParseOrDie();

        memcpy(&buffer[len], (bs.Data + bs.DataOffset), (bs.DataLength - bs.DataOffset));
        len = len + (bs.DataLength - bs.DataOffset);

        bs.DataLength = 0;
        for (Bs32u iNALU = 0; iNALU < hdr.NumUnits; iNALU++)
        {
            auto& nalu = *hdr.nalu[iNALU];

            if (nalu.nal_unit_type != SLICE_IDR
                && nalu.nal_unit_type != SLICE_NONIDR)
                continue;

            if (nalu.slice->slice_type == (SLICE_P + 5))
            {
                EXPECT_EQ(nalu.slice->pps->weighted_pred_flag, (WeightPredP == MFX_WEIGHTED_PRED_EXPLICIT));
            }
            if (nalu.slice->slice_type == (SLICE_B + 5))
            {
                if ((WeightPredB == EXP_WEIGHT_PEREDICTION) || (WeightPredB == IMP_WEIGHT_PREDICTION))
                    EXPECT_EQ(nalu.slice->pps->weighted_bipred_idc, WeightPredB - 1);
            }
        }


        return MFX_ERR_NONE;
    }

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        /*
        {
        Expected Query/Init Status;
        WeightPredP;
        WeightPredB;
        Expected Reset Status;
        WeightPredP;
        WeightPredB;
        DenomLuma;
        DenomChroma;
        WLuma;
        WChroma;
        OLuma;
        OCroma;
        }
        */
        /*0000*/{ MFX_ERR_NONE, EXP_WEIGHT_PEREDICTION, NONE, MFX_ERR_NONE, NONE, NONE, { 6, 0, 64, 1, 7, 0 } },
        /*0001*/{ MFX_ERR_NONE, EXP_WEIGHT_PEREDICTION, NONE, MFX_ERR_NONE, NONE, NONE, { 7, 0, 128, 1, 10, 0 } },
        /*0002*/{ MFX_ERR_NONE, NONE, EXP_WEIGHT_PEREDICTION, MFX_ERR_NONE, NONE, NONE, { 6, 0, 64, 1, 7, 0 } },
        /*0003*/{ MFX_ERR_NONE, NONE, EXP_WEIGHT_PEREDICTION, MFX_ERR_NONE, NONE, NONE, { 7, 0, 128, 1, 10, 0 } },
        /*0004*/{ MFX_ERR_NONE, EXP_WEIGHT_PEREDICTION, EXP_WEIGHT_PEREDICTION, MFX_ERR_NONE, NONE, IMP_WEIGHT_PREDICTION, { 6, 0, 64, 1, 7, 0 } },
        /*0005*/{ MFX_ERR_NONE, EXP_WEIGHT_PEREDICTION, EXP_WEIGHT_PEREDICTION, MFX_ERR_NONE, NONE, IMP_WEIGHT_PREDICTION, { 7, 0, 128, 1, 10, 0 } },
        /*0006*/{ MFX_ERR_NONE, NONE, DEFAULT_WEIGHT_PREDICTION_B, MFX_ERR_NONE, NONE, NONE, { 6, 0, 64, 1, 7, 0 } },
        /*0007*/{ MFX_ERR_NONE, NONE, DEFAULT_WEIGHT_PREDICTION_B, MFX_ERR_NONE, NONE, NONE, { 7, 0, 128, 1, 10, 0 } },
        //Invalid
        /*0008*/{ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 10, NONE, MFX_ERR_NONE, NONE, NONE, {} },
        /*0009*/{ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, NONE, 10, MFX_ERR_NONE, NONE, NONE, {} },
        /*0010*/{ MFX_ERR_NONE, NONE, NONE, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 10, NONE, { 6, 0, 64, 1, 7, 0 } },
        /*0011*/{ MFX_ERR_NONE, NONE, NONE, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, NONE, 10, { 6, 0, 64, 1, 7, 0 } },

    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;
        auto& tc = test_case[id];
        WeightPredP = tc.WeightPredP;
        WeightPredB = tc.WeightPredB;
        logWC_luma = tc.DenomLuma;
        logWC_chroma = tc.DenomChroma;
        w_luma = tc.WLuma;
        w_chroma = tc.WChroma;
        o_luma = tc.OLuma;
        o_chroma = tc.OChroma;
        mfxExtCodingOption2& CO2 = m_par;
        mfxExtCodingOption3& CO3 = m_par;

        m_par.AsyncDepth = 1;
        m_par.mfx.GopRefDist = 2;
        m_par.mfx.NumRefFrame = 1;
        m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
        m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        m_par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
        m_par.mfx.FrameInfo.Width = 352;
        m_par.mfx.FrameInfo.Height = 288;
        m_par.mfx.FrameInfo.CropW = 352;
        m_par.mfx.FrameInfo.CropH = 288;

        stream = g_tsStreamPool.Get("forBehaviorTest/foreman_cif.nv12");
        g_tsStreamPool.Reg();

        CO3.WeightedPred = tc.WeightPredP;
        CO3.WeightedBiPred = tc.WeightPredB;
        if (tc.WeightPredB == EXP_WEIGHT_PEREDICTION)
            m_par.mfx.GopRefDist = 2;
        if (tc.WeightPredB == DEFAULT_WEIGHT_PREDICTION_B)
        {
            CO3.WeightedBiPred = 0;
            m_par.mfx.GopRefDist = 3;
        }

        if (!m_session)
        {
            MFXInit(); TS_CHECK_MFX;
        }

        if (!m_pFrameAllocator
            && ((m_request.Type & (MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET | MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET))
                    || (m_par.IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY)))
        {
            if (!GetAllocator())
            {
                if (m_pVAHandle)
                    SetAllocator(m_pVAHandle, true);
                else
                    UseDefaultAllocator(false);
            }
            m_pFrameAllocator = GetAllocator();
            SetFrameAllocator(); TS_CHECK_MFX;
        }
        if (m_par.IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY)
        {
            QueryIOSurf();
            AllocOpaque(m_request, m_par);
        }

        g_tsStatus.expect(tc.sts);
        Query(m_session, m_pPar, m_pParOut);
        Init(m_session, m_pPar);
        if (tc.sts == MFX_ERR_NONE)
        {
            EncodeFrames(NumFrames);
        }
#ifdef DEBUG
        FILE *f = fopen("out.h264", "wb");
        fwrite(buffer, 1, len, f);
        fclose(f);
#endif
        //set reset par
        CO3.WeightedPred = tc.ResetWeightPredP;
        CO3.WeightedBiPred = tc.ResetWeightPredB;
        WeightPredP = tc.ResetWeightPredP;
        WeightPredB = tc.ResetWeightPredB;
        if (tc.ResetWeightPredB == DEFAULT_WEIGHT_PREDICTION_B)
        {
            CO3.WeightedBiPred = 0;
            m_par.mfx.GopRefDist = 3;
        }
        else
            m_par.mfx.GopRefDist = 1;

        g_tsStatus.expect(tc.Resetsts);
        Reset(m_session, m_pPar);

        if (tc.sts == MFX_ERR_NONE)//Check per frame PSNR
        {
            tsVideoDecoder dec(MFX_CODEC_AVC);
            dec.m_par.AsyncDepth = 1;
            dec.m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
            dec.m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            dec.m_par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
            dec.m_par.mfx.FrameInfo.Width = 352;
            dec.m_par.mfx.FrameInfo.Height = 288;
            dec.m_par.mfx.FrameInfo.CropW = 352;
            dec.m_par.mfx.FrameInfo.CropH = 288;
            dec.m_session = m_session;
            dec.m_bitstream.Data = buffer;
            dec.m_bitstream.DataLength = len;
            dec.m_bitstream.MaxLength = 1000000;
            mfxFrameSurface1 *ref = GetSurface();
            Verifier v(*ref);
            dec.m_surf_processor = &v;

            mfxStatus res = MFX_ERR_NONE;
            while (true)
            {
                res = dec.DecodeFrameAsync();

                if (MFX_ERR_MORE_DATA == res)
                {
                    if (!dec.m_bitstream.DataLength)
                    {
                        while (dec.m_surf_out.size()) dec.SyncOperation();
                        break;
                    }
                    continue;
                }

                if (MFX_ERR_MORE_SURFACE == res || (res > 0 && *dec.m_pSyncPoint == NULL))
                {
                    continue;
                }

                while (dec.m_surf_out.size()) dec.SyncOperation();
            }
            dec.Close();
        }

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(avce_weight_prediction);
};
