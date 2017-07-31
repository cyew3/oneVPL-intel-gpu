/******************************************************************************* *\

Copyright (C) 2016-2017 Intel Corporation.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
- Neither the name of Intel Corporation nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY INTEL CORPORATION "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL INTEL CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************/

/*
[MQ1220] - test for slice size in LCU for HEVCe HW plugin.
    mfxExtCodingOption2.MumMbPerSlice is not supported configuration during runtime,
    only init/reset.
*/


#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

namespace hevce_co2_max_slice_size_in_ctu
{

class TestSuite : public tsVideoEncoder, public tsSurfaceProcessor, public tsBitstreamProcessor, public tsParserHEVCAU
{
public:
    TestSuite()
        : tsVideoEncoder(MFX_CODEC_HEVC)
        , tsParserHEVCAU(BS_HEVC::INIT_MODE_CABAC)
        , m_mode(0)
        , m_expected(0)
        , m_nFrame(0)
    {
        srand(time(NULL));
        m_bs_processor = this;
        m_filler = this;
    }

    ~TestSuite()
    {
    }

    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    mfxU32 m_mode;
    mfxU16 m_expected;
    mfxU32 m_nFrame;
    mfxU32 m_numLCU; //LCU number in frame

    enum
    {
        MFX_PAR = 1,
        EXT_COD2,
    };

    enum
    {
        INIT    = 1 << 1,       //to set on initialization
        RESET   = 1 << 2,
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
        char* skips;
    };

    static const tc_struct test_case[];

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        return MFX_ERR_NONE;
    }

    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        set_buffer(bs.Data + bs.DataOffset, bs.DataLength);

        while (nFrames--)
        {
            UnitType& au = ParseOrDie();
            for (Bs32u i = 0; i < au.NumUnits; i++)
            {
                Bs16u type = au.nalu[i]->nal_unit_type;
                //check slice segment only
                if (type > 21 || ((type > 9) && (type < 16)))
                {
                    continue;
                }

                if (m_expected == 0) {
                    //not set, so expected value is m_numLCU
                    m_expected = m_numLCU;
                }

                auto& s = *au.nalu[i]->slice;
                if (s.NumCU != m_expected) {
                    if (s.NumCU > m_expected)
                    {
                        g_tsLog << "ERROR: Slice's LCU num exceeds expected value.\n"
                            << "frame#" << m_nFrame <<": Slice's LCU num = " << (mfxU32)s.NumCU
                            << " > " << (mfxU32)m_expected << " (expected value).\n";
                        return MFX_ERR_ABORTED;
                    } else
                    if (i != (au.NumUnits - 1))
                    {
                        g_tsLog << "ERROR: Slice's LCU num is not as expected and it is not the last slice.\n"
                            << "frame#" << m_nFrame <<": Slice's LCU num = " << (mfxU32)s.NumCU
                            << " != " << (mfxU32)m_expected << " (expected value).\n";
                        return MFX_ERR_ABORTED;
                    }
                }
            }
            m_nFrame++;
        }

        bs.DataLength = 0;

        return MFX_ERR_NONE;
    }
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    //If NumMbPerSlice is set > LTU_size_in_frame return WRN_INCOMPATIBLE_VIDEO_PARAM
    /*00*/{MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, 0, {EXT_COD2, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, 0xFFFF}},
    /*01*/{MFX_ERR_NONE, 0, {}},
    /*02*/{MFX_ERR_NONE, INIT, {}},
    /*03*/{MFX_ERR_NONE, RESET, {}},
    /*04*/{MFX_ERR_NONE, RESET|INIT, {}},
    /*05*/{MFX_ERR_NONE, INIT, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 30},
                                {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1}}},
    /*06*/{MFX_ERR_NONE, INIT|RESET, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 30},
                                      {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 10}}},
    /*07*/{MFX_ERR_NONE, INIT|RESET, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 30},
                                      {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 8}}},
    /*08*/{MFX_ERR_NONE, INIT|RESET, {
        {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 700},
        {MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 0},
        {MFX_PAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 0},
        {MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR}}},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

const int frameNumber = 20;

int TestSuite::RunTest(unsigned int id)
{
    int err = 0;
    TS_START;
    const tc_struct& tc = test_case[id];
    m_mode = tc.mode;

    //This calculation is based on SKL limitation that LCU is always 32x32.
    //If LCU can be configured, the calcuation should be updated with the
    //following values configured at initialization.
    //sps.log2_min_luma_coding_block_size_minus3
    //sps.log2_diff_max_min_luma_coding_block_size
    mfxU32 LCUSize = 32;
    if (g_tsHWtype >= MFX_HW_CNL)
        LCUSize = 64;
    mfxU32 widthLCU  = (m_par.mfx.FrameInfo.CropW + (LCUSize - 1)) / LCUSize;
    mfxU32 heightLCU = (m_par.mfx.FrameInfo.CropH + (LCUSize - 1)) / LCUSize;
    m_numLCU = heightLCU * widthLCU;

    MFXInit();

    SETPARS(m_pPar, MFX_PAR);

    if (tc.sts == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM) {
        mfxExtCodingOption2& cod2 = m_par;
        SETPARS(&cod2, EXT_COD2);
    } else if (tc.mode & INIT) {
        mfxExtCodingOption2& cod2 = m_par;
        if (g_tsHWtype >= MFX_HW_CNL)   // row aligned
            cod2.NumMbPerSlice = (rand() % heightLCU) * widthLCU;
        else
            cod2.NumMbPerSlice = rand() % m_numLCU;
        m_expected = cod2.NumMbPerSlice;
    }

    //load the plugin in advance.
    if(!m_loaded)
    {
        Load();
    }

    //This test is for hw plugin only
    if (0 == memcmp(m_uid->Data, MFX_PLUGINID_HEVCE_HW.Data, sizeof(MFX_PLUGINID_HEVCE_HW.Data))) {
        if (g_tsHWtype < MFX_HW_SKL)
        {
            g_tsLog << "WARNING: Unsupported HW Platform!\n";
            return 0;
        }
    } else {
        g_tsLog << "WARNING: only HEVCe HW plugin is tested\n";
        return 0;
    }

    g_tsStatus.expect(tc.sts);
    Init();

    if (tc.sts == MFX_ERR_NONE) {
        m_max = frameNumber;
        m_cur = 0;
        EncodeFrames(frameNumber);

        if (tc.mode & RESET) {
            mfxExtCodingOption2& cod2 = m_par;
            if (g_tsHWtype >= MFX_HW_CNL)   // row aligned
                cod2.NumMbPerSlice = (rand() % heightLCU) * widthLCU;
            else
                cod2.NumMbPerSlice = rand() % m_numLCU;

            if ((m_par.mfx.NumSlice != 0)
                && (m_par.mfx.NumSlice != (m_numLCU + cod2.NumMbPerSlice - 1) / cod2.NumMbPerSlice))
                g_tsStatus.expect(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
            if (   (m_par.mfx.NumSlice != 0)
                && (m_par.mfx.NumSlice != (m_numLCU + cod2.NumMbPerSlice - 1) / cod2.NumMbPerSlice))
                g_tsStatus.expect(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
            m_expected = cod2.NumMbPerSlice;
            Reset();
            m_max = frameNumber;
            m_cur = 0;
            EncodeFrames(frameNumber);
        }
    }

    TS_END;
    return err;
}

TS_REG_TEST_SUITE_CLASS(hevce_co2_max_slice_size_in_ctu);
};
