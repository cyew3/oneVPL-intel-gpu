#include "ts_encoder.h"
#include "ts_struct.h"
#include "mfxcamera.h"

#define EXT_BUF_PAR(eb) tsExtBufTypeToId<eb>::id, sizeof(eb)

namespace avce_2014r2_unsupport
{
    
typedef void (*callback)(tsExtBufType<mfxVideoParam>&, mfxU32, mfxU32);

void ext_buf(tsExtBufType<mfxVideoParam>& par, mfxU32 id, mfxU32 size)
{
    par.AddExtBuffer(id, size); 
}

class TestSuite : tsVideoEncoder
{
public:
    TestSuite() : tsVideoEncoder(MFX_CODEC_AVC) {}
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    static const mfxU32 n_par = 6;

    typedef struct 
    {
        mfxSession          session;
        mfxEncodeCtrl*      pCtrl;
        mfxFrameSurface1*   pSurf;
        mfxBitstream*       pBs;
        mfxSyncPoint*       pSP;
    } EFApar;

    enum
    {
        FOURCC = 1,
        FRAMEDATA,
        Y16,
        ExtAVCEncodedFrameInfo,
        //
        NULL_PARAMS,
    };

    struct tc_struct
    {
        mfxStatus sts_query;
        mfxStatus sts_init;
        mfxStatus sts_reset;
        mfxU32 without_feature;
        mfxU32 feature;
        struct f_pair 
        {
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[n_par];
        struct 
        {
            callback func;
            mfxU32 buf;
            mfxU32 size;
        } set_buf;
        struct f_pair111
        {
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_co2[1];
    };

