/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2017 Intel Corporation. All Rights Reserved.

File Name: avce_qp_report.cpp
\* ****************************************************************************** */

/*!
\file avce_qp_report.cpp
\brief Gmock test avce_qp_report

Description:
This suite tests AVC encoder\n

Algorithm:
- Initialize MSDK session
- Set test suite and test case params
- Encode stream and collect reported qp from each frame
- Parse stream and calculate QP
- Compare caltulated and reported QP

*/
#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"
#include <fstream>

#define DUMP_BS(bs);  std::fstream fout("out.bin", std::fstream::binary | std::fstream::out);fout.write((char*)bs.Data, bs.DataLength); fout.close();
/*! \brief Main test name space */
namespace avce_qp_report
{

    enum
    {
        MFX_PAR = 1,
        CDO2_PAR = 2,
    };

    /*!\brief Structure of test suite parameters */
    struct tc_struct
    {
        //! \brief Number of farmes to encode
        int n_frames;
        /*! \brief Structure contains params for some fields of encoder */
        struct f_pair
        {
            //! Number of the params set (if there is more than one in a test case)
            mfxU32 ext_type;
            //! Field name
            const  tsStruct::Field* f;
            //! Field value
            mfxU32 v;
        }set_par[MAX_NPARS];
    };

    //!\brief Main test class
    class TestSuite:tsVideoEncoder
    {
    public:
        //! \brief A constructor (AVC encoder)
        TestSuite(): tsVideoEncoder(MFX_CODEC_AVC) {}
        //! \brief A destructor
        ~TestSuite() {}
        //! \brief Main method. Runs test case
        //! \param id - test case number
        int RunTest(unsigned int id);
        //! The number of test cases
        static const unsigned int n_cases;
        //! \brief Initialize params common for whole test suite
        void initParams();
        //! \brief Encodes n frames and put QP reported values in a reported_qp vector
        //! \param n - number of frames to encode
        void encode(int n);
        //! \brief Parse stream and calculate QP value for each frame. Compare the results with reported QP values
        //! \param n - number of frames to check
        void check_bs(int n);
        //! \brief check_bs if ratecontrol = MFX_RATECONTROL_CQP
        //! \param n - number of frames to check
        void check_bs_cqp(int n);
        //! \brief check_bs if ratecontrol = MFX_RATECONTROL_CBR or FX_RATECONTROL_VBR but MMBRC is off
        //! \param n - number of frames to check
        void check_bs_cbr_vbr(int n);
        //! \brief check_bs if ratecontrol = MFX_RATECONTROL_CBR or FX_RATECONTROL_VBR but MMBRC is on
        //! \param n - number of frames to check
        void check_bs_cbr_vbr_mbbrc(int n);
        //! \brief Compares reported and encoded QP values with special message (in case of compare fail)
        //! \param expected - Encoded QP value
        //! \param reported - Reported QP value
        //! \param message - special message (fail reason)
        void check_qp_msg(Bs32s expected, Bs32s reported, const char* message);
        //! \brief Compares reported and encoded QP values with standart message
        //! \param expected - Encoded QP value
        //! \param reported - Reported QP value
        void check_qp(Bs32s expected, Bs32s reported);
        //! \brief vector with reported QP values
        std::vector <mfxU32> reported_qp;
        //! \brief vector with reported frame types
        std::vector <mfxU32> reported_ft;
        //! \brief Set of test cases
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
        m_bitstream.AddExtBuffer(MFX_EXTBUFF_ENCODED_FRAME_INFO, sizeof(mfxExtAVCEncodedFrameInfo));

    }

    void TestSuite::check_qp_msg(Bs32s expected, Bs32s reported, const char* msg)
    {
#if defined (WIN32)||(WIN64)
        if ((m_impl & MFX_IMPL_HARDWARE) && (m_impl & MFX_IMPL_VIA_D3D11)) {
            EXPECT_EQ(expected, reported) << msg;
        }
        else {
            EXPECT_EQ(0, reported) << "ERROR: QP report feature is unsupported, reported QP must be 0";
        }
#else
            EXPECT_EQ(0, reported) << "ERROR: QP report feature is unsupported, reported QP must be 0";
#endif
    }

