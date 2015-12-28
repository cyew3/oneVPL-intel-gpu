#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

namespace hevce_level_profile
{

    const mfxU32 TableA1[][6] =
    {
        //  Level   |MaxLumaPs |    MaxCPB    | MaxSlice  | MaxTileRows | MaxTileCols
        //                                      Segments  |
        //                                      PerPicture|
        /*  1   */{ 36864, 350, 350, 16, 1, 1 },
        /*  2   */{ 122880, 1500, 1500, 16, 1, 1 },
        /*  2.1 */{ 245760, 3000, 3000, 20, 1, 1 },
        /*  3   */{ 552960, 6000, 6000, 30, 2, 2 },
        /*  3.1 */{ 983040, 10000, 10000, 40, 3, 3 },
        /*  4   */{ 2228224, 12000, 30000, 75, 5, 5 },
        /*  4.1 */{ 2228224, 20000, 50000, 75, 5, 5 },
        /*  5   */{ 8912896, 25000, 100000, 200, 11, 10 },
        /*  5.1 */{ 8912896, 40000, 160000, 200, 11, 10 },
        /*  5.2 */{ 8912896, 60000, 240000, 200, 11, 10 },
        /*  6   */{ 35651584, 60000, 240000, 600, 22, 20 },
        /*  6.1 */{ 35651584, 120000, 480000, 600, 22, 20 },
        /*  6.2 */{ 35651584, 240000, 800000, 600, 22, 20 },
    };

    const mfxU32 TableA2[][4] =
    {
        //  Level   | MaxLumaSr |      MaxBR   | MinCr
        /*  1    */{ 552960, 128, 128, 2 },
        /*  2    */{ 3686400, 1500, 1500, 2 },
        /*  2.1  */{ 7372800, 3000, 3000, 2 },
        /*  3    */{ 16588800, 6000, 6000, 2 },
        /*  3.1  */{ 33177600, 10000, 10000, 2 },
        /*  4    */{ 66846720, 12000, 30000, 4 },
        /*  4.1  */{ 133693440, 20000, 50000, 4 },
        /*  5    */{ 267386880, 25000, 100000, 6 },
        /*  5.1  */{ 534773760, 40000, 160000, 8 },
        /*  5.2  */{ 1069547520, 60000, 240000, 8 },
        /*  6    */{ 1069547520, 60000, 240000, 8 },
        /*  6.1  */{ 2139095040, 120000, 480000, 8 },
        /*  6.2  */{ 4278190080, 240000, 800000, 6 },
    };

class TestSuite : tsVideoEncoder
{
public:
    static const unsigned int n_cases;

    TestSuite() : tsVideoEncoder(MFX_CODEC_HEVC) {}
    ~TestSuite() {}

    enum
    {
        MFXPAR = 1,
        SLICE,
        RESOLUTION,
        FRAME_RATE,
        BR,
        PROFILE,
        NONE
    };

    enum
    {
        BIG_H = 1,
        BIG_W,
        BIG_WH,
        NOT_ALLIGNED
    };

    int RunTest(unsigned int id);

private:
    struct tc_struct
    {
        mfxStatus sts;
        mfxU32 exp_level;
        mfxU32 type;
        mfxU32 sub_type;
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        } set_par[MAX_NPARS];
    };

    static const tc_struct test_case[];

    mfxU16 LevelIdx(mfxU16 mfx_level)
    {
        switch (mfx_level & 0xFF)
        {
            case  MFX_LEVEL_HEVC_1: return  0;
            case  MFX_LEVEL_HEVC_2: return  1;
            case  MFX_LEVEL_HEVC_21: return  2;
            case  MFX_LEVEL_HEVC_3: return  3;
            case  MFX_LEVEL_HEVC_31: return  4;
            case  MFX_LEVEL_HEVC_4: return  5;
            case  MFX_LEVEL_HEVC_41: return  6;
            case  MFX_LEVEL_HEVC_5: return  7;
            case  MFX_LEVEL_HEVC_51: return  8;
            case  MFX_LEVEL_HEVC_52: return  9;
            case  MFX_LEVEL_HEVC_6: return 10;
            case  MFX_LEVEL_HEVC_61: return 11;
            case  MFX_LEVEL_HEVC_62: return 12;
            default: break;
        }
        return -1;
    }

