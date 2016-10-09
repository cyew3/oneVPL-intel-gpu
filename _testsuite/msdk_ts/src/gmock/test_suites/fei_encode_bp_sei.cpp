/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2015-2016 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

namespace fei_encode_bp_sei
{

class TestSuite : public tsVideoEncoder
{
public:
    TestSuite() : tsVideoEncoder(MFX_CODEC_AVC) {}
    ~TestSuite() {}

    int RunTest(unsigned int id);

    enum
    {
        MFXPAR  = 1,
        MFXPAR2 = 2,
        COPAR   = 3,
        CO2PAR  = 4,
    };

    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 flags;
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];
    };
    static const tc_struct test_case[];
    static const unsigned int n_cases;
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/**/ MFX_ERR_NONE, MFX_BPSEI_IFRAME, {{CO2PAR, &tsStruct::mfxExtCodingOption2.BufferingPeriodSEI, MFX_BPSEI_IFRAME}}},
    {/**/ MFX_ERR_NONE, MFX_BPSEI_DEFAULT, {{CO2PAR, &tsStruct::mfxExtCodingOption2.BufferingPeriodSEI, MFX_BPSEI_DEFAULT}}}
};

class BsChecker : public tsBitstreamProcessor, tsParserH264AU
{
    mfxU32 m_bp_sei_flg;
    mfxU32 m_gap_bp_sei;
public:
    BsChecker(mfxU32 Flags) : tsParserH264AU(BS_H264_INIT_MODE_DEFAULT)
    {
        m_bp_sei_flg = Flags;
        m_gap_bp_sei = 0;
    }

    ~BsChecker() {}

    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        if (bs.Data)
            set_buffer(bs.Data + bs.DataOffset, bs.DataLength+1);

        UnitType& au = ParseOrDie();

        for (Bs32u i = 0; i < au.NumUnits; i++)
        {
            if (m_gap_bp_sei)
            {
                m_gap_bp_sei++;
            }
            if (au.NALU[i].nal_unit_type == 0x06) //SEI
            {
                bool meet_bs_sei = false;
                for (Bs32u j = 0; j < au.NALU[i].SEI->numMessages; j++)
                {
                    if (au.NALU[i].SEI->message[j].payloadType == 0)
                    {
                        meet_bs_sei = true;
                        break;
                    }
                }
                if (meet_bs_sei)
                {
                    if(m_gap_bp_sei)
                    {
                        g_tsLog << "ERROR: The last BP SEI inserted without I Frame followed?\n";
                        return MFX_ERR_ABORTED;
                    }
                    m_gap_bp_sei = 1;
                    continue;
                }
            }
            if (au.NALU[i].nal_unit_type == 0x05)
            {
                if(m_gap_bp_sei == 2) //IDR Frame follows the BP SEI
                {
                    m_gap_bp_sei = 0; //this is passed, prepare for next round
                    continue;
                }
                else
                {
                    g_tsLog << "ERROR: The BP SEI is not inserted or without IDR followed\n";
                    return MFX_ERR_ABORTED;
                }
            }
            if (au.NALU[i].nal_unit_type == 0x01)
            {
                byte type = au.NALU[i].slice_hdr->slice_type % 5;
                if ((type % 5) == 2) //It's I Frame
                {
                    if (m_bp_sei_flg == MFX_BPSEI_IFRAME)
                    {
                        if(m_gap_bp_sei == 2) //I Frame follows the BP SEI
                        {
                            m_gap_bp_sei = 0; //this is passed, prepare for next round
                            continue;
                        }
                        else
                        {
                            g_tsLog << "ERROR: The BP SEI is not inserted or without I Frame followed\n";
                            return MFX_ERR_ABORTED;
                        }
                    }
                }
            }
        }

        bs.DataLength = 0;
        bs.DataOffset = 0;

        return MFX_ERR_NONE;
    }
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    mfxU32 nframes = 30;
    m_pPar->mfx.FrameInfo.CropW = m_pPar->mfx.FrameInfo.Width = 352;
    m_pPar->mfx.FrameInfo.CropH = m_pPar->mfx.FrameInfo.Height = 288;
    m_pPar->mfx.GopRefDist = 5;
    m_pPar->mfx.GopPicSize = 5;
    m_pPar->mfx.IdrInterval = 10;
    mfxExtCodingOption2 & co2 = m_par;
    SETPARS(&co2, CO2PAR);

    tsRawReader stream(g_tsStreamPool.Get("YUV/foreman_352x288_300.yuv"), m_pPar->mfx.FrameInfo);
    g_tsStreamPool.Reg();
    m_filler = &stream;

    MFXInit();

    AllocSurfaces();
    AllocBitstream((m_par.mfx.FrameInfo.Width*m_par.mfx.FrameInfo.Height) * 1024 * 1024 * 10);

    SetFrameAllocator();

    BsChecker bsChecker(tc.flags);
    m_bs_processor = &bsChecker;

    Init();

    EncodeFrames(nframes);
    Close();

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(fei_encode_bp_sei);
};
