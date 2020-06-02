/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2020 Intel Corporation. All Rights Reserved.

File Name: avce_mbbrc.cpp
\* ****************************************************************************** */

/*!
\file avce_mbbrc.cpp
\brief Gmock test avce_mbbrc

Description:
This suite tests AVC encoder\n

Algorithm:
- Initialize MSDK session
- Set test suite and test case params
- Encode stream w/ MBBRC and w/o MBBRC
- Parse streams and calculate QP
- Check MBBRC on and MBBRC off

*/
#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"
#include <fstream>

#define DUMP_BS(bs);  std::fstream fout("out.bin", std::fstream::binary | std::fstream::out);fout.write((char*)bs.Data, bs.DataLength); fout.close();
/*! \brief Main test name space */
namespace avce_mbbrc
{

    enum
    {
        MFX_PAR = 1,
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

    class VideoEncoder :public tsVideoEncoder
    {
    public:
        //! \brief A constructor (AVC encoder)
        VideoEncoder() :tsVideoEncoder(MFX_CODEC_AVC) {}
        //! \brief A destructor
        ~VideoEncoder() {}
        //! \brief Initialize params common for whole test suite
        void InitParams();
        //! \brief Encodes n frames
        //! \param n - number of frames to encode
        void Encode(int n);
        //! \brief IsMBBRCWorkOnIPB checks that MBBRC works on all type of frames
        //! \param n - number of frames to check, is_field need for correct checking of AUs
        bool IsMBBRCWorkOnIPB(int n, bool is_field);
    private:
        bool IsValidQP(BS_AVC2::MB & mb);
        //! \brief vector with reported frame types
        std::vector <mfxU32> reported_ft;
    };

    //!\brief Main test class
    class TestSuite
    {
    public:
        //! \brief A constructor (AVC encoder)
        TestSuite() {}
        //! \brief A destructor
        ~TestSuite() {}
        //! \brief Main method. Runs test case
        //! \param id - test case number
        int RunTest(unsigned int id);
        //! The number of test cases
        static const unsigned int n_cases;
        //! \brief Set of test cases
        static const tc_struct test_case[];
        //! Encoder with enabled MBBRC
        VideoEncoder encoder_MBBRC_on;
        //! Encoder with disabled MBBRC
        VideoEncoder encoder_MBBRC_off;
    };

    void VideoEncoder::InitParams()
    {
        m_par.mfx.CodecId                 = MFX_CODEC_AVC;
        if (m_par.mfx.RateControlMethod == MFX_RATECONTROL_CBR)
        {
            m_par.mfx.MaxKbps = 0;
        }
        m_par.mfx.FrameInfo.FourCC        = MFX_FOURCC_NV12;
        m_par.mfx.FrameInfo.FrameRateExtN = 30;
        m_par.mfx.FrameInfo.FrameRateExtD = 1;
        m_par.mfx.FrameInfo.ChromaFormat  = MFX_CHROMAFORMAT_YUV420;
        m_par.IOPattern                   = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
        m_par.mfx.FrameInfo.CropW = m_par.mfx.FrameInfo.Width;
        m_par.mfx.FrameInfo.CropH = m_par.mfx.FrameInfo.Height;

        m_par.AddExtBuffer(MFX_EXTBUFF_CODING_OPTION2, sizeof(mfxExtCodingOption2));

    }

