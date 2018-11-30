//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2018 Intel Corporation. All Rights Reserved.
//

//
// Description:
// The test verifies that the HEVC encoder correctly sets the QP when
// the MinQPI/MaxQPI/MinQPP/MaxQPP/MinQPB/MaxQPB values are set.
//

#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"
#include <fstream>

namespace avce_qp_range
{
    enum
    {
        MFX_PAR  = 1,
        EXT_COD2 = 2,
    };

    struct tc_struct
    {
        int n_frames;
        struct f_pair
        {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxU32 v;
        }set_par[MAX_NPARS];
    };

    class TestSuite:tsVideoEncoder
    {
    private:
        enum
        {
            MFX_PAR = 1,
            EXT_COD2,
            EXT_COD3
        };

    public:
        TestSuite(): tsVideoEncoder(MFX_CODEC_AVC) {}
        ~TestSuite() {}

        int RunTest(unsigned int id);
        static const unsigned int n_cases;

        void initParams();
        void encode(int n);
        void check_bs(int n);
        void check_qp(mfxU32 QP, mfxU32 frame_type);

        std::vector <mfxU32> reported_ft;
        static const tc_struct test_case[];
    };

    void TestSuite::initParams()
    {
        m_par.mfx.CodecId                 = MFX_CODEC_AVC;
        m_par.mfx.MaxKbps                 = 0;
        m_par.mfx.FrameInfo.FourCC        = MFX_FOURCC_NV12;
        m_par.mfx.FrameInfo.FrameRateExtN = 30;
        m_par.mfx.FrameInfo.FrameRateExtD = 1;
        m_par.mfx.FrameInfo.ChromaFormat  = MFX_CHROMAFORMAT_YUV420;
        m_par.mfx.FrameInfo.PicStruct     = MFX_PICSTRUCT_PROGRESSIVE;
        m_par.IOPattern                   = MFX_IOPATTERN_IN_SYSTEM_MEMORY;

        m_par.AddExtBuffer(MFX_EXTBUFF_CODING_OPTION2, sizeof(mfxExtCodingOption2));
        m_par.AddExtBuffer(MFX_EXTBUFF_CODING_OPTION3, sizeof(mfxExtCodingOption3));
        m_bitstream.AddExtBuffer(MFX_EXTBUFF_ENCODED_FRAME_INFO, sizeof(mfxExtAVCEncodedFrameInfo));
    }

    void TestSuite::encode(int n)
    {
        AllocSurfaces();
        AllocBitstream(m_par.mfx.FrameInfo.Width * m_par.mfx.FrameInfo.Height * 3 * n);

        m_pSurf = 0;
        int c = 0;
        const char *filename = NULL;

        switch (m_par.mfx.FrameInfo.Width)
        {
            case 320:
                filename = "YUV/To_The_Limit_320x240_300.yuv";
                break;
            case 720:
                filename = "YUV/iceage_720x480_491.yuv";
                break;
            case 1920:
                filename = "YUV/matrix_1920x1088_500.yuv";
                break;
            case 3840:
                filename = "YUV/horsecab_3840x2160_30fps_1s_YUV.yuv";
                break;
        }

        tsRawReader reader(g_tsStreamPool.Get(filename), m_par.mfx.FrameInfo, n*2);
        g_tsStreamPool.Reg();
        m_filler = &reader;

        int e, s;
        for(e = 0, s = 0; e<=s; )
        {
            mfxStatus sts = EncodeFrameAsync();

            if (m_pSurf)
                s++;
            if (m_pSurf && sts == MFX_ERR_MORE_DATA)
            {
                if (reader.m_eos) break;
                continue;
            }

            if (sts == MFX_ERR_NONE )
            {
                SyncOperation();
                mfxExtAVCEncodedFrameInfo* efi = m_bitstream;
                TestSuite::reported_ft.push_back(m_bitstream.FrameType & (MFX_FRAMETYPE_I | MFX_FRAMETYPE_P | MFX_FRAMETYPE_B));
                e++;
            }
            else
                break;
        }

        if (e < n)
        {
            g_tsLog << "\n\nERROR: Encoded frames count (" << e << ") < requested frame count (" << n << ")\n\n\n";
            throw tsFAIL;
        }

    }

