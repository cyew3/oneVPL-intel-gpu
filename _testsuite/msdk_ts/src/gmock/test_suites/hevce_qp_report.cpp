/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2019 Intel Corporation. All Rights Reserved.

File Name: hevce_qp_report.cpp
\* ****************************************************************************** */

/*!
\file hevce_qp_report.cpp
\brief Gmock test hevce_qp_report

Description:
This suite checks QPs from frame info buffer for HEVC encoder\n

Algorithm:
1) Encode stream in different modes and gets average QP, frame type
   and display order reporting from mfxExtAVCEncodedFrameInfo buffer.
2) For CQP, CBR, VBR:
   Parse stream and calculate QP
3) For CBR, VBR with MBBRC:
-  Encode 2 streams in encoded order (in CQP control method) using reported info (from step 1):
   first stream with QP+3, second stream with QP-1
   (Since it is hard to say exactly what the frame size will be depending on the QP,
    QPs were approximately matched.)
-  Calculate frame sizes for these 2 new streams
-  Thus, if reported QPs are correct, must be right next sentence:
   for each frame: fs(with QP+2)(-delta) <= fs(with QP) <= fs(with QP-2)
*/
#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"
#include <fstream>
#include <cstdio>

#define DUMP_BS(bs, name); { std::fstream fout(std::string("")+name, std::fstream::binary | std::fstream::out);fout.write((char*)bs.Data, bs.DataLength); fout.close(); }
/*! \brief Main test name space */
namespace hevce_qp_report
{

    enum
    {
        MFX_PAR = 1,
        CDO2_PAR = 2,
        CDO3_PAR = 3,
    };

    /*!\brief Structure of test suite parameters */
    struct tc_struct
    {
        //! \brief Number of farmes to encode
        mfxU32 n_frames;
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
        //! \brief A constructor (HEVC encoder)
        TestSuite(): tsVideoEncoder(MFX_CODEC_HEVC), num_ref_frame(2) {}
        //! \brief A destructor
        ~TestSuite() {}
        //! \brief Main method. Runs test case
        //! \param id - test case number
        int RunTest(unsigned int id);
        //! The number of test cases
        static const unsigned int n_cases;
        //! \brief Initialize params common for whole test suite
        void initParams();
        //! \brief Selects a stream depending on the input parameters
        //! \param filename - string with chosen stream name
        void choose_stream(const char*& filename);
        //! \brief Encodes n frames and put QP reported values in a reported_qp vector
        //! \param n - number of frames to encode
        void encode(int n, bool is_encoded_order, mfxI32 qp_delta = 0);
        //! \brief Parse stream and calculate QP value for each frame. Compare the results with reported QP values
        //! \param n - number of frames to check
        void check_bs(int n);
        //! \brief Compares reported and encoded QP values with special message (in case of compare fail)
        //! \param expected - Encoded QP value
        //! \param reported - Reported QP value
        //! \param message - special message (fail reason)
        void check_qp_msg(Bs32s expected, Bs32s reported, const char* message);
        //! \brief Compares reported and encoded QP values with standart message
        //! \param expected - Encoded QP value
        //! \param reported - Reported QP value
        void check_qp(Bs32s expected, Bs32s reported);
        //! \brief Overrided function using for encode in encoded order
        //! \param e - current number of frame in encoded ordder
        //! \param encoded_order - indicates whether encoded order is used
        mfxStatus EncodeFrameAsync(int e, bool encoded_order, mfxI32 qp_delta);
        //! \brief Calculate frame sizes and put it into vector
        //! \param fs_vec - output vector that will contain frame sizes
        void calcFrameSizes(std::vector<mfxU32>& fs_vec);
        //! \brief Recursively fill std::vector with TransTree from input coding_quadtree
        //! \param cqt - input CQT object from which TransTrees will be retrieved
        //! \param tu_list - output vector that will contain TransTrees
        void fillTuList(BS_HEVC::CQT & cqt, std::vector<BS_HEVC::TransTree*> & tu_list);
        //! \brief Recursively add TransTree to output std::vector
        //! \param tu - input TransTree object from which sub TransTrees will be retrieved
        //! \param tu_list - output vector that will contain TransTrees
        void addTu(BS_HEVC::TransTree * tu, std::vector<BS_HEVC::TransTree*> & tu_list);
        //! \brief Return true if cu_qp_delta in cqt changed
        //! \param cqt - input CQT object from which TransTrees will be retrieved
        //! \param cu_qp_delta - output found changed cu_qp_delta
        bool delta_qp_changed(BS_HEVC::CQT & cqt, mfxU32 & cu_qp_delta);
        //! \brief check_bs if ratecontrol = MFX_RATECONTROL_CQP
        //! \param n - number of frames to check
        void check_bs_cqp(int n_fr);
        //! \brief check_bs if ratecontrol = MFX_RATECONTROL_CBR or MFX_RATECONTROL_VBR but MMBRC is off
        //! \param n - number of frames to check
        void check_bs_cbr_vbr(int n);
        //! \brief vector with reported QP values
        std::vector <mfxU32> reported_qp;
        //! \brief vector with reported frame types
        std::vector <mfxU32> reported_ft;
        //! \brief vector with reported display frame order
        std::vector <mfxU32> reported_fo;
        //! \brief vector with encoded frame sizes of original stream
        std::vector <mfxU32> fs_original;
        //! \brief vector with encoded frame sizes of stream with QP + 2 from reported QP
        std::vector <mfxU32> fs_plus;
        //! \brief vector with encoded frame sizes of stream with QP - 2 from reported QP
        std::vector <mfxU32> fs_minus;
        //! \brief number of reference frames, used for encoding in encoded order
        mfxU32 num_ref_frame;
        //! \brief vector for storing TransTrees from CU
        std::vector<BS_HEVC::TransTree*> tu_list_in_cu;
        //! \brief Set of test cases
        static const tc_struct test_case[];
        //! \brief Selected test case
        tc_struct m_tc;
        //! \brief Selected test case id
        mfxU32    m_id;
    };

