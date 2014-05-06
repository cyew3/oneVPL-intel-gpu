#include "ts_encoder.h"
#include "ts_parser.h"

namespace
{

class TestSuite : tsVideoEncoder
{
public:
    TestSuite() : tsVideoEncoder(MFX_CODEC_VP8) {}
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    typedef struct 
    {
        mfxSession          session;
        mfxEncodeCtrl*      pCtrl;
        mfxFrameSurface1*   pSurf;
        mfxBitstream*       pBs;
        mfxSyncPoint*       pSP;
    } EFApar;

    struct tc_par;
    typedef void (TestSuite::*callback)(tc_par&, EFApar*);

    struct tc_par
    {
        callback set_par;
        mfxU32   p0;
        mfxU32   p1;
        mfxU32   p2;
        mfxU32   p3;
    };

    struct tc_struct
    {
        mfxStatus sts;
        tc_par pre_init;
        tc_par pre_encode;
    };

    static const tc_struct test_case[];

    enum
    {
          EFA = 0
        , CTRL
        , SURF
        , BS
        , MFX
    };

    mfxU8* GetP8(EFApar* efa, mfxU32 par_id)
    {
        switch(par_id)
        {
        case EFA:   return (mfxU8*)efa;
        case CTRL:  return (mfxU8*)efa->pCtrl;
        case SURF:  return (mfxU8*)efa->pSurf;
        case BS:    return (mfxU8*)efa->pBs;
        case MFX:   return (mfxU8*)m_pPar;
        default:    return 0;
        }
    }

    void set_par(tc_par& arg, EFApar* p)
    {
        memcpy(GetP8(p, arg.p0) + arg.p1, &arg.p3, TS_MIN(4, arg.p2)); 
    }

    void close_encoder(tc_par& arg, EFApar* p)
    {
        Close();
    }

    void async(tc_par& arg, EFApar* p)
    {
        for(mfxI32 i = 1; i < (mfxI32)arg.p0; i ++)
        {
            EncodeFrameAsync();
            if(g_tsStatus.get() == MFX_ERR_MORE_DATA)
                i --;
            else g_tsStatus.check();
        }
    }

