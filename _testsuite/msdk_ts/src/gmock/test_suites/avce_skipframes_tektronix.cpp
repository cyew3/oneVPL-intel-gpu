#include <fstream>

#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

namespace avce_skipframes_tektronix
{

class TestSuite : tsVideoEncoder
{
public:
    TestSuite() : tsVideoEncoder(MFX_CODEC_AVC) {}
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    struct tc_par;
    static const mfxU32 n_par = 6;

    enum
    {
        MFXPAR = 1,
        EXT_COD2
    };

    struct tc_struct
    {
        mfxStatus sts;
        struct stream_info
        {
            const char* name;
            mfxU32 width;
            mfxU32 height;
        } stream;
        mfxU32 mode;
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[n_par];
        char* skips;
    };

    static const tc_struct test_case[];
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    // IPPP
    {/*00*/ MFX_ERR_NONE, {"YUV/iceage_720x480_491.yuv", 720, 480}, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 30},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "1 2 3 4 5 6 7 8 9"},
    {/*01*/ MFX_ERR_NONE, {"YUV/iceage_720x480_491.yuv", 720, 480}, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 30},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 700},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "1 2 3 4 5 6 7 8 9"},
    {/*02*/ MFX_ERR_NONE, {"YUV/iceage_720x480_491.yuv", 720, 480}, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 30},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 700},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 1000},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "1 2 3 4 5 6 7 8 9"},
    {/*03*/ MFX_ERR_NONE, {"YUV/iceage_720x480_491.yuv", 720, 480}, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 30},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "each2"},
    {/*04*/ MFX_ERR_NONE, {"YUV/iceage_720x480_491.yuv", 720, 480}, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 30},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 700},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "each2"},
    {/*05*/ MFX_ERR_NONE, {"YUV/iceage_720x480_491.yuv", 720, 480}, 0, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 30},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 700},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 1000},
        {EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY}}, "each2"},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

#define SETPARS(p, type)\
for(mfxU32 i = 0; i < n_par; i++) \
{ \
    if(tc.set_par[i].f && tc.set_par[i].ext_type == type) \
    { \
        tsStruct::set(p, *tc.set_par[i].f, tc.set_par[i].v); \
    } \
}

class SurfProc : public tsSurfaceProcessor
{
    std::string m_file_name;
    mfxU32 m_nframes;
    tsRawReader m_raw_reader;
    mfxEncodeCtrl* pCtrl;
    mfxFrameInfo* pFrmInfo;
    std::vector<mfxU32> m_skip_frames;
    mfxU32 m_curr_frame;
    mfxU32 m_target_frames;
  public:
    SurfProc(const char* fname, mfxFrameInfo& fi, mfxU32 n_frames)
        : pCtrl(0)
        , m_curr_frame(0)
        , m_raw_reader(fname, fi, n_frames)
        , m_target_frames(n_frames)
        , pFrmInfo(&fi)
        , m_file_name(fname)
        , m_nframes(n_frames)
    {}
    ~SurfProc() {} ;

    mfxStatus Init(mfxEncodeCtrl& ctrl, std::vector<mfxU32>& skip_frames)
    {
        pCtrl = &ctrl;
        m_skip_frames = skip_frames;
        return MFX_ERR_NONE;
    }

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        if (m_curr_frame >= m_nframes)
        {
            s.Data.Locked++;
            m_eos = true;
            return MFX_ERR_NONE;
        }

        mfxStatus sts = m_raw_reader.ProcessSurface(s);

        if (m_raw_reader.m_eos)  // re-read stream from the beginning to reach target NumFrames
        {
            m_raw_reader.ResetFile();
            sts = m_raw_reader.ProcessSurface(s);
        }

        if (m_skip_frames.size())
        {
            if (std::find(m_skip_frames.begin(), m_skip_frames.end(), m_curr_frame) != m_skip_frames.end())
            {
                pCtrl->SkipFrame = 1;
            }
            else
            {
                pCtrl->SkipFrame = 0;
            }
        }
        else
        {
            pCtrl->SkipFrame = 1;
        }
        m_curr_frame++;
        return sts;
    }
};

class BsDump : public tsBitstreamProcessor, tsParserH264AU
{
    tsBitstreamWriter m_writer;
    std::vector<mfxU32> m_skip_frames;
    mfxU32 m_curr_frame;
    mfxU32 numMb;
public:
    BsDump(const char* fname)
        : m_writer(fname)
        , tsParserH264AU(BS_H264_INIT_MODE_CABAC|BS_H264_INIT_MODE_CAVLC)
        , m_curr_frame(0)
        , numMb(0)
    {
        set_trace_level(0);
    }
    ~BsDump() {}
    mfxStatus Init(std::vector<mfxU32>& skip_frames, mfxFrameInfo& fi)
    {
        numMb = (fi.Width / 16) * (fi.Height / 16);
        m_skip_frames = skip_frames;
        return MFX_ERR_NONE;
    }

    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        SetBuffer(bs);
        UnitType& au = ParseOrDie();

        bool skipped_frame = (bool)!m_skip_frames.size();
        if (std::find(m_skip_frames.begin(), m_skip_frames.end(), m_curr_frame) != m_skip_frames.end())
        {
            skipped_frame = true;
        }