    void TestSuite::check_qp(Bs32s expected, Bs32s reported)
    {
        check_qp_msg(expected, reported, "ERROR: Reported QP != Encoded QP");
    }


    void TestSuite::encode(int n)
    {

        AllocSurfaces();
        AllocBitstream(m_par.mfx.FrameInfo.Width * m_par.mfx.FrameInfo.Height * 3 * n);
        m_pSurf = 0;

        int c = 0;

        const char *filename = NULL;

        switch (m_par.mfx.FrameInfo.Width) {
        case 320:
                filename = "YUV/To_The_Limit_320x240_300.yuv";
            break; //case w:320
        case 640:
                filename = "YUV/To_The_Limit_640x480_300.yuv";
            break; //case w:640
        case 720:
            switch (m_par.mfx.FrameInfo.Height) {
            case 240:
                filename = "YUV/iceage_720x240_491.yuv";
                break; //case h:240
            default:
                filename = "YUV/iceage_720x480_491.yuv";
                break; //case h:480
            }
            break; //case w:720
        case 1920:
        case 2048:
                filename = "YUV/matrix_1920x1088_500.yuv";
            break; //case w:1920,2048
        case 3840:
                filename = "YUV/horsecab_3840x2160_30fps_1s_YUV.yuv";
            break; //case w:3840
        }

        /*
            n multiply 2 because counter increments for two for every ProcessSurface step
            inside the method EncodeFrameAsync
        */
        tsRawReader reader(g_tsStreamPool.Get(filename), m_par.mfx.FrameInfo, n*2);
        g_tsStreamPool.Reg();
        m_filler = &reader;

        int e, s;
        for(e = 0, s = 0; e<=s; ) {
            mfxStatus sts = EncodeFrameAsync();

            if (m_pSurf)
                s++;
            if (m_pSurf && sts == MFX_ERR_MORE_DATA) {
                if (reader.m_eos) break;
                continue;
            }

            if (sts == MFX_ERR_NONE ) {
                SyncOperation();
                mfxExtAVCEncodedFrameInfo* efi = m_bitstream; /* GetExtBuffer */
                TestSuite::reported_qp.push_back(efi->QP);
                TestSuite::reported_ft.push_back(m_bitstream.FrameType & (MFX_FRAMETYPE_I | MFX_FRAMETYPE_P | MFX_FRAMETYPE_B));
                e++;
            } else {
                break;
            }
        }

        if (e < n) {
            g_tsLog << "\n\nERROR: Encoded frames count (" << e << ") < requested frame count (" << n << ")\n\n\n";
            throw tsFAIL;
        }
    }

    /// Counting QP for CQP in the bitstream
    /*! Algorithm is:
    QP is counting for the slice (multi-slice does not supported).
    After that QP is comparing to the QPI,QPP,QPB.
    */
    void TestSuite::check_bs_cqp(int n_fr)
    {
        tsParserH264AU parser(m_bitstream);
        for(int i = 0; i < n_fr; i++) {
            BS_H264_au* hdr = parser.Parse();

            if(hdr == nullptr) return;
            Bs32u n = hdr->NumUnits;

            BS_H264_au::nalu_param* nalu = hdr->NALU;
            mfxU8 frame_found = 0;
            mfxU32 qp = 0;

            for(mfxU32 j = 0; j < n; j++) {
                byte type = nalu[j].nal_unit_type;
                if(!qp && ((type == 5) || (type == 1))) {
                    frame_found = 1;
                    slice_header* slice = nalu[j].slice_hdr;

                    Bs32s pic_init_qp_minus26 = slice->pps_active->pic_init_qp_minus26;

                    Bs8s slice_qp_delta = slice->slice_qp_delta;
                    qp = slice_qp_delta + pic_init_qp_minus26 + 26;
                }
            }
            g_tsLog << "Frame: " << i << "\nReported QP = " << TestSuite::reported_qp[i] << " Encoded QP = " << qp << "\n";
            if(frame_found) {
                check_qp(qp, TestSuite::reported_qp[i]);
            }

            if(MFX_FRAMETYPE_I == TestSuite::reported_ft[i]) {
                check_qp_msg(m_par.mfx.QPI, TestSuite::reported_qp[i], "ERROR: Reported QP != provided to encoder QPI");
            } else if(MFX_FRAMETYPE_P == TestSuite::reported_ft[i]) {
                check_qp_msg(m_par.mfx.QPP, TestSuite::reported_qp[i], "ERROR: Reported QP != provided to encoder QPP");
            } else if(MFX_FRAMETYPE_B == TestSuite::reported_ft[i]) {
                check_qp_msg(m_par.mfx.QPB, TestSuite::reported_qp[i], "ERROR: Reported QP != provided to encoder QPB");
            }
        }

    }

