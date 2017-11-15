/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

/* Test uses a special set of mfx initialization parametes which generate '0x0 0x0 0x(0)(1)(2)' sequence in a slice header(s) of LAST frame.
 * The test checks that the sequence '0x0 0x0 0x3 0x(0)(1)(2)' is present in a slice header(s) of LAST frame of output bitstream.
**/

// #define DUMP_BS

namespace avce_emulation_prevention_bytes
{
    using namespace BS_AVC2;


    struct NalUnit
    {
        NalUnit() : begin(0), end(0), type(0), numZero(0)
        {}

        NalUnit(mfxU8 * b, mfxU8 * e, mfxU8 t, mfxU8 z) : begin(b), end(e), type(t), numZero(z)
        {}

        mfxU8 * begin;
        mfxU8 * end;
        mfxU8   type;
        mfxU32  numZero;
    };

    NalUnit GetNalUnit(mfxU8 * begin, mfxU8 * end)
    {
        for (; begin < end - 5; ++begin) {
            if ((begin[0] == 0 && begin[1] == 0 && begin[2] == 1) ||
                (begin[0] == 0 && begin[1] == 0 && begin[2] == 0 && begin[3] == 1)) {
            mfxU8 numZero = (begin[2] == 1 ? 2 : 3);
            mfxU8 type    = (begin[2] == 1 ? begin[3] : begin[4]) & 0x1f;

            for (mfxU8 * next = begin + 4; next < end - 4; ++next) {
                if (next[0] == 0 && next[1] == 0 && next[2] == 1) {
                if (*(next - 1) == 0) {
                    --next;
                }

                return NalUnit(begin, next, type, numZero);
                }
            }

            return NalUnit(begin, end, type, numZero);
            }
        }

        return NalUnit();
    }

    bool operator ==(NalUnit const & left, NalUnit const & right)
    {
        return left.begin == right.begin && left.end == right.end;
    }

    bool operator !=(NalUnit const & left, NalUnit const & right)
    {
        return !(left == right);
    }

