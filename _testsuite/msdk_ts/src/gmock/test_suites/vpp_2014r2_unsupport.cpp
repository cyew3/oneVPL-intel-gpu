#include "ts_vpp.h"
#include "ts_alloc.h"
#include "ts_struct.h"
#include "mfxcamera.h"

#define EXT_BUF_PAR(eb) tsExtBufTypeToId<eb>::id, sizeof(eb)

namespace vpp_2014r2_unsupport
{
    
typedef void (*callback)(tsExtBufType<mfxVideoParam>&, mfxU32, mfxU32);

void ext_buf(tsExtBufType<mfxVideoParam>& par, mfxU32 id, mfxU32 size)
{
    par.AddExtBuffer(id, size); 
}

class TestSuite : tsVideoVPP
{
public:
    TestSuite() : tsVideoVPP() {}
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    typedef struct 
    {
        mfxSession          session;
        mfxFrameSurface1*   pSurfIn;
        mfxFrameSurface1*   pSurfOut;
        mfxSyncPoint*       pSP;
    } EFApar;
    static const mfxU32 n_par = 6;

    enum
    {
        FOURCC = 1,
        FRAMEDATA,
        Y16,
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
    };

    static const tc_struct test_case[];
};

const TestSuite::tc_struct TestSuite::test_case[] = 
{
    // Unsupported FourCC
    {/* 0*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NOT_INITIALIZED, 0, 0, 
                                   {{&tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_BGR4}, 
                                    {&tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_BGR4}}},
    {/* 1*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NOT_INITIALIZED, 0, 0, 
                                 {{&tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010}, 
                                  {&tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_P010}}},
    {/* 2*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NOT_INITIALIZED, 0, 0, 
                                 {{&tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_A2RGB10}, 
                                  {&tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10}}},
    {/* 3*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NOT_INITIALIZED, 0, 0, 
                                 {{&tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_ARGB16}, 
                                  {&tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_ARGB16}}},
    {/* 4*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NOT_INITIALIZED, 0, 0, 
                                 {{&tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_R16}, 
                                  {&tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_R16}}},
    {/* 5*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, 1, 0, 
                                 {{&tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_BGR4}, 
                                  {&tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_BGR4}}},
    {/* 6*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, 1, 0, 
                                 {{&tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_P010}, 
                                  {&tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_P010}}},
    {/* 7*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, 1, 0, 
                                 {{&tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_A2RGB10}, 
                                  {&tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_A2RGB10}}},
    {/* 8*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, 1, 0, 
                                 {{&tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_ARGB16}, 
                                  {&tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_ARGB16}}},
    {/* 9*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, 1, 0, 
                                 {{&tsStruct::mfxVideoParam.vpp.In.FourCC, MFX_FOURCC_R16}, 
                                  {&tsStruct::mfxVideoParam.vpp.Out.FourCC, MFX_FOURCC_R16}}},

    // Unsupported camera buffers
    {/*10*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NOT_INITIALIZED, 0, 0, 
                                 {}, {ext_buf, EXT_BUF_PAR(mfxExtCamGammaCorrection)}},
    {/*11*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NOT_INITIALIZED, 0, 0, 
                                 {}, {ext_buf, EXT_BUF_PAR(mfxExtCamWhiteBalance)}},
    {/*12*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NOT_INITIALIZED, 0, 0, 
                                 {}, {ext_buf, EXT_BUF_PAR(mfxExtCamHotPixelRemoval)}},
    {/*13*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NOT_INITIALIZED, 0, 0,  
                                 {}, {ext_buf, EXT_BUF_PAR(mfxExtCamBlackLevelCorrection)}},
    {/*14*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NOT_INITIALIZED, 0, 0, 
                                 {}, {ext_buf, EXT_BUF_PAR(mfxExtCamVignetteCorrection)}},
    {/*15*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NOT_INITIALIZED, 0, 0, 
                                 {}, {ext_buf, EXT_BUF_PAR(mfxExtCamBayerDenoise)}},
    {/*16*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NOT_INITIALIZED, 0, 0, 
                                 {}, {ext_buf, EXT_BUF_PAR(mfxExtCamColorCorrection3x3)}},
    {/*17*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NOT_INITIALIZED, 0, 0, 
                                 {}, {ext_buf, EXT_BUF_PAR(mfxExtCamPadding)}},
    {/*18*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NOT_INITIALIZED, 0, 0, 
                                 {}, {ext_buf, EXT_BUF_PAR(mfxExtCamPipeControl)}},

    // Unsupported buffer from API
    {/*19*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NOT_INITIALIZED, 0, 0, 
                                 {}, {ext_buf, EXT_BUF_PAR(mfxVPPCompInputStream)}},

    //Unsupported parameters in mfxFrameInfo
    {/*20*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NOT_INITIALIZED, 0, 0, 
                                 {{&tsStruct::mfxVideoParam.vpp.In.BitDepthLuma, 10}, 
                                  {&tsStruct::mfxVideoParam.vpp.Out.BitDepthLuma, 10}}},
    {/*21*/ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NOT_INITIALIZED, 0, 0, 
                                 {{&tsStruct::mfxVideoParam.vpp.In.BitDepthChroma, 10}, 
                                  {&tsStruct::mfxVideoParam.vpp.Out.BitDepthChroma, 10}}},
    {/*22*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, 1, 0, 
                                 {{&tsStruct::mfxVideoParam.vpp.In.BitDepthLuma, 10}, 
                                  {&tsStruct::mfxVideoParam.vpp.Out.BitDepthLuma, 10}}},
    {/*23*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, 1, 0, 
                                 {{&tsStruct::mfxVideoParam.vpp.In.BitDepthChroma, 10}, 
                                  {&tsStruct::mfxVideoParam.vpp.Out.BitDepthChroma, 10}}},

    //Unsupported surfaces
    {/*24*/ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_INCOMPATIBLE_VIDEO_PARAM, 1, Y16, {}},


};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    MFXInit();

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