    template<class T>
    constexpr const T& clamp(const T& v, const T& lo, const T& hi)
    {
        return std::min(hi, std::max(v, lo));
    }

    void TestSuite::initParams()
    {
        m_par.mfx.CodecId                 = MFX_CODEC_HEVC;
        m_par.mfx.MaxKbps                 = 0;
        m_par.mfx.FrameInfo.FourCC        = MFX_FOURCC_NV12;
        m_par.mfx.FrameInfo.FrameRateExtN = 30;
        m_par.mfx.FrameInfo.FrameRateExtD = 1;
        m_par.mfx.FrameInfo.ChromaFormat  = MFX_CHROMAFORMAT_YUV420;
        m_par.mfx.FrameInfo.PicStruct     = MFX_PICSTRUCT_PROGRESSIVE;
        m_par.IOPattern                   = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
        m_par.AsyncDepth                  = 1;

        m_par.AddExtBuffer(MFX_EXTBUFF_CODING_OPTION2, sizeof(mfxExtCodingOption2));
        m_par.AddExtBuffer(MFX_EXTBUFF_CODING_OPTION3, sizeof(mfxExtCodingOption3));
        m_bitstream.AddExtBuffer(MFX_EXTBUFF_ENCODED_FRAME_INFO, sizeof(mfxExtAVCEncodedFrameInfo));
    }

