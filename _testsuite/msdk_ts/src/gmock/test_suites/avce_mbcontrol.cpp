/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2017 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

//#include <memory>

using namespace BS_AVC2;

#define EXT_BUF_PAR(eb) tsExtBufTypeToId<eb>::id, sizeof(eb)

namespace avce_mbcontrol
{
//#define DEBUG_OUT

class MyBitstreamProcessor : public tsBitstreamProcessor
{
public:
    MyBitstreamProcessor() :tsBitstreamProcessor()
    {
#ifdef DEBUG_OUT
        m_pImpl.reset(new tsBitstreamWriter("avce_mbcontrol.h264"));
#else
        m_pImpl.reset(new tsBitstreamErazer());
#endif
    }
    
    void SetCaseNumber(unsigned int id)
    {
#ifdef DEBUG_OUT
        std::string fname = "avce_mbcontrol_";
        fname += std::to_string(id);
        fname += ".h264";
        m_pImpl.reset(new tsBitstreamWriter(fname.c_str()));
#else
            m_pImpl.reset(new tsBitstreamErazer());
#endif
    }

    virtual mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        return m_pImpl->ProcessBitstream(bs, nFrames);
    }
private:
    std::auto_ptr<tsBitstreamProcessor> m_pImpl;
};

class TestSuite : public tsVideoEncoder, public tsSurfaceProcessor, public tsBitstreamProcessor, public tsParserAVC2
{
private:
    struct Ctrl
    {
        tsExtBufType<mfxEncodeCtrl> ctrl;
    };

    std::map<mfxU32, Ctrl> m_ctrl;
    mfxU32 m_fo;
    bool mbqp_on;
    mfxU32 mode;
    std::auto_ptr<tsRawReader> m_reader;
    MyBitstreamProcessor m_writer;

    //tsBitstreamWriter m_writer;
public:
    TestSuite()
        : tsVideoEncoder(MFX_CODEC_AVC)
        , tsParserAVC2(INIT_MODE_PARSE_SD)
        , m_fo(0)
        , mbqp_on(true)
        , mode(0)
        , m_reader(NULL)
        //, m_writer("debug.264")
    {
        set_trace_level(0);
        srand(0);

        m_filler = this;
        m_bs_processor = this;
        m_par.AsyncDepth = 1;
    }

    ~TestSuite() {}

    enum
    {
        MFXPAR = 1,
        MFXEC3 = 2,
        MFXEC3_EXPECTED = 3,
        MFXEC2 = 4
    };
    enum
    {
        INIT               = 0x1,
        QUERY              = 0x2,
        ENCODE             = 0x4 | 0x1 ,
        CHECK_EXPECTATION  = 0x8,
    };


    struct ForceIntraPattern
    {
        mfxU8* array;
        size_t h;
        size_t w;
    };

