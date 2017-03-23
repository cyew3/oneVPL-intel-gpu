/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2017 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_struct.h"

namespace avce_co2_use_rawref
{

class TestSuite : tsVideoEncoder, tsSurfaceProcessor
{
public:
    TestSuite() : tsVideoEncoder(MFX_CODEC_AVC)
        , mode(0)
        , nframe(0)
    {
        srand(0);
        m_filler = this;

        mfxFrameInfo fi = {0};
        fi.Width = 352;
        fi.Height = 288;
        fi.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        fi.FourCC = MFX_FOURCC_NV12;
        m_reader = new tsRawReader(g_tsStreamPool.Get("forBehaviorTest/foreman_cif.nv12"), fi);
        g_tsStreamPool.Reg();
    }
    ~TestSuite() { delete m_reader; }
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        mfxStatus sts = m_reader->ProcessSurface(s);

        {
            mfxExtCodingOption2& co2 = ctrl[nframe];
                co2.UseRawRef = mode & RAND ?
                    ((rand() % 9000) % 2 == 0 ? MFX_CODINGOPTION_ON : MFX_CODINGOPTION_OFF)
                                              : MFX_CODINGOPTION_ON;
        }

        m_pCtrl = &ctrl[nframe];
        nframe++;

        return sts;
    }

private:
    struct tc_par;
    mfxU32 mode;
    tsRawReader* m_reader;
    std::map<mfxU32, tsExtBufType<mfxEncodeCtrl>> ctrl;
    mfxU32 nframe;

    enum
    {
        MFXPAR = 1,
        EXT_COD2,
        MFXPAR_LOWPOWER
    };

    enum
    {
        INIT    = 1,
        NO_INIT = 1 << 1,

        RAND = 1 << 2
    };

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 mode;
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];
    };

    static const tc_struct test_case[];
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*00*/ MFX_ERR_NONE, INIT, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 4},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1}}},
    {/*01*/ MFX_ERR_NONE, INIT, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 3},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 10},
        {MFXPAR_LOWPOWER, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1}}},
    {/*02*/ MFX_ERR_NONE, INIT, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 6},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 10},
        {MFXPAR_LOWPOWER, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1}}},
    {/*03*/ MFX_ERR_NONE, RAND, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 2},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4},
        {MFXPAR_LOWPOWER, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1}}},
    {/*04*/ MFX_ERR_NONE, RAND, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 2},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 200},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 600},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 600}}},
    {/*05*/ MFX_ERR_NONE, RAND, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 2},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 300},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 600},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 900}}},
    {/*06*/ MFX_ERR_NONE, RAND, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 2},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 200},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 600},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 600},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4}}},

    // without initialization with CO2::UseRawRef feature should be ignored in runtime
    {/*07*/ MFX_ERR_NONE, NO_INIT, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 2},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1}}},
    {/*08*/ MFX_ERR_NONE, NO_INIT, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 2},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 4}}},

    {/*09*/ MFX_ERR_NONE, INIT, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 1},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE}}},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];
    mode = tc.mode;

    tsBitstreamCRC32 bs_crc;
    m_bs_processor = &bs_crc;
    MFXInit();

    // set up parameters for case
    m_par.mfx.FrameInfo.Width = m_par.mfx.FrameInfo.CropW = 352;
    m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 288;
    SETPARS(m_pPar, MFXPAR);
    if (m_par.mfx.LowPower == MFX_CODINGOPTION_ON)
        SETPARS(m_pPar, MFXPAR_LOWPOWER);


    if (!(tc.mode & NO_INIT))
    {
        mfxExtCodingOption2& cod2 = m_par;
        cod2.UseRawRef = MFX_CODINGOPTION_ON;
        SETPARS(&cod2, EXT_COD2);
    }
    else
    {
        m_par.mfx.NumSlice = 1;
    }

    SetFrameAllocator();

    ///////////////////////////////////////////////////////////////////////////
    g_tsStatus.expect(tc.sts);
    mfxU32 nf = 60;
    mfxVideoParam pars = {0};
    memcpy(&pars, m_pPar, sizeof(mfxVideoParam));

    if ((MFX_CODINGOPTION_ON == m_par.mfx.LowPower) && (m_par.mfx.GopRefDist > 1))
    {
        g_tsLog << "WARNING: CASE WAS SKIPPED\n";
    }
    else
    {

        if (tc.mode & INIT)
        {
            Init();
        }
        if (tc.sts == MFX_ERR_NONE)
        {
            EncodeFrames(nf);

            if (tc.mode & NO_INIT)
            {
                DrainEncodedBitstream();
                Close();
                Ipp32u crc = bs_crc.GetCRC();
 
                memset(&m_bitstream, 0, sizeof(mfxBitstream));
                m_pPar = &pars;
                m_pPar->NumExtParam = 0;
                m_pPar->ExtParam = 0;
                m_pCtrl->NumExtParam = 0;
                m_pCtrl->ExtParam = 0;

                mfxFrameInfo fi = { 0 };
                fi.Width = 352;
                fi.Height = 288;
                fi.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
                fi.FourCC = MFX_FOURCC_NV12;
                tsRawReader reader(g_tsStreamPool.Get("forBehaviorTest/foreman_cif.nv12"), fi);
                m_filler = &reader;

                tsBitstreamCRC32 bs_cmp_crc;
                m_bs_processor = &bs_cmp_crc;

                Init(m_session, m_pPar);

                EncodeFrames(nf);
                DrainEncodedBitstream();
                Ipp32u cmp_crc = bs_cmp_crc.GetCRC();
                if (crc != cmp_crc)
                {
                    g_tsLog << "ERROR: CO2::UseRawRef was enabled in runtime without initialization\n";
                    g_tsStatus.check(MFX_ERR_ABORTED);
                }
            }
        }
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(avce_co2_use_rawref);
};
