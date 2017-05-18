/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_parser.h"

//#define DEBUG_STREAM "hevce_transformskip.265"

namespace hevce_transformskip
{
    using namespace BS_HEVC2;

    class BitstreamChecker : public tsBitstreamProcessor, public tsParserHEVC2
    {
    public:
        mfxU16  m_transformskipInit;
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

                TS_TRACE(slice.POC);
                TS_TRACE(!!slice.pps->transform_skip_enabled_flag);

                EXPECT_EQ(m_transformskipInit == MFX_CODINGOPTION_ON, !!slice.pps->transform_skip_enabled_flag);

                if (!slice.pps->transform_skip_enabled_flag || (slice.POC % 2 == 1))
                    continue;

                mfxU32 nTU[3] = {};
                mfxF64 nTransformSkipPercent[3] = {};

                for (auto pCTU = slice.ctu; pCTU; pCTU = pCTU->Next)
                {
                    for (auto pCU = pCTU->Cu; pCU; pCU = pCU->Next)
                    {
                        for (auto pTU = pCU->Tu; pTU; pTU = pTU->Next)
                        {
                            nTransformSkipPercent[0] += !!(pTU->transform_skip_flag & (1 << 0));
                            nTransformSkipPercent[1] += !!(pTU->transform_skip_flag & (1 << 1));
                            nTransformSkipPercent[2] += !!(pTU->transform_skip_flag & (1 << 2));
                            nTU[0] += pTU->cbf_luma;
                            nTU[1] += (pTU->cbf_cb || pTU->cbf_cb1);
                            nTU[2] += (pTU->cbf_cr || pTU->cbf_cr1);
                        }
                    }
                }

                nTransformSkipPercent[0] *= 100;
                nTransformSkipPercent[1] *= 100;
                nTransformSkipPercent[2] *= 100;
                nTransformSkipPercent[0] /= TS_MAX(nTU[0], 1);
                nTransformSkipPercent[1] /= TS_MAX(nTU[1], 1);
                nTransformSkipPercent[2] /= TS_MAX(nTU[2], 1);

                TS_TRACE(nTransformSkipPercent[0]);
                TS_TRACE(nTransformSkipPercent[1]);
                TS_TRACE(nTransformSkipPercent[2]);

