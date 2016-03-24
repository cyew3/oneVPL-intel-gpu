/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2016 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

namespace mpeg2e_skipframes
{

class TestSuite : tsVideoEncoder
{
public:
    TestSuite() : tsVideoEncoder(MFX_CODEC_MPEG2) {}
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    struct tc_par;

    enum
    {
        MFXPAR = 1,
        EXT_COD2
    };

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 nframes;
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];
        char* skips;
    };

    static const tc_struct test_case[];
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    // IBBBBP, not all skipped
    {/*00*/ MFX_ERR_NONE, 30, {
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 30 },
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 5 },
        { EXT_COD2, &tsStruct::mfxExtCodingOption2.SkipFrame, MFX_SKIPFRAME_INSERT_DUMMY } }, "1 5 7" },

};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

class SurfProc : public tsSurfaceProcessor
{
    mfxEncodeCtrl* pCtrl;
    mfxU32 m_curr_frame;
    tsRawReader m_raw_reader;
    mfxU32 m_target_frames;
    mfxFrameInfo* pFrmInfo;
    std::string m_file_name;
    mfxU32 m_nframes;
    std::vector<mfxU32> m_skip_frames;
    std::map<mfxU32, mfxU32> m_skip_values;

public:

    SurfProc(const char* fname, mfxFrameInfo& fi, mfxU32 n_frames = 0xFFFFFFFF)
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
        for (size_t i = 0; i < skip_frames.size(); i++) {
            m_skip_values[m_skip_frames[i]] = 1;
        }
        return MFX_ERR_NONE;
    }

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
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

        return m_raw_reader.ProcessSurface(s);
    }
};

class BsDump : public tsBitstreamProcessor, tsParserMPEG2AU
{
    tsBitstreamWriter m_writer;
public:
    BsDump(const char* fname)
        : tsParserMPEG2AU(BS_MPEG2_INIT_MODE_MB)
        , m_writer(fname)
    {
        set_trace_level(0);
    }
    ~BsDump() {}

    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        if (bs.Data)
            set_buffer(bs.Data + bs.DataOffset, bs.DataLength+1);

        m_writer.ProcessBitstream(bs, nFrames);

        /*while (nFrames--)
        {
            UnitType& au = ParseOrDie();

            for (mfxU32 i = 0; i < au.NumSlice; i++)
            {
                g_tsLog << au.slice[i].NumMb << "\n";
            }
        }*/

        bs.DataLength = 0;

        return MFX_ERR_NONE;
    }
};

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    MFXInit();

    // set up parameters for case
    SETPARS(m_pPar, MFXPAR);

    mfxExtCodingOption2& cod2 = m_par;
    SETPARS(&cod2, EXT_COD2);

    std::stringstream stream(tc.skips);
    std::vector<mfxU32> skip_frames;
    mfxU32 f;
    while (stream >> f)
    {
        skip_frames.push_back(f);
    }
    g_tsLog << "Skipped frames: " << tc.skips << "\n";


    std::string input = ENV("TS_INPUT_YUV","");
    std::string in_name;
    if (input.length())
    {
        m_par.mfx.FrameInfo.CropH = m_par.mfx.FrameInfo.Height = 0;
        m_par.mfx.FrameInfo.CropW = m_par.mfx.FrameInfo.Width = 0;

        std::stringstream s(input);
        s >> in_name;
        s >> m_par.mfx.FrameInfo.Width;
        s >> m_par.mfx.FrameInfo.Height;
        m_par.mfx.FrameInfo.CropH = m_par.mfx.FrameInfo.Height;
        m_par.mfx.FrameInfo.CropW = m_par.mfx.FrameInfo.Width;
        m_par.mfx.FrameInfo.Width += m_par.mfx.FrameInfo.Width & 15;
        m_par.mfx.FrameInfo.Height += m_par.mfx.FrameInfo.Height & 15;
    }
    if (!in_name.length() || !m_par.mfx.FrameInfo.Width || !m_par.mfx.FrameInfo.Height)
    {
        m_par.mfx.FrameInfo.Width = 720;
        m_par.mfx.FrameInfo.Height = 480;
        m_par.mfx.FrameInfo.Width += m_par.mfx.FrameInfo.Width & 15;
        m_par.mfx.FrameInfo.Height += m_par.mfx.FrameInfo.Height & 15;

        m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;

        in_name = std::string(g_tsStreamPool.Get("YUV/calendar_720x480_600_nv12.yuv"));
    }

    ///////////////////////////////////////////////////////////////////////////
    mfxU32 n = tc.nframes;
    AllocBitstream((m_par.mfx.FrameInfo.Width*m_par.mfx.FrameInfo.Height) * 1024 * 1024 * n);
    SetFrameAllocator();

    SurfProc surf_proc(in_name.c_str(), m_par.mfx.FrameInfo);
    surf_proc.Init(m_ctrl, skip_frames);
    m_filler = &surf_proc;
    g_tsStreamPool.Reg();

    // setup output stream
    char tmp_out[10];
#pragma warning(disable:4996)
    sprintf(tmp_out, "%04d.mpeg", id+1);
#pragma warning(default:4996)
    std::string out_name = ENV("TS_OUTPUT_NAME", tmp_out);
    BsDump b(out_name.c_str());
    m_bs_processor = &b;

    g_tsStatus.expect(tc.sts);
    EncodeFrames(n);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(mpeg2e_skipframes);
};
