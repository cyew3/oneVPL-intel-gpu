#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

#define EXT_BUF_PAR(eb) tsExtBufTypeToId<eb>::id, sizeof(eb)

namespace avce_start_new_sequence
{

typedef void (*callback)(tsExtBufType<mfxVideoParam>&, mfxU32, mfxU32);

void set_buf(tsExtBufType<mfxVideoParam>& par, mfxU32 id, mfxU32 size)
{
    par.AddExtBuffer(id, size); 
}

void br_init(tsExtBufType<mfxVideoParam>& par, mfxU32, mfxU32)
{
    par.mfx.RateControlMethod = MFX_RATECONTROL_CBR;
    par.mfx.TargetKbps = 5000;
    par.mfx.InitialDelayInKB = 0;
    par.mfx.MaxKbps = 0;
}

void br_reset(tsExtBufType<mfxVideoParam>& par, mfxU32, mfxU32)
{
    par.mfx.TargetKbps = 3000;
    par.mfx.MaxKbps = 0;
}

void br_init_no_hrd(tsExtBufType<mfxVideoParam>& par, mfxU32, mfxU32)
{
    par.mfx.RateControlMethod = MFX_RATECONTROL_CBR;
    par.mfx.TargetKbps = 5000;
    par.mfx.InitialDelayInKB = 0;
    par.mfx.MaxKbps = 0;

    mfxExtCodingOption& extco = par;
    extco.NalHrdConformance = MFX_CODINGOPTION_OFF;
    extco.VuiNalHrdParameters = MFX_CODINGOPTION_OFF;
}

void br_reset_no_hrd(tsExtBufType<mfxVideoParam>& par, mfxU32, mfxU32)
{
    par.mfx.TargetKbps = 3000;
    par.mfx.MaxKbps = 0;

    mfxExtCodingOption& extco = par;
    extco.NalHrdConformance = MFX_CODINGOPTION_OFF;
    extco.VuiNalHrdParameters = MFX_CODINGOPTION_OFF;
}

void qp_reset(tsExtBufType<mfxVideoParam>& par, mfxU32, mfxU32)
{
    par.mfx.RateControlMethod = MFX_RATECONTROL_CQP;
    par.mfx.QPI = par.mfx.QPP = par.mfx.QPB = 16;
}

void set_slice(tsExtBufType<mfxVideoParam>& par, mfxU32 v, mfxU32) { par.mfx.NumSlice = v; }

void set_res(tsExtBufType<mfxVideoParam>& par, mfxU32 w, mfxU32 h)
{
    par.mfx.FrameInfo.Width = par.mfx.FrameInfo.CropW = w;
    par.mfx.FrameInfo.Height = par.mfx.FrameInfo.CropH = h;
}

void tl_init(tsExtBufType<mfxVideoParam>& par, mfxU32 id, mfxU32 size)
{
    par.AddExtBuffer(id, size);
    mfxExtAvcTemporalLayers *atl = (mfxExtAvcTemporalLayers*)par.GetExtBuffer(MFX_EXTBUFF_AVC_TEMPORAL_LAYERS);
    atl->Layer[0].Scale = 1;
    atl->Layer[1].Scale = 2;
    atl->Layer[2].Scale = 0;
    atl->Layer[3].Scale = 0;
}

void tl_reset(tsExtBufType<mfxVideoParam>& par, mfxU32, mfxU32)
{
    mfxExtAvcTemporalLayers *atl = (mfxExtAvcTemporalLayers*)par.GetExtBuffer(MFX_EXTBUFF_AVC_TEMPORAL_LAYERS);
    atl->Layer[0].Scale = 1;
    atl->Layer[1].Scale = 0;
    atl->Layer[2].Scale = 0;
    atl->Layer[3].Scale = 0;
}

class TestSuite : tsVideoEncoder, public tsBitstreamProcessor, public tsParserH264AU
{
private:
    //tsBitstreamWriter m_writer;
    mfxU32 m_frames;

public:
    TestSuite()
        : tsVideoEncoder(MFX_CODEC_AVC, false)
        , tsParserH264AU(BS_H264_INIT_MODE_CABAC|BS_H264_INIT_MODE_CAVLC)
        //, m_writer("debug.264")
    {
        set_trace_level(0);
        m_bs_processor = 0;
    }
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