    void VideoEncoder::Encode(int n)
    {
        AllocSurfaces();
        AllocBitstream(m_par.mfx.FrameInfo.Width * m_par.mfx.FrameInfo.Height * 3 * n);
        m_pSurf = 0;

        const char *filename = NULL;

        switch (m_par.mfx.FrameInfo.Width)
        {
            case 720:
                filename = "YUV/720x480i29_jetcar_CC60f.nv12";
                break;
            default:
                break;
        }

        /*
            n multiply 2 because counter increments for two for every ProcessSurface step
            inside the method EncodeFrameAsync
        */
        tsRawReader reader(g_tsStreamPool.Get(filename), m_par.mfx.FrameInfo, n*2);
        g_tsStreamPool.Reg();
        m_filler = &reader;

        int e, s;
        for(e = 0, s = 0; e <= s; ) {
            mfxStatus sts = EncodeFrameAsync();

            if (m_pSurf)
                s++;
            if (m_pSurf && sts == MFX_ERR_MORE_DATA) {
                if (reader.m_eos) break;
                continue;
            }

            if (sts == MFX_ERR_NONE ) {
                SyncOperation();
                reported_ft.push_back(m_bitstream.FrameType & (MFX_FRAMETYPE_I | MFX_FRAMETYPE_P | MFX_FRAMETYPE_B));
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

    bool VideoEncoder::IsValidQP(BS_AVC2::MB & mb)
    {
        if ((mb.MbType >= BS_AVC2::I_16x16_0_1_0 && mb.MbType <= BS_AVC2::I_16x16_3_2_1) || (mb.coded_block_pattern != 0))
            return true;

        return false;
    };

    /// Checking QP for MBBRC execution
    /*! Algorithm is (on 23 May 2017):
    MB QPs are just compared with first valid QP in a frame
    */
    bool VideoEncoder::IsMBBRCWorkOnIPB(int n_fr, bool is_field)
    {
        tsParserAVC2 parser(m_bitstream, BS_AVC2::INIT_MODE_PARSE_SD);
        bool media_data_found = false;
        mfxU32 qp = 0;

        bool is_changed_on_I = false, is_changed_on_P = false, is_changed_on_B = false;

        for (int f = 0; f < (is_field ? (n_fr * 2) : n_fr); f++) {// each frame/field as new loop iteration
            BS_AVC2::AU* hdr = parser.Parse();
            Bs32u numUnits = hdr->NumUnits;
            BS_AVC2::NALU** nalu = hdr->nalu;

            // Need to skip second field in frame with first I field, because it can add changing of the isChangedOnI due to second P field
            if (is_field) {
                if ((reported_ft[(is_field ? (f / 2) : f)] == MFX_FRAMETYPE_I) && f % 2)
                    continue;
            }

            mfxU32 first_valid_mb_qp = 0;
            bool is_first_valid_mb_qp = true;

            for (mfxU32 i = 0; i < numUnits; i++) {
                mfxU8 type = nalu[i][0].nal_unit_type;
                if ((type == 5) || (type == 1)) {
                    media_data_found = true;

                    BS_AVC2::Slice* slice = nalu[i][0].slice;
                    BS_AVC2::MB* mb = slice->mb;
                    mfxU32 n_mb = 0;
                    mfxU32 mb_qp_delta = 0;

                    Bs32u QpBdOffsetY = 6 * nalu[i][0].sps->bit_depth_luma_minus8;
                    Bs8s slice_qp_delta = int(slice->slice_qp_delta);

                    Bs32s pic_init_qp_minus26 = nalu[i][0].pps->pic_init_qp_minus26;
                    Bs32s sqp = pic_init_qp_minus26 + 26 + slice_qp_delta;
                    Bs32s prev_qp = sqp;

                    while (n_mb < slice->NumMB) {
                        if (!IsValidQP(*mb)) {
                            mb = mb->next;
                            n_mb++;
                            continue;
                        }

                        qp = ((prev_qp + mb->mb_qp_delta + 52 + 2 * QpBdOffsetY) % (52 + QpBdOffsetY)) - QpBdOffsetY;
                        prev_qp = qp;

                        //Find first MB with real QP value
                        if (is_first_valid_mb_qp) {
                            first_valid_mb_qp = qp;
                            is_first_valid_mb_qp = false;
                        }

                        //Check that QP of the first MB with real QP equals another MB QP on different frame types
                        if (first_valid_mb_qp != qp) {
                            if (MFX_FRAMETYPE_I == reported_ft[(is_field ? (f/2): f)])// reported_ft includes information about frames (2 fields simultaneously)
                                is_changed_on_I = true;
                            else if (MFX_FRAMETYPE_P == reported_ft[(is_field ? (f / 2) : f)])// reported_ft includes information about frames (2 fields simultaneously)
                                is_changed_on_P = true;
                            else if (MFX_FRAMETYPE_B == reported_ft[(is_field ? (f / 2) : f)])// reported_ft includes information about frames (2 fields simultaneously)
                                is_changed_on_B = true;
                        }

                        mb = mb->next;
                        n_mb++;
                    }
                }
            }
        }
        EXPECT_TRUE(media_data_found) << "ERROR: No real slice data\n";

        if(is_changed_on_I != is_changed_on_P || is_changed_on_I != is_changed_on_B)
            EXPECT_TRUE(false) << "ERROR: Different behavior on different frame/field types\n";

        if (is_changed_on_I && is_changed_on_P && is_changed_on_B)
            return true;

        return false;
    }

    //GOP structure: IBBPBBPBBP...
    const tc_struct TestSuite::test_case[] =
    {
        {/*00*/ 7,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  720 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       1000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE } } },

        {/*01*/ 7,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  720 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       1000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF } } },

        {/*02*/ 7,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  720 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       1000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_BFF } } },


        {/*03*/ 7,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  720 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       1000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          1500 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE } } },

        {/*04*/ 7,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  720 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       1000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          1500 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF } } },

        {/*05*/ 7,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  720 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       1000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          1500 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_BFF } } },
#if defined (WIN32)||(WIN64)
        {/*06*/ 7,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_QVBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  720 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       1000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          1500 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE } } },

        {/*07*/ 7,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_QVBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  720 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       1000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          1500 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF } } },

        {/*08*/ 7,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_QVBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  720 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       1000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          1500 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_BFF } } },
#endif

    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::test_case[0]);

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;
        auto& tc = test_case[id];

        mfxExtCodingOption2 *coding_option2 = nullptr;

        bool is_qp_changed_MBBRC_on = false, is_qp_changed_MBBRC_off = false;
        bool is_field = false;

        //MBBRC enabled
        encoder_MBBRC_on.m_impl = MFX_IMPL_HARDWARE;
        encoder_MBBRC_on.MFXInit();
        SETPARS(&encoder_MBBRC_on.m_par, MFX_PAR);
        encoder_MBBRC_on.InitParams();

        coding_option2 = reinterpret_cast<mfxExtCodingOption2*>(encoder_MBBRC_on.m_par.GetExtBuffer(MFX_EXTBUFF_CODING_OPTION2));

        if (!(encoder_MBBRC_on.m_impl & MFX_IMPL_HARDWARE)) {
            g_tsLog << "\n\nWARNING: MBBRC is only supported by HW library\n\n\n";
            throw tsSKIP;
        }

        if (encoder_MBBRC_on.m_par.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_FIELD_TFF || encoder_MBBRC_on.m_par.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_FIELD_BFF)
            is_field = true;

        coding_option2->MBBRC = MFX_CODINGOPTION_ON;
        encoder_MBBRC_on.Query();
        encoder_MBBRC_on.Init();
        encoder_MBBRC_on.Encode(tc.n_frames);
        //DUMP_BS(encoder_MBBRC_on.m_bitstream);
        is_qp_changed_MBBRC_on = encoder_MBBRC_on.IsMBBRCWorkOnIPB(tc.n_frames, is_field);
        encoder_MBBRC_on.Close();
        EXPECT_TRUE(is_qp_changed_MBBRC_on) << "ERROR: All QP values are identical with enabled MBBRC\n";

        //MBBRC disabled
        encoder_MBBRC_off.m_impl = MFX_IMPL_HARDWARE;
        encoder_MBBRC_off.MFXInit();
        SETPARS(&encoder_MBBRC_off.m_par, MFX_PAR);
        if (encoder_MBBRC_off.m_par.mfx.RateControlMethod != MFX_RATECONTROL_QVBR) {//QVBR works only with MBBRC on
            encoder_MBBRC_off.InitParams();

            coding_option2 = reinterpret_cast<mfxExtCodingOption2*>(encoder_MBBRC_off.m_par.GetExtBuffer(MFX_EXTBUFF_CODING_OPTION2));

            if (!(encoder_MBBRC_off.m_impl & MFX_IMPL_HARDWARE)) {
                g_tsLog << "\n\nWARNING: MBBRC is only supported by HW library\n\n\n";
                throw tsSKIP;
            }

            if (encoder_MBBRC_off.m_par.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_FIELD_TFF || encoder_MBBRC_off.m_par.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_FIELD_BFF)
                is_field = true;

            coding_option2->MBBRC = MFX_CODINGOPTION_OFF;
            encoder_MBBRC_off.Query();
            encoder_MBBRC_off.Init();
            encoder_MBBRC_off.Encode(tc.n_frames);
            //DUMP_BS(encoder_MBBRC_off.m_bitstream);
            is_qp_changed_MBBRC_off = encoder_MBBRC_off.IsMBBRCWorkOnIPB(tc.n_frames, is_field);
            encoder_MBBRC_off.Close();
            EXPECT_TRUE(!is_qp_changed_MBBRC_off) << "ERROR: Different QP values on MBs with disabled MBBRC\n";
        }

        TS_END;
        return 0;
    }
    //! \brief Regs test suite into test system
    TS_REG_TEST_SUITE_CLASS(avce_mbbrc);
}
