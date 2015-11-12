#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

namespace mpeg2e_aspectratio_reset
{

class TestSuite : tsVideoEncoder
{
public:
    TestSuite() : tsVideoEncoder(MFX_CODEC_MPEG2) {}
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

private:
    mfxSession m_session;

    enum
    {
        MFXINIT = 1,
        MFXRESET
    };

    struct tc_struct
    {
        mfxStatus sts;
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];
    };

    static const tc_struct test_case[];
};

#if !defined(MSDK_ALIGN16)
#define MSDK_ALIGN16(value) (((value + 15) >> 4) << 4)
#endif

const TestSuite::tc_struct TestSuite::test_case[] =
{
    {/* 0*/ MFX_ERR_NONE, {
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioW,4},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioH,3},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,196},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,232},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.Width,MSDK_ALIGN16(196)},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.Height,MSDK_ALIGN16(232)}}},

    {/* 1*/ MFX_ERR_NONE, {
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioW,1},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioH,1},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,198},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,232},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.Width,MSDK_ALIGN16(198)},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.Height,MSDK_ALIGN16(232)}}},

    {/* 2*/ MFX_ERR_NONE, {
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,196},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,232},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.Width,MSDK_ALIGN16(196)},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.Height,MSDK_ALIGN16(232)},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioW,16},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioH,9},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,180},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,200},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.Width,MSDK_ALIGN16(180)},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.Height,MSDK_ALIGN16(200)}}},

    {/* 3*/ MFX_ERR_NONE, {
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,198},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,232},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.Width,MSDK_ALIGN16(198)},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.Height,MSDK_ALIGN16(232)},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioW,1},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioH,1},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,180},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,200},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.Width,MSDK_ALIGN16(180)},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.Height,MSDK_ALIGN16(200)}}},

    {/* 4*/ MFX_ERR_NONE, {
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,1920},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,1080},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.Width,MSDK_ALIGN16(1920)},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.Height,MSDK_ALIGN16(1080)},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioW,1},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioH,1},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,1280},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,720},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.Width,MSDK_ALIGN16(1280)},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.Height,MSDK_ALIGN16(720)}}},

    {/* 5*/ MFX_ERR_NONE, {
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,1920},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,1080},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.Width,MSDK_ALIGN16(1920)},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.Height,MSDK_ALIGN16(1080)},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioW,16},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioH,19},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,1280},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,720},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.Width,MSDK_ALIGN16(1280)},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.Height,MSDK_ALIGN16(720)}}},

    {/* 6*/ MFX_ERR_NONE, {
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,1280},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,720},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.Width,MSDK_ALIGN16(1280)},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.Height,MSDK_ALIGN16(720)},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioW,1},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioH,1},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,720},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,480},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.Width,MSDK_ALIGN16(720)},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.Height,MSDK_ALIGN16(480)}}},

    {/* 7*/ MFX_ERR_NONE, {
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,1280},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,720},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.Width,MSDK_ALIGN16(1280)},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.Height,MSDK_ALIGN16(720)},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioW,4},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioH,3},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,720},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,480},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.Width,MSDK_ALIGN16(720)},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.Height,MSDK_ALIGN16(480)}}},

    {/* 8*/ MFX_ERR_NONE, {
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,720},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,480},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.Width,MSDK_ALIGN16(720)},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.Height,MSDK_ALIGN16(480)},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioW,1},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioH,1},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,352},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,288},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.Width,MSDK_ALIGN16(352)},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.Height,MSDK_ALIGN16(288)}}},

    {/* 9*/ MFX_ERR_NONE, {
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,720},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,480},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.Width,MSDK_ALIGN16(720)},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.Height,MSDK_ALIGN16(480)},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioW,4},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioH,3},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,352},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,288},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.Width,MSDK_ALIGN16(352)},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.Height,MSDK_ALIGN16(288)}}},

    {/*10*/ MFX_ERR_NONE, {
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioW,1},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioH,1},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,194},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,232},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.Width,MSDK_ALIGN16(194)},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.Height,MSDK_ALIGN16(232)}}},

    {/*11*/ MFX_ERR_NONE, {
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,194},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,232},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.Width,MSDK_ALIGN16(194)},
                              {MFXINIT,&tsStruct::mfxVideoParam.mfx.FrameInfo.Height,MSDK_ALIGN16(232)},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioW,4},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.AspectRatioH,3},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropW,180},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.CropH,200},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.Width,MSDK_ALIGN16(180)},
                              {MFXRESET,&tsStruct::mfxVideoParam.mfx.FrameInfo.Height,MSDK_ALIGN16(200)}}},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

class Verifier : public tsBitstreamProcessor, public tsParserMPEG2AU
{
    mfxU16 m_aspectratioW;
    mfxU16 m_aspectratioH;
public:
    Verifier() : m_aspectratioW(0),m_aspectratioH(0) {}
    mfxStatus Init(mfxU16 ar_w, mfxU16 ar_h)
    {
        m_aspectratioW = ar_w;
        m_aspectratioH = ar_h;
        return MFX_ERR_NONE;
    }
    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        SetBuffer(bs);