    mfxStatus EncodeOnlySubmittedFrames(const mfxU32 to_submit);

private:

    enum
    {
        INIT,
        RESET
    };
    enum
    {
        QUERY_ONLY = 1,
        ON = 1 << 2,
        OFF = 1 << 3
    };

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 mode;
        struct
        {
            mfxU32 type;
            callback func;
            mfxU32 v0;
            mfxU32 v1;
        } set_par[MAX_NPARS];
        mfxU32 frames;
        mfxU32 SNS;
    };

    static const tc_struct test_case[];

    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        mfxExtEncoderResetOption* ero = (mfxExtEncoderResetOption*)m_par.GetExtBuffer(MFX_EXTBUFF_ENCODER_RESET_OPTION);
        if (!ero)
            return MFX_ERR_NONE;

        bool idr_expected = false;
        mfxU32 qp_expected = 0;
        if (ero->StartNewSequence != MFX_CODINGOPTION_OFF)
            idr_expected = true;

        if (m_par.mfx.RateControlMethod == MFX_RATECONTROL_CQP)
            qp_expected = m_par.mfx.QPI;


        if (bs.Data)
            set_buffer(bs.Data + bs.DataOffset, bs.DataLength+1);

        UnitType& au = ParseOrDie();

        bool idr = false;
        mfxU32 qp = 0, nslices = 0;
        for (Bs32u i = 0; i < au.NumUnits; i++)
        {
            byte type = au.NALU[i].nal_unit_type;
            slice_header* sh = au.NALU[i].slice_hdr;
            if (!(type == 0x05 || type == 0x01))
            {
                continue;
            }
            if (type == 0x05)
                idr = true;
            nslices++;
            if (!qp)
                qp = sh->slice_qp_delta + sh->pps_active->pic_init_qp_minus26 + 26;
        }

        if (idr_expected)
        {
            EXPECT_EQ(idr_expected, idr) << "ERROR: IDR expected\n";
        } else
        {
            EXPECT_EQ(idr_expected, idr) << "ERROR: No IDR expected\n";
        }

        EXPECT_EQ(m_par.mfx.NumSlice, nslices) << "ERROR: " << m_par.mfx.NumSlice << " expected, " << nslices << " encoded\n";

        if (qp_expected)
        {
            EXPECT_EQ(qp_expected, qp) << "ERROR: QP=" << qp << ", expected " << qp_expected << "\n";
        }

        bs.DataLength = 0;
        return MFX_ERR_NONE;

        //return m_writer.ProcessBitstream(bs, nFrames);
    }
};

mfxStatus TestSuite::EncodeOnlySubmittedFrames(const mfxU32 to_encode)
{
    mfxU32 encoded = 0;
    mfxU32 submitted = 0;
    mfxU32 async = TS_MAX(1, m_par.AsyncDepth);

    async = TS_MIN(to_encode, async - 1);

    while( true )
    {
        m_pSurf = 0;
        if(submitted < to_encode)
        {
            m_pSurf = GetSurface();TS_CHECK_MFX;
            submitted++;
            if(m_filler)
            {
                m_pSurf = m_filler->ProcessSurface(m_pSurf, m_pFrameAllocator);
            }
        }

        mfxStatus mfxRes = EncodeFrameAsync(m_session, m_pCtrl, m_pSurf, m_pBitstream, m_pSyncPoint);
        if(MFX_ERR_NONE == mfxRes)
        {
            SyncOperation();
            g_tsStatus.check();TS_CHECK_MFX;
            encoded++;

            m_bitstream.DataOffset = 0;
            m_bitstream.DataLength = 0;

            if(0 != m_bs_processor)
                break;
        }
        else if(MFX_ERR_MORE_DATA == mfxRes)
        {
            if(!m_pSurf)
                break;
            else
                continue;
        }
        else
        {
            g_tsLog << "ERROR: FAILED in EncodeFrameAsync loop: " << mfxRes << "\n";
            g_tsStatus.check(mfxRes);
        }
    }

    g_tsLog << encoded << " FRAMES ENCODED\n";

    return g_tsStatus.get();
}

