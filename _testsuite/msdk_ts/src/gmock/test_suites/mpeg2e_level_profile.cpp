#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

/*
 * Encode stream with pre-defined parameters (including profile/level/GOP configs/etc) and check if
 * profile/level is as defined in the encoded stream
 *
 * Tests configs as follows:
 * TEST               Profiles      Level     B-Pyramid   GOP-size
 * 1080i59.94         HP, MP	    HL        3 Bs        32
 * 1080i50            HP, MP	    HL        3 Bs        32
 * 720p59.94          HP, MP	    H-14      3 Bs        32
 * 720p50             HP, MP	    H-14      3 Bs        32
 * 480i59.94          HP, MP	    ML        3 Bs        32
 * 576i50             HP, MP	    ML        3 Bs        32
 * 352x288p29.97      MP, SP	    LL        3 Bs, no B  32
 * 192x192p29.97      MP, SP	    LL        3 Bs, no B  32
 * 96x96p29.97        MP, SP	    LL        3 Bs, no B  32
 * 352x288p25         MP, SP	    LL        3 Bs, no B  32
 * 192x192p25         MP, SP	    LL        3 Bs, no B  32
 * 96x96p25           MP, SP	    LL        3 Bs, no B  32
 * 320x192p29.97      MP, SP	    LL        3 Bs, no B  32
 * 320x192p25         MP, SP	    LL        3 Bs, no B  32
 *
 * Explanations:
 * 1. Tests configs above are copied from ones in avce_level_profile which were from Harmonic.
 *    "Baseline" profile are changed to "Simple" profile; levels are decided by its resolution.
 */

namespace mpeg2e_level_profile
{
#if !defined(MSDK_ALIGN16)
#define MSDK_ALIGN16(value)                      (((value + 15) >> 4) << 4) // round up to a multiple of 16
#endif

class TestSuite : public tsVideoEncoder, public tsParserMPEG2AU, public tsBitstreamProcessor
{
public:

    TestSuite() : tsVideoEncoder(MFX_CODEC_MPEG2)
    {
        m_bs_processor = this;
    }
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        SetBuffer(bs);

        byte profile = 0;
        byte level   = 0;
        while (nFrames--) {
            UnitType& au = ParseOrDie();

            //for AU without sequence extension(non-IDR), au.seq_ext points to non-NULL address.
            //So the following judgement doesn't work as expected.
            if (au.seq_ext == NULL) continue;

            profile = au.seq_ext->profile_and_level_indication & 0x70;
            //refer to Table 8-2
            if (profile > 0x0 && profile < 0x60) {
                EXPECT_EQ(profile, m_ex_profile);
                m_profile.push_back(profile);
            }

            level = au.seq_ext->profile_and_level_indication & 0x0F;
            //refer to Table 8-3
            if ((level > 0x3) && (level < 0xB) && ((level & 0x1) == 0)) {
                EXPECT_EQ(level, m_ex_level);
                m_level.push_back(level);
            }
        }

        bs.DataOffset = 0;
        bs.DataLength = 0;

        return MFX_ERR_NONE;
    }

private:
    enum
    {
        MFX_PAR = 1,
    };

    struct tc_struct
    {
        mfxStatus sts;
        struct f_pair
        {
            mfxU32 ext_type;
            const tsStruct::Field* f;
            mfxU32 v;
        }set_par[MAX_NPARS];
    };

    static const tc_struct test_case[];
    //expected(pre-set) profile/level
    mfxU16 m_ex_profile;
    mfxU16 m_ex_level;
    //profile/level in stream, there might be multiple sequence headers
    std::vector<byte> m_profile;
    std::vector<byte> m_level;
};

const mfxStatus HIGH_PROFILE_EXP_RESULT =
#if defined(LINUX32) || defined(LINUX64)
    MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
#else
    MFX_ERR_NONE;
#endif