                EXPECT_GT(nTransformSkipPercent[0], 5);
                EXPECT_GT(nTransformSkipPercent[1], 0);
                EXPECT_GT(nTransformSkipPercent[2], 0);
            }
        }

        bs.DataLength = 0;

        BreakOnFailure();

        return MFX_ERR_NONE;
    }

    struct TestCase
    {
        mfxU16 TUInit;
        mfxU16 TSInit;
        mfxU16 TUReset;
        mfxU16 TSReset;
    };

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
            if (m_cur % 2 == 0)
            {
                tsFrame d(s);

                if (d.isYUV16())
                {
                    for (mfxU32 h = 0; h < s.Info.Height; h++)
                    {
                        for (mfxU32 w = 0; w < s.Info.Width; w++)
                        {
                            d.Y16(w, h) = 0;
                            d.U16(w, h) = 0;
                            d.V16(w, h) = 0;
                        }
                    }
                }
                else
                {
                    for (mfxU32 h = 0; h < s.Info.Height; h++)
                    {
                        for (mfxU32 w = 0; w < s.Info.Width; w++)
                        {
                            d.Y(w, h) = 0;
                            d.U(w, h) = 0;
                            d.V(w, h) = 0;
                        }
                    }
                }

                return MFX_ERR_NONE;
            }

            return m_reader.ProcessSurface(s);
        }
    };

    const TestCase TCList[] =
    {
        //{ 1,  0, 1, 16 },
        //{ 1,  0, 1, 32 },
        //{ 1, 16, 1,  0 },
        //{ 1, 32, 1,  0 },
        //{ 1, 16, 1, 32 },
        //{ 1, 32, 1, 16 },
        { 4,  0, 4, 16 },
        { 4,  0, 4, 32 },
        { 4, 16, 4,  0 },
        { 4, 32, 4,  0 },
        { 4, 16, 4, 32 },
        { 4, 32, 4, 16 },
        //{ 7,  0, 1, 16 },
        //{ 7,  0, 7, 32 },
        //{ 7, 16, 7,  0 },
        //{ 7, 32, 7,  0 },
        //{ 7, 16, 7, 32 },
        //{ 7, 32, 7, 16 },
        //{ 1, 16, 4, 16 },
        //{ 1, 16, 7, 16 },
        //{ 4, 16, 1, 16 },
        //{ 4, 16, 7, 16 },
        //{ 7, 16, 1, 16 },
        //{ 7, 16, 4, 16 },
    };

    bool isUnsupported(tsExtBufType<mfxVideoParam>& /*par*/)
    {
        return (g_tsHWtype < MFX_HW_CNL);
    }


    int test(unsigned int id, tsVideoEncoder& enc)
    {
        auto stream = g_tsStreamPool.Get("YUV/wididump_1280x720p_513.yuv");
        g_tsStreamPool.Reg();

        auto& tc = TCList[id];
        mfxExtCodingOption3& CO3 = enc.m_par;
        BitstreamChecker checker;
        const mfxU32 nFrames = 4;

        mfxFrameInfo srcFI = {};
        srcFI.BitDepthLuma = 8;
        srcFI.BitDepthChroma = 8;
        srcFI.Shift = 0;
        srcFI.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        srcFI.FourCC = MFX_FOURCC_YV12;
        srcFI.CropW = srcFI.Width = 1280;
        srcFI.CropH = srcFI.Height = 720;
        srcFI.CropX = srcFI.CropY = 0;

        enc.m_par.mfx.FrameInfo.CropW = enc.m_par.mfx.FrameInfo.Width = 1280;
        enc.m_par.mfx.FrameInfo.CropH = enc.m_par.mfx.FrameInfo.Height = 720;
        enc.m_par.mfx.FrameInfo.CropX = enc.m_par.mfx.FrameInfo.CropY = 0;
        enc.m_par.mfx.GopRefDist = 1;
        enc.m_par.mfx.NumRefFrame = 1;
        //enc.m_par.mfx.GopPicSize = 1;

        tsRawReader reader(stream, srcFI, nFrames);
        SProc sproc(reader, tc, enc);

        enc.m_filler = &sproc;
        enc.m_bs_processor = &checker;

        enc.m_par.mfx.TargetUsage = tc.TUInit;
        CO3.TransformSkip = tc.TSInit;

        bool unsupported = isUnsupported(enc.m_par);

        enc.MFXInit();
        enc.Load();

        if (unsupported && CO3.TransformSkip == MFX_CODINGOPTION_ON)
            g_tsStatus.expect(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);

        enc.Query();

        if (unsupported && tc.TSInit != MFX_CODINGOPTION_OFF)
        {
            EXPECT_EQ(MFX_CODINGOPTION_UNKNOWN, CO3.TransformSkip);
        }
        else
        {
            EXPECT_EQ(tc.TSInit, CO3.TransformSkip);
        }

        if (unsupported && (CO3.TransformSkip == MFX_CODINGOPTION_ON))
            g_tsStatus.expect(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);

        enc.Init();
        enc.GetVideoParam();

        if (unsupported)
        {
            EXPECT_EQ(MFX_CODINGOPTION_OFF, CO3.TransformSkip);
        }
        else if (tc.TSInit)
        {
            EXPECT_EQ(tc.TSInit, CO3.TransformSkip);
        }
        else
        {
            EXPECT_EQ(MFX_CODINGOPTION_OFF, CO3.TransformSkip);
        }

        BreakOnFailure();

        checker.m_transformskipInit = CO3.TransformSkip;

        enc.EncodeFrames(nFrames);

        mfxExtEncoderResetOption& ro = enc.m_par;
        mfxU16 TSInit = CO3.TransformSkip;
        mfxU16 TSReset = tc.TSReset ? tc.TSReset : TSInit;
        enc.m_par.mfx.TargetUsage = tc.TUReset;
        CO3.TransformSkip = tc.TSReset;
        unsupported = isUnsupported(enc.m_par);
        ro.StartNewSequence = MFX_CODINGOPTION_OFF;
        bool idrRequired = false;

        if (tc.TUReset != tc.TUInit)
        {
            ro.StartNewSequence = MFX_CODINGOPTION_ON;
            idrRequired = true;
        }

        if (unsupported && (CO3.TransformSkip == MFX_CODINGOPTION_ON))
            g_tsStatus.expect(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);

        enc.Query();

        if (unsupported && tc.TSReset != MFX_CODINGOPTION_OFF)
        {
            EXPECT_EQ(MFX_CODINGOPTION_UNKNOWN, CO3.TransformSkip);
        }
        else
        {
            EXPECT_EQ(tc.TSReset, CO3.TransformSkip);
        }

    l_reset:
        if (idrRequired && ro.StartNewSequence == MFX_CODINGOPTION_OFF && !unsupported)
            g_tsStatus.expect(MFX_ERR_INVALID_VIDEO_PARAM);
        else if (unsupported && (CO3.TransformSkip == MFX_CODINGOPTION_ON))
            g_tsStatus.expect(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);

        enc.Reset();

        if (idrRequired && ro.StartNewSequence == MFX_CODINGOPTION_OFF)
        {
            ro.StartNewSequence = MFX_CODINGOPTION_ON;
            goto l_reset;
        }

        enc.GetVideoParam();

        if (unsupported)
        {
            EXPECT_EQ(MFX_CODINGOPTION_OFF, CO3.TransformSkip);
        }
        else if (TSReset)
        {
            EXPECT_EQ(TSReset, CO3.TransformSkip);
        }
        else
        {
            EXPECT_EQ(MFX_CODINGOPTION_OFF, CO3.TransformSkip);
        }

        BreakOnFailure();

        sproc.m_cur = 0;
        reader.m_cur = 0;

        checker.m_transformskipInit = CO3.TransformSkip;

        enc.EncodeFrames(nFrames);

        return 0;
    }

    int testNV12(unsigned int id)
    {
        TS_START;
        tsVideoEncoder enc(MFX_CODEC_HEVC);
        enc.m_par.mfx.FrameInfo.BitDepthLuma = 8;
        enc.m_par.mfx.FrameInfo.BitDepthChroma = 8;
        enc.m_par.mfx.FrameInfo.Shift = 0;
        enc.m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        enc.m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;

        return test(id, enc);
        TS_END;

        return 0;
    }

    int testYUY2(unsigned int id)
    {
        TS_START;
        tsVideoEncoder enc(MFX_CODEC_HEVC);
        enc.m_par.mfx.FrameInfo.BitDepthLuma = 8;
        enc.m_par.mfx.FrameInfo.BitDepthChroma = 8;
        enc.m_par.mfx.FrameInfo.Shift = 0;
        enc.m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
        enc.m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_YUY2;

        return test(id, enc);
        TS_END;

        return 0;
    }

    int testAYUV(unsigned int id)
    {
        TS_START;
        tsVideoEncoder enc(MFX_CODEC_HEVC);
        enc.m_par.mfx.FrameInfo.BitDepthLuma = 8;
        enc.m_par.mfx.FrameInfo.BitDepthChroma = 8;
        enc.m_par.mfx.FrameInfo.Shift = 0;
        enc.m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
        enc.m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_AYUV;

        return test(id, enc);
        TS_END;

        return 0;
    }

    int testP010(unsigned int id)
    {
        TS_START;
        tsVideoEncoder enc(MFX_CODEC_HEVC);
        enc.m_par.mfx.FrameInfo.BitDepthLuma = 10;
        enc.m_par.mfx.FrameInfo.BitDepthChroma = 10;
        enc.m_par.mfx.FrameInfo.Shift = 1;
        enc.m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        enc.m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_P010;
        enc.m_par.mfx.QPI = enc.m_par.mfx.QPP = enc.m_par.mfx.QPB = 26 + 12;

        return test(id, enc);
        TS_END;

        return 0;
    }

    int testY210(unsigned int id)
    {
        TS_START;
        tsVideoEncoder enc(MFX_CODEC_HEVC);
        enc.m_par.mfx.FrameInfo.BitDepthLuma = 10;
        enc.m_par.mfx.FrameInfo.BitDepthChroma = 10;
        enc.m_par.mfx.FrameInfo.Shift = 1;
        enc.m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV422;
        enc.m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y210;
        enc.m_par.mfx.QPI = enc.m_par.mfx.QPP = enc.m_par.mfx.QPB = 26 + 12;

        return test(id, enc);
        TS_END;

        return 0;
    }

    int testY410(unsigned int id)
    {
        TS_START;
        tsVideoEncoder enc(MFX_CODEC_HEVC);
        enc.m_par.mfx.FrameInfo.BitDepthLuma = 10;
        enc.m_par.mfx.FrameInfo.BitDepthChroma = 10;
        enc.m_par.mfx.FrameInfo.Shift = 0;
        enc.m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
        enc.m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_Y410;
        enc.m_par.mfx.QPI = enc.m_par.mfx.QPP = enc.m_par.mfx.QPB = 26 + 12;

        return test(id, enc);
        TS_END;

        return 0;
    }

    TS_REG_TEST_SUITE(hevce_8b_420_nv12_transformskip, testNV12, sizeof(TCList) / sizeof(TCList[0]));
    TS_REG_TEST_SUITE(hevce_8b_422_yuy2_transformskip, testYUY2, sizeof(TCList) / sizeof(TCList[0]));
    TS_REG_TEST_SUITE(hevce_8b_444_ayuv_transformskip, testAYUV, sizeof(TCList) / sizeof(TCList[0]));
    TS_REG_TEST_SUITE(hevce_10b_420_p010_transformskip, testP010, sizeof(TCList) / sizeof(TCList[0]));
    TS_REG_TEST_SUITE(hevce_10b_422_y210_transformskip, testY210, sizeof(TCList) / sizeof(TCList[0]));
    TS_REG_TEST_SUITE(hevce_10b_444_y410_transformskip, testY410, sizeof(TCList) / sizeof(TCList[0]));
};