    class TestSuite
        : public tsSurfaceProcessor
        , public tsBitstreamProcessor
        , public tsVideoEncoder
    {
    private:
#ifdef DUMP_BS
        tsBitstreamWriter m_writer;
#endif
        mfxU32 m_currIDRIndex = 0;
        mfxU32 m_fo           = 0;
        mfxU32 m_BsCount      = 0;

    public:
        static const unsigned int n_cases;

        enum
        {
            MFX_PAR = 1,
            EXT_COD2,
            TEMP_LRS
        };

#define MAX_IDR 30
        typedef struct
        {
            mfxU32 nFrames;
            mfxU8 nIdrRequests;
            mfxU32 IDRs[MAX_IDR];
            struct
            {
                mfxU32 ext_type;
                const  tsStruct::Field* f;
                mfxU32 v;
            } set_par[MAX_NPARS];

            mfxU8 expectedSeqeunce[4];
        } tc_struct;

        static const tc_struct test_case[];

        const tc_struct * m_tc;

        TestSuite()
            : tsVideoEncoder(MFX_CODEC_AVC)
#ifdef DUMP_BS
            , m_writer("debug.264")
#endif
        {
            m_filler = this;
            m_bs_processor = this;
        }

        ~TestSuite(){}

        int RunTest(unsigned int id);

        mfxStatus ProcessSurface(mfxFrameSurface1& s)
        {
            s.Data.FrameOrder = m_fo++;
            if (m_currIDRIndex < m_tc->nIdrRequests && m_tc->IDRs[m_currIDRIndex] == s.Data.FrameOrder)
            {
                m_currIDRIndex++;
                m_pCtrl->FrameType = MFX_FRAMETYPE_I | MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_REF;
            }
            else
                m_pCtrl->FrameType = 0;

            return MFX_ERR_NONE;
        }

        mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
        {
            mfxStatus sts = MFX_ERR_NONE;
            if ((++m_BsCount) == m_tc->nFrames)
            {
                mfxU8 * begin = bs.Data + bs.DataOffset;
                mfxU8 * end = bs.Data + bs.DataOffset + bs.DataLength;
                for (NalUnit nalu = GetNalUnit(begin, end); nalu != NalUnit(); nalu = GetNalUnit(begin, end))
                {
                    if (nalu.type == 5)
                    {
                        auto it = std::search(nalu.begin, nalu.end, &m_tc->expectedSeqeunce[0], &m_tc->expectedSeqeunce[4]);
                        if (it == nalu.end)
                        {
                            g_tsLog << "ERROR: ";
                            g_tsLog << "Emulation prevention byte was expected but not found\n";
                            sts = MFX_ERR_ABORTED;
                        }
                    }

                    begin = nalu.end;
                }
            }

#ifdef DUMP_BS
            m_writer.ProcessBitstream(bs, nFrames);
#endif
            bs.DataLength = 0;

            return sts;
        }
    };

#define mfx_PicStruct   tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct
#define mfx_GopPicSize  tsStruct::mfxVideoParam.mfx.GopPicSize
#define mfx_GopRefDist  tsStruct::mfxVideoParam.mfx.GopRefDist
#define mfx_GopOptFlag  tsStruct::mfxVideoParam.mfx.GopOptFlag
#define mfx_IdrInterval tsStruct::mfxVideoParam.mfx.IdrInterval
#define mfx_PRefType    tsStruct::mfxExtCodingOption3.PRefType
#define mfx_BRefType    tsStruct::mfxExtCodingOption2.BRefType
#define mfx_GPB         tsStruct::mfxExtCodingOption3.GPB

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
        /* slice header: 0x0 0x0 0x1 0x25 0x88 0x80 0x4 0x0 0x0 0x0 0x65 0xc0 */
        {/*00*/16, 16, {0, 1, 2, 3, 4, 5 ,6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
                        {
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,     32 },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.Height,    32 },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,     32 },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,     32 },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.GopRefDist,          0 },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.GopPicSize,          8192 },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.RateControlMethod,   MFX_RATECONTROL_CQP },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.QPI,                 51 },
                            { EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame,         MFX_SKIPFRAME_INSERT_DUMMY },
                        },
                        {0x0, 0x0, 0x3, 0x0}
        },
        /* slice header: 0x0 0x0 0x1 0x25 0x88 0x80 0x4 0x0 0x0 0x1 0xbc */
        {/*01*/16, 16, {0, 1, 2, 3, 4, 5 ,6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
                        {
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,     32 },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.Height,    32 },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,     32 },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,     32 },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.GopRefDist,          0 },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.GopPicSize,          8192 },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.RateControlMethod,   MFX_RATECONTROL_CQP },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.QPI,                 20 },
                            { EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame,         MFX_SKIPFRAME_INSERT_DUMMY },
                        },
                        {0x0, 0x0, 0x3, 0x1}
        },
        /* slice header #0: 0x0 0x0 0x1 0x25 0x88 0x80 0x4 0x0 0x0 0x1 0xbc */
        /* slice header #1: 0x0 0x0 0x1 0x25 0x8 0x88 0x80 0x4 0x0 0x0 0x1 0xbc */
        {/*02*/16, 16, {0, 1, 2, 3, 4, 5 ,6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
                        {
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,     32 },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.Height,    256 },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,     32 },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,     256 },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.GopRefDist,          0 },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.GopPicSize,          8192 },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.RateControlMethod,   MFX_RATECONTROL_CQP },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.QPI,                 20 },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.NumSlice,            2 },
                            { EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame,         MFX_SKIPFRAME_INSERT_DUMMY },
                        },
                        {0x0, 0x0, 0x3, 0x1}
        },
        /* slice header: 0x0 0x0 0x1 0x25 0x88 0x80 0x4 0x0 0x0 0x1 0xbc */
        {/*03*/16, 16, {0, 1, 2, 3, 4, 5 ,6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
                        {
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,     32 },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.Height,    32 },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,     32 },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,     32 },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.GopRefDist,          0 },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.GopPicSize,          8192 },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.RateControlMethod,   MFX_RATECONTROL_CQP },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.QPI,                 20 },
                            { EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame,         MFX_SKIPFRAME_INSERT_DUMMY },
                            { TEMP_LRS, &tsStruct::mfxExtAvcTemporalLayers.Layer[0].Scale,1 },
                        },
                        {0x0, 0x0, 0x3, 0x1}
        },
        /* slice header: 0x0 0x0 0x1 0x25 0x88 0x80 0x4 0x0 0x0 0x1 0xbc */
        {/*04*/16, 16, {0, 1, 2, 3, 4, 5 ,6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
                        {
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,     128 },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.Height,    128},
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,     128},
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,     128},
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.GopRefDist,          0 },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.GopPicSize,          8192 },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.RateControlMethod,   MFX_RATECONTROL_CQP },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.QPI,                 20 },
                            { EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame,         MFX_SKIPFRAME_INSERT_DUMMY },
                        },
                        {0x0, 0x0, 0x3, 0x1}
        },
        /* slice header: 0x0 0x0 0x1 0x25 0x88 0x80 0x0 0x2 0x0 0x0 0x1 0xe */
        {/*05*/32765, 0, {},
                        {
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,     32 },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.Height,    32 },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,     32 },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,     32 },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.GopRefDist,          4 },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.GopPicSize,          4 },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.RateControlMethod,   MFX_RATECONTROL_CQP },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.QPI,                 34 },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.QPP,                 34 },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.QPB,                 34 },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.NumRefFrame,         2 },
                        },
                        {0x0, 0x0, 0x3, 0x1}
        },
        /* slice header: 0x0 0x0 0x1 0x25 0x88 0x80 0x4 0x0 0x0 0x2 0xf0 */
        {/*06*/16, 16, {0, 1, 2, 3, 4, 5 ,6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
                        {
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,     32 },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.Height,    32 },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,     32 },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,     32 },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.GopRefDist,          0 },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.GopPicSize,          8192 },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.RateControlMethod,   MFX_RATECONTROL_CQP },
                            { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.QPI,                 24 },
                            { EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame,         MFX_SKIPFRAME_INSERT_DUMMY },
                        },
                        {0x0, 0x0, 0x3, 0x2}
        },
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    int TestSuite::RunTest(unsigned int id)
    {
        const tc_struct& tc = test_case[id];

        TS_START;

        m_tc = &tc;

        SETPARS(m_pPar, MFX_PAR);

        mfxExtCodingOption2& co2 = m_par;
        SETPARS(&co2, EXT_COD2);

        mfxExtAvcTemporalLayers& layers = m_par;
        SETPARS(&layers, TEMP_LRS);

        m_par.AsyncDepth = 1;

        Init();
        GetVideoParam();

        EncodeFrames(tc.nFrames);
        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(avce_emulation_prevention_bytes);
}