const TestSuite::tc_struct TestSuite::test_case[] =
{
     //Test#0, 1080i59.94/HP,MP/HL/3Bs/32/5M
    {/* 0*/ HIGH_PROFILE_EXP_RESULT, { { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 1920 },
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 1080},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_MPEG2_HIGH},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_MPEG2_HIGH},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct,        MFX_PICSTRUCT_FIELD_BFF}}},
    {/* 1*/ HIGH_PROFILE_EXP_RESULT, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 1920},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 1080},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_MPEG2_HIGH},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_MPEG2_HIGH},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct,        MFX_PICSTRUCT_FIELD_TFF}}},
    {/* 2*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 1920},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 1080},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_MPEG2_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_MPEG2_HIGH},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct,        MFX_PICSTRUCT_FIELD_BFF}}},
    {/* 3*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 1920},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 1080},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_MPEG2_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_MPEG2_HIGH},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct,        MFX_PICSTRUCT_FIELD_TFF}}},
    //Test#1, 1080i50/HP,MP/HL/3Bs/32/5M
    {/* 4*/ HIGH_PROFILE_EXP_RESULT, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 1920},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 1080},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_MPEG2_HIGH},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_MPEG2_HIGH},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 25},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct,        MFX_PICSTRUCT_FIELD_BFF}}},
    {/* 5*/ HIGH_PROFILE_EXP_RESULT, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 1920},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 1088},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_MPEG2_HIGH},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_MPEG2_HIGH},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 25},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct,        MFX_PICSTRUCT_FIELD_TFF}}},
    {/* 6*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 1920},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 1080},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_MPEG2_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_MPEG2_HIGH},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 25},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct,        MFX_PICSTRUCT_FIELD_BFF}}},
    {/* 7*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 1920},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 1080},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_MPEG2_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_MPEG2_HIGH},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 25},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct,        MFX_PICSTRUCT_FIELD_TFF}}},
    //Test#2, 720p59.94/HP,MP/H-14/3Bs/32/4M
    {/* 8*/ HIGH_PROFILE_EXP_RESULT, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 1280},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 720},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_MPEG2_HIGH},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_MPEG2_HIGH1440},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 60000},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001}}},
    //Accordint to Table 8-12, max lum samples per second is 47001600 for MP, so for this case, level should be set as HL.
    {/* 9*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 1280},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 720},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_MPEG2_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_MPEG2_HIGH},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 60000},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001}}},
    //Test#3, 720p50/High,Main/H-14/3Bs/32/4M
    {/*10*/ HIGH_PROFILE_EXP_RESULT, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 1280},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 720},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_MPEG2_HIGH},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_MPEG2_HIGH1440},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 50},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1}}},
    {/*11*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 1280},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 720},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_MPEG2_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_MPEG2_HIGH1440},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 50},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1}}},
    //Test#4, 480i59.94/HP,MP/ML/3Bs/32/1.5M
    {/*12*/ HIGH_PROFILE_EXP_RESULT, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 720},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 480},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_MPEG2_HIGH},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_MPEG2_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct,        MFX_PICSTRUCT_FIELD_TFF}}},
    {/*13*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 720},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 480},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_MPEG2_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_MPEG2_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct,        MFX_PICSTRUCT_FIELD_TFF}}},
    {/*14*/ HIGH_PROFILE_EXP_RESULT, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 720},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 480},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_MPEG2_HIGH},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_MPEG2_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct,        MFX_PICSTRUCT_FIELD_BFF}}},
    {/*15*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 720},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 480},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_MPEG2_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_MPEG2_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct,        MFX_PICSTRUCT_FIELD_BFF}}},
    //Test#5, 576i50/HP,MP/ML/3Bs/32/1.5M
    {/*16*/ HIGH_PROFILE_EXP_RESULT, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 720},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 576},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_MPEG2_HIGH},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_MPEG2_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 25},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct,        MFX_PICSTRUCT_FIELD_TFF}}},
    {/*17*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 720},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 576},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_MPEG2_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_MPEG2_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 25},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct,        MFX_PICSTRUCT_FIELD_TFF}}},
    {/*18*/ HIGH_PROFILE_EXP_RESULT, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 720},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 576},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_MPEG2_HIGH},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_MPEG2_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 25},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct,        MFX_PICSTRUCT_FIELD_BFF}}},
    {/*19*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 720},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 576},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_MPEG2_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_MPEG2_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 25},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct,        MFX_PICSTRUCT_FIELD_BFF}}},
    //Test#6, 352x288p29.97/MP,SP/LL/3Bs,NoB/32/0.5M
    {/*20*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 352},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 288},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_MPEG2_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_MPEG2_LOW},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001}}},
    //SP with 3Bs
    {/*21*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 352},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 288},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_MPEG2_SIMPLE},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_MPEG2_LOW},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001}}},
    {/*22*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 352},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 288},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_MPEG2_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_MPEG2_LOW},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   1},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001}}},
    {/*23*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 352},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 288},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_MPEG2_SIMPLE},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_MPEG2_LOW},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   1},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001}}},
    //Test#7, 192x192p29.97/MP,SP/LL/3Bs,NoB/32/0.15M
    {/*24*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 192},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 192},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_MPEG2_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_MPEG2_LOW},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001}}},
    {/*25*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 192},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 192},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_MPEG2_SIMPLE},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_MPEG2_LOW},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001}}},
    {/*26*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 192},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 192},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_MPEG2_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_MPEG2_LOW},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   1},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001}}},
    {/*27*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 192},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 192},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_MPEG2_SIMPLE},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_MPEG2_LOW},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   1},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001}}},
    //Test#8, 96x96p29.97/MP,SP/LL/3Bs,NoB/32/0.08M
    {/*28*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 96},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 96},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_MPEG2_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_MPEG2_LOW},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001}}},
    {/*29*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 96},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 96},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_MPEG2_SIMPLE},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_MPEG2_LOW},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001}}},
    {/*30*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 96},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 96},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_MPEG2_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_MPEG2_LOW},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   1},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001}}},
    {/*31*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 96},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 96},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_MPEG2_SIMPLE},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_MPEG2_LOW},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   1},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001}}},
    //Test#9, 352x288p25/MP,SP/LL/3Bs,NoB/32/0.5M
    {/*32*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 352},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 288},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_MPEG2_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_MPEG2_LOW},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 25},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1}}},
    {/*33*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 352},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 288},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_MPEG2_SIMPLE},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_MPEG2_LOW},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 25},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1}}},
    {/*34*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 352},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 288},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_MPEG2_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_MPEG2_LOW},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   1},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 25},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1}}},
    {/*35*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 352},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 288},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_MPEG2_SIMPLE},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_MPEG2_LOW},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   1},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 25},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1}}},
    //Test#10, 192x192p25/MP,SP/LL/3Bs,NoB/32/0.15M
    {/*36*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 192},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 192},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_MPEG2_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_MPEG2_LOW},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 25},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1}}},
    {/*37*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 192},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 192},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_MPEG2_SIMPLE},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_MPEG2_LOW},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 25},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1}}},
    {/*38*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 192},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 192},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_MPEG2_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_MPEG2_LOW},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   1},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 25},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1}}},
    {/*39*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 192},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 192},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_MPEG2_SIMPLE},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_MPEG2_LOW},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   1},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 25},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1}}},
    //Test#11, 96x96p25/MP,SP/LL/3Bs,NoB/32/0.08M
    {/*40*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 96},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 96},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_MPEG2_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_MPEG2_LOW},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 25},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1}}},
    {/*41*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 96},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 96},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_MPEG2_SIMPLE},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_MPEG2_LOW},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 25},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1}}},
    {/*42*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 96},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 96},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_MPEG2_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_MPEG2_LOW},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   1},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 25},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1}}},
    {/*43*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 96},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 96},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_MPEG2_SIMPLE},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_MPEG2_LOW},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   1},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 25},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1}}},
    //Test#12, 320x192p29.97/MP,SP/LL/3Bs,NoB/32/0.4M
    {/*44*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 320},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 192},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_MPEG2_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_MPEG2_LOW},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001}}},
    {/*45*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 320},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 192},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_MPEG2_SIMPLE},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_MPEG2_LOW},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001}}},
    {/*46*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 320},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 192},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_MPEG2_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_MPEG2_LOW},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   1},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001}}},
    {/*47*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 320},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 192},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_MPEG2_SIMPLE},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_MPEG2_LOW},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   1},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001}}},
    //Test#13, 320x192p25/MP,SP/LL/3Bs,NoB/32/0.4M
    {/*48*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 320},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 192},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_MPEG2_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_MPEG2_LOW},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 25},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1}}},
    {/*49*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 320},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 192},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_MPEG2_SIMPLE},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_MPEG2_LOW},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 25},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1}}},
    {/*50*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 320},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 192},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_MPEG2_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_MPEG2_LOW},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   1},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 25},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1}}},
    {/*51*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 320},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 192},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_MPEG2_SIMPLE},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_MPEG2_LOW},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   1},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 25},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1}}},
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    //set mfxVideoParam
    SETPARS(m_pPar, MFX_PAR);
    m_par.mfx.FrameInfo.Width  = MSDK_ALIGN16(m_par.mfx.FrameInfo.CropW);
    m_par.mfx.FrameInfo.Height = MSDK_ALIGN16(m_par.mfx.FrameInfo.CropH);
    m_par.mfx.RateControlMethod = MFX_RATECONTROL_CBR;
    //to calcuate target bitrate.
    set_brc_params(&m_par);
    if (m_par.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_UNKNOWN) {
        //If PicStruct is not explicitly set in the case
        m_par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    }

    //set expected profile/level
    m_ex_profile = m_par.mfx.CodecProfile;
    m_ex_level   = m_par.mfx.CodecLevel;

    mfxU32 nf = 60;
    MFXInit();
    g_tsStatus.expect(tc.sts);
    Init();
    if (tc.sts == MFX_ERR_NONE) {
        g_tsStatus.expect(tc.sts);
        EncodeFrames(nf);

        //ensure there are >=1 Sequence Extention in the stream.
        EXPECT_GT(m_profile.size(), 0);
        EXPECT_GT(m_level.size(), 0);
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(mpeg2e_level_profile);
};