    /// Checking QP for non-MBBRC execution
    /*! Algorithm is (on 23 May 2017):
    1. Search for first QP change in the slice
    2. qp1 = Get sum of QP before change (count is n_mb)
    3. qp2 = Get sum of QP after change (count is slice->NumMB minus n_mb)
    4. Get average QP for slice ((qp1*n_mb)+(qp2*(slice->NumMB-n_mb))/slice->NumMB
    */
    void TestSuite::check_bs_cbr_vbr(int n_fr)
    {
        tsParserAVC2 parser(m_bitstream, BS_AVC2::INIT_MODE_PARSE_SD);
        mfxU8 frame_found = 0;
        mfxU32 qp1 = 0, qp2 = 0;
        mfxU32 average_qp = 0;

        for(int f = 0; f < n_fr; f++) {
            BS_AVC2::AU* hdr = parser.Parse();
            Bs32u n = hdr->NumUnits;
            BS_AVC2::NALU** nalu = hdr->nalu;

            for(mfxU32 i = 0; i < n; i++) {
                mfxU8 type = nalu[i][0].nal_unit_type;
                if((type == 5) || (type == 1)) {
                    frame_found = 1;

                    BS_AVC2::Slice* slice = nalu[i][0].slice;
                    BS_AVC2::MB* mb = slice->mb;
                    mfxU32 n_mb = 0;
                    mfxI32 mb_qp_delta = 0;

                    while((mb->mb_skip_flag == 1 || mb->mb_qp_delta == mb_qp_delta) && n_mb < slice->NumMB) {
                        if(mb->next) {
                            mb = mb->next;
                        }
                        n_mb++;
                    }

                    if(mb && mb->mb_qp_delta) {
                        mb_qp_delta = mb->mb_qp_delta;
                    }
                    Bs32u QpBdOffsetY = 6 * nalu[i][0].sps->bit_depth_luma_minus8;

                    Bs8s slice_qp_delta = int(slice->slice_qp_delta);

                    Bs32s pic_init_qp_minus26 = nalu[i][0].pps->pic_init_qp_minus26 + 0;

                    Bs32s sqp = pic_init_qp_minus26 + 26 + slice_qp_delta;

                    qp1 = ((sqp +      0      + 52 + 2 * QpBdOffsetY) % (52 + QpBdOffsetY)) - QpBdOffsetY;
                    qp2 = ((sqp + mb_qp_delta + 52 + 2 * QpBdOffsetY) % (52 + QpBdOffsetY)) - QpBdOffsetY;

                    average_qp = (mfxU32)( (qp1*n_mb + qp2*(slice->NumMB-n_mb))/slice->NumMB );
               }
            }

            g_tsLog << "Frame: " << f << "\nReported QP = " << reported_qp[f] << ", Encoded QP = " << average_qp << "\n";
            if(frame_found) {
                check_qp(average_qp, reported_qp[f]);
            }

        }
    }