    static const unsigned int n_cases;
    int RunTest(unsigned int id);
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
        ForceIntraPattern pattern; // make sence for encode only
    };
    static const tc_struct test_case[];
    tc_struct tc;

    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        m_reader->ProcessSurface(s);
        mfxU32 wMB = ((m_par.mfx.FrameInfo.Width + 15) / 16);
        mfxU32 hMB = ((m_par.mfx.FrameInfo.Height + 15) / 16);
        mfxU32 numMB = wMB * hMB;

        Ctrl& ctrl = m_ctrl[m_fo];
        m_pCtrl = &ctrl.ctrl;

        if (tc.pattern.array != NULL)
        {
            ctrl.ctrl.AddExtBuffer<mfxExtMBForceIntra>(sizeof(mfxU8)*numMB);
            mfxExtMBForceIntra* pEMBC = ctrl.ctrl;
            pEMBC->MapSize = numMB;
            pEMBC->Map = (mfxU8*)(pEMBC + 1);

            mfxU8* pMb = pEMBC->Map;
            for (mfxU32 y = 0; y < hMB; y++)
                for (mfxU32 x = 0; x < wMB; x++, pMb++)
                    *pMb = tc.pattern.array[(y % tc.pattern.h)*tc.pattern.w + (x % tc.pattern.w)];

        }

        s.Data.TimeStamp = s.Data.FrameOrder = m_fo++;

        return MFX_ERR_NONE;
    }

    inline bool IsIntra(MB* mb) { return mb->MbType <= I_PCM; };

    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        SetBuffer0(bs);
        UnitType& hdr = ParseOrDie();

        Ctrl& ctrl = m_ctrl[bs.TimeStamp];
        mfxExtMBForceIntra* pEMBC = ctrl.ctrl;
        if (pEMBC == NULL)
        {
            // skip check
            return m_writer.ProcessBitstream(bs, nFrames);
        }
        mfxU8* pMb = pEMBC->Map;
        bool CheckOK = true;
        MB* badMb = NULL;

        g_tsLog << "Frame:"<< bs.TimeStamp <<"\n";
        g_tsLog << "Actual:"<< "\n";
        for (Bs32u iNALU = 0; iNALU < hdr.NumUnits; iNALU++)
        {
            auto& nalu = *hdr.nalu[iNALU];

            if (nalu.nal_unit_type != SLICE_IDR
                && nalu.nal_unit_type != SLICE_NONIDR)
                continue;

            auto mb = nalu.slice->mb;

            while (mb)
            {
                if (!mb->x && mb->y)
                    g_tsLog << "\n";
                bool isIntra = IsIntra(mb);
                g_tsLog << isIntra /* << "(" << pMb->IntraMode <<")" */<< " ";

                if (CheckOK && *pMb && isIntra == false)
                {
                    CheckOK = false;
                    badMb = mb;
                }

                mb = mb->next;
                pMb++;
            }
            g_tsLog << "\n";
        }
        g_tsLog << "Expected:" << "\n";
        {
            mfxU8 *pMb = pEMBC->Map;
            mfxU32 wMB = ((m_par.mfx.FrameInfo.Width + 15) / 16);
            mfxU32 hMB = ((m_par.mfx.FrameInfo.Height + 15) / 16);
            for (mfxU32 y = 0; y < hMB; y++)
            {
                for (mfxU32 x = 0; x < wMB; x++, pMb++)
                {
                    g_tsLog << *pMb << " ";
                }
                g_tsLog << "\n";
            }

        }

        EXPECT_TRUE(CheckOK) << "ERROR: MB is not encoded as Intra" << std::endl
            << "DETAILS:" << " in Frame #" << (bs.TimeStamp) << " MB #" << badMb->Addr << std::endl;

        m_ctrl.erase((mfxU32)bs.TimeStamp);


        return m_writer.ProcessBitstream(bs, nFrames);
    }
};

mfxU8 pattern1[7][20] = {
    { 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0 },
    { 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0 },
    { 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0 },
    { 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0 },
    { 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0 },
    { 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0 },
    { 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0, 0,0,0,0,0 },
};

mfxU8 Zpat10x10[10][10] = {
    { 1,1,1,1,1, 1,1,1,1,1, },
    { 0,0,0,0,0, 0,0,0,1,0, },
    { 0,0,0,0,0, 0,0,1,0,0, },
    { 0,0,0,0,0, 0,1,0,0,0, },
    { 0,0,0,0,0, 1,0,0,0,0, },
    { 0,0,0,0,1, 0,0,0,0,0, },
    { 0,0,0,1,0, 0,0,0,0,0, },
    { 0,0,1,0,0, 0,0,0,0,0, },
    { 0,1,0,0,0, 0,0,0,0,0, },
    { 1,1,1,1,1, 1,1,1,1,1, },
};

mfxU8 Xpat10x10[10][10] = {
    { 1,0,0,0,0, 0,0,0,0,1, },
    { 0,1,0,0,0, 0,0,0,1,0, },
    { 0,0,1,0,0, 0,0,1,0,0, },
    { 0,0,0,1,0, 0,1,0,0,0, },
    { 0,0,0,0,1, 1,0,0,0,0, },
    { 0,0,0,0,1, 1,0,0,0,0, },
    { 0,0,0,1,0, 0,1,0,0,0, },
    { 0,0,1,0,0, 0,0,1,0,0, },
    { 0,1,0,0,0, 0,0,0,1,0, },
    { 1,0,0,0,0, 0,0,0,0,1, },
};