    static const tc_struct test_case[];
};

const TestSuite::tc_struct TestSuite::test_case[] = 
{
    //Unsupported parameters in mfxFrameInfo
    {/* 0*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, 1, 0, 
                                 {{&tsStruct::mfxVideoParam.mfx.FrameInfo.BitDepthLuma, 10}}},
    {/* 1*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, 1, 0, 
                                 {{&tsStruct::mfxVideoParam.mfx.FrameInfo.BitDepthChroma, 10}}},
    {/* 2*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NOT_INITIALIZED, 0, 0, 
                                 {{&tsStruct::mfxVideoParam.mfx.FrameInfo.BitDepthLuma, 10}}},
    {/* 3*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NOT_INITIALIZED, 0, 0, 
                                 {{&tsStruct::mfxVideoParam.mfx.FrameInfo.BitDepthChroma, 10}}},

    // Unsupported FourCC
    // if without feature - return unsupported, not incompatible
    {/* 4*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NOT_INITIALIZED, 0, 0, 
                                   {{&tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_BGR4}}},
    {/* 5*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NOT_INITIALIZED, 0, 0, 
                                 {{&tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_P010}}},
    {/* 6*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NOT_INITIALIZED, 0, 0, 
                                 {{&tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_A2RGB10}}},
    {/* 7*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NOT_INITIALIZED, 0, 0, 
                                 {{&tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_ARGB16}}},
    {/* 8*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NOT_INITIALIZED, 0, 0, 
                                 {{&tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_R16}}},
    {/* 9*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, 1, 0, 
                                 {{&tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_BGR4}}},
    {/*10*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, 1, 0, 
                                 {{&tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_P010}}},
    {/*11*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, 1, 0, 
                                 {{&tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_A2RGB10}}},
    {/*12*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, 1, 0, 
                                 {{&tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_ARGB16}}},
    {/*13*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, 1, 0, 
                                 {{&tsStruct::mfxVideoParam.mfx.FrameInfo.FourCC, MFX_FOURCC_R16}}},

    //Unsupported surfaces
    // encodeasync returns not enough buffer
    // buffer defined only in reset
    {/*14*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, 1, Y16, {}},


    // Unsupported mfxExtAVCRefLists buffer - there's no such buffer

    // Unsupported params in mfxExtCodingOption2
    {/*15*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NOT_INITIALIZED, 0, 0, 
                                 {}, {ext_buf, EXT_BUF_PAR(mfxExtCodingOption2)}, 
                                 {{&tsStruct::mfxExtCodingOption2.MinQPI, 1}}},
    {/*16*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NOT_INITIALIZED, 0, 0, 
                                     {}, {ext_buf, EXT_BUF_PAR(mfxExtCodingOption2)}, 
                                     {{&tsStruct::mfxExtCodingOption2.MaxQPI, 1}}},
    {/*17*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NOT_INITIALIZED, 0, 0, 
                                     {}, {ext_buf, EXT_BUF_PAR(mfxExtCodingOption2)}, 
                                     {{&tsStruct::mfxExtCodingOption2.MinQPP, 1}}},
    {/*18*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NOT_INITIALIZED, 0, 0, 
                                     {}, {ext_buf, EXT_BUF_PAR(mfxExtCodingOption2)}, 
                                     {{&tsStruct::mfxExtCodingOption2.MaxQPP, 1}}},
    {/*19*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NOT_INITIALIZED, 0, 0, 
                                     {}, {ext_buf, EXT_BUF_PAR(mfxExtCodingOption2)}, 
                                     {{&tsStruct::mfxExtCodingOption2.MinQPB, 1}}},
    {/*20*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NOT_INITIALIZED, 0, 0, 
                                     {}, {ext_buf, EXT_BUF_PAR(mfxExtCodingOption2)}, 
                                     {{&tsStruct::mfxExtCodingOption2.MaxQPB, 1}}},
    {/*21*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NOT_INITIALIZED, 0, 0, 
                                     {}, {ext_buf, EXT_BUF_PAR(mfxExtCodingOption2)}, 
                                     {{&tsStruct::mfxExtCodingOption2.FixedFrameRate, 1}}},

    // mfxExtAVCEncodedFrameInfo has no SecondFieldOffset variable
    {/*22*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NOT_INITIALIZED, 0, ExtAVCEncodedFrameInfo, 
                                     {}, {ext_buf, EXT_BUF_PAR(mfxExtAVCEncodedFrameInfo)}, 
                                     {{&tsStruct::mfxExtAVCEncodedFrameInfo.SecondFieldOffset, 1}}},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    MFXInit();
    if(tc.set_buf.func) // set ExtBuffer
    {
        (*tc.set_buf.func)(this->m_par, tc.set_buf.buf, tc.set_buf.size);
    }

    if (tc.set_co2[0].f) 
    {
        if (tc.feature == ExtAVCEncodedFrameInfo) 
        {
            mfxExtAVCEncodedFrameInfo* co2 = (mfxExtAVCEncodedFrameInfo*) m_par.GetExtBuffer(MFX_EXTBUFF_ENCODED_FRAME_INFO);
            tsStruct::set(co2, *tc.set_co2[0].f, tc.set_co2[0].v);
        }
        else 
        {
            mfxExtCodingOption2* co2 = (mfxExtCodingOption2*) m_par.GetExtBuffer(MFX_EXTBUFF_CODING_OPTION2);
            tsStruct::set(co2, *tc.set_co2[0].f, tc.set_co2[0].v);
        }
    }

    if (tc.without_feature == 0)
    {
        for(mfxU32 i = 0; i < n_par; i++)
        {
            if(tc.set_par[i].f)
            {
                tsStruct::set(m_pPar, *tc.set_par[i].f, tc.set_par[i].v);
            }
        }
    }

    //mfxVideoParam* par_out = new mfxVideoParam;
    tsExtBufType<mfxVideoParam> par_out;
    par_out.mfx.CodecId = m_pPar->mfx.CodecId;
    if(tc.set_buf.func) // set ExtBuffer
    {
        (*tc.set_buf.func)(par_out, tc.set_buf.buf, tc.set_buf.size);
    }
    //memcpy(par_out, m_pPar, sizeof(mfxVideoParam));

    g_tsStatus.expect(tc.sts_query);
    Query(m_session, m_pPar, &par_out);

    if (tc.sts_query == MFX_ERR_UNSUPPORTED) 
    { 
        for(mfxU32 i = 0; i < n_par; i++) 
        { 
            if(tc.set_par[i].f) 
            { 
                tsStruct::check_eq(&par_out, *tc.set_par[i].f, 0); 
            } 
        } 

        if (tc.set_co2[0].f) 
        {
            if (tc.feature == ExtAVCEncodedFrameInfo) 
            {
                mfxExtAVCEncodedFrameInfo* co2 = (mfxExtAVCEncodedFrameInfo*) m_par.GetExtBuffer(MFX_EXTBUFF_ENCODED_FRAME_INFO);
                tsStruct::check_eq(co2, *tc.set_co2[0].f, 0);
            }
            else 
            {
                mfxExtCodingOption2* co2 = (mfxExtCodingOption2*) m_par.GetExtBuffer(MFX_EXTBUFF_CODING_OPTION2);
                tsStruct::check_eq(co2, *tc.set_co2[0].f, 0);
            }
        }
    } 


    g_tsStatus.expect(tc.sts_init);
    Init(m_session, m_pPar);

    if (tc.without_feature == 1)
    {
        for(mfxU32 i = 0; i < n_par; i++)
        {
            if(tc.set_par[i].f)
            {
                tsStruct::set(m_pPar, *tc.set_par[i].f, tc.set_par[i].v);
            }
        }
    }

    if (tc.feature == Y16)
    {
        //hack: let's pretend that it is a valid Y16 surface
        AllocSurfaces();
        AllocBitstream();
        EFApar par = {m_session, m_pCtrl, GetSurface(), m_pBitstream, m_pSyncPoint};
        par.pSurf = GetSurface();
        mfxFrameSurface1 pSurfTmp  = *par.pSurf;

        par.pSurf->Info.FourCC = MFX_FOURCC_P010;
        par.pSurf->Data.Y16 = (mfxU16*) par.pSurf->Data.Y;
        par.pSurf->Data.U16 = (mfxU16*) par.pSurf->Data.U;
        par.pSurf->Data.V16 = (mfxU16*) par.pSurf->Data.V;
        par.pSurf->Data.Y = 0;
        par.pSurf->Data.U = 0;
        par.pSurf->Data.V = 0;

        g_tsStatus.expect(tc.sts_reset);
        EncodeFrameAsync(par.session, par.pCtrl, par.pSurf, par.pBs, par.pSP);
        g_tsStatus.check();

        *par.pSurf = pSurfTmp;
    }

    if (tc.feature != Y16) 
    {
        g_tsStatus.expect(tc.sts_reset);
        Reset(m_session, m_pPar);
    }


    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(avce_2014r2_unsupport);

}