const TestSuite::tc_struct TestSuite::test_case[] = 
{
    {/*00*/ MFX_ERR_UNDEFINED_BEHAVIOR, QUERY_ONLY, {}, 0, MFX_CODINGOPTION_UNKNOWN},
    {/*01*/ MFX_ERR_UNDEFINED_BEHAVIOR, QUERY_ONLY, {
        {RESET, set_buf, EXT_BUF_PAR(mfxExtEncoderCapability)}}, 0, MFX_CODINGOPTION_UNKNOWN},
    {/*02*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, QUERY_ONLY, {
        {RESET, set_res, 640, 480}}, 0, MFX_CODINGOPTION_UNKNOWN},
    {/*03*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, QUERY_ONLY, {
        {INIT, set_slice, 2},
        {RESET, set_slice, 4}}, 0, MFX_CODINGOPTION_UNKNOWN},
    {/*04*/ MFX_ERR_NONE, QUERY_ONLY|ON, {
        {RESET, set_res, 320, 240}}, 3, MFX_CODINGOPTION_UNKNOWN},
    {/*05*/ MFX_ERR_NONE, QUERY_ONLY|OFF, {
        {INIT, set_slice, 4},
        {RESET, set_slice, 2}}, 3, MFX_CODINGOPTION_UNKNOWN},
    {/*06*/ MFX_ERR_NONE, QUERY_ONLY|OFF, {
        {INIT, br_init_no_hrd},
        {RESET, br_reset}}, 3, MFX_CODINGOPTION_OFF},
    {/*07*/ MFX_ERR_NONE, QUERY_ONLY|ON, {
        {INIT, br_init},
        {RESET, br_reset_no_hrd}}, 3, MFX_CODINGOPTION_ON},
    {/*08*/ MFX_ERR_NONE, QUERY_ONLY|OFF, {
        {RESET, qp_reset}}, 3, MFX_CODINGOPTION_UNKNOWN},
    {/*09*/ MFX_ERR_NONE, QUERY_ONLY|ON, {
        {INIT, tl_init, EXT_BUF_PAR(mfxExtAvcTemporalLayers)},
        {RESET, tl_reset}}, 1, MFX_CODINGOPTION_UNKNOWN},
    {/*10*/ MFX_ERR_NONE, QUERY_ONLY|OFF, {
        {INIT, tl_init, EXT_BUF_PAR(mfxExtAvcTemporalLayers)},
        {RESET, tl_reset}}, 2, MFX_CODINGOPTION_UNKNOWN},
    {/*11*/ MFX_ERR_NONE, 0, {}, 3, MFX_CODINGOPTION_OFF},
    {/*12*/ MFX_ERR_NONE, 0, {
        {INIT, br_init_no_hrd},
        {RESET, br_reset}}, 3, MFX_CODINGOPTION_OFF},
    {/*13*/ MFX_ERR_INVALID_VIDEO_PARAM, QUERY_ONLY, {
        {INIT, br_init},
        {RESET, br_reset_no_hrd}}, 3, MFX_CODINGOPTION_OFF},
    {/*14*/ MFX_ERR_NONE, 0, {
        {RESET, qp_reset}}, 3, MFX_CODINGOPTION_OFF},
    {/*15*/ MFX_ERR_NONE, 0, {
        {INIT, set_slice, 4},
        {RESET, set_slice, 2}}, 3, MFX_CODINGOPTION_OFF},
    {/*16*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, 0, {
        {INIT, set_slice, 2},
        {RESET, set_slice, 4}}, 0, MFX_CODINGOPTION_OFF},
    {/*17*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {
        {RESET, set_res, 320, 240}}, 0, MFX_CODINGOPTION_OFF},
    {/*18*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {
        {RESET, set_res, 640, 480}}, 0, MFX_CODINGOPTION_OFF},
    {/*19*/ MFX_ERR_INVALID_VIDEO_PARAM, 0, {
        {INIT, tl_init, EXT_BUF_PAR(mfxExtAvcTemporalLayers)},
        {RESET, tl_reset}}, 1, MFX_CODINGOPTION_OFF},
    {/*20*/ MFX_ERR_NONE, 0, {
        {INIT, tl_init, EXT_BUF_PAR(mfxExtAvcTemporalLayers)},
        {RESET, tl_reset}}, 2, MFX_CODINGOPTION_OFF},
    {/*21*/ MFX_ERR_NONE, 0, {}, 3, MFX_CODINGOPTION_ON},
    {/*22*/ MFX_ERR_NONE, 0, {
        {INIT, br_init_no_hrd},
        {RESET, br_reset}}, 3, MFX_CODINGOPTION_ON},
    {/*23*/ MFX_ERR_NONE, 0, {
        {INIT, br_init},
        {RESET, br_reset_no_hrd}}, 3, MFX_CODINGOPTION_ON},
    {/*24*/ MFX_ERR_NONE, 0, {
        {RESET, qp_reset}}, 3, MFX_CODINGOPTION_ON},
    {/*25*/ MFX_ERR_NONE, 0, {
        {INIT, set_slice, 4},
        {RESET, set_slice, 2}}, 3, MFX_CODINGOPTION_ON},
    {/*26*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, 0, {
        {INIT, set_slice, 2},
        {RESET, set_slice, 4}}, 0, MFX_CODINGOPTION_ON},
    {/*27*/ MFX_ERR_NONE, 0, {
        {RESET, set_res, 320, 240}}, 3, MFX_CODINGOPTION_ON},
    {/*28*/ MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, 0, {
        {RESET, set_res, 640, 480}}, 0, MFX_CODINGOPTION_ON},
    {/*29*/ MFX_ERR_NONE, 0, {
        {INIT, tl_init, EXT_BUF_PAR(mfxExtAvcTemporalLayers)},
        {RESET, tl_reset}}, 1, MFX_CODINGOPTION_ON},
    {/*30*/ MFX_ERR_NONE, 0, {
        {INIT, tl_init, EXT_BUF_PAR(mfxExtAvcTemporalLayers)},
        {RESET, tl_reset}}, 2, MFX_CODINGOPTION_ON},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    MFXInit();

    // default params
    m_par.mfx.RateControlMethod = MFX_RATECONTROL_CQP;
    m_par.mfx.QPI = m_par.mfx.QPP = m_par.mfx.QPB = 26;
    m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
    m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    m_par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    m_par.mfx.FrameInfo.FrameRateExtN = 30;
    m_par.mfx.FrameInfo.FrameRateExtD = 1;
    m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 352;
    m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 288;
    m_par.mfx.GopRefDist = 1;
    m_par.mfx.NumRefFrame = 4;
    m_par.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;

    m_default = true;

    if (id)
    {
        for (mfxU32 i = 0; i < MAX_NPARS; i++)
        {
            if (tc.set_par[i].func && tc.set_par[i].type == INIT)
                (*tc.set_par[i].func)(m_par, tc.set_par[i].v0, tc.set_par[i].v1);
        }

        Init();
        GetVideoParam();

        AllocSurfaces();
        AllocBitstream();
        EncodeOnlySubmittedFrames(tc.frames);
    }

    mfxExtEncoderResetOption& ero = m_par;
    ero.StartNewSequence = tc.SNS;

    for (mfxU32 i = 0; i < MAX_NPARS; i++)
    {
        if (tc.set_par[i].func && tc.set_par[i].type == RESET)
            (*tc.set_par[i].func)(m_par, tc.set_par[i].v0, tc.set_par[i].v1);
    }

    g_tsStatus.expect(tc.sts);
    Query();

    if (tc.mode & QUERY_ONLY)
    {
        mfxExtEncoderResetOption *check_sns = (mfxExtEncoderResetOption*)m_par.GetExtBuffer(MFX_EXTBUFF_ENCODER_RESET_OPTION);
        if (tc.mode & ON)
            EXPECT_EQ(MFX_CODINGOPTION_ON, check_sns->StartNewSequence) << "ERROR: StartNewSequence is OFF, expected ON\n";
        else if (tc.mode & OFF)
            EXPECT_EQ(MFX_CODINGOPTION_OFF, check_sns->StartNewSequence) << "ERROR: StartNewSequence is ON, expected OFF\n";
    }
    else
    {
        g_tsStatus.expect(tc.sts);
        Reset();

        //BS parser cannot parse only P stream without IDR
        if(MFX_CODINGOPTION_OFF != ero.StartNewSequence)
            m_bs_processor = this;

        if (tc.sts == MFX_ERR_NONE)
            EncodeOnlySubmittedFrames(1);
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(avce_start_new_sequence);

}