    mfxU16 TierIdx(mfxU16 mfx_level)
    {
        return !!(mfx_level & MFX_TIER_HEVC_HIGH);
    }

    mfxU16 PreparePar(tc_struct tc)
    {
        mfxU16 lidx = LevelIdx(m_par.mfx.CodecLevel);
        mfxU16 tidx = TierIdx(m_par.mfx.CodecLevel);

        if (tidx > (lidx >= 5)) // titx = 0, if level < 4, titx = 1 || 0, if level >=4
            tidx = 0;

        mfxU32 MaxLumaPs = TableA1[lidx][0];
        mfxU32 MaxCPB = TableA1[lidx][1 + tidx];
        mfxU32 MaxSSPP = TableA1[lidx][3];
        mfxU32 MaxTileRows = TableA1[lidx][4];
        mfxU32 MaxTileCols = TableA1[lidx][5];
        mfxU32 MaxLumaSr = TableA2[lidx][0];
        mfxU32 MaxBR = TableA2[lidx][1 + tidx];

        switch (tc.type)
        {
        case  RESOLUTION:
        {
                            if (tc.sub_type == BIG_W)
                            {
                                m_par.mfx.FrameInfo.Width = (((mfxU16)(sqrt((mfxF32)MaxLumaPs * 8)) + 32 - 1) & ~(32 - 1)) + 32;
                                m_par.mfx.FrameInfo.Height = ((MaxLumaPs / m_par.mfx.FrameInfo.Width) & ~0x1f);
                                m_par.mfx.FrameInfo.FrameRateExtN = TS_MIN( (MaxLumaSr / (m_par.mfx.FrameInfo.Width * m_par.mfx.FrameInfo.Height)), 299);
                                m_par.mfx.FrameInfo.FrameRateExtD = 1;
                            }
                            else if (tc.sub_type == BIG_H)
                            {
                                m_par.mfx.FrameInfo.Height = (((mfxU16)(sqrt((mfxF32)MaxLumaPs * 8)) + 32 - 1) & ~(32 - 1)) + 32;
                                m_par.mfx.FrameInfo.Width = ((MaxLumaPs / m_par.mfx.FrameInfo.Height) & ~0x1f);
                                m_par.mfx.FrameInfo.FrameRateExtN = TS_MIN( (MaxLumaSr / (m_par.mfx.FrameInfo.Width * m_par.mfx.FrameInfo.Height)), 299);
                                m_par.mfx.FrameInfo.FrameRateExtD = 1;
                            }
                            else if (tc.sub_type == BIG_WH)
                            {
                                m_par.mfx.FrameInfo.Width = (((mfxU16)(sqrt((mfxF32)MaxLumaPs * 8))) & ~0x1f);
                                m_par.mfx.FrameInfo.Height = (((mfxU16)(sqrt((mfxF32)MaxLumaPs * 8))) & ~0x1f);
                                m_par.mfx.FrameInfo.FrameRateExtN = TS_MIN( (MaxLumaSr / (m_par.mfx.FrameInfo.Width * m_par.mfx.FrameInfo.Height)), 299);
                                m_par.mfx.FrameInfo.FrameRateExtD = 1;
                            }
                            else if (tc.sub_type == NOT_ALLIGNED)
                            {
                                m_par.mfx.FrameInfo.Width = TS_MIN((((mfxU16)(sqrt((mfxF32)MaxLumaPs * 8))) & ~0x1f), 4096) - 15;
                                m_par.mfx.FrameInfo.Height = TS_MIN(((MaxLumaPs / m_par.mfx.FrameInfo.Width) & ~0x1f), 2176);
                                m_par.mfx.FrameInfo.FrameRateExtN = TS_MIN( (MaxLumaSr / (m_par.mfx.FrameInfo.Width * m_par.mfx.FrameInfo.Height)), 299);
                                m_par.mfx.FrameInfo.FrameRateExtD = 1;
                            }
                            break;
        }
        case  FRAME_RATE:
        {
                            if (tc.sts != MFX_ERR_NONE)
                            {
                                m_par.mfx.FrameInfo.Width = TS_MIN( (((mfxU16)(sqrt((mfxF32)MaxLumaPs * 8))) & ~0x1f), 4096 );
                                m_par.mfx.FrameInfo.Height = TS_MIN( ((MaxLumaPs / m_par.mfx.FrameInfo.Width) & ~0x1f), 2176 );
                                m_par.mfx.FrameInfo.FrameRateExtN = TS_MIN( (MaxLumaSr / (m_par.mfx.FrameInfo.Width * m_par.mfx.FrameInfo.Height)), 289 ) + 10;
                                m_par.mfx.FrameInfo.FrameRateExtD = 1;
                            }
                            else
                            {
                                m_par.mfx.FrameInfo.Width = TS_MIN((((mfxU16)(sqrt((mfxF32)MaxLumaPs * 8))) & ~0x1f), 4096);
                                m_par.mfx.FrameInfo.Height = TS_MIN(((MaxLumaPs / m_par.mfx.FrameInfo.Width) & ~0x1f), 2176);
                                m_par.mfx.FrameInfo.FrameRateExtN = TS_MIN( (MaxLumaSr / (m_par.mfx.FrameInfo.Width * m_par.mfx.FrameInfo.Height)), 299);
                                m_par.mfx.FrameInfo.FrameRateExtD = 1;
                            }
                            break;
        }
        case  SLICE:
        {
                       if (tc.sts != MFX_ERR_NONE)
                       {
                           m_par.mfx.FrameInfo.Width = TS_MIN((((mfxU16)(sqrt((mfxF32)MaxLumaPs * 8))) & ~0x1f), 4096);
                           m_par.mfx.FrameInfo.Height = TS_MIN(((MaxLumaPs / m_par.mfx.FrameInfo.Width) & ~0x1f), 2176);
                           m_par.mfx.FrameInfo.FrameRateExtN = TS_MIN( (MaxLumaSr / (m_par.mfx.FrameInfo.Width * m_par.mfx.FrameInfo.Height)), 299);
                           m_par.mfx.FrameInfo.FrameRateExtD = 1;
                           m_par.mfx.NumSlice = MaxSSPP + 1;
                       }
                       else
                       {
                           m_par.mfx.FrameInfo.Width = TS_MIN((((mfxU16)(sqrt((mfxF32)MaxLumaPs * 8))) & ~0x1f), 4096);
                           m_par.mfx.FrameInfo.Height = TS_MIN(((MaxLumaPs / m_par.mfx.FrameInfo.Width) & ~0x1f), 2176);
                           m_par.mfx.FrameInfo.FrameRateExtN = TS_MIN( (MaxLumaSr / (m_par.mfx.FrameInfo.Width * m_par.mfx.FrameInfo.Height)), 299);
                           m_par.mfx.FrameInfo.FrameRateExtD = 1;
                           m_par.mfx.NumSlice = TS_MIN( TS_MIN( MaxSSPP, 68 ), (m_par.mfx.FrameInfo.Height / 64) );
                       }
                       break;
        }
        case  BR:
        {
                    m_par.mfx.FrameInfo.Width = TS_MIN((((mfxU16)(sqrt((mfxF32)MaxLumaPs * 8))) & ~0x1f), 4096);
                    m_par.mfx.FrameInfo.Height = TS_MIN(((MaxLumaPs / m_par.mfx.FrameInfo.Width) & ~0x1f), 2176);
                    m_par.mfx.TargetKbps = (mfxU16)(MaxBR * 1.1) + 10;
                    m_par.mfx.RateControlMethod = 0;
                    break;
        }
        default: break;
        }
        return 0;
    }
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    // --- MAIN Profile ---//
    //------- Level 1 -------//
    // ok resolution min, ok framerate
    {/*00*/ MFX_ERR_NONE, MFX_LEVEL_HEVC_1, NONE, NONE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_1},
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 128 },
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 96 },
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30 },
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1 },
       },
    },
    // wrong resolution width
    {/*01*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_LEVEL_HEVC_2, RESOLUTION, BIG_W, {
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN },
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_1 },

       },
    },
    // wrong resolution heigth
    {/*02*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_LEVEL_HEVC_2, RESOLUTION, BIG_H, {
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN },
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_1 },

       },
    },
    // wrong resolution
    {/*03*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_LEVEL_HEVC_3, RESOLUTION, BIG_WH, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_1},

       },
    },
    // ok res, wrong framerate
    {/*04*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_LEVEL_HEVC_2, FRAME_RATE, NONE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_1},

       },
    },
    // ok slices
    {/*05*/ MFX_ERR_NONE, MFX_LEVEL_HEVC_1, SLICE, NONE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_1},

       },
    },
    // wrong slices
    {/*06*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_LEVEL_HEVC_21, SLICE, NONE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_1},

       },
    },
    // wrong kbps
    {/*07*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_LEVEL_HEVC_2, BR, NONE, {
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN },
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_1 },

       },
    },

    //------- Level 2 -------//
    // ok res, ok framerate
    {/*08*/ MFX_ERR_NONE, MFX_LEVEL_HEVC_2, FRAME_RATE, NONE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_2},

       },
    },
    // wrong resolution width
    {/*09*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_LEVEL_HEVC_21, RESOLUTION, BIG_W, {
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN },
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_2 },

       },
    },
    // wrong resolution heigth
    {/*10*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_LEVEL_HEVC_21, RESOLUTION, BIG_H, {
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN },
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_2 },

       },
    },
    // wrong resolution
    {/*11*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_LEVEL_HEVC_31, RESOLUTION, BIG_WH, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_2},

       },
    },
    // ok res, wrong framerate
    {/*12*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_LEVEL_HEVC_21, FRAME_RATE, NONE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_2},

       },
    },
    // ok slices
    {/*13*/ MFX_ERR_NONE, MFX_LEVEL_HEVC_2, SLICE, NONE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_2},

       },
    },
    // wrong slices
    {/*14*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_LEVEL_HEVC_21, SLICE, NONE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_2},

       },
    },
    // wrong kbps
    {/*15*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_LEVEL_HEVC_21, BR, NONE, {
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN },
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_2 },

       },
    },

    //------- Level 2.1 -------//
    // ok res, ok framerate
    {/*16*/ MFX_ERR_NONE, MFX_LEVEL_HEVC_21, FRAME_RATE, NONE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_21},

       },
    },
    // wrong resolution width
    {/*17*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_LEVEL_HEVC_3, RESOLUTION, BIG_W, {
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN },
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_21 },

       },
    },
    // wrong resolution heigth
    {/*18*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_LEVEL_HEVC_3, RESOLUTION, BIG_H, {
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN },
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_21 },

       },
    },
    // wrong resolution
    {/*19*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_LEVEL_HEVC_4, RESOLUTION, BIG_WH, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_21},

       },
    },
    // ok res, wrong framerate
    {/*20*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_LEVEL_HEVC_3, FRAME_RATE, NONE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_21},

       },
    },
    // ok slices
    {/*21*/ MFX_ERR_NONE, MFX_LEVEL_HEVC_21, SLICE, NONE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_21},

       },
    },
    // wrong slices
    {/*22*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_LEVEL_HEVC_3, SLICE, NONE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_21},

       },
    },
    // wrong kbps
    {/*23*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_LEVEL_HEVC_3, BR, NONE, {
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN },
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_21 },

       },
    },

    //------- Level 3 -------//
    // ok res, ok framerate
    {/*24*/ MFX_ERR_NONE, MFX_LEVEL_HEVC_3, FRAME_RATE, NONE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_3},

       },
    },
    // wrong resolution width
    {/*25*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_LEVEL_HEVC_31, RESOLUTION, BIG_W, {
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN },
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_3 },

       },
    },
    // wrong resolution heigth
    {/*26*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_LEVEL_HEVC_31, RESOLUTION, BIG_H, {
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN },
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_3 },

       },
    },
    // wrong resolution
    {/*27*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_LEVEL_HEVC_5, RESOLUTION, BIG_WH, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_3},

       },
    },
    // ok res, wrong framerate
    {/*28*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_LEVEL_HEVC_31, FRAME_RATE, NONE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_3},

       },
    },
    // ok slices
    {/*29*/ MFX_ERR_NONE, MFX_LEVEL_HEVC_3, SLICE, NONE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_3},

       },
    },
    // wrong slices
    {/*30*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_LEVEL_HEVC_31, SLICE, NONE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_3},

       },
    },
    // wrong kbps
    {/*31*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_LEVEL_HEVC_31, BR, NONE, {
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN },
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_3 },

       },
    },

    //------- Level 3.1 -------//
    // ok res, ok framerate
    {/*32*/ MFX_ERR_NONE, MFX_LEVEL_HEVC_31, FRAME_RATE, NONE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_31},

       },
    },
    // wrong resolution width
    {/*33*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_LEVEL_HEVC_4, RESOLUTION, BIG_W, {
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN },
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_31 },

       },
    },
    // wrong resolution heigth
    {/*34*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_LEVEL_HEVC_4, RESOLUTION, BIG_H, {
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN },
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_31 },

       },
    },
    // wrong resolution
    {/*35*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_LEVEL_HEVC_5, RESOLUTION, BIG_WH, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_31},

       },
    },
    // ok res, wrong framerate
    {/*36*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_LEVEL_HEVC_4, FRAME_RATE, NONE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_31},

       },
    },
    // ok slices
    {/*37*/ MFX_ERR_NONE, MFX_LEVEL_HEVC_31, SLICE, NONE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_31},

       },
    },
    // wrong slices
    {/*38*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_LEVEL_HEVC_4, SLICE, NONE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_31},

       },
    },
    // wrong kbps
    {/*39*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_LEVEL_HEVC_4, BR, NONE, {
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN },
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_31 },

       },
    },

    //------- Level 4 -------//
    // ok res, ok framerate
    {/*40*/ MFX_ERR_NONE, MFX_LEVEL_HEVC_4, FRAME_RATE, NONE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_4},

       },
    },
    // wrong resolution width
    {/*41*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_LEVEL_HEVC_5, RESOLUTION, BIG_W, {
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN },
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_4 },

       },
    },
    // wrong resolution heigth
    {/*42*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_LEVEL_HEVC_5, RESOLUTION, BIG_H, {
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN },
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_4 },

       },
    },
    // wrong resolution
    {/*43*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_LEVEL_HEVC_6, RESOLUTION, BIG_WH, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_4},

       },
    },
    // ok res, wrong framerate
    {/*44*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_LEVEL_HEVC_41, FRAME_RATE, NONE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_4},

       },
    },
    // ok slices
    {/*45*/ MFX_ERR_NONE, MFX_LEVEL_HEVC_4, SLICE, NONE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_4},

       },
    },
    // wrong slices
    {/*46*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_LEVEL_HEVC_5, SLICE, NONE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_4},

       },
    },
    // wrong kbps
    {/*47*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_LEVEL_HEVC_4, BR, NONE, {
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN },
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_4 },

       },
    },

    //------- Level 4.1  -------//
    // ok res, ok framerate
    {/*48*/ MFX_ERR_NONE, MFX_LEVEL_HEVC_41, FRAME_RATE, NONE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_41},

       },
    },
    // wrong resolution width
    {/*49*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_LEVEL_HEVC_5, RESOLUTION, BIG_W, {
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN },
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_41 },

       },
    },
    // wrong resolution heigth
    {/*50*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_LEVEL_HEVC_5, RESOLUTION, BIG_H, {
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN },
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_41 },

       },
    },
    // wrong resolution
    {/*51*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_LEVEL_HEVC_6, RESOLUTION, BIG_WH, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_41},

       },
    },
    // ok res, wrong framerate
    {/*52*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_LEVEL_HEVC_5, FRAME_RATE, NONE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_41},

       },
    },
    // ok slices
    {/*53*/ MFX_ERR_NONE, MFX_LEVEL_HEVC_41, SLICE, NONE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_41},

       },
    },
    // wrong slices
    {/*54*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_LEVEL_HEVC_5, SLICE, NONE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_41},

       },
    },
    // wrong kbps
    {/*55*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_LEVEL_HEVC_4, BR, NONE, {
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN },
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_4 },

       },
    },

    //------- Level 5  -------//
    // ok res, ok framerate
    {/*56*/ MFX_ERR_NONE, MFX_LEVEL_HEVC_5, FRAME_RATE, NONE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_5},

       },
    },
    // wrong resolution
    {/*57*/ MFX_ERR_UNSUPPORTED, MFX_LEVEL_HEVC_5, RESOLUTION, NOT_ALLIGNED, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_5},

       },
    },
    // ok res, wrong framerate
    {/*58*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_LEVEL_HEVC_51, FRAME_RATE, NONE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_5},

       },
    },
    // ok slices
    {/*59*/ MFX_ERR_NONE, MFX_LEVEL_HEVC_5, SLICE, NONE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_5},

       },
    },
    // wrong slices
    {/*60*/ MFX_ERR_UNSUPPORTED, MFX_LEVEL_HEVC_5, SLICE, NONE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_5},

       },
    },
    // wrong kbps
    {/*61*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_LEVEL_HEVC_5, BR, NONE, {
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN },
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_5 },

       },
    },
    //------- Level 5.1  -------//
    // ok res, ok framerate
    {/*62*/ MFX_ERR_NONE, MFX_LEVEL_HEVC_51, FRAME_RATE, NONE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_51},

       },
    },
    // wrong resolution
    {/*63*/ MFX_ERR_UNSUPPORTED, MFX_LEVEL_HEVC_51, RESOLUTION, NOT_ALLIGNED, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_51},

       },
    },
    // ok res, wrong framerate
    {/*64*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_LEVEL_HEVC_52, FRAME_RATE, NONE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_51},

       },
    },
    // ok slices
    {/*65*/ MFX_ERR_NONE, MFX_LEVEL_HEVC_51, SLICE, NONE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_51},

       },
    },
    // wrong slices
    {/*66*/ MFX_ERR_UNSUPPORTED, MFX_LEVEL_HEVC_51, SLICE, NONE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_51},

       },
    },
    // wrong kbps
    {/*67*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_LEVEL_HEVC_51, BR, NONE, {
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN },
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_51 },

       },
    },
    //------- Level 5.2  -------//
    // ok res, ok framerate
    {/*68*/ MFX_ERR_NONE, MFX_LEVEL_HEVC_52, FRAME_RATE, NONE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_52},

       },
    },
    // wrong resolution
    {/*69*/ MFX_ERR_UNSUPPORTED, MFX_LEVEL_HEVC_52, RESOLUTION, NOT_ALLIGNED, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_52},

       },
    },
    // ok res, wrong framerate
    {/*70*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_LEVEL_HEVC_61, FRAME_RATE, NONE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_52},

       },
    },
    // ok slices
    {/*71*/ MFX_ERR_NONE, MFX_LEVEL_HEVC_52, SLICE, NONE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_52},

       },
    },
    // wrong slices
    {/*72*/ MFX_ERR_UNSUPPORTED, MFX_LEVEL_HEVC_52, SLICE, NONE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_52},

       },
    },
    //------- Level 6  -------//
    // ok res, ok framerate
    {/*74*/ MFX_ERR_NONE, MFX_LEVEL_HEVC_6, FRAME_RATE, NONE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_6},

       },
    },
    // wrong resolution
    {/*75*/ MFX_ERR_UNSUPPORTED, MFX_LEVEL_HEVC_6, RESOLUTION, NOT_ALLIGNED, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_6},

       },
    },
    // ok res, wrong framerate
    {/*76*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_LEVEL_HEVC_61, FRAME_RATE, NONE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_6},

       },
    },
    // ok slices
    {/*77*/ MFX_ERR_NONE, MFX_LEVEL_HEVC_6, SLICE, NONE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_6},

       },
    },
    // wrong slices
    {/*78*/ MFX_ERR_UNSUPPORTED, MFX_LEVEL_HEVC_6, SLICE, NONE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_6},

       },
    },
    //------- Level 6.1  -------//
    // ok res, ok framerate
    {/*79*/ MFX_ERR_NONE, MFX_LEVEL_HEVC_61, FRAME_RATE, NONE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_61},

       },
    },
    // wrong resolution
    {/*80*/ MFX_ERR_UNSUPPORTED, MFX_LEVEL_HEVC_61, RESOLUTION, NOT_ALLIGNED, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_61},

       },
    },
    // ok res, wrong framerate
    {/*81*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_LEVEL_HEVC_62, FRAME_RATE, NONE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_61},

       },
    },
    // ok slices
    {/*82*/ MFX_ERR_NONE, MFX_LEVEL_HEVC_61, SLICE, NONE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_61},

       },
    },
    // wrong slices
    {/*83*/ MFX_ERR_UNSUPPORTED, MFX_LEVEL_HEVC_61, SLICE, NONE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_61},

       },
    },
    //------- Level 6.2  -------//
    // ok res, ok framerate
    {/*84*/ MFX_ERR_NONE, MFX_LEVEL_HEVC_62, FRAME_RATE, NONE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_62},

       },
    },
    // wrong resolution
    {/*85*/ MFX_ERR_UNSUPPORTED, MFX_LEVEL_HEVC_62, RESOLUTION, NOT_ALLIGNED, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_62},

       },
    },
    // ok res, wrong framerate
    {/*86*/ MFX_ERR_UNSUPPORTED, MFX_LEVEL_HEVC_62, NONE, NONE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_62},
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 8192 },
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 4320 },
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 140 },
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1 },
       },
    },
    // ok slices
    {/*87*/ MFX_ERR_NONE, MFX_LEVEL_HEVC_62, SLICE, NONE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_62},

       },
    },
    // wrong slices
    {/*88*/ MFX_ERR_UNSUPPORTED, MFX_LEVEL_HEVC_62, SLICE, NONE, {
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAIN},
        {MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_62},

       },
    },
    //------- unsupported profiles  -------//
    {/*89*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_LEVEL_HEVC_1, PROFILE, NONE, {
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_HEVC_MAINSP },
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_1 },
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 128 },
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 96 },
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30 },
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1 },
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.GopPicSize, 2 },
       },
    },
    {/*90*/ MFX_ERR_UNSUPPORTED, MFX_LEVEL_HEVC_1, PROFILE, NONE, {
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_HIGH },
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.CodecLevel, MFX_LEVEL_HEVC_1 },
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width, 128 },
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 96 },
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30 },
        { MFXPAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1 },
       },
    },
};