    void TestSuite::check_qp(mfxU32 QP, mfxU32 frame_type)
    {
        mfxExtCodingOption2* co2 = m_par;

        switch (frame_type)
        {
            case MFX_FRAMETYPE_I:
            {
                if (co2->MinQPI)
                    EXPECT_TRUE(QP >= co2->MinQPI) << "Frame QP is less than MinQPI: "    << QP << " < " << co2->MinQPI << "\n";
                if (co2->MaxQPI)
                    EXPECT_TRUE(QP <= co2->MaxQPI) << "Frame QP is greater than MaxQPI: " << QP << " > " << co2->MaxQPI << "\n";
                break;
            }
            case MFX_FRAMETYPE_P:
            {
                if (co2->MinQPP)
                    EXPECT_TRUE(QP >= co2->MinQPP) << "Frame QP is less than MinQPP: "    << QP << " < " << co2->MinQPP << "\n";
                if (co2->MaxQPP)
                    EXPECT_TRUE(QP <= co2->MaxQPP) << "Frame QP is greater than MaxQPP: " << QP << " > " << co2->MaxQPP << "\n";
                break;
            }
            case MFX_FRAMETYPE_B:
            {
                if (co2->MinQPB)
                    EXPECT_TRUE(QP >= co2->MinQPB) << "Frame QP is less than MinQPB: "    << QP << " < " << co2->MinQPB << "\n";
                if (co2->MaxQPB)
                    EXPECT_TRUE(QP <= co2->MaxQPB) << "Frame QP is greater than MaxQPB: " << QP << " > " << co2->MaxQPB << "\n";
                break;
            }
        }
    }

    void TestSuite::check_bs(int n_fr)
    {
        mfxU16 MBBRC = 0;

        if(m_par.NumExtParam)
        {
            mfxExtCodingOption2* co2 = m_par;
            MBBRC = co2->MBBRC;
            if(MBBRC == MFX_CODINGOPTION_OFF)
                MBBRC = 0;
        }

        tsParserAVC2 parser(m_bitstream, BS_AVC2::INIT_MODE_PARSE_SD);
        mfxU8 frame_found = 0;
        mfxU32 qp         = 0;
        mfxU32 qp1        = 0;
        mfxU32 qp2        = 0;

        for(int f = 0; f < n_fr; f++)
        {
            BS_AVC2::AU* hdr     = parser.Parse();
            Bs32u n              = hdr->NumUnits;
            BS_AVC2::NALU** nalu = hdr->nalu;
            mfxU32 average_qp    = 0;

            for(mfxU32 i = 0; i < n; i++)
            {
                mfxU8 type = nalu[i][0].nal_unit_type;
                if(!average_qp && ((type == 5) || (type == 1)))
                {
                    frame_found = 1;

                    BS_AVC2::Slice* slice = nalu[i][0].slice;
                    BS_AVC2::MB* mb       = slice->mb;
                    mfxU32 n_mb           = 0;
                    mfxU32 mb_qp_delta    = 0;

                    Bs32u QpBdOffsetY         = 6 * nalu[i][0].sps->bit_depth_luma_minus8;
                    Bs8s  slice_qp_delta      = int(slice->slice_qp_delta);
                    Bs32s pic_init_qp_minus26 = nalu[i][0].pps->pic_init_qp_minus26;
                    Bs32s sqp                 = pic_init_qp_minus26 + 26 + slice_qp_delta;
                    Bs32s prev_qp             = sqp;

                    if (MBBRC)
                    {
                        while(n_mb < slice->NumMB)
                        {
                            qp = ((prev_qp + mb->mb_qp_delta + 52 + 2 * QpBdOffsetY) % (52 + QpBdOffsetY)) - QpBdOffsetY;
                            prev_qp = qp;
                            average_qp += qp;

                            if(mb->next)
                                mb = mb->next;
                            n_mb++;
                        }
                        average_qp = (mfxU32)(average_qp / slice->NumMB);
                    }
                    else
                    {
                        while((mb->mb_skip_flag == 1 || mb->mb_qp_delta == mb_qp_delta) && n_mb < slice->NumMB)
                        {
                            if(mb->next) mb = mb->next;
                            n_mb++;
                        }

                        if(mb && mb->mb_qp_delta)
                            mb_qp_delta = mb->mb_qp_delta;

                        qp1 = ((sqp + 0 + 52 + 2 * QpBdOffsetY) % (52 + QpBdOffsetY)) - QpBdOffsetY;
                        qp2 = ((sqp + mb_qp_delta + 52 + 2 * QpBdOffsetY) % (52 + QpBdOffsetY)) - QpBdOffsetY;
                        average_qp = (mfxU32)( (qp1*n_mb + qp2*(slice->NumMB-n_mb))/slice->NumMB );
                    }
                }
            }
            if (frame_found)
                TestSuite::check_qp(average_qp, TestSuite::reported_ft[f]);
        }
    }