#define mfx_PicStruct tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct
const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/*00/00*/ MFX_ERR_NONE, QUERY | CHECK_EXPECTATION,{
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
        { MFXEC3, &tsStruct::mfxExtCodingOption3.EnableMBForceIntra, MFX_CODINGOPTION_UNKNOWN },
        { MFXEC3_EXPECTED, &tsStruct::mfxExtCodingOption3.EnableMBForceIntra, MFX_CODINGOPTION_UNKNOWN }
    }
    },
    {/*00/01*/ MFX_ERR_NONE, INIT | QUERY | CHECK_EXPECTATION,{
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
        { MFXEC3, &tsStruct::mfxExtCodingOption3.EnableMBForceIntra, MFX_CODINGOPTION_OFF },
        { MFXEC3_EXPECTED, &tsStruct::mfxExtCodingOption3.EnableMBForceIntra, MFX_CODINGOPTION_OFF }
    }
    },
    {/*00/02*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QUERY | CHECK_EXPECTATION,{
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
        { MFXEC3, &tsStruct::mfxExtCodingOption3.EnableMBForceIntra, 1 },
        { MFXEC3_EXPECTED, &tsStruct::mfxExtCodingOption3.EnableMBForceIntra, 0 }
    }
    },
#ifdef WIN32
    {/*00/03*/ MFX_ERR_NONE, INIT | QUERY | CHECK_EXPECTATION,{
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
        { MFXEC3, &tsStruct::mfxExtCodingOption3.EnableMBForceIntra, MFX_CODINGOPTION_ON }, 
        { MFXEC3_EXPECTED, &tsStruct::mfxExtCodingOption3.EnableMBForceIntra, MFX_CODINGOPTION_ON }
    }
    },
    {/*00/04*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT | CHECK_EXPECTATION,{
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
        { MFXEC3, &tsStruct::mfxExtCodingOption3.EnableMBForceIntra, 1 },
        { MFXEC3_EXPECTED, &tsStruct::mfxExtCodingOption3.EnableMBForceIntra, MFX_CODINGOPTION_OFF }
    }
    },

    {/*00/05*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QUERY | CHECK_EXPECTATION,{
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.LowPower, MFX_CODINGOPTION_ON },
        { MFXEC3, &tsStruct::mfxExtCodingOption3.EnableMBForceIntra, MFX_CODINGOPTION_ON },
        { MFXEC3_EXPECTED, &tsStruct::mfxExtCodingOption3.EnableMBForceIntra, 0 }
    }
    },

    {/*00/06*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT  | CHECK_EXPECTATION,{
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.LowPower, MFX_CODINGOPTION_ON },
        { MFXEC3, &tsStruct::mfxExtCodingOption3.EnableMBForceIntra, MFX_CODINGOPTION_ON },
        { MFXEC3_EXPECTED, &tsStruct::mfxExtCodingOption3.EnableMBForceIntra, MFX_CODINGOPTION_OFF }
    }
    },

    {/*01/07*/ MFX_ERR_NONE, INIT | QUERY | ENCODE | CHECK_EXPECTATION,{
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
        { MFXEC3, &tsStruct::mfxExtCodingOption3.EnableMBForceIntra, MFX_CODINGOPTION_ON },
        { MFXEC3_EXPECTED, &tsStruct::mfxExtCodingOption3.EnableMBForceIntra, MFX_CODINGOPTION_ON }
        },
        { &(Zpat10x10[0][0]),10,10 }
    },

    {/*01/08*/ MFX_ERR_NONE, INIT | QUERY | ENCODE | CHECK_EXPECTATION,{
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
        { MFXEC3, &tsStruct::mfxExtCodingOption3.EnableMBForceIntra, MFX_CODINGOPTION_ON },
        { MFXEC3_EXPECTED, &tsStruct::mfxExtCodingOption3.EnableMBForceIntra, MFX_CODINGOPTION_ON }
        },
        { &(Xpat10x10[0][0]),10,10 }
    },
    {/*02/09*/ MFX_ERR_NONE, INIT | QUERY | ENCODE | CHECK_EXPECTATION,{
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
        { MFXEC3, &tsStruct::mfxExtCodingOption3.EnableMBForceIntra, MFX_CODINGOPTION_ON },
        { MFXEC3_EXPECTED, &tsStruct::mfxExtCodingOption3.EnableMBForceIntra, MFX_CODINGOPTION_ON }
    },
    },
    {/*03/10*/ MFX_ERR_NONE, INIT | QUERY | ENCODE | CHECK_EXPECTATION,{
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_QVBR },
        { MFXEC3, &tsStruct::mfxExtCodingOption3.EnableMBForceIntra, MFX_CODINGOPTION_ON },
        { MFXEC3_EXPECTED, &tsStruct::mfxExtCodingOption3.EnableMBForceIntra, MFX_CODINGOPTION_ON }
    },
    { &(Zpat10x10[0][0]),10,10 }
    },

    {/*03/11*/ MFX_ERR_NONE, INIT | QUERY | ENCODE | CHECK_EXPECTATION,{
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_QVBR },
        { MFXEC3, &tsStruct::mfxExtCodingOption3.EnableMBForceIntra, MFX_CODINGOPTION_ON },
        { MFXEC3_EXPECTED, &tsStruct::mfxExtCodingOption3.EnableMBForceIntra, MFX_CODINGOPTION_ON }
    },
    { &(Xpat10x10[0][0]),10,10 }
    },

    {/*03/12*/ MFX_ERR_NONE, INIT | QUERY | ENCODE | CHECK_EXPECTATION,{
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_QVBR },
        { MFXEC3, &tsStruct::mfxExtCodingOption3.EnableMBForceIntra, MFX_CODINGOPTION_ON },
        { MFXEC3_EXPECTED, &tsStruct::mfxExtCodingOption3.EnableMBForceIntra, MFX_CODINGOPTION_ON }
    },
    },

    {/*03/13*/ MFX_ERR_NONE, INIT | QUERY | ENCODE | CHECK_EXPECTATION,{
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_QVBR },
        { MFXEC3, &tsStruct::mfxExtCodingOption3.EnableMBForceIntra, MFX_CODINGOPTION_ON },
        { MFXEC2, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_ON },
        { MFXEC3_EXPECTED, &tsStruct::mfxExtCodingOption3.EnableMBForceIntra, MFX_CODINGOPTION_ON }
    },
    { &(Zpat10x10[0][0]),10,10 }
    },

    {/*03/14*/ MFX_ERR_NONE, INIT | QUERY | ENCODE | CHECK_EXPECTATION,{
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_QVBR },
        { MFXEC3, &tsStruct::mfxExtCodingOption3.EnableMBForceIntra, MFX_CODINGOPTION_ON },
        { MFXEC2, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_ON },
        { MFXEC3_EXPECTED, &tsStruct::mfxExtCodingOption3.EnableMBForceIntra, MFX_CODINGOPTION_ON }
    },
    { &(Xpat10x10[0][0]),10,10 }
    },

    {/*03/15*/ MFX_ERR_NONE, INIT | QUERY | ENCODE | CHECK_EXPECTATION,{
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_QVBR },
        { MFXEC3, &tsStruct::mfxExtCodingOption3.EnableMBForceIntra, MFX_CODINGOPTION_ON },
        { MFXEC2, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_ON },
        { MFXEC3_EXPECTED, &tsStruct::mfxExtCodingOption3.EnableMBForceIntra, MFX_CODINGOPTION_ON }
    },
    },

    {/*04/16*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QUERY | CHECK_EXPECTATION,{
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct , MFX_PICSTRUCT_FIELD_TFF },
        { MFXEC3, &tsStruct::mfxExtCodingOption3.EnableMBForceIntra, MFX_CODINGOPTION_ON },
        { MFXEC3_EXPECTED, &tsStruct::mfxExtCodingOption3.EnableMBForceIntra, 0 }
    }
    },

    {/*04/17*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT | CHECK_EXPECTATION,{
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct , MFX_PICSTRUCT_FIELD_TFF },
        { MFXEC3, &tsStruct::mfxExtCodingOption3.EnableMBForceIntra, MFX_CODINGOPTION_ON },
        { MFXEC3_EXPECTED, &tsStruct::mfxExtCodingOption3.EnableMBForceIntra, MFX_CODINGOPTION_OFF }
    }
    },
    {/*04/17*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QUERY | CHECK_EXPECTATION,{
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct , MFX_PICSTRUCT_FIELD_BFF },
        { MFXEC3, &tsStruct::mfxExtCodingOption3.EnableMBForceIntra, MFX_CODINGOPTION_ON },
        { MFXEC3_EXPECTED, &tsStruct::mfxExtCodingOption3.EnableMBForceIntra, 0 }
    }
    },

    {/*04/18*/ MFX_ERR_NONE, INIT | QUERY | CHECK_EXPECTATION,{
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_QVBR },
        { MFXEC3, &tsStruct::mfxExtCodingOption3.EnableMBForceIntra, MFX_CODINGOPTION_ON },
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct , MFX_PICSTRUCT_UNKNOWN },
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.NumRefFrame, 2},
        { MFXEC3_EXPECTED, &tsStruct::mfxExtCodingOption3.EnableMBForceIntra, MFX_CODINGOPTION_ON }
    },
    },