    /// Checking QP for MBBRC execution
    /*! Algorithm is (on 23 May 2017):
    Get average QP for all MB in the slice
    */
    void TestSuite::check_bs_cbr_vbr_mbbrc(int n_fr)
    {
        tsParserAVC2 parser(m_bitstream, BS_AVC2::INIT_MODE_PARSE_SD);
        mfxU8 frame_found = 0;
        mfxU32 qp = 0;

        for(int f = 0; f < n_fr; f++) {
            BS_AVC2::AU* hdr = parser.Parse();
            Bs32u n = hdr->NumUnits;
            BS_AVC2::NALU** nalu = hdr->nalu;
            mfxU32 average_qp = 0;

            for(mfxU32 i = 0; i < n; i++) {
                mfxU8 type = nalu[i][0].nal_unit_type;
                if((type == 5) || (type == 1)) {
                    frame_found = 1;

                    BS_AVC2::Slice* slice = nalu[i][0].slice;
                    BS_AVC2::MB* mb = slice->mb;
                    mfxU32 n_mb = 0;
                    mfxU32 mb_qp_delta = 0;

                    Bs32u QpBdOffsetY = 6 * nalu[i][0].sps->bit_depth_luma_minus8;
                    Bs8s slice_qp_delta = int(slice->slice_qp_delta);

                    Bs32s pic_init_qp_minus26 = nalu[i][0].pps->pic_init_qp_minus26;
                    Bs32s sqp = pic_init_qp_minus26 + 26 + slice_qp_delta;
                    Bs32s prev_qp = sqp;

                    while(n_mb < slice->NumMB) {
                        qp= ((prev_qp + mb->mb_qp_delta + 52 + 2 * QpBdOffsetY) % (52 + QpBdOffsetY)) - QpBdOffsetY;
                        prev_qp = qp;
                        average_qp += qp;
                        mb = mb->next;
                        n_mb++;
                    }
                    average_qp = (mfxU32)(average_qp / slice->NumMB);
                }
            }

            if(frame_found) {
                g_tsLog << "Frame: " << f << "\nReported QP = " << reported_qp[f] << ", Encoded QP = " << average_qp << "\n";
#if defined (WIN32) || (WIN64)
                if(average_qp + 1 < reported_qp[f] || average_qp - 1 > reported_qp[f]) {
                    g_tsLog << "ERROR: Reported QP much bigger/much smaller than average Encoded QP";
                    check_qp(qp, reported_qp[f]);
                }
#endif
            }

        }
    }

    void TestSuite::check_bs(int f)
    {
        mfxU16 MBBRC = 0;

        if(m_par.NumExtParam) {
            mfxExtCodingOption2* co2 = m_par; /* GetExtBuffer */
            MBBRC = co2->MBBRC;
            if(MBBRC == MFX_CODINGOPTION_OFF)
                MBBRC = 0;
        }

        if(m_par.mfx.RateControlMethod == MFX_RATECONTROL_CQP) {
            check_bs_cqp(f);
        } else if(((m_par.mfx.RateControlMethod == MFX_RATECONTROL_CBR || m_par.mfx.RateControlMethod == MFX_RATECONTROL_VBR) && !MBBRC)) {
            check_bs_cbr_vbr(f);
        } else if(((m_par.mfx.RateControlMethod == MFX_RATECONTROL_CBR || m_par.mfx.RateControlMethod == MFX_RATECONTROL_VBR) && MBBRC)) {
            check_bs_cbr_vbr_mbbrc(f);
        }
    }