const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case)/sizeof(TestSuite::tc_struct);

int TestSuite::RunTest(unsigned int id)
{
    TS_START;
    const tc_struct& tc = test_case[id];

    m_pParOut = new mfxVideoParam;
    *m_pParOut = m_par;

    MFXInit();

    tsExtBufType<mfxVideoParam> par_out(m_par);
    SETPARS(m_pPar, MFXPAR);

    m_par.mfx.FrameInfo.CropW = 0;
    m_par.mfx.FrameInfo.CropH = 0;

    PreparePar(tc);

    //load plugin
    Load();

    g_tsStatus.expect(tc.sts);
    bool skip = false;
    bool skip_check_level = false;

    if (m_loaded)
    {
        if (0 == memcmp(m_uid->Data, MFX_PLUGINID_HEVCE_HW.Data, sizeof(mfxU8)* 16))
        {
            if (g_tsHWtype < MFX_HW_SKL) // MFX_PLUGIN_HEVCE_HW - unsupported on platform less SKL
            {
                g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
                g_tsLog << "WARNING: Unsupported HW Platform!\n";
                Query();
                return 0;
            }
            if (tc.sts == MFX_ERR_UNSUPPORTED)
            {
                if ( (tc.type == RESOLUTION) && (tc.sub_type == NOT_ALLIGNED) ||
                     (tc.type == SLICE) )
                {
                    g_tsStatus.expect(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
                }
            }
            if (g_tsStatus.m_expected != MFX_ERR_UNSUPPORTED)
            {
                if ((m_par.mfx.FrameInfo.Width > 4096) || (m_par.mfx.FrameInfo.Height > 2176))
                {
                    g_tsStatus.expect(MFX_ERR_NONE);
                    skip = true;
                    g_tsLog << "Case skipped. HEVCE HW doesn't support width > 4096 and height > 2176 \n";
                }

            }
        }
        else
        {
            if ((tc.type == SLICE) && (tc.sts == MFX_WRN_INCOMPATIBLE_VIDEO_PARAM))
            {
                m_par.mfx.NumSlice = TS_MIN(m_par.mfx.NumSlice, 34);
                if ((((m_par.mfx.FrameInfo.Width + 63) / 64) < m_par.mfx.NumSlice) ||
                    (((m_par.mfx.FrameInfo.Height + 63) / 64) < m_par.mfx.NumSlice))
                {

                    skip_check_level = true;
                    g_tsLog << "Skip Check Level!\n";
                }
            }
            if (tc.type == PROFILE)
            {
                g_tsStatus.expect(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
            }
            if (tc.type == BR)
            {
                m_par.mfx.RateControlMethod = MFX_RATECONTROL_VBR;
            }
        }
    }

    if (!skip)
    {
        Query();
        if ((tc.exp_level != m_pParOut->mfx.CodecLevel) &&
            (tc.exp_level != (m_pParOut->mfx.CodecLevel & 255)) &&
            (!skip_check_level))
        {
            g_tsLog << "Return invalid CodecLevel: " << m_pParOut->mfx.CodecLevel << ", expected: " << tc.exp_level << "!\n";
            g_tsStatus.m_failed = true;
            if (g_tsStatus.m_throw_exceptions)
                throw tsFAIL;
        }
    }


    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(hevce_level_profile);
}