    const tc_struct TestSuite::test_case[] =
    {
        {/*00*/ 10,
        {{MFX_PAR,  &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.GopPicSize,        4},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.GopRefDist,        1},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,   320},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.Height,  240},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.TargetKbps,        190},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.MaxKbps,           200},
        { EXT_COD2, &tsStruct::mfxExtCodingOption2.ExtBRC,          MFX_CODINGOPTION_OFF},
        { EXT_COD2, &tsStruct::mfxExtCodingOption2.MaxQPI,          10}}},

        {/*01*/ 10,
        {{MFX_PAR,  &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.GopPicSize,        4},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.GopRefDist,        2},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,   720},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.Height,  480},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.TargetKbps,        1300},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.MaxKbps,           2000},
        { EXT_COD2, &tsStruct::mfxExtCodingOption2.ExtBRC,          MFX_CODINGOPTION_OFF},
        { EXT_COD2, &tsStruct::mfxExtCodingOption2.MBBRC,           MFX_CODINGOPTION_ON},
        { EXT_COD2, &tsStruct::mfxExtCodingOption2.MinQPP,          40}}},

        {/*02*/ 10,
        {{MFX_PAR,  &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_VBR},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.GopPicSize,        4},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.GopRefDist,        2},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,   1920},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.Height,  1088},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.TargetKbps,        2500},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.MaxKbps,           4000},
        { EXT_COD2, &tsStruct::mfxExtCodingOption2.ExtBRC,          MFX_CODINGOPTION_OFF},
        { EXT_COD2, &tsStruct::mfxExtCodingOption2.MBBRC,           MFX_CODINGOPTION_ON},
        { EXT_COD2, &tsStruct::mfxExtCodingOption2.MaxQPI,          10},
        { EXT_COD2, &tsStruct::mfxExtCodingOption2.MaxQPP,          15},
        { EXT_COD2, &tsStruct::mfxExtCodingOption2.MaxQPB,          25}}},

        {/*03*/ 10,
        {{MFX_PAR,  &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.GopPicSize,        4},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,   720},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.Height,  480},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.TargetKbps,        1300},
        { EXT_COD2, &tsStruct::mfxExtCodingOption2.ExtBRC,          MFX_CODINGOPTION_OFF},
        { EXT_COD2, &tsStruct::mfxExtCodingOption2.MBBRC,           MFX_CODINGOPTION_ON},
        { EXT_COD2, &tsStruct::mfxExtCodingOption2.MinQPI,          10},
        { EXT_COD2, &tsStruct::mfxExtCodingOption2.MaxQPI,          15}}},

        {/*04*/ 10,
        {{MFX_PAR,  &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.GopRefDist,        3},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,   3840},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.Height,  2160},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.TargetKbps,        5000},
        { EXT_COD2, &tsStruct::mfxExtCodingOption2.ExtBRC,          MFX_CODINGOPTION_OFF},
        { EXT_COD2, &tsStruct::mfxExtCodingOption2.MBBRC,           MFX_CODINGOPTION_ON},
        { EXT_COD2, &tsStruct::mfxExtCodingOption2.MaxQPI,          30},
        { EXT_COD2, &tsStruct::mfxExtCodingOption2.MinQPP,          40},
        { EXT_COD2, &tsStruct::mfxExtCodingOption2.MinQPB,          20}}},

        {/*05*/ 10,
        {{MFX_PAR,  &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.GopPicSize,        4},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.GopRefDist,        3},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,   720},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.Height,  480},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.TargetKbps,        190},
        { EXT_COD2, &tsStruct::mfxExtCodingOption2.LookAheadDepth,  10},
        { EXT_COD2, &tsStruct::mfxExtCodingOption2.LookAheadDS,     MFX_LOOKAHEAD_DS_OFF},
        { EXT_COD2, &tsStruct::mfxExtCodingOption2.MBBRC,           MFX_CODINGOPTION_ON},
        { EXT_COD2, &tsStruct::mfxExtCodingOption2.MaxQPB,          10}}},

        {/*06*/ 10,
        {{MFX_PAR,  &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.GopPicSize,        4},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.GopRefDist,        3},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,   1920},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.Height,  1088},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.TargetKbps,        2500},
        { EXT_COD2, &tsStruct::mfxExtCodingOption2.LookAheadDepth,  100},
        { EXT_COD2, &tsStruct::mfxExtCodingOption2.LookAheadDS,     MFX_LOOKAHEAD_DS_2x},
        { EXT_COD2, &tsStruct::mfxExtCodingOption2.MBBRC,           MFX_CODINGOPTION_ON},
        { EXT_COD2, &tsStruct::mfxExtCodingOption2.MaxQPI,          5},
        { EXT_COD2, &tsStruct::mfxExtCodingOption2.MaxQPP,          5},
        { EXT_COD2, &tsStruct::mfxExtCodingOption2.MaxQPB,          5}}},

        {/*07*/ 10,
        {{MFX_PAR,  &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.GopPicSize,        4},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.GopRefDist,        3},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,   3840},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.Height,  2160},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.TargetKbps,        5000},
        { EXT_COD2, &tsStruct::mfxExtCodingOption2.LookAheadDepth,  10},
        { EXT_COD2, &tsStruct::mfxExtCodingOption2.LookAheadDS,     4},
        { EXT_COD2, &tsStruct::mfxExtCodingOption2.MBBRC,           MFX_CODINGOPTION_ON},
        { EXT_COD2, &tsStruct::mfxExtCodingOption2.MinQPI,          50},
        { EXT_COD2, &tsStruct::mfxExtCodingOption2.MinQPP,          50},
        { EXT_COD2, &tsStruct::mfxExtCodingOption2.MinQPB,          50}}},

        {/*08*/ 10,
        {{MFX_PAR,  &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_HRD},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.TargetUsage,       MFX_TARGETUSAGE_4},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,   320},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.Height,  240},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.TargetKbps,        190},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.MaxKbps,           200},
        { EXT_COD2, &tsStruct::mfxExtCodingOption2.MinQPI,          45},
        { EXT_COD2, &tsStruct::mfxExtCodingOption2.MaxQPI,          50}}},

        {/*09*/ 10,
        {{MFX_PAR,  &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_LA_HRD},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.TargetUsage,       MFX_TARGETUSAGE_4},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,   720},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.Height,  480},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.TargetKbps,        1300},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.MaxKbps,           2000},
        { EXT_COD2, &tsStruct::mfxExtCodingOption2.MBBRC,           MFX_CODINGOPTION_ON},
        { EXT_COD2, &tsStruct::mfxExtCodingOption2.MinQPI,          30},
        { EXT_COD2, &tsStruct::mfxExtCodingOption2.MinQPP,          50}}},

        {/*10*/ 10,
        {{MFX_PAR,  &tsStruct::mfxVideoParam.mfx.RateControlMethod,  MFX_RATECONTROL_LA_HRD},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.TargetUsage,        MFX_TARGETUSAGE_1},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,    1920},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.Height,   1088},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.TargetKbps,         2500},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.MaxKbps,            4000},
        { EXT_COD2, &tsStruct::mfxExtCodingOption2.LookAheadDS,      MFX_LOOKAHEAD_DS_2x},
        { EXT_COD2, &tsStruct::mfxExtCodingOption2.MBBRC,            MFX_CODINGOPTION_ON},
        { EXT_COD2, &tsStruct::mfxExtCodingOption2.MinQPP,           45},
        { EXT_COD2, &tsStruct::mfxExtCodingOption2.MaxQPP,           50}}},

        {/*11*/ 10,
        {{MFX_PAR,  &tsStruct::mfxVideoParam.mfx.RateControlMethod,  MFX_RATECONTROL_LA_HRD},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.TargetUsage,        MFX_TARGETUSAGE_1},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,    3840},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.FrameInfo.Height,   2160},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.TargetKbps,         5000},
        { MFX_PAR,  &tsStruct::mfxVideoParam.mfx.MaxKbps,            8000},
        { EXT_COD2, &tsStruct::mfxExtCodingOption2.MaxQPP,           40},
        { EXT_COD2, &tsStruct::mfxExtCodingOption2.MaxQPB,           40}}},
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::test_case[0]);

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;
        auto& tc = test_case[id];
        MFXInit();
        initParams();

        SETPARS(&m_par, MFX_PAR);

        m_par.mfx.FrameInfo.CropW = m_par.mfx.FrameInfo.Width;
        m_par.mfx.FrameInfo.CropH = m_par.mfx.FrameInfo.Height;

        Init();
        encode(tc.n_frames);
        check_bs(tc.n_frames);

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS(avce_qp_range);
}
