/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2016 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

namespace avce_max_slice_size
{

typedef void (*callback)(tsExtBufType<mfxVideoParam>&, mfxU32, mfxU32);

class TestSuite : tsVideoEncoder
{
public:
    TestSuite() : tsVideoEncoder(MFX_CODEC_AVC) {}
    ~TestSuite() { }
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    static const char path[];
    static const mfxU32 max_num_ctrl     = 5;
    static const mfxU32 max_num_ctrl_par = 4;
    mfxSession m_session;
    mfxU32 m_repeat;

    enum STREAM_STAGE
    {
          AFTER_INIT  = 0
        , AFTER_RESET = 1
    };

    struct tc_struct
    {
        mfxStatus sts;
        struct tctrl{
            mfxU32 type;
            const tsStruct::Field* field;
            mfxU32 par[max_num_ctrl_par];
        } ctrl[max_num_ctrl];
    };

    static const tc_struct test_case[];

    enum CTRL_TYPE
    {
          STAGE     = 0xF0000000
        , INIT      = 0x80000000
        , RESET     = 0x40000000
        , QUERY     = 0x20000000
        , MFXVPAR   = 1 <<  1
        , EXT_BUF   = 1 <<  2
        , BUFPAR    = 1 <<  3
    };
};

#define EXT_BUF_PAR(eb) tsExtBufTypeToId<eb>::id, sizeof(eb)

const TestSuite::tc_struct TestSuite::test_case[] =
{
    // Query + Init
    //default
    {/*00*/ MFX_ERR_NONE,
        {{QUERY|INIT|BUFPAR, &tsStruct::mfxExtCodingOption2.MaxSliceSize, {1460}},
         {MFXVPAR, &tsStruct::mfxVideoParam.AsyncDepth, {1}},
         {MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, {MFX_RATECONTROL_LA}},
         {BUFPAR, &tsStruct::mfxExtCodingOption2.LookAheadDepth, {1}}}
    },
    {/*01*/ MFX_ERR_NONE,
        {{QUERY|INIT|BUFPAR, &tsStruct::mfxExtCodingOption2.MaxSliceSize, {650}},
         {MFXVPAR, &tsStruct::mfxVideoParam.AsyncDepth, {1}},
         {MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, {MFX_RATECONTROL_LA}},
         {BUFPAR, &tsStruct::mfxExtCodingOption2.LookAheadDepth, {1}}}
    },
    {/*02*/ MFX_ERR_NONE,
        {{QUERY|INIT|BUFPAR, &tsStruct::mfxExtCodingOption2.MaxSliceSize, {100}},
         {MFXVPAR, &tsStruct::mfxVideoParam.AsyncDepth, {1}},
         {MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, {MFX_RATECONTROL_LA}},
         {BUFPAR, &tsStruct::mfxExtCodingOption2.LookAheadDepth, {1}}}
    },
    // wrong AsyncDepth
    {/*03*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
        {{QUERY|INIT|BUFPAR, &tsStruct::mfxExtCodingOption2.MaxSliceSize, {1460}},
         {MFXVPAR, &tsStruct::mfxVideoParam.AsyncDepth, {5}},
         {MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, {MFX_RATECONTROL_LA}},
         {BUFPAR, &tsStruct::mfxExtCodingOption2.LookAheadDepth, {1}}}
    },
    // wrong RateControlMethod
    {/*04*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
        {{QUERY|INIT|BUFPAR, &tsStruct::mfxExtCodingOption2.MaxSliceSize, {1460}},
         {MFXVPAR, &tsStruct::mfxVideoParam.AsyncDepth, {1}},
         {MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, {MFX_RATECONTROL_CBR}},
         {BUFPAR, &tsStruct::mfxExtCodingOption2.LookAheadDepth, {1}}}
    },
    // wrong LADepth
    {/*05*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM,
        {{QUERY|INIT|BUFPAR, &tsStruct::mfxExtCodingOption2.MaxSliceSize, {1460}},
         {MFXVPAR, &tsStruct::mfxVideoParam.AsyncDepth, {1}},
         {MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, {MFX_RATECONTROL_LA}},
         {BUFPAR, &tsStruct::mfxExtCodingOption2.LookAheadDepth, {5}}}
    },
    // NumSlice
    {/*06*/ MFX_ERR_NONE,
        {{QUERY|INIT|BUFPAR, &tsStruct::mfxExtCodingOption2.MaxSliceSize, {1460}},
         {MFXVPAR, &tsStruct::mfxVideoParam.mfx.NumSlice, {30}},
         {MFXVPAR, &tsStruct::mfxVideoParam.AsyncDepth, {1}},
         {MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, {MFX_RATECONTROL_LA}},
         {BUFPAR, &tsStruct::mfxExtCodingOption2.LookAheadDepth, {1}}}
    },
    // NumMbPerSlice
    {/*07*/ MFX_ERR_UNSUPPORTED,
        {{QUERY|INIT|BUFPAR, &tsStruct::mfxExtCodingOption2.MaxSliceSize, {1460}},
         {BUFPAR, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, {30}},
         {MFXVPAR, &tsStruct::mfxVideoParam.AsyncDepth, {1}},
         {MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, {MFX_RATECONTROL_LA}},
         {BUFPAR, &tsStruct::mfxExtCodingOption2.LookAheadDepth, {1}}}
    },

    // Init only
    // NumMbPerSlice
    {/*08*/ MFX_ERR_INVALID_VIDEO_PARAM,
        {{INIT|BUFPAR, &tsStruct::mfxExtCodingOption2.MaxSliceSize, {1460}},
         {BUFPAR, &tsStruct::mfxExtCodingOption2.NumMbPerSlice, {30}},
         {MFXVPAR, &tsStruct::mfxVideoParam.AsyncDepth, {1}},
         {MFXVPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, {MFX_RATECONTROL_LA}},
         {BUFPAR, &tsStruct::mfxExtCodingOption2.LookAheadDepth, {1}}}
    }
};
const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    MFXInit();
    m_session = tsSession::m_session;

