/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2015-2016 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_parser.h"
#include "ts_struct.h"

/*
 * Encode stream with pre-defined parameters (including profile/level/GOP configs/etc) and check if
 * profile/level is as defined in the encoded stream
 *
 * Tests configs (from Harmonic) as follows:
 * TEST			Profiles	Level	B Pyramid	GOP size	Bit-rate (Mbps)
 * 1080i59.94		High, Main	4	3 Bs		32		5
 * 1080i50		High, Main	4	3 Bs		32		5
 * 720p59.94		High, Main	3.2	3 Bs		32		4
 * 720p50		High, Main	3.2	3 Bs		32		4
 * 480i59.94		High, Main	3	3 Bs		32		1.5
 * 576i50		High, Main	3	3 Bs		32		1.5
 * 352x288p29.97	Main, Base	1.3	3 Bs, no B	32		0.5
 * 192x192p29.97	Main, Base	1.2	3 Bs, no B	32		0.15
 * 96x96p29.97		Main, Base	1	3 Bs, no B	32		0.08
 * 352x288p25		Main, Base	1.3	3 Bs, no B	32		0.5
 * 192x192p25		Main, Base	1.2	3 Bs, no B	32		0.15
 * 96x96p25 		Main, Base	1	3 Bs, no B	32		0.08
 * 320x192p29.97	Main, Base	1.2	3 Bs, no B	32		0.4
 * 320x192p25		Main, Base	1.2	3 Bs, no B	32		0.4
 *
 * Explanations:
 * 1. For 1080i59.94, frame rate is set as 59.94/2. Same for other interlacing cases.
 * 2. For cases which are set as Basline profile while B is enabled, only Init() is executed and
 *    MFX_WRN_INCOMPATIBLE_VIDEO_PARAM is expected.
 * 3. For 320x192p29.97, the expected level is set as 1.3 in the test, not following that of
 *    "1.2" in the table.
 */

namespace avce_level_profile
{
#if !defined(MSDK_ALIGN16)
#define MSDK_ALIGN16(value)                      (((value + 15) >> 4) << 4) // round up to a multiple of 16
#endif

class TestSuite : public tsVideoEncoder, public tsParserH264AU, public tsBitstreamProcessor
{
public:

    TestSuite() : tsVideoEncoder(MFX_CODEC_AVC)
    {
        m_bs_processor = this;
    }
    ~TestSuite() {}
    int RunTest(unsigned int id);
    static const unsigned int n_cases;

    mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames)
    {
        if (m_par.mfx.FrameInfo.PicStruct & (MFX_PICSTRUCT_FIELD_TFF | MFX_PICSTRUCT_FIELD_BFF))
            nFrames *= 2;

        SetBuffer(bs);

        mfxU16 profile = 0;
        mfxU16 level   = 0;
        while (nFrames--) {
            UnitType& au = ParseOrDie();
            for(mfxU32 i = 0; i < au.NumUnits; i ++)
            {
                if(au.NALU[i].nal_unit_type != 0x7/*SPS*/) continue;

                profile = au.NALU[i].SPS->profile_idc;
                EXPECT_EQ(profile, m_ex_profile);
                m_profile.push_back(profile);

                if (au.NALU[i].SPS->level_idc == 11 && au.NALU[i].SPS->constraint_set3_flag == 1) {
                    level = MFX_LEVEL_AVC_1b;
                } else if (au.NALU[i].SPS->constraint_set3_flag == 0) {
                    level = au.NALU[i].SPS->level_idc;
                }
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
    //profile/level in stream, there might be multiple SPS
    std::vector<mfxU16> m_profile;
    std::vector<mfxU16> m_level;
};

const TestSuite::tc_struct TestSuite::test_case[] =
{
    //Test#0, 1080i59.94/High,Main/4/3Bs/32/5M
    {/* 0*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 1920},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 1080},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_HIGH},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_AVC_4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,   5*1024},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct,        MFX_PICSTRUCT_FIELD_BFF}}},
    {/* 1*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 1920},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 1080},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_HIGH},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_AVC_4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,   5*1024},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct,        MFX_PICSTRUCT_FIELD_TFF}}},
    {/* 2*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 1920},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 1080},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_AVC_4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,   5*1024},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct,        MFX_PICSTRUCT_FIELD_BFF}}},
    {/* 3*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 1920},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 1080},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_AVC_4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,   5*1024},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct,        MFX_PICSTRUCT_FIELD_TFF}}},
    //Test#1, 1080i50/High,Main/4/3Bs/32/5M
    {/* 4*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 1920},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 1080},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_HIGH},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_AVC_4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,   5*1024},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 25},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct,        MFX_PICSTRUCT_FIELD_BFF}}},
    {/* 5*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 1920},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 1088},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_HIGH},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_AVC_4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,   5*1024},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 25},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct,        MFX_PICSTRUCT_FIELD_TFF}}},
    {/* 6*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 1920},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 1080},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_AVC_4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,   5*1024},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 25},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct,        MFX_PICSTRUCT_FIELD_BFF}}},
    {/* 7*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 1920},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 1080},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_AVC_4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,   5*1024},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 25},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct,        MFX_PICSTRUCT_FIELD_TFF}}},
    //Test#2, 720p59.94/High,Main/3.2/3Bs/32/4M
    {/* 8*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 1280},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 720},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_HIGH},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_AVC_32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,   4*1024},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 60000},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001}}},
    {/* 9*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 1280},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 720},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_AVC_32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,   4*1024},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 60000},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001}}},
    //Test#3, 720p50/High,Main/3.2/3Bs/32/4M
    {/*10*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 1280},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 720},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_HIGH},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_AVC_32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,   4*1024},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 50},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1}}},
    {/*11*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 1280},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 720},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_AVC_32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,   4*1024},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 50},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1}}},
    //Test#4, 480i59.94/High,Main/3.2/3Bs/32/1.5M
    {/*12*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 720},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 480},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_HIGH},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_AVC_3},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,   (mfxU16)(1.5*1024)},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct,        MFX_PICSTRUCT_FIELD_TFF}}},
    {/*13*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 720},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 480},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_AVC_3},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,   (mfxU16)(1.5*1024)},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct,        MFX_PICSTRUCT_FIELD_TFF}}},
    {/*14*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 720},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 480},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_HIGH},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_AVC_3},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,   (mfxU16)(1.5*1024)},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct,        MFX_PICSTRUCT_FIELD_BFF}}},
    {/*15*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 720},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 480},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_AVC_3},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,   (mfxU16)(1.5*1024)},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct,        MFX_PICSTRUCT_FIELD_BFF}}},
    //Test#5, 576i50/High,Main/3.2/3Bs/32/1.5M
    {/*16*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 720},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 576},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_HIGH},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_AVC_3},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,   (mfxU16)(1.5*1024)},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 25},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct,        MFX_PICSTRUCT_FIELD_TFF}}},
    {/*17*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 720},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 576},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_AVC_3},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,   (mfxU16)(1.5*1024)},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 25},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct,        MFX_PICSTRUCT_FIELD_TFF}}},
    {/*18*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 720},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 576},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_HIGH},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_AVC_3},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,   (mfxU16)(1.5*1024)},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 25},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct,        MFX_PICSTRUCT_FIELD_BFF}}},
    {/*19*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 720},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 576},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_AVC_3},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,   (mfxU16)(1.5*1024)},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 25},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct,        MFX_PICSTRUCT_FIELD_BFF}}},
    //Test#6, 352x288p29.97/Main,Base/1.3/3Bs,NoB/32/0.5M
    {/*20*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 352},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 288},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_AVC_13},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,   512},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001}}},
    //BASELINE with 3Bs
    {/*21*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 352},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 288},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_BASELINE},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_AVC_13},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,   512},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001}}},
    {/*22*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 352},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 288},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_AVC_13},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   1},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,   512},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001}}},
    {/*23*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 352},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 288},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_BASELINE},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_AVC_13},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   1},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,   512},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001}}},
    //Test#7, 192x192p29.97/Main,Base/1.2/3Bs,NoB/32/0.15M
    {/*24*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 192},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 192},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_AVC_12},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,   (mfxU16)(0.15*1024)},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001}}},
    {/*25*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 192},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 192},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_BASELINE},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_AVC_12},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,   (mfxU16)(0.15*1024)},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001}}},
    {/*26*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 192},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 192},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_AVC_12},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   1},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,   (mfxU16)(0.15*1024)},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001}}},
    {/*27*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 192},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 192},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_BASELINE},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_AVC_12},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   1},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,   (mfxU16)(0.15*1024)},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001}}},
    //Test#8, 96x96p29.97/Main,Base/1/3Bs,NoB/32/0.08M
    {/*28*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 96},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 96},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_AVC_12},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,   (mfxU16)(0.08*1024)},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001}}},
    {/*29*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 96},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 96},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_BASELINE},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_AVC_12},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,   (mfxU16)(0.08*1024)},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001}}},
    {/*30*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 96},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 96},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_AVC_12},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   1},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,   (mfxU16)(0.08*1024)},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001}}},
    {/*31*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 96},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 96},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_BASELINE},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_AVC_12},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   1},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,   (mfxU16)(0.08*1024)},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001}}},
    //Test#9, 352x288p25/Main,Base/1.3/3Bs,NoB/32/0.5M
    {/*32*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 352},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 288},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_AVC_13},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,   512},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 25},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1}}},
    {/*33*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 352},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 288},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_BASELINE},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_AVC_13},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,   512},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 25},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1}}},
    {/*34*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 352},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 288},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_AVC_13},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   1},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,   512},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 25},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1}}},
    {/*35*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 352},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 288},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_BASELINE},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_AVC_13},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   1},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,   512},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 25},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1}}},
    //Test#10, 192x192p25/Main,Base/1.2/3Bs,NoB/32/0.15M
    {/*36*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 192},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 192},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_AVC_12},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,   (mfxU16)(0.15*1024)},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 25},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1}}},
    {/*37*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 192},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 192},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_BASELINE},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_AVC_12},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,   (mfxU16)(0.15*1024)},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 25},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1}}},
    {/*38*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 192},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 192},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_AVC_12},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   1},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,   (mfxU16)(0.15*1024)},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 25},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1}}},
    {/*39*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 192},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 192},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_BASELINE},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_AVC_12},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   1},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,   (mfxU16)(0.15*1024)},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 25},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1}}},
    //Test#11, 96x96p25/Main,Base/1/3Bs,NoB/32/0.08M
    {/*40*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 96},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 96},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_AVC_12},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,   (mfxU16)(0.08*1024)},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 25},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1}}},
    {/*41*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 96},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 96},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_BASELINE},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_AVC_12},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,   (mfxU16)(0.08*1024)},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 25},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1}}},
    {/*42*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 96},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 96},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_AVC_12},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   1},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,   (mfxU16)(0.08*1024)},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 25},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1}}},
    {/*43*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 96},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 96},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_BASELINE},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_AVC_12},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   1},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,   (mfxU16)(0.08*1024)},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 25},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1}}},
    //Test#12, 320x192p29.97/Main,Base/1.2/3Bs,NoB/32/0.4M
    {/*44*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 320},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 192},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_AVC_13},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,   (mfxU16)(0.4*1024)},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001}}},
    {/*45*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 320},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 192},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_BASELINE},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_AVC_13},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,   (mfxU16)(0.4*1024)},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001}}},
    {/*46*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 320},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 192},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_AVC_13},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   1},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,   (mfxU16)(0.4*1024)},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001}}},
    {/*47*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 320},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 192},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_BASELINE},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_AVC_13},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   1},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,   (mfxU16)(0.4*1024)},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 30000},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1001}}},
    //Test#13, 320x192p25/Main,Base/1.2/3Bs,NoB/32/0.4M
    {/*48*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 320},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 192},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_AVC_12},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,   (mfxU16)(0.4*1024)},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 25},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1}}},
    {/*49*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 320},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 192},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_BASELINE},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_AVC_12},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   4},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,   (mfxU16)(0.4*1024)},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 25},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1}}},
    {/*50*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 320},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 192},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_MAIN},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_AVC_12},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   1},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,   (mfxU16)(0.4*1024)},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtN, 25},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.FrameRateExtD, 1}}},
    {/*51*/ MFX_ERR_NONE, {{MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropW, 320},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.CropH, 192},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecProfile, MFX_PROFILE_AVC_BASELINE},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.CodecLevel,   MFX_LEVEL_AVC_12},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopPicSize,   32},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist,   1},
                           {MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,   (mfxU16)(0.4*1024)},
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
    //bitrate is specified, so use CBR
    if (m_par.mfx.TargetKbps) {
        m_par.mfx.RateControlMethod = MFX_RATECONTROL_CBR;
        m_par.mfx.MaxKbps = m_par.mfx.InitialDelayInKB = 0;
    }
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
        AllocBitstream();
        EncodeFrames(nf);

        //ensure there are >=1 SPS in the stream.
        EXPECT_GT(m_profile.size(), (size_t)0);
        EXPECT_GT(m_level.size(), (size_t)0);
    }

    TS_END;
    return 0;
}

TS_REG_TEST_SUITE_CLASS(avce_level_profile);
};
