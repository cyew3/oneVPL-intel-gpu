/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2021 Intel Corporation. All Rights Reserved.

File Name: le_hevce_pts_dts.cpp

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

#define MAX_PTS 30

namespace le_hevce_pts_dts
{
    enum
    {
        MFX_PAR,
        NULL_FRAME_ORDER,
        NONE
    };

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 n;
        mfxU32 nB;
        mfxU32 type;
        mfxHyperMode hyper_encode_mode;
        int pts[MAX_PTS];
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];
    };

    inline Ipp32s H265_CeilLog2(Ipp32s a)
    {
        Ipp32s r = 0;
        while (a > (1 << r)) r++;
        return r;
    }

    inline mfxU64 dist1d(mfxU64 x, mfxU64 y)
    {
        if (x > y)
            return x - y;
        else
            return y - x;
    }

    class TestSuite : tsVideoEncoder
    {
    public:
        TestSuite() : tsVideoEncoder(MFX_CODEC_HEVC) {}
        ~TestSuite() {}

        static const unsigned int n_cases;

        template<mfxU32 fourcc>
        int RunTest_Subtype(const unsigned int id);

        int RunTest(tc_struct tc, unsigned int fourcc_id);

    private:
        
        static const tc_struct test_case[];
    };

    class SFiller : public tsSurfaceProcessor
    {
    private:
        int pts[MAX_PTS];
        mfxU32 surf_number;
        mfxU32 type;
    public:
        SFiller(mfxU32 n, mfxU32 _type, const int _pts[MAX_PTS])
        {
            surf_number = 0;
            type = _type;
            for (mfxU32 i = 0; i < n; i++)
                if ((i < MAX_PTS) && (_pts))
                {
                    pts[i] = _pts[i];
                }
        }
        mfxFrameSurface1* ProcessSurface(mfxFrameSurface1* ps, mfxFrameAllocator* pfa);
        mfxStatus ProcessSurface(mfxFrameSurface1& s);
    };
    mfxStatus SFiller::ProcessSurface(mfxFrameSurface1& s)
    {
        tsFrame d(s);

        if (d.isYUV())
        {
            for (mfxU32 h = 0; h < s.Info.Height; h++)
            {
                for (mfxU32 w = 0; w < s.Info.Width; w++)
                {
                    d.Y(w, h) = rand() % (1 << 8);
                    d.U(w, h) = rand() % (1 << 8);
                    d.V(w, h) = rand() % (1 << 8);
                }
            }
        }
        else if (d.isYUV16())
        {
            for (mfxU32 h = 0; h < s.Info.Height; h++)
            {
                for (mfxU32 w = 0; w < s.Info.Width; w++)
                {
                    d.Y16(w, h) = rand() % (1 << 16);
                    d.U16(w, h) = rand() % (1 << 16);
                    d.V16(w, h) = rand() % (1 << 16);
                }
            }
        }
        else if (d.isRGB())
        {
            for (mfxU32 h = 0; h < s.Info.Height; h++)
            {
                for (mfxU32 w = 0; w < s.Info.Width; w++)
                {
                    d.R(w, h) = rand() % (1 << 8);
                    d.G(w, h) = rand() % (1 << 8);
                    d.B(w, h) = rand() % (1 << 8);
                    d.A(w, h) = rand() % (1 << 8);
                }
            }
        }
        else
        {
            return MFX_ERR_UNSUPPORTED;
        }

        return MFX_ERR_NONE;
    }

    mfxFrameSurface1* SFiller::ProcessSurface(mfxFrameSurface1* ps, mfxFrameAllocator* pfa)
    {
        if (ps)
        {
            if (m_cur++ >= m_max)
            {
                return 0;
            }
            bool useAllocator = pfa && !ps->Data.Y;

            if (useAllocator)
            {
                TRACE_FUNC3(mfxFrameAllocator::Lock, pfa->pthis, ps->Data.MemId, &(ps->Data));
                g_tsStatus.check(pfa->Lock(pfa->pthis, ps->Data.MemId, &(ps->Data)));
            }

            TRACE_FUNC1(tsSurfaceProcessor::ProcessSurface, *ps);
            g_tsStatus.check(ProcessSurface(*ps));

            if (useAllocator)
            {
                TRACE_FUNC3(mfxFrameAllocator::Unlock, pfa->pthis, ps->Data.MemId, &(ps->Data));
                g_tsStatus.check(pfa->Unlock(pfa->pthis, ps->Data.MemId, &(ps->Data)));
            }

            ps->Data.TimeStamp = pts[surf_number];
            ps->Data.FrameOrder = -1; // default behaviour for our application
            if (type == NULL_FRAME_ORDER)
            {
                ps->Data.FrameOrder = 0;
            }
            surf_number++;
        }

        if (m_eos)
        {
            m_max = m_cur;
            return 0;
        }

        return ps;
    }

    class BitstreamChecker : public tsBitstreamProcessor, public tsParserH264AU
    {
    private:
        mfxU32 frame_number;
        struct _pts_
        {
            int set_pts;
            int real_pts;
        };
        _pts_ pts[MAX_PTS];
        int delay;
        mfxU32 num_BFrames;
        mfxU32 frD;
        mfxU32 frN;
        mfxU32 frame_duration;

    public:
        BitstreamChecker(mfxU32 n, const int _pts[MAX_PTS], mfxU32 nB, mfxVideoParam& par)
            : tsParserH264AU()
        {
            frame_number = 0;
            delay = 0;
            frD = par.mfx.FrameInfo.FrameRateExtD;
            frN = par.mfx.FrameInfo.FrameRateExtN;
            frame_duration = frD * 90000 / frN;

            for (mfxU32 i = 0; i < n; i++)
                if ((i < MAX_PTS) && (_pts))
                {
                    pts[i].set_pts = _pts[i];
                    pts[i].real_pts = _pts[i];
                    if (_pts[i] == -1)
                    {
                        if ((i > 0) && (pts[i - 1].real_pts != -1))
                            pts[i].real_pts = pts[i - 1].real_pts + frame_duration;
                    }
                }

            frD = par.mfx.FrameInfo.FrameRateExtD;
            frN = par.mfx.FrameInfo.FrameRateExtN;
            num_BFrames = nB;
        }

        mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames);
        mfxU32 GetFrameOreder(mfxU32 _pts);
    };

    mfxStatus BitstreamChecker::ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        int frame_order = GetFrameOreder(bs.TimeStamp);
        delay = H265_CeilLog2(num_BFrames + 1) + frame_order - frame_number;
        int dts_expected = (bs.TimeStamp - (delay * 90000 * frD) / frN);
        if (frame_order == -1)
            dts_expected = MFX_TIMESTAMP_UNKNOWN;

        frame_number++;

        if ((dist1d(bs.DecodeTimeStamp, (mfxU64)dts_expected) > 90 * frD / frN) && (dts_expected != MFX_TIMESTAMP_UNKNOWN))
        {
            g_tsLog << "ERROR: DTS is wrong\nFrame: " << (frame_number - 1) << "real: " << bs.DecodeTimeStamp << ", expected " << dts_expected << "\n";
            return MFX_ERR_ABORTED;
        }
        return MFX_ERR_NONE;
    }

    mfxU32 BitstreamChecker::GetFrameOreder(mfxU32 _pts)
    {
        if (_pts != (mfxU32)-1)
        {
            for (int i = 0; i < MAX_PTS; i++)
            {
                if (dist1d(pts[i].real_pts, _pts) < 90 * frD / frN)
                    return i;
            }
        }
        else
        {
            return -1;
        }
        return -1;
    }

    const tc_struct TestSuite::test_case[] =
    {
        {/*0*/ MFX_ERR_NONE, 30, 0, NONE, MFX_HYPERMODE_ADAPTIVE,
        /*pts*/{ 0, 3600, 7200, 10800, 14400, 18000, 21600, 25200, 28800, 32400,
                 36000, 39600, 43200, 46800, 50400, 54000, 57600, 61200, 64800, 68400,
                 72000, 75600, 79200, 82800, 86400, 90000, 93600, 97200, 100800, 104400 },
               {
                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000 },
                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1000 },
               },
        },
        {/*1*/ MFX_ERR_NONE, 30, 0, NONE, MFX_HYPERMODE_ON,
        /*pts*/{ 0, 3600, 7200, 10800, 14400, 18000, 21600, 25200, 28800, 32400,
                 36000, 39600, 43200, 46800, 50400, 54000, 57600, 61200, 64800, 68400,
                 72000, 75600, 79200, 82800, 86400, 90000, 93600, 97200, 100800, 104400 },
               {
                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000 },
                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1000 },
               },
        },
        {/*2*/ MFX_ERR_NONE, 30, 0, NONE, MFX_HYPERMODE_ADAPTIVE,
        /*pts*/{ 100000, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                 -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                 -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
               {
                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 60000 },
                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001 },
               },
        },
        {/*3*/ MFX_ERR_NONE, 30, 0, NONE, MFX_HYPERMODE_ON,
        /*pts*/{ 100000, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                 -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                 -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
               {
                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 60000 },
                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001 },
               },
        },
        {/*4*/ MFX_ERR_NONE, 30, 1, NONE, MFX_HYPERMODE_ADAPTIVE,
        /*pts*/{ 0, 3600, 7200, 10800, 14400, 18000, 21600, 25200, 28800, 32400,
                 36000, 39600, 43200, 46800, 50400, 54000, 57600, 61200, 64800, 68400,
                 72000, 75600, 79200, 82800, 86400, 90000, 93600, 97200, 100800, 104400 },
               {
                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000 },
                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1000 },
               },
        },
        {/*5*/ MFX_ERR_NONE, 30, 1, NONE, MFX_HYPERMODE_ON,
        /*pts*/{ 0, 3600, 7200, 10800, 14400, 18000, 21600, 25200, 28800, 32400,
                 36000, 39600, 43200, 46800, 50400, 54000, 57600, 61200, 64800, 68400,
                 72000, 75600, 79200, 82800, 86400, 90000, 93600, 97200, 100800, 104400 },
               {
                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000 },
                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1000 },
               },
        },
        {/*6*/ MFX_ERR_NONE, 30, 1, NONE, MFX_HYPERMODE_ADAPTIVE,
        /*pts*/{ 100000, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                 -1, -1, -1, -1, 0, -1, -1, -1, -1, -1,
                 -1, -1, -1, -1, -1, -1, 1000, -1, -1, -1 },
               {
                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 60000 },
                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001 },
               },
        },
        {/*7*/ MFX_ERR_NONE, 30, 1, NONE, MFX_HYPERMODE_ON,
        /*pts*/{ 100000, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                 -1, -1, -1, -1, 0, -1, -1, -1, -1, -1,
                 -1, -1, -1, -1, -1, -1, 1000, -1, -1, -1 },
               {
                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 60000 },
                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001 },
               },
        },
        {/*8*/ MFX_ERR_NONE, 30, 3, NONE, MFX_HYPERMODE_ADAPTIVE,
        /*pts*/{ 0, 3600, 7200, 10800, 14400, 18000, 21600, 25200, 28800, 32400,
                 36000, 39600, 43200, 46800, 50400, 54000, 57600, 61200, 64800, 68400,
                 72000, 75600, 79200, 82800, 86400, 90000, 93600, 97200, 100800, 104400 },
               {
                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 25000 },
                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1000 },
               },
        },
        {/*9*/ MFX_ERR_NONE, 30, 3, NONE, MFX_HYPERMODE_ON,
        /*pts*/{ 0, 3600, 7200, 10800, 14400, 18000, 21600, 25200, 28800, 32400,
                 36000, 39600, 43200, 46800, 50400, 54000, 57600, 61200, 64800, 68400,
                 72000, 75600, 79200, 82800, 86400, 90000, 93600, 97200, 100800, 104400 },
               {
                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 25000 },
                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1000 },
               },
        },
        {/*10*/ MFX_ERR_NONE, 30, 3, NONE, MFX_HYPERMODE_ADAPTIVE,
        /*pts*/{ 100000, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                 -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                 -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
               {
                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 60000 },
                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001 },
               },
        },
        {/*11*/ MFX_ERR_NONE, 30, 3, NONE, MFX_HYPERMODE_ON,
        /*pts*/{ 100000, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                 -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                 -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 },
               {
                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 60000 },
                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001 },
               },
        },
        {/*12*/ MFX_ERR_NONE, 30, 7, NONE, MFX_HYPERMODE_ADAPTIVE,
        /*pts*/{ 0, 3600, 7200, 10800, 14400, 18000, 21600, 25200, 28800, 32400,
                 36000, 39600, 43200, 46800, 50400, 54000, 57600, 61200, 64800, 68400,
                 72000, 75600, 79200, 82800, 86400, 90000, 93600, 97200, 100800, 104400 },
               {
                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000 },
                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1000 },
               },
        },
        {/*13*/ MFX_ERR_NONE, 30, 7, NONE, MFX_HYPERMODE_ON,
        /*pts*/{ 0, 3600, 7200, 10800, 14400, 18000, 21600, 25200, 28800, 32400,
                 36000, 39600, 43200, 46800, 50400, 54000, 57600, 61200, 64800, 68400,
                 72000, 75600, 79200, 82800, 86400, 90000, 93600, 97200, 100800, 104400 },
               {
                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000 },
                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1000 },
               },
        },
        {/*14*/ MFX_ERR_NONE, 30, 7, NONE, MFX_HYPERMODE_ADAPTIVE,
        /*pts*/{ 100000, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                 -1, -1, -1, -1, 0, -1, -1, -1, -1, -1,
                 -1, -1, -1, -1, -1, -1, 1000, -1, -1, -1 },
               {
                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 60000 },
                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001 },
               },
        },
        {/*15*/ MFX_ERR_NONE, 30, 7, NONE, MFX_HYPERMODE_ON,
        /*pts*/{ 100000, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                 -1, -1, -1, -1, 0, -1, -1, -1, -1, -1,
                 -1, -1, -1, -1, -1, -1, 1000, -1, -1, -1 },
               {
                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 60000 },
                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001 },
               },
        },
        {/*16*/ MFX_ERR_NONE, 30, 1, NULL_FRAME_ORDER, MFX_HYPERMODE_ADAPTIVE,
        /*pts*/{ 0, 3600, 7200, 10800, 14400, 18000, 21600, 25200, 28800, 32400,
                 36000, 39600, 43200, 46800, 50400, 54000, 57600, 61200, 64800, 68400,
                 72000, 75600, 79200, 82800, 86400, 90000, 93600, 97200, 100800, 104400 },
               {
                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000 },
                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1000 },
               }, 
        },
        {/*17*/ MFX_ERR_NONE, 30, 1, NULL_FRAME_ORDER, MFX_HYPERMODE_ON,
        /*pts*/{ 0, 3600, 7200, 10800, 14400, 18000, 21600, 25200, 28800, 32400,
                 36000, 39600, 43200, 46800, 50400, 54000, 57600, 61200, 64800, 68400,
                 72000, 75600, 79200, 82800, 86400, 90000, 93600, 97200, 100800, 104400 },
               {
                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000 },
                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1000 },
               },
        },
        {/*18*/ MFX_ERR_NONE, 30, 1, NULL_FRAME_ORDER, MFX_HYPERMODE_ADAPTIVE,
        /*pts*/{ 100000, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                 -1, -1, -1, -1, 0, -1, -1, -1, -1, -1,
                 -1, -1, -1, -1, -1, -1, 1000, -1, -1, -1 },
               {
                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 60000 },
                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001 },
               },
        },
        {/*19*/ MFX_ERR_NONE, 30, 1, NULL_FRAME_ORDER, MFX_HYPERMODE_ON,
        /*pts*/{ 100000, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                 -1, -1, -1, -1, 0, -1, -1, -1, -1, -1,
                 -1, -1, -1, -1, -1, -1, 1000, -1, -1, -1 },
               {
                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 60000 },
                   { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001 },
               },
        },
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(tc_struct);

    template<mfxU32 fourcc>
    int TestSuite::RunTest_Subtype(const unsigned int id)
    {
        const tc_struct& tc = test_case[id];
        return RunTest(tc, fourcc);
    }

    int TestSuite::RunTest(tc_struct tc, unsigned int fourcc_id)
    {
        TS_START;
        mfxStatus init_sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM; // LE turns off HRD at the Init stage
        mfxStatus enc_sts = tc.sts;

        MFXInit();
        Load();

        if (fourcc_id == MFX_FOURCC_NV12)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
        }
        else if (fourcc_id == MFX_FOURCC_RGB4)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_RGB4;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
        }
        else if (fourcc_id == MFX_FOURCC_P010)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_P010;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.Shift = 1;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 10;
        }
        else
        {
            g_tsLog << "ERROR: invalid fourcc_id parameter: " << fourcc_id << "\n";
            return 0;
        }

        // set the common options
        m_par.mfx.FrameInfo.Width = 736;
        m_par.mfx.FrameInfo.Height = 480;
        m_par.mfx.GopPicSize = (tc.nB + 1) * 10;
        m_par.mfx.GopRefDist = tc.nB + 1;
        m_par.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;

        // set the options for dual GPU mode
        m_par.mfx.IdrInterval = 1;
        m_par.mfx.LowPower = MFX_CODINGOPTION_ON;
        m_par.mfx.RateControlMethod = MFX_RATECONTROL_VBR;

        // add the buffer to enable dual GPU mode        
        m_par.AddExtBuffer(MFX_EXTBUFF_HYPER_MODE_PARAM, sizeof(mfxExtHyperModeParam));
        mfxExtBuffer* hyperModeParam = m_par.GetExtBuffer(MFX_EXTBUFF_HYPER_MODE_PARAM);
        mfxExtHyperModeParam* hyperModeParamTmp = (mfxExtHyperModeParam*)hyperModeParam;
        hyperModeParamTmp->Mode = tc.hyper_encode_mode;

        m_pPar->NumExtParam = 1;
        m_pParOut->NumExtParam = 1;
        m_pPar->ExtParam = &hyperModeParam;
        m_pParOut->ExtParam = &hyperModeParam;

        AllocBitstream(16427520); TS_CHECK_MFX;

        SETPARS(m_pPar, MFX_PAR);

        if (0 == memcmp(m_uid->Data, MFX_PLUGINID_HEVCE_HW.Data, sizeof(MFX_PLUGINID_HEVCE_HW.Data)))
        {
            // HEVCE_HW need aligned width and height for 32
            m_par.mfx.FrameInfo.Width = ((m_par.mfx.FrameInfo.Width + 32 - 1) & ~(32 - 1));
            m_par.mfx.FrameInfo.Height = ((m_par.mfx.FrameInfo.Height + 32 - 1) & ~(32 - 1));
        }

        SFiller sf(tc.n, tc.type, tc.pts);
        BitstreamChecker bs_check(tc.n, tc.pts, tc.nB, *m_pPar);
        m_filler = &sf;
        m_bs_processor = &bs_check;

        g_tsStatus.expect(init_sts);
        Init();

        g_tsStatus.expect(enc_sts);
        EncodeFrames(MAX_PTS);

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS_ROUTINE(le_hevce_8b_420_nv12_pts_dts, RunTest_Subtype<MFX_FOURCC_NV12>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(le_hevce_8b_444_rgb4_pts_dts, RunTest_Subtype<MFX_FOURCC_RGB4>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(le_hevce_10b_420_p010_pts_dts, RunTest_Subtype<MFX_FOURCC_P010>, n_cases);
}
