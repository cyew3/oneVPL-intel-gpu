/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2018 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */
#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

// #define DEBUG_STREAM "gmock_scc.h265"

namespace hevce_scc
{
    using namespace BS_HEVC2;

    const int MFX_PAR = 1;

    struct streamDesc
    {
        mfxU16 w;
        mfxU16 h;
        const char *name;
        mfxU32 FourCC;
        mfxU16 ChromaFormat;
        mfxU16 Shift;
        mfxU16 BitDepth;
    };

    const streamDesc streams[] = {
        { 480, 256, "SCC-YUV/scc_480x256_250_nv12.yuv", MFX_FOURCC_NV12, MFX_CHROMAFORMAT_YUV420, 0,  8 },
        { 480, 256, "SCC-YUV/scc_480x256_250_p010.yuv", MFX_FOURCC_P010, MFX_CHROMAFORMAT_YUV420, 1, 10 },
        { 480, 256, "SCC-YUV/scc_480x256_250_ayuv.yuv", MFX_FOURCC_AYUV, MFX_CHROMAFORMAT_YUV444, 0,  8 },
        { 480, 256, "SCC-YUV/scc_480x256_250_y410.yuv", MFX_FOURCC_Y410, MFX_CHROMAFORMAT_YUV444, 0, 10 },
    };

    const streamDesc& getStreamDesc(const mfxU32& id)
    {
        for (unsigned int i = 0; i < sizeof(streams)/sizeof(streams[0]); i++)
            if (streams[i].FourCC == id) return streams[i];

        g_tsLog << "WARNING: invalid fourcc_id parameter: " << id << "\n";
        assert(0);

        return streams[0];
    }

    enum TYPE {
        QUERY = 0x1,
        INIT = 0x2,
        ENCODE = 0x4
    };

    struct tc_struct {
        struct status {
            mfxStatus query;
            mfxStatus init;
            mfxStatus encode;
        } sts;
        mfxU32 type;
        struct tctrl {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];
    };

    class TestSuite : tsVideoEncoder, public tsBitstreamProcessor, public tsParserHEVC2 {
    public:
        static const unsigned int n_cases;
        static const tc_struct test_case[];

#ifdef DEBUG_STREAM
        tsBitstreamWriter m_bsw;
#endif
    public:
        TestSuite() : tsVideoEncoder(MFX_CODEC_HEVC), tsParserHEVC2(PARSE_SSD)
#ifdef DEBUG_STREAM
            , m_bsw(DEBUG_STREAM)
#endif
        {
            m_bs_processor = this;
            set_trace_level(0);
        }
        ~TestSuite() { }

        template<mfxU32 fourcc> int RunTest_Subtype(const unsigned int id) {
            return RunTest(id, fourcc);
        }
        int RunTest(const unsigned int id, unsigned int fourcc_id);
        mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames);
    };

    const tc_struct TestSuite::test_case[] =
    {
        {/*00*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
        QUERY | INIT | ENCODE, {
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_SCC },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.LowPower, MFX_CODINGOPTION_ON },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 1 },
        } },
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::test_case[0]);

    mfxStatus TestSuite::ProcessBitstream(mfxBitstream &bs, mfxU32 nFrames)
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
                if (!IsHEVCSlice(pNALU->nal_unit_type)) continue;

                auto& slice = *pNALU->slice;
                TS_TRACE(slice.POC);

                TS_TRACE((mfxU32)slice.sps->ptl->profile_idc);
                TS_TRACE((mfxU32)slice.sps->scc_extension_flag);
                TS_TRACE((mfxU32)slice.pps->scc_extension_flag);
                TS_TRACE((mfxU32)slice.sps->palette_mode_enabled_flag);
                TS_TRACE((mfxU32)slice.sps->curr_pic_ref_enabled_flag);

                // All frames must have SCC enabled
                if (slice.sps->ptl->profile_idc != SCC || !slice.pps->scc_extension_flag || !slice.sps->scc_extension_flag )
                      return MFX_ERR_UNSUPPORTED;

                mfxI32 palCnt = 0;
                for (auto pCTU = slice.ctu; pCTU; pCTU = pCTU->Next) {
                    for (auto pCU = pCTU->Cu; pCU; pCU = pCU->Next) {
                        palCnt += pCU->palette_mode_flag;
                    }
                }
                TS_TRACE(palCnt);

                /* For I frames or P frames with single reference and enabled IBC */
                if(slice.type == SLICE_TYPE::I || (slice.type == SLICE_TYPE::P && slice.sps->curr_pic_ref_enabled_flag && slice.num_ref_idx_l0_active == 1))
                    EXPECT_GT(palCnt, 0);
            }
        }

        bs.DataLength = 0;

        BreakOnFailure();

        return MFX_ERR_NONE;
    }

    int TestSuite::RunTest(unsigned int id, unsigned int fourcc_id) {
        TS_START;
        auto& tc = test_case[id];
        mfxStatus sts = MFX_ERR_NONE;

        if (g_tsOSFamily != MFX_OS_FAMILY_WINDOWS) {
            g_tsLog << "[ SKIPPED ] This test is only for windows platform\n";
            throw tsSKIP;
        }

        if (0 == memcmp(m_uid->Data, MFX_PLUGINID_HEVCE_HW.Data, sizeof(MFX_PLUGINID_HEVCE_HW.Data))
            && g_tsHWtype < MFX_HW_TGL)
        {
            g_tsLog << "[ SKIPPED ] This test is for TGL+ platforms\n";
            throw tsSKIP;
        }

        MFXInit();
        set_brc_params(&m_par);
        Load();

        const streamDesc& sd = getStreamDesc(fourcc_id);

        m_par.mfx.FrameInfo.FourCC = sd.FourCC;
        m_par.mfx.FrameInfo.ChromaFormat = sd.ChromaFormat;
        m_par.mfx.FrameInfo.Shift = sd.Shift;
        m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = sd.BitDepth;
        m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = sd.w;
        m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = sd.h;

        const char* stream = g_tsStreamPool.Get(sd.name);
        g_tsStreamPool.Reg();

        tsRawReader reader(stream, m_pPar->mfx.FrameInfo);
        reader.m_disable_shift_hack = true;

        m_filler = &reader;

        if (tc.type & QUERY) {
            SETPARS(m_par, MFX_PAR);
            g_tsStatus.expect(tc.sts.query);
            Query();
        }

        if (tc.type & INIT) {
            SETPARS(m_par, MFX_PAR);
            g_tsStatus.expect(tc.sts.init);
            sts = Init();
            }

        if (tc.type & ENCODE) {
            SETPARS(m_par, MFX_PAR);
            g_tsStatus.expect(tc.sts.encode);
            EncodeFrames(g_tsConfig.sim ? 30 : 250);
        }

        if (m_initialized) {
            Close();
        }

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_scc, RunTest_Subtype<MFX_FOURCC_NV12>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_420_p010_scc, RunTest_Subtype<MFX_FOURCC_P010>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_8b_444_ayuv_scc, RunTest_Subtype<MFX_FOURCC_AYUV>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(hevce_10b_444_y410_scc, RunTest_Subtype<MFX_FOURCC_Y410>, n_cases);
}