    if(tc.set_buf.func) // set ExtBuffer
    {
        (*tc.set_buf.func)(this->m_par, tc.set_buf.buf, tc.set_buf.size);
    }

    mfxVideoParam* par_out = new mfxVideoParam;
    memcpy(par_out, m_pPar, sizeof(mfxVideoParam));

    g_tsStatus.expect(tc.sts_query);
    Query(m_session, m_pPar, par_out);

    if (tc.sts_query == MFX_ERR_UNSUPPORTED) 
    { 
        for(mfxU32 i = 0; i < n_par; i++) 
        { 
            if(tc.set_par[i].f) 
            { 
                tsStruct::check_eq(&par_out, *tc.set_par[i].f, 0); 
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
        EFApar par = {m_session, m_pSurfPoolIn->GetSurface(), m_pSurfPoolOut->GetSurface(), m_pSyncPoint};
        mfxFrameSurface1 pSurfInTmp  = *par.pSurfIn;
        mfxFrameSurface1 pSurfOutTmp = *par.pSurfOut;

        //par.pSurfIn->Info.FourCC = MFX_FOURCC_P010;
        par.pSurfIn->Data.Y16 = (mfxU16*) par.pSurfIn->Data.Y;
        par.pSurfIn->Data.Y = 0;
        par.pSurfIn->Data.U16 = (mfxU16*) par.pSurfIn->Data.U;
        par.pSurfIn->Data.U = 0;
        par.pSurfIn->Data.V16 = (mfxU16*) par.pSurfIn->Data.V;
        par.pSurfIn->Data.V = 0;

        par.pSurfOut->Info.FourCC = MFX_FOURCC_P010;
        par.pSurfOut->Data.Y16 = (mfxU16*) par.pSurfOut->Data.Y;
        par.pSurfOut->Data.Y = 0;
        par.pSurfOut->Data.U16 = (mfxU16*) par.pSurfOut->Data.U;
        par.pSurfOut->Data.U = 0;
        par.pSurfOut->Data.V16 = (mfxU16*) par.pSurfOut->Data.V;
        par.pSurfOut->Data.V = 0;
        g_tsStatus.expect(tc.sts_reset);

        RunFrameVPPAsync(par.session, par.pSurfIn, par.pSurfOut, 0, par.pSP);
        g_tsStatus.check();

        *par.pSurfIn = pSurfInTmp;
        *par.pSurfOut = pSurfOutTmp;
    }

    if (tc.feature != Y16) 
    {
        g_tsStatus.expect(tc.sts_reset);
        Reset(m_session, m_pPar);
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(vpp_2014r2_unsupport);

}