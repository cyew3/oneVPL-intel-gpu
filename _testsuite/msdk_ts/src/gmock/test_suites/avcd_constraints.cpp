#include "ts_decoder.h"
#include "ts_struct.h"

namespace avcd_constraints
{

class TestSuite : tsVideoDecoder, tsSurfaceProcessor
{
public:
    TestSuite() : tsVideoDecoder(MFX_CODEC_AVC)  { m_surf_processor = this; }
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

    std::string getPicStructStr(mfxU16 pic_struct)
    {
        std::string pic_struct_str = "<not recognized>";
        if (pic_struct == MFX_PICSTRUCT_UNKNOWN)
            pic_struct_str = "MFX_PICSTRUCT_UNKNOWN";
        else if (pic_struct == MFX_PICSTRUCT_PROGRESSIVE)
            pic_struct_str = "MFX_PICSTRUCT_PROGRESSIVE";
        else if (pic_struct == MFX_PICSTRUCT_FIELD_TFF)
            pic_struct_str = "MFX_PICSTRUCT_FIELD_TFF";
        else if (pic_struct == MFX_PICSTRUCT_FIELD_BFF)
            pic_struct_str = "MFX_PICSTRUCT_FIELD_BFF";

        return pic_struct_str;
    }

    void checkSurf(mfxFrameSurface1& surf_out, mfxU32 val)
    {
        EXPECT_NE(surf_out.Info.CropW, val) << "ERROR: Surface's CropW was not changed in SYNC part! \n";
        EXPECT_NE(surf_out.Info.CropH, val) << "ERROR: Surface's CropH was not changed in SYNC part! \n";

        EXPECT_NE(surf_out.Info.CropX, val) << "ERROR: Surface's CropX was not changed in SYNC part! \n";
        EXPECT_NE(surf_out.Info.CropY, val) << "ERROR: Surface's CropY was not changed in SYNC part! \n";

        EXPECT_NE(surf_out.Info.AspectRatioW, val) << "ERROR: Surface's AspectRatioW was not changed in SYNC part! \n";
        EXPECT_NE(surf_out.Info.AspectRatioH, val) << "ERROR: Surface's AspectRatioH was not changed in SYNC part! \n";

        EXPECT_NE(surf_out.Info.FrameRateExtD, val) << "ERROR: Surface's FrameRateExtD was not changed in SYNC part! \n";
        EXPECT_NE(surf_out.Info.FrameRateExtN, val) << "ERROR: Surface's FrameRateExtN was not changed in SYNC part! \n";

        EXPECT_NE(surf_out.Info.PicStruct, val) << "ERROR: Surface's PicStruct was not changed in SYNC part! \n";
    }

    mfxStatus ProcessSurface(mfxFrameSurface1& surf_out)
    {
        checkSurf(surf_out, 0x55);

        if (((m_par.mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_UNKNOWN) &&
                        (surf_out.Info.PicStruct != m_par.mfx.FrameInfo.PicStruct)) ||
                    ((m_par.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_UNKNOWN) &&
                        (surf_out.Info.PicStruct != MFX_PICSTRUCT_PROGRESSIVE) &&
                        (surf_out.Info.PicStruct != MFX_PICSTRUCT_FIELD_TFF) &&
                        (surf_out.Info.PicStruct != MFX_PICSTRUCT_FIELD_BFF)))
        {
            std::string PicStructDH = getPicStructStr(m_par.mfx.FrameInfo.PicStruct);
            std::string PicStructFA = getPicStructStr(surf_out.Info.PicStruct);

            g_tsLog << "DecodeHeader PicStruct = " << PicStructDH << "\n";
            g_tsLog << "DecodeFrameAsync PicStruct = " << PicStructFA << "\n";

            TS_FAIL_TEST("PicStruct value after DecodeFrameAsync is not equal to the same in DecodeHeader!", MFX_ERR_NONE);
        }

        return MFX_ERR_NONE;
    }

private:

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
    {/*00*/ MFX_ERR_NONE, 0, { } },
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;

    const tc_struct& tc = test_case[id];

    MFXInit();
    if(m_uid)
        Load();

    std::string stream = "performance/h264/1920x1088_fld_vlc_24mbs.264";
    const char* sname = g_tsStreamPool.Get(stream);
    g_tsStreamPool.Reg();
    tsBitstreamReader reader(sname, 100000);
    m_bs_processor = &reader;

    DecodeHeader();

    Init(m_session, m_pPar);

    AllocSurfaces();

    for (mfxU32 i = 0; i< tsSurfacePool::PoolSize(); i++)
    {
        mfxFrameSurface1* surf  = tsSurfacePool::GetSurface();
        surf->Info.CropW = surf->Info.CropH = surf->Info.CropX = surf->Info.CropY =
            surf->Info.AspectRatioW = surf->Info.AspectRatioH =
            surf->Info.FrameRateExtD = surf->Info.FrameRateExtN = surf->Info.PicStruct = 0x55; //this value should be changed in SYNC part
    }

    DecodeFrames(2);

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(avcd_constraints);

}

// Legacy test: behavior_h264_decode_suite, b_AVC_DECODE_Constraints