#else
    {/*00/03*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, QUERY | CHECK_EXPECTATION,{
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
        { MFXEC3, &tsStruct::mfxExtCodingOption3.EnableMBForceIntra, MFX_CODINGOPTION_ON },
        { MFXEC3_EXPECTED, &tsStruct::mfxExtCodingOption3.EnableMBForceIntra, 0 }
    }
    },
    {/*00/04*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, INIT | CHECK_EXPECTATION,{
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
        { MFXEC3, &tsStruct::mfxExtCodingOption3.EnableMBForceIntra, MFX_CODINGOPTION_ON },
        { MFXEC3_EXPECTED, &tsStruct::mfxExtCodingOption3.EnableMBForceIntra, MFX_CODINGOPTION_OFF }
    }
    },
#endif
};

#undef mfx_PicStruct

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
 
    TS_START;
    tc = test_case[id];
    m_writer.SetCaseNumber(id);
 
    mode = tc.mode;
    tsExtBufType<mfxVideoParam> out_par;

    MFXInit();

    m_par.AsyncDepth = 1;
    m_par.mfx.GopRefDist = 1;
    m_par.mfx.NumRefFrame = 1;
    m_par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    m_par.mfx.FrameInfo.Width = 352;
    m_par.mfx.FrameInfo.Height = 288;
    m_par.mfx.FrameInfo.CropW = 352;
    m_par.mfx.FrameInfo.CropH = 288;

    m_reader.reset( new tsRawReader(g_tsStreamPool.Get("forBehaviorTest/foreman_cif.nv12"), m_par.mfx.FrameInfo, 30));
    g_tsStreamPool.Reg();

    SETPARS(m_pPar, MFXPAR);
    mfxExtCodingOption3& cod3 = m_par;
    SETPARS(&cod3, MFXEC3);
    mfxExtCodingOption2& cod2 = m_par;
    SETPARS(&cod2, MFXEC2);
    mfxExtCodingOption3 cod3_expected = {};
    SETPARS(&cod3_expected, MFXEC3_EXPECTED);
    set_brc_params(&m_par);
    g_tsStatus.expect(tc.sts);


    if (tc.mode & QUERY)
    {
        SETPARS(m_pPar, MFXPAR);
        SETPARS(&cod3, MFXEC3);
        SETPARS(&cod2, MFXEC2);
        SETPARS(&cod3_expected, MFXEC3_EXPECTED);
        g_tsStatus.expect(tc.sts);

        out_par = m_par;
        m_pParOut = &out_par;
        Query();

        if (tc.sts >= MFX_ERR_NONE && mode & CHECK_EXPECTATION)
        {
            mfxExtCodingOption3& cod3_actual = out_par;
            EXPECT_EQ(cod3_expected.EnableMBForceIntra, cod3_actual.EnableMBForceIntra)
                << "ERROR: QUERY EXPECT_EQ( expected EnableMBForceIntra:" << cod3_expected.EnableMBForceIntra << ", actual EnableMBForceIntra:" << cod3_actual.EnableMBForceIntra << ") --";
        }
    }

    if (tc.mode & INIT)
    {
        SETPARS(m_pPar, MFXPAR);
        SETPARS(&cod3, MFXEC3);
        SETPARS(&cod2, MFXEC2);
        SETPARS(&cod3_expected, MFXEC3_EXPECTED);
        g_tsStatus.expect(tc.sts);

        Init();

        if (tc.sts >= MFX_ERR_NONE && mode & CHECK_EXPECTATION)
        {
            out_par = m_par;
            GetVideoParam(m_session, &out_par);
            mfxExtCodingOption3& cod3_actual = out_par;
            EXPECT_EQ(cod3_expected.EnableMBForceIntra, cod3_actual.EnableMBForceIntra)
                << "ERROR: QUERY EXPECT_EQ( expected EnableMBForceIntra:" << cod3_expected.EnableMBForceIntra << ", actual EnableMBForceIntra:" << cod3_actual.EnableMBForceIntra << ") --";
        }
    }

    if ( (tc.mode & ENCODE) == ENCODE)
    {
        AllocBitstream();
        tsSurfaceProcessor::m_max = 30; // to auto-drain buffered frames before reset
        EncodeFrames(m_max);
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(avce_mbcontrol);
};