        while (1)
        {
            BSErr sts = parse_next_unit();

            if (sts == BS_ERR_NOT_IMPLEMENTED) continue;
            if (sts) break;

            BS_MPEG2_header* pHdr = (BS_MPEG2_header*)get_header();
            if (pHdr->seq_hdr != 0)
            {
                mfxU8 aspectratio_info = 0;
                aspectratio_info |= pHdr->seq_hdr->aspect_ratio_information;
                if(aspectratio_info < 1 || aspectratio_info > 4)
                {
                    g_tsLog << "ERROR: not defined aspect_ratio_information :" << aspectratio_info << "\n";
                    return MFX_ERR_ABORTED;
                }

                if(aspectratio_info == 1)
                {
                    if((m_aspectratioW == 1 && m_aspectratioH == 1) || (m_aspectratioW == 0 && m_aspectratioH == 0))
                    {
                        return MFX_ERR_NONE;
                    }
                    else
                    {
                        g_tsLog << "ERROR: aspect_ratio_information is not set correctly." << "\n";
                        return MFX_ERR_ABORTED;
                    }
                }

                //according to Table 6-3 in ITU-T H.262
                if(m_aspectratioW == 4 && m_aspectratioH == 3)
                {
                    if(aspectratio_info == 2) return MFX_ERR_NONE;
                    else
                    {
                        g_tsLog << "ERROR: aspect_ratio_information is:" << aspectratio_info << ", but expected 2 here. " << "\n";
                        return MFX_ERR_ABORTED;
                    }
                }
                if(m_aspectratioW == 16 && m_aspectratioH == 9)
                {
                    if(aspectratio_info == 3) return MFX_ERR_NONE;
                    else
                    {
                        g_tsLog << "ERROR: aspect_ratio_information is:" << aspectratio_info << ", but expected 3 here. " << "\n";
                        return MFX_ERR_ABORTED;
                    }
                }
                if((m_aspectratioW == 2 && m_aspectratioH == 1) || (m_aspectratioW == 21 && m_aspectratioH == 1))
                {
                    if(aspectratio_info == 4) return MFX_ERR_NONE;
                    else
                    {
                        g_tsLog << "ERROR: aspect_ratio_information is:" << aspectratio_info << ", but expected 4 here. " << "\n";
                        return MFX_ERR_ABORTED;
                    }
                }
            }
        }
        bs.DataLength = 0;
        return MFX_ERR_NONE;
    }
};

class ResChange : public tsSurfaceProcessor
{
private:
    mfxU32 m_dynamic_width, m_dynamic_height;
public:
    ResChange() : m_dynamic_width(0),m_dynamic_height(0) {}
    mfxStatus Init(mfxU16 res_w, mfxU16 res_h)
    {
        m_dynamic_width = res_w;
        m_dynamic_height = res_h;
        return MFX_ERR_NONE;
    }
    mfxStatus ProcessSurface(mfxFrameSurface1& s)
    {
        if(m_dynamic_width == 0 && m_dynamic_height == 0)
        {
            return MFX_ERR_NONE;
        }
        if(s.Info.CropW != m_dynamic_width)
        {
            s.Info.Width = MSDK_ALIGN16(m_dynamic_width);
            s.Info.CropW = m_dynamic_width;
        }
        if(s.Info.CropH != m_dynamic_height)
        {
            s.Info.Height = MSDK_ALIGN16(m_dynamic_height);
            s.Info.CropH = m_dynamic_height;
        }
        return MFX_ERR_NONE;
    }
};

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    auto& tc = test_case[id];
    int nframes = 10;

    MFXInit();

    SETPARS(&m_par, MFXINIT);
    AllocBitstream((m_par.mfx.FrameInfo.Width * m_par.mfx.FrameInfo.Height) * nframes);
    m_par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE; // if PicStruct is not progressive, the FrameInfo.Height should be aligned by 32 but not 16.
    Init();

    Verifier verifier;
    m_bs_processor = &verifier;
    ResChange resolution;
    m_filler = &resolution;
    EncodeFrames(nframes);

    bool b_reset = false;
    for(unsigned int i = 0; i < MAX_NPARS; i++)
    {
        if(tc.set_par[i].ext_type & MFXRESET)
        {
            b_reset = true;
            break;
        }
    }
    if(b_reset) {
        SETPARS(&m_par, MFXRESET);
        resolution.Init(m_pPar->mfx.FrameInfo.Width, m_pPar->mfx.FrameInfo.Height);
        verifier.Init(m_pPar->mfx.FrameInfo.AspectRatioW,m_pPar->mfx.FrameInfo.AspectRatioH);

        Reset();
        EncodeFrames(nframes);
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(mpeg2e_aspectratio_reset);
};