        if (skipped_frame)
        {
            for (Bs32u i = 0; i < au.NumUnits; i++)
            {
                if (!(au.NALU[i].nal_unit_type == 0x01 || au.NALU[i].nal_unit_type == 0x05))
                {
                    continue;
                }
                if ((au.NALU[i].slice_hdr->slice_type % 5) == 2)   // skip I frames
                {
                    continue;
                }

                if (au.NALU[i].slice_hdr->NumMb != numMb)
                {
                    g_tsLog << "ERROR: real NumMb=" << au.NALU[i].slice_hdr->NumMb
                            << "is not equal to targer=" << numMb;
                    return MFX_ERR_INVALID_VIDEO_PARAM;
                }

                for (Bs32u nmb = 0; nmb < au.NALU[i].slice_hdr->NumMb; nmb++)
                {
                    if (0 == au.NALU[i].slice_hdr->mb[nmb].mb_skip_flag)
                    {
                        g_tsLog << "ERROR: mb_skip_flag should be set\n";
                        return MFX_ERR_INVALID_VIDEO_PARAM;
                    }
                }
            }
        }
        m_curr_frame++;

        return m_writer.ProcessBitstream(bs, nFrames);
    }
};

int TestSuite::RunTest(unsigned int id)
{
    int err = 0;
    TS_START;
    const tc_struct& tc = test_case[id];

    MFXInit();

    mfxU32 nframes = 900;

    std::vector<mfxU32> skip_frames;
    if (0 == strncmp(tc.skips, "each2", strlen("each2")))
    {
        for (mfxU32 i = 1; i < nframes; i += 2)
            skip_frames.push_back(i);
    }
    else
    {
        std::stringstream stream(tc.skips);
        mfxU32 f;
        while (stream >> f)
        {
            skip_frames.push_back(f);
        }
    }

    m_par.mfx.GopPicSize = 300;
    m_par.mfx.GopRefDist = 1;
    // set up parameters for case
    SETPARS(m_pPar, MFXPAR);
    if (m_par.mfx.RateControlMethod == MFX_RATECONTROL_CBR)
    {
        m_par.mfx.MaxKbps = m_par.mfx.TargetKbps;
        m_par.mfx.InitialDelayInKB = 0;
    }
    if (m_par.mfx.RateControlMethod != MFX_RATECONTROL_CQP)
    {
        mfxU32 fr = mfxU32(m_par.mfx.FrameInfo.FrameRateExtN / m_par.mfx.FrameInfo.FrameRateExtD);
        // buffer = 0.5 sec
        m_par.mfx.BufferSizeInKB = mfxU16(m_par.mfx.MaxKbps / fr * mfxU16(fr / 2));
        m_par.mfx.InitialDelayInKB = mfxU16(m_par.mfx.BufferSizeInKB / 2);
    }

    m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = tc.stream.width;
    m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = tc.stream.height;

    mfxExtCodingOption2& cod2 = m_par;
    SETPARS(&cod2, EXT_COD2);

    SetFrameAllocator();

    // setup input stream
    SurfProc surf_proc(g_tsStreamPool.Get(tc.stream.name), m_par.mfx.FrameInfo, nframes);
    surf_proc.Init(m_ctrl, skip_frames);
    m_filler = &surf_proc;
    g_tsStreamPool.Reg();

    // setup output stream
    char tmp_out[10];
    sprintf(tmp_out, "%04d.h264", id+1);
    std::string out_name = ENV("TS_OUTPUT_NAME", tmp_out);
    BsDump b(out_name.c_str());
    b.Init(skip_frames, m_par.mfx.FrameInfo);
    m_bs_processor = &b;

    ///////////////////////////////////////////////////////////////////////////
    g_tsStatus.expect(tc.sts);
    EncodeFrames(nframes);

    // check bitrate
    if (m_par.mfx.RateControlMethod != MFX_RATECONTROL_CQP)
    {
        g_tsLog << "Checking bitrate...\n";
        std::ifstream in(out_name.c_str(), std::ifstream::ate | std::ifstream::binary);
        mfxU32 size = (mfxU32)in.tellg();
        mfxI32 bitrate = 8 * size +
            (m_par.mfx.FrameInfo.FrameRateExtN / m_par.mfx.FrameInfo.FrameRateExtD) / nframes;
        mfxI32 target = m_par.mfx.TargetKbps * 1000 * 8;

        if (m_par.mfx.RateControlMethod == MFX_RATECONTROL_CBR &&
            abs(target - bitrate) > target * 0.1)
        {
            g_tsLog << "ERROR: Real bitrate=" << bitrate << " is differ from required=" << target << "\n";
            err++;
        }
        if (m_par.mfx.RateControlMethod == MFX_RATECONTROL_AVBR &&
            bitrate > m_par.mfx.MaxKbps)
        {
            g_tsLog << "ERROR: Real bitrate=" << bitrate << " is bigger than MaxKbps=" << m_par.mfx.MaxKbps << "\n";
            err++;
        }
    }

    TS_END;
    return err;
}

TS_REG_TEST_SUITE_CLASS(avce_skipframes_tektronix);
};