    const tc_struct TestSuite::test_case[] =
    {
        {/*00*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CQP },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  720 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP,              1 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI,              1 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPB,              1 } } },
        {/*01*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CQP },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  320 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 240 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP,              8 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI,              8 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPB,              8 } } },
        {/*02*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CQP },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  1920 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 1088 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP,              16 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI,              16 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPB,              16 } } },
        {/*03*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CQP },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  640 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP,              26 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI,              26 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPB,              26 } } },
        {/*04*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CQP },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  1920 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 1088 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP,              32 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI,              32 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPB,              32 } } },
        {/*05*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CQP },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  1920 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP,              48 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI,              48 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPB,              48 } } },
        {/*06*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CQP },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  720 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 240 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP,              51 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI,              51 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPB,              51 } } },
        {/*07*/ 48,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CQP },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  720 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP,              32 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI,              16 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPB,              28 } } },
        {/*08*/ 48,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CQP },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  720 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 448 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP,              18 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI,              8 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPB,              40 } } },

        {/*09*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  320 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 240 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       190 } } },
        {/*10*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  720 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       190 } } },
        {/*11*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  720 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       1000 } } },
        {/*12*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  720 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       2000 } } },
        {/*13*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  1920 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 1088 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       1080 } } },

        {/*14*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  320 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 240 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       190 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          200 } } },
        {/*15*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  720 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       190 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          300 } } },
        {/*16*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  720 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       1000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          1500 } } },
        {/*17*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  720 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       2000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          3000 } } },
        {/*18*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  1920 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 1088 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       1080 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          2000 } } },
        {/*19*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  720 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       4000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          7000 } } },
        {/*20*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  720 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       4000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          7000 } } },
        {/*21*/ 2,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  2048 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 2048 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       8000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          10000 } } },
        {/*22*/ 2,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  2048 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 2048 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       10000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          15000 } } },

        {/*23*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  320 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 240 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       190 },
        { CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_ON } } },
        {/*24*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  720 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       190 },
        { CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_ON } } },
        {/*25*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  720 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       1000 },
        { CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_ON } } },
        {/*26*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  720 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       2000 },
        { CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_ON } } },
        {/*27*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  1920 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 1088 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       1080 },
        { CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_ON } } },

        {/*28*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  320 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 240 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       190 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          200 },
        { CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_ON } } },
        {/*29*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  720 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       190 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          300 },
        { CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_ON } } },
        {/*30*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  720 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       1000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          1500 },
        { CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_ON } } },
        {/*31*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  720 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       2000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          3000 },
        { CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_ON } } },
        {/*32*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  1920 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 1088 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       1080 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          2000 },
        { CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_ON } } },
        {/*33*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  720 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       4000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          7000 },
        { CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_ON } } },
        {/*34*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  720 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       4000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          7000 },
        { CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_ON } } },
        {/*35*/ 2,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  2048 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 2048 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       8000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          10000 },
        { CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_ON } } },
        {/*36*/ 2,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  2048 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 2048 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       10000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          15000 },
        { CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_ON } } },
        {/*37*/ 1,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  3840 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 1088 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       4000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          7000 },
        { CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_ON } } },
        {/*38*/ 1,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  3840 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 1088 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       8000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          10000 },
        { CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_ON } } },
        {/*39*/ 1,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  3840 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 1088 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       10000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          15000 },
        { CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_ON } } },


    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::test_case[0]);

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;
        auto& tc = test_case[id];
        MFXInit();
        initParams();
        SETPARS(&m_par, MFX_PAR);
        SETPARS(&m_par, CDO2_PAR);
        m_par.mfx.FrameInfo.CropW = m_par.mfx.FrameInfo.Width;
        m_par.mfx.FrameInfo.CropH = m_par.mfx.FrameInfo.Height;
        mfxExtCodingOption2 *codingOption2 = reinterpret_cast<mfxExtCodingOption2*>(m_par.GetExtBuffer(MFX_EXTBUFF_CODING_OPTION2));

        if (!(m_impl & MFX_IMPL_SOFTWARE) && (!(m_impl & MFX_IMPL_HARDWARE) || !(m_impl & MFX_IMPL_VIA_D3D11)) ) {
            g_tsLog << "\n\nWARNING: QP reporting is only supported by hw and d3d11 mode\n\n\n";
            throw tsSKIP;
        }
        if ((codingOption2->MBBRC==MFX_CODINGOPTION_ON) && (m_impl & MFX_IMPL_SOFTWARE)) {
            g_tsLog << "\n\nWARNING: Software library does not support MBBRC\n\n\n";
            throw tsSKIP;
        }

        Query();
        Init();
        encode(tc.n_frames);
        //DUMP_BS(m_bitstream);
        check_bs(tc.n_frames);
        TS_END;
        return 0;
    }
    //! \brief Regs test suite into test system
    TS_REG_TEST_SUITE_CLASS(avce_qp_report);
}