    mfxStatus TestSuite::EncodeFrameAsync(int e, bool encoded_order, mfxI32 qp_delta)
    {
        if (m_default)
        {
            if (m_field_processed == 0) {
                //if SingleFieldProcessing is enabled, then don't get new surface if the second field to be processed.
                //if SingleFieldProcessing is not enabled, m_field_processed == 0 always.
                m_pSurf = GetSurface(); TS_CHECK_MFX;
            }

            if (m_filler)
            {
                m_pSurf = encoded_order ?
                          m_filler->ProcessSurface(m_pSurf, m_pFrameAllocator, reported_fo[e])
                        : m_filler->ProcessSurface(m_pSurf, m_pFrameAllocator);
            }
        }

        mfxEncodeCtrl * cur_ctrl = m_pSurf ? m_pCtrl : nullptr;

        if (m_pSurf && encoded_order)
        {
            m_pCtrl->FrameType = reported_ft[e];
            m_pCtrl->QP = (mfxU16)clamp<mfxI32>((mfxI32)reported_qp[e] + qp_delta, 1, 51);
        }

        mfxStatus mfxRes = tsVideoEncoder::EncodeFrameAsync(m_session, cur_ctrl, m_pSurf, m_pBitstream, m_pSyncPoint);

        return mfxRes;
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

    void TestSuite::calcFrameSizes(std::vector<mfxU32>& fs_vec)
    {
        BSErr bs_sts = BS_ERR_NONE;
        tsParserHEVCAU parser(m_bitstream);
        BS_HEVC::AU* pAU = nullptr;
        Bs64u frame_size = 0;
        while (true)
        {
            bs_sts = parser.parse_next_au(pAU);
            if (bs_sts == BS_ERR_NOT_IMPLEMENTED) continue;
            if (bs_sts) break;
            frame_size = 0;
            for (Bs32u i = 0; i < pAU->NumUnits; i++) {
                if (IsHEVCSlice(pAU->nalu[i]->nal_unit_type))
                    frame_size = pAU->nalu[i]->NumBytesInNalUnit;
            }
            fs_vec.push_back((mfxU32)frame_size);
        }
    }

    void TestSuite::choose_stream(const char*& filename)
    {
        switch (m_par.mfx.FrameInfo.Width) {
        case 352:
            filename = "YUV/interlaced_352x288i_bff_nv12.yuv";
            break; //case w:352
        case 720:
            filename = "YUV/720x480i29_jetcar_CC60f.nv12";
            break; //case w:720
        case 1920:
            filename = "YUV/1920x1080i29_flag_60f.nv12";
            break; //case w:1920
        }
    }
    void TestSuite::encode(int n, bool is_encoded_order, mfxI32 qp_delta)
    {
        Query();
        Init();

        if (!is_encoded_order)
        {
            GetVideoParam(m_session, m_pPar);
            num_ref_frame = m_par.mfx.NumRefFrame;
        }
        else m_par.mfx.NumRefFrame = num_ref_frame;

        AllocSurfaces();
        AllocBitstream(m_par.mfx.FrameInfo.Width * m_par.mfx.FrameInfo.Height * 3 * n);
        m_pSurf = 0;

        const char *filename = nullptr;
        choose_stream(filename);

        /*
            n multiply 2 because counter increments for two for every ProcessSurface step
            inside the method EncodeFrameAsync
        */
        tsRawReader reader(g_tsStreamPool.Get(filename), m_par.mfx.FrameInfo, n*2);
        g_tsStreamPool.Reg();
        m_filler = &reader;

        int e, s, QP = 0;
        for(e = 0, s = 0; e<=s; ) {
            if (e >= m_tc.n_frames) break;

            mfxStatus sts = EncodeFrameAsync(e, is_encoded_order, qp_delta);

            if (m_pSurf)
            {
                s++;
                if (sts == MFX_ERR_MORE_DATA) {
                    if (reader.m_eos) break;
                    continue;
                }
            }

            if (sts == MFX_ERR_NONE ) {
                SyncOperation();
                if (!is_encoded_order)
                {
                    mfxExtAVCEncodedFrameInfo* efi = m_bitstream;
                    TestSuite::reported_qp.push_back(efi->QP);
                    TestSuite::reported_ft.push_back(m_bitstream.FrameType);
                    TestSuite::reported_fo.push_back(efi->FrameOrder);
                }
                e++;
            } else {
                break;
            }
        }

        Close();

        if (e < n) {
            g_tsLog << "\n\nERROR: Encoded frames count (" << e << ") < requested frame count (" << n << ")\n\n\n";
            throw tsFAIL;
        }
    }

    void TestSuite::fillTuList(BS_HEVC::CQT & cqt, std::vector<BS_HEVC::TransTree*> & tu_list)
    {
        if (cqt.cu_skip_flag) return;
        if (cqt.split_cu_flag)
        {
            fillTuList(cqt.split[0], tu_list);
            fillTuList(cqt.split[1], tu_list);
            fillTuList(cqt.split[2], tu_list);
            fillTuList(cqt.split[3], tu_list);
        }
        else addTu(cqt.tu, tu_list);
    }

    void TestSuite::addTu(BS_HEVC::TransTree * tu, std::vector<BS_HEVC::TransTree*> & tu_list)
    {
        if (tu)
        {
            if (tu->split_transform_flag)
            {
                addTu(&tu->split[0], tu_list);
                addTu(&tu->split[1], tu_list);
                addTu(&tu->split[2], tu_list);
                addTu(&tu->split[3], tu_list);
            }
            else tu_list.push_back(tu);
        }
    }

    bool TestSuite::delta_qp_changed(BS_HEVC::CQT & cqt, mfxU32 & cu_qp_delta)
    {
        tu_list_in_cu.clear();
        fillTuList(cqt, tu_list_in_cu);

        for (auto& tu : tu_list_in_cu)
        {
            if (tu->cu_qp_delta)
            {
                cu_qp_delta = tu->cu_qp_delta;
                return true;
            }
        }
        return false;
    }

    /// Counting QP for CQP in the bitstream
    /*! Algorithm is:
    QP is counting for the slice (multi-slice does not supported).
    After that QP is comparing to the QPI,QPP,QPB.
    */
    void TestSuite::check_bs_cqp(int n_fr)
    {
        tsParserHEVCAU parser(m_bitstream);
        for (int i = 0; i < n_fr; i++) {
            BS_HEVC::AU* au = parser.Parse();

            if (au == nullptr) return;

            Bs32u n = au->NumUnits;
            BS_HEVC::NALU** nalu = au->nalu;
            mfxU8 frame_found = 0;
            mfxU32 qp = 0;

            for (mfxU32 j = 0; j < n; j++) {
                byte type = nalu[j]->nal_unit_type;
                if (!qp && IsHEVCSlice(type)) {
                    frame_found = 1;
                    BS_HEVC::Slice* slice = nalu[j]->slice;
                    Bs32s pic_init_qp_minus26 = slice->pps->init_qp_minus26;

                    Bs8s slice_qp_delta = slice->slice_qp_delta;
                    qp = slice_qp_delta + pic_init_qp_minus26 + 26;
                }
            }
            g_tsLog << "Frame: " << i << "\nReported QP = " << TestSuite::reported_qp[i] << " Encoded QP = " << qp << "\n";
            if (frame_found) {
                check_qp(qp, TestSuite::reported_qp[i]);
            }

            if (MFX_FRAMETYPE_I == TestSuite::reported_ft[i]) {
                check_qp_msg(m_par.mfx.QPI, TestSuite::reported_qp[i], "ERROR: Reported QP != provided to encoder QPI");
            }
            else if (MFX_FRAMETYPE_P == TestSuite::reported_ft[i]) {
                check_qp_msg(m_par.mfx.QPP, TestSuite::reported_qp[i], "ERROR: Reported QP != provided to encoder QPP");
            }
            else if (MFX_FRAMETYPE_B == TestSuite::reported_ft[i]) {
                check_qp_msg(m_par.mfx.QPB, TestSuite::reported_qp[i], "ERROR: Reported QP != provided to encoder QPB");
            }
        }

    }

    /// Checking QP for non-MBBRC execution
    /*! Algorithm is:
    1. Search for first QP change in the slice
    2. Found QP must equal to a reported average QP
    */
    void TestSuite::check_bs_cbr_vbr(int n_fr)
    {
        tsParserHEVC parser(m_bitstream, BS_HEVC::INIT_MODE::INIT_MODE_CABAC);
        mfxU8 frame_found = 0;

        for (int f = 0; f < n_fr; f++) {
            BS_HEVC::AU* hdr = parser.Parse();
            Bs32u n = hdr->NumUnits;
            BS_HEVC::NALU** nalu = hdr->nalu;
            mfxU32 qp = 0;
            mfxU32 cu_qp_delta = 0;

            for (mfxU32 i = 0; i < n; i++) {
                mfxU8 type = nalu[i][0].nal_unit_type;
                if (IsHEVCSlice(nalu[i]->nal_unit_type)) {
                    frame_found = 1;

                    BS_HEVC::Slice* slice = nalu[i][0].slice;
                    BS_HEVC::CTU** ctu = slice->CuInTs;

                    for (mfxU32 n_ctu = 0; n_ctu < slice->NumCU; n_ctu++)
                        if (ctu[n_ctu]->cqt.cu_skip_flag == 0 && delta_qp_changed(ctu[n_ctu]->cqt, cu_qp_delta))
                            break;

                    Bs32u QpBdOffsetY = 6 * nalu[i][0].sps->bit_depth_luma_minus8;
                    Bs8s slice_qp_delta = int(slice->slice_qp_delta);
                    Bs32s pic_init_qp_minus26 = nalu[i][0].pps->init_qp_minus26;
                    Bs32s sqp = pic_init_qp_minus26 + 26 + slice_qp_delta;
                    qp = ((sqp + cu_qp_delta + 52 + 2 * QpBdOffsetY) % (52 + QpBdOffsetY)) - QpBdOffsetY;
                }
            }

            g_tsLog << "Frame: " << f << "\nReported QP = " << reported_qp[f] << ", Encoded QP = " << qp << "\n";
            if (frame_found) {
                if (!cu_qp_delta)
                {
                    g_tsLog << "Frame: " << f << " - skipped\n";
                    continue;
                }
                check_qp(qp, reported_qp[f]);
            }
        }
    }

    void TestSuite::check_bs(int f)
    {
        // encode stream and get reported QP, frame type and frame order
        encode(f, false);
        //DUMP_BS(m_bitstream, std::to_string(m_id) + std::string("_orig.h265"));
        // CQP
        if (m_par.mfx.RateControlMethod == MFX_RATECONTROL_CQP)
        {
            check_bs_cqp(f);
            return;
        }
        mfxExtCodingOption2* co2 = m_par;
        bool MBBRC = co2->MBBRC == MFX_CODINGOPTION_ON;
        // CBR / VBR
        if ((m_par.mfx.RateControlMethod == MFX_RATECONTROL_CBR ||
            m_par.mfx.RateControlMethod == MFX_RATECONTROL_VBR) && !MBBRC)
        {
            check_bs_cbr_vbr(f);
            return;
        }
        // CBR / VBR with MBBRC
        mfxI32 fs_delta = 70;
        mfxI32 qp_plus_delta = 3;
        mfxI32 qp_minus_delta = -1;
        // calculate frame sizes
        calcFrameSizes(fs_original);
        // set encoded order flag, cqp mode and
        // reset invalid params for next encoding
        m_par.mfx.EncodedOrder = 1;
        m_par.mfx.RateControlMethod = 3;
        m_par.mfx.QPI = m_par.mfx.QPP = m_par.mfx.QPB = 1;
        m_par.mfx.BufferSizeInKB = 0;
        mfxExtCodingOption2& CO2 = m_par;
        mfxExtCodingOption3& CO3 = m_par;
        CO2.MBBRC = MFX_CODINGOPTION_OFF;
        CO3.EnableMBQP = MFX_CODINGOPTION_OFF;
        m_bitstream.RemoveExtBuffer(MFX_EXTBUFF_ENCODED_FRAME_INFO);
        memset(&m_bitstream, 0, sizeof(m_bitstream));
        // encode stream with reported values (QP+qp_plus_delta)
        encode(f, true, qp_plus_delta);
        //DUMP_BS(m_bitstream, std::to_string(m_id) + std::string("_plus.h265"));
        calcFrameSizes(fs_plus);
        memset(&m_bitstream, 0, sizeof(m_bitstream));
        // encode stream with reported values (QP+qp_minus_delta)
        encode(f, true, qp_minus_delta);
        //DUMP_BS(m_bitstream, std::to_string(m_id) + std::string("_minus.h265"));
        calcFrameSizes(fs_minus);
        memset(&m_bitstream, 0, sizeof(m_bitstream));
        // check frame sizes
        printf("\nCase number: %d\n", m_id);
        printf("\nFrame sizes of encoded stream must be in the range from frame_size(QP+%d)-%d to frame_size(QP%d) (in bytes)\n",
               qp_plus_delta, fs_delta, qp_minus_delta);
        printf("\nframe\treported QP\tframe size(QP+%d)-%d\tframe size\tframe size(QP-%d)\n",
               qp_plus_delta, fs_delta, abs(qp_minus_delta));
        for (mfxU32 i = 0; i < m_tc.n_frames; ++i)
        {
            if (reported_qp[i]+qp_plus_delta > 51 || reported_qp[i]+qp_minus_delta < 1)
            {
                printf("%d\t%d\t\t-\tskip (QP range length for this frame < 4)\n", i, reported_qp[i]);
                continue;
            }
            printf("%d\t%d\t\t%d\t\t<=\t%d\t<=\t%d\n", i, reported_qp[i], fs_plus[i]-fs_delta, fs_original[i], fs_minus[i]);
            EXPECT_TRUE(fs_plus[i] - fs_delta <= fs_original[i] && fs_original[i] <= fs_minus[i])
                << "\nERROR: frame " << i << ", frame size is out of range\n";
        }
    }


    const tc_struct TestSuite::test_case[] =
    {
        {/*00*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CQP },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  720 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP,              1 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI,              1 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPB,              1 },
        { CDO3_PAR, &tsStruct::mfxExtCodingOption3.EnableQPOffset, MFX_CODINGOPTION_OFF } } },
        {/*01*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CQP },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  352 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 288 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP,              8 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI,              8 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPB,              8 },
        { CDO3_PAR, &tsStruct::mfxExtCodingOption3.EnableQPOffset, MFX_CODINGOPTION_OFF } } },
        {/*02*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CQP },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  1920 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 1088 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP,              16 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI,              16 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPB,              16 },
        { CDO3_PAR, &tsStruct::mfxExtCodingOption3.EnableQPOffset, MFX_CODINGOPTION_OFF } } },
        {/*03*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CQP },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  1920 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 1088 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP,              32 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI,              32 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPB,              32 },
        { CDO3_PAR, &tsStruct::mfxExtCodingOption3.EnableQPOffset, MFX_CODINGOPTION_OFF } } },
        {/*04*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CQP },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  1920 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 1088 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP,              48 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI,              48 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPB,              48 },
        { CDO3_PAR, &tsStruct::mfxExtCodingOption3.EnableQPOffset, MFX_CODINGOPTION_OFF } } },
        {/*05*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CQP },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  720 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP,              32 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI,              16 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPB,              28 },
        { CDO3_PAR, &tsStruct::mfxExtCodingOption3.EnableQPOffset, MFX_CODINGOPTION_OFF } } },
        {/*06*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CQP },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  720 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP,              18 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI,              8 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPB,              40 },
        { CDO3_PAR, &tsStruct::mfxExtCodingOption3.EnableQPOffset, MFX_CODINGOPTION_OFF } } },

        {/*07*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  352 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 288 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       190 },
        { CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_OFF } } },
        {/*08*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  720 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       190 },
        { CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_OFF } } },
        {/*09*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  720 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       1000 },
        { CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_OFF } } },
        {/*10*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  720 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       2000 },
        { CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_OFF } } },
        {/*11*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  1920 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 1088 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       1080 },
        { CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_OFF } } },
        {/*12*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  352 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 288 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       190 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          200 },
        { CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_OFF } } },
        {/*13*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  720 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       190 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          300 },
        { CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_OFF } } },
        {/*14*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  720 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       1000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          1500 },
        { CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_OFF } } },
        {/*15*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  720 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       2000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          3000 },
        { CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_OFF } } },
        {/*16*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  1920 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 1088 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       1080 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          2000 },
        { CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_OFF } } },
        {/*17*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  720 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       4000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          7000 },
        { CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_OFF } } },
        {/*18*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  720 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       4000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          7000 },
        { CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_OFF } } },

        {/*19*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  720 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       190 },
        { CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_ON } } },
        {/*20*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  720 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       1000 },
        { CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_ON } } },
        {/*21*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  720 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       2000 },
        { CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_ON } } },
        {/*22*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_CBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  1920 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 1088 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       1080 },
        { CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_ON } } },
        {/*23*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  720 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       190 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          300 },
        { CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_ON } } },
        {/*24*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  720 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       1000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          1500 },
        { CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_ON } } },
        {/*25*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  720 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       2000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          3000 },
        { CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_ON } } },
        {/*26*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  1920 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 1088 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       1080 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          2000 },
        { CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_ON } } },
        {/*27*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  720 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       4000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          7000 },
        { CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_ON } } },
        {/*28*/ 16,{ { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod,    MFX_RATECONTROL_VBR },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Width,  720 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.Height, 480 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps,       4000 },
        { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps,          7000 },
        { CDO2_PAR, &tsStruct::mfxExtCodingOption2.MBBRC, MFX_CODINGOPTION_ON } } },

    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::test_case[0]);

    int TestSuite::RunTest(unsigned int id)
    {
        TS_START;
        auto& tc = test_case[id];
        m_id = id;
        m_tc = tc;
        m_impl = MFX_IMPL_HARDWARE | MFX_IMPL_VIA_D3D11;
        initParams();
        SETPARS(&m_par, MFX_PAR);
        SETPARS(&m_par, CDO2_PAR);
        SETPARS(&m_par, CDO3_PAR);
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

        MFXInit();

        check_bs(tc.n_frames);
        TS_END;
        return 0;
    }
    //! \brief Regs test suite into test system
    TS_REG_TEST_SUITE_CLASS(hevce_qp_report);
}