    void COVP8(tc_par& arg, EFApar* p)
    {
        mfxExtCodingOptionVP8& co = m_par;
        co.EnableAutoAltRef       = arg.p0;      
        co.TokenPartitions        = arg.p1;       
        co.EnableMultipleSegments = arg.p2;
    }
};

const TestSuite::tc_struct TestSuite::test_case[] = 
{
    {/* 0*/ MFX_ERR_INVALID_HANDLE,     {}, {&set_par, EFA, offsetof(EFApar, session), sizeof(mfxSession), 0} },
    {/* 1*/ MFX_ERR_NONE,               {}, {&set_par, EFA, offsetof(EFApar, pCtrl),   sizeof(mfxEncodeCtrl*), 0} },
    {/* 2*/ MFX_ERR_MORE_DATA,          {}, {&set_par, EFA, offsetof(EFApar, pSurf),   sizeof(mfxFrameSurface1*), 0} },
    {/* 3*/ MFX_ERR_NULL_PTR,           {}, {&set_par, EFA, offsetof(EFApar, pBs),     sizeof(mfxBitstream*), 0} },
    {/* 4*/ MFX_ERR_NULL_PTR,           {}, {&set_par, EFA, offsetof(EFApar, pSP),     sizeof(mfxSyncPoint*), 0} },
    {/* 5*/ MFX_ERR_NOT_INITIALIZED,    {}, {&close_encoder} },
    {/* 6*/ MFX_ERR_NOT_ENOUGH_BUFFER,  {}, {&set_par, BS, offsetof(mfxBitstream, MaxLength), sizeof(mfxU32), 100} },
    {/* 7*/ MFX_ERR_UNDEFINED_BEHAVIOR, {}, {&set_par, BS, offsetof(mfxBitstream, DataOffset), sizeof(mfxU32), 0xFFFFFFFF} },
    {/* 8*/ MFX_ERR_UNDEFINED_BEHAVIOR, {}, {&set_par, SURF, (offsetof(mfxFrameSurface1, Data) + offsetof(mfxFrameData, Y)), sizeof(mfxU8*), 0} },
    {/* 9*/ 
            MFX_ERR_UNDEFINED_BEHAVIOR, 
            {&set_par, MFX,   offsetof(mfxVideoParam, IOPattern), sizeof(mfxU16), MFX_IOPATTERN_IN_VIDEO_MEMORY}, 
            {&set_par, SURF, (offsetof(mfxFrameSurface1, Data) + offsetof(mfxFrameData, MemId)), sizeof(mfxMemId), 0} 
    },
    {/*10*/ MFX_ERR_NULL_PTR,  {}, {&set_par, BS, offsetof(mfxBitstream, Data), sizeof(mfxU8*), 0} },
    {/*11*/ 
            MFX_ERR_NONE, 
            {&set_par, MFX,   offsetof(mfxVideoParam, AsyncDepth), sizeof(mfxU16), 4}, 
            {&async, 4} 
    },
    {/*12*/ MFX_ERR_NONE,{&set_par, MFX, offsetof(mfxVideoParam, mfx) + offsetof(mfxInfoMFX, CodecProfile), sizeof(mfxU16), MFX_PROFILE_VP8_0}, {} },
    {/*13*/ MFX_ERR_NONE,{&set_par, MFX, offsetof(mfxVideoParam, mfx) + offsetof(mfxInfoMFX, CodecProfile), sizeof(mfxU16), MFX_PROFILE_VP8_1}, {} },
    {/*14*/ MFX_ERR_NONE,{&set_par, MFX, offsetof(mfxVideoParam, mfx) + offsetof(mfxInfoMFX, CodecProfile), sizeof(mfxU16), MFX_PROFILE_VP8_2}, {} },
    {/*15*/ MFX_ERR_NONE,{&set_par, MFX, offsetof(mfxVideoParam, mfx) + offsetof(mfxInfoMFX, CodecProfile), sizeof(mfxU16), MFX_PROFILE_VP8_3}, {} },
    {/*16*/ MFX_ERR_NONE,{&COVP8, MFX_CODINGOPTION_UNKNOWN, MFX_TOKENPART_VP8_1, MFX_CODINGOPTION_UNKNOWN}, {} },
    {/*17*/ MFX_ERR_NONE,{&COVP8, MFX_CODINGOPTION_UNKNOWN, MFX_TOKENPART_VP8_2, MFX_CODINGOPTION_UNKNOWN}, {} },
    {/*18*/ MFX_ERR_NONE,{&COVP8, MFX_CODINGOPTION_UNKNOWN, MFX_TOKENPART_VP8_4, MFX_CODINGOPTION_UNKNOWN}, {} },
    {/*19*/ MFX_ERR_NONE,{&COVP8, MFX_CODINGOPTION_UNKNOWN, MFX_TOKENPART_VP8_8, MFX_CODINGOPTION_UNKNOWN}, {} },
    {/*20*/ MFX_ERR_NONE,{&COVP8, MFX_CODINGOPTION_UNKNOWN, MFX_TOKENPART_VP8_UNKNOWN, MFX_CODINGOPTION_ON}, {} },
    {/*21*/ MFX_ERR_NONE,{&COVP8, MFX_CODINGOPTION_UNKNOWN, MFX_TOKENPART_VP8_UNKNOWN, MFX_CODINGOPTION_OFF}, {} },
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);


int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    tc_struct tc = test_case[id];

    if(tc.pre_init.set_par)
    {
        (this->*tc.pre_init.set_par)(tc.pre_init, 0);
    }

    AllocSurfaces();
    AllocBitstream();

    EFApar par = {m_session, m_pCtrl, GetSurface(), m_pBitstream, m_pSyncPoint};
    
    if(tc.pre_encode.set_par)
    {
        (this->*tc.pre_encode.set_par)(tc.pre_encode, &par);
    }

    g_tsStatus.expect(tc.sts);
    for(;;)
    {
        EncodeFrameAsync(par.session, par.pCtrl, par.pSurf, par.pBs, par.pSP);

        if(g_tsStatus.get() == MFX_ERR_MORE_DATA && par.pSurf && g_tsStatus.m_expected >= 0)
        {
            par.pSurf = GetSurface();
            continue;
        }
        break;
    }
    g_tsStatus.check();

    if(g_tsStatus.get() >= 0)
    {
        SyncOperation();
        tsParserVP8 p(m_bitstream);

        for(mfxU32 i = 0; i < m_par.AsyncDepth; i++)
        {
            tsParserVP8::UnitType& hdr = p.ParseOrDie();

            EXPECT_EQ(m_par.mfx.CodecProfile - 1, hdr.udc->version);

            if(!hdr.udc->key_frame)
            {
                EXPECT_EQ(m_par.mfx.FrameInfo.Width, hdr.udc->Width);
                EXPECT_EQ(m_par.mfx.FrameInfo.Height, hdr.udc->Height);
            }

            if((mfxExtCodingOptionVP8*)m_par)
            {
                mfxExtCodingOptionVP8& co = m_par;
                if(co.TokenPartitions)
                {
                    EXPECT_EQ(co.TokenPartitions - 1, hdr.udc->fh.log2_nbr_of_dct_partitions);
                }
                if(co.EnableMultipleSegments)
                {
                    EXPECT_EQ(mfxU32(co.EnableMultipleSegments == MFX_CODINGOPTION_ON), hdr.udc->fh.segmentation_enabled);
                }
            }
        }

    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(vp8e_encode_frame_async);
};