    // plugin
    if(m_uid)
        Load();

    // set buffer mfxExtCodingOption2
    mfxExtCodingOption2& ext_params = m_par;

    for(mfxU32 i = 0; i < max_num_ctrl; i++)
    {
        if (tc.ctrl[i].type & MFXVPAR)
        {
            if(tc.ctrl[i].field)
            {
                tsStruct::set(&m_par, *tc.ctrl[i].field, tc.ctrl[i].par[0]);
            }
        }

        if (tc.ctrl[i].type & BUFPAR)
        {
            if(tc.ctrl[i].field)
            {
                tsStruct::set(&ext_params, *tc.ctrl[i].field, tc.ctrl[i].par[0]);
            }
        }
    }

    if (g_tsImpl == MFX_IMPL_SOFTWARE)
        g_tsStatus.expect(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
    else
        g_tsStatus.expect(tc.sts);

    m_par.mfx.FrameInfo.Width  = m_par.mfx.FrameInfo.CropW = 720;
    m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 480;
    m_par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    m_par.mfx.QPI = 0;
    m_par.mfx.QPP = 0;
    m_par.mfx.QPB = 0;
    m_par.mfx.TargetKbps = 600;
    mfxVideoParam out = m_par;

    if (tc.ctrl[0].type & QUERY)
    {
        Query(m_session, &m_par, &out);
    }

    if (g_tsImpl == MFX_IMPL_SOFTWARE)
        g_tsStatus.expect(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
   /* else
        g_tsStatus.expect(tc.sts);*/

    if (tc.ctrl[0].type & INIT)
    {
        Init(m_session, &out);
    }

    if(m_initialized)
    {
        Close();
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(avce_max_slice_size);
}
