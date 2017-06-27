/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014-2017 Intel Corporation. All Rights Reserved.

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_struct.h"
#include "ts_parser.h"

/*! \brief Main test name space */
namespace avce_encoder_roi {

#define ROI_PARS(type, i, numROI, ROImode, left,top,right,bottom, priority) \
            {type, &tsStruct::mfxExtEncoderROI.ROIMode,         ROImode},   \
            {type, &tsStruct::mfxExtEncoderROI.NumROI,          numROI},    \
            {type, &tsStruct::mfxExtEncoderROI.ROI[i].Left,     left},      \
            {type, &tsStruct::mfxExtEncoderROI.ROI[i].Top,      top},       \
            {type, &tsStruct::mfxExtEncoderROI.ROI[i].Right,    right},     \
            {type, &tsStruct::mfxExtEncoderROI.ROI[i].Bottom,   bottom},    \
            {type, &tsStruct::mfxExtEncoderROI.ROI[i].Priority, priority}

#define CHECK_PARS(expected, actual) {                                                                                \
    EXPECT_EQ(expected.Left, actual.Left)                                                                             \
        << "ERROR: Expect Left = " << expected.Left << ", but real Left = " << actual.Left << "!\n";                  \
    EXPECT_EQ(expected.Top, actual.Top)                                                                               \
        << "ERROR: Expect Top = " << expected.Top << ", but real Top = " << actual.Top << "!\n";                      \
    EXPECT_EQ(expected.Right, actual.Right)                                                                           \
        << "ERROR: Expect Right = " << expected.Right << ", but real Right = " << actual.Right << "!\n";              \
    EXPECT_EQ(expected.Bottom, actual.Bottom)                                                                         \
        << "ERROR: Expect Bottom = " << expected.Bottom << ", but real Bottom = " << actual.Bottom << "!\n";          \
    EXPECT_EQ(expected.Priority, actual.Priority)                                                                     \
        << "ERROR: Expect Priority = " << expected.Priority << ", but real Priority = " << actual.Priority << "!\n";  \
}

    enum {
        QP_INVALID = 255,
        QP_BY_FRM_TYPE = 254,
        QP_DO_NOT_CHECK = 253
    };

    enum {
        MFX_PAR = 1,
        ROI,
        ROI_EXPECTED_QUERY
    };

    enum TYPE {
        QUERY = 0x1,
        INIT = 0x2,
        ENCODE = 0x4
    };

    enum ROI_MODE {
        ROI_PRIORITY = 0,
        ROI_DELTAQP = 1
    };

    /*!\brief Structure of test suite parameters
    */

    struct tc_struct {
        struct status {
            mfxStatus query;
            mfxStatus init;
            mfxStatus encode;
        } sts;
        mfxU32 type;
        struct tctrl {
            mfxU32 ext_type;
            const  tsStruct::Field* f;
            mfxI32 v;
        } set_par[MAX_NPARS];
    };

    class AUWrap : public H264AUWrapper
    {
    private:
        mfxU8 m_prevqp;
    public:

        AUWrap(BS_H264_au& au)
            : H264AUWrapper(au)
            , m_prevqp(QP_INVALID)
        {
        }

        ~AUWrap()
        {
        }

        mfxU32 GetFrameType()
        {
            if (!m_sh)
                return 0;

            mfxU32 type = 0;
            switch (m_sh->slice_type % 5)
            {
            case 0:
                type = MFX_FRAMETYPE_P;
                break;
            case 1:
                type = MFX_FRAMETYPE_B;
                break;
            case 2:
                type = MFX_FRAMETYPE_I;
                break;
            default:
                break;
            }

            return type;
        }

        bool IsBottomField() {
            return m_sh && m_sh->bottom_field_flag;
        }

        mfxU8 NextQP() {
            if (!NextMB())
                return QP_INVALID;

            //assume bit-depth=8
            if (m_mb == m_sh->mb)
                m_prevqp = 26 + m_sh->pps_active->pic_init_qp_minus26 + m_sh->slice_qp_delta;

            m_prevqp = (m_prevqp + m_mb->mb_qp_delta + 52) % 52;

            if (m_sh->slice_type % 5 == 2) {  // Intra
                // 7-11
                if ((m_mb->mb_type == 0 && m_mb->coded_block_pattern == 0) || m_mb->mb_type == 25)
                    return QP_DO_NOT_CHECK;
            }
            else {
                if (m_mb->mb_skip_flag)
                    return QP_DO_NOT_CHECK;

                if (m_sh->slice_type % 5 == 0) {  // P
                    // 7-13 + 7-11
                    if ((m_mb->mb_type <= 5 && m_mb->coded_block_pattern == 0) || m_mb->mb_type == 30)
                        return QP_DO_NOT_CHECK;
                }
                else {
                    // 7-14 + 7-11
                    if ((m_mb->mb_type <= 23 && m_mb->coded_block_pattern == 0) || m_mb->mb_type == 48)
                        return QP_DO_NOT_CHECK;
                }
            }

            return m_prevqp;
        }
    };

    //!\brief Main test class
    class TestSuite : public tsVideoEncoder, public tsSurfaceProcessor, public tsBitstreamProcessor, public tsParserH264AU {
    private:
        struct Ctrl {
            tsExtBufType<mfxEncodeCtrl> ctrl;
            std::vector<mfxI8>          buf;
        };

        std::map<mfxU32, Ctrl> m_ctrl;
        mfxU32 m_fo;
        std::auto_ptr<tsRawReader> m_reader;

    public:
        //! \brief A constructor (AVC encoder)
        TestSuite()
            : tsVideoEncoder(MFX_CODEC_AVC)
            , tsParserH264AU(BS_H264_INIT_MODE_CABAC | BS_H264_INIT_MODE_CAVLC)
            , m_fo(0)
            , m_reader()
        {
            m_par.mfx.CodecId = MFX_CODEC_AVC;
            m_par.mfx.TargetKbps = 9000;
            m_par.mfx.MaxKbps = 12000;
            m_par.mfx.RateControlMethod = MFX_RATECONTROL_CQP;
            m_par.mfx.QPI = 24;
            m_par.mfx.QPP = 24;
            m_par.mfx.QPB = 24;
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
            m_par.mfx.FrameInfo.FrameRateExtN = 30;
            m_par.mfx.FrameInfo.FrameRateExtD = 1;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
            m_par.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
            m_par.mfx.FrameInfo.Width = 352;
            m_par.mfx.FrameInfo.Height = 288;
            m_par.mfx.FrameInfo.CropW = 352;
            m_par.mfx.FrameInfo.CropH = 288;

            set_trace_level(0);

            m_filler = this;
            m_bs_processor = this;
        }
        //! \brief A destructor
        ~TestSuite() { }
        //! \brief Main method. Runs test case
        //! \param id - test case number
        int RunTest(unsigned int id);
        //! The number of test cases
        static const unsigned int n_cases;
        //! \brief Initialize params common mfor whole test suite
        //! \brief Set of test cases
        static const tc_struct test_case[];
        mfxExtEncoderROI extROI_expected;

        mfxStatus ProcessSurface(mfxFrameSurface1& s) {
            m_reader->ProcessSurface(s);

            mfxU32 wMB = ((m_par.mfx.FrameInfo.CropW + 15) / 16);
            mfxU32 hMB = ((m_par.mfx.FrameInfo.CropH + 15) / 16);
            mfxU32 numMB = wMB * hMB;
            Ctrl& ctrl = m_ctrl[m_fo];
            ctrl.buf.resize(numMB, 0);

            mfxU32 top, bottom, left, right;

            for (int j = extROI_expected.NumROI - 1; j >= 0; j--) {
                top = extROI_expected.ROI[j].Top / 16;
                bottom = extROI_expected.ROI[j].Bottom / 16;
                left = extROI_expected.ROI[j].Left / 16;
                right = extROI_expected.ROI[j].Right / 16;

                for (mfxU32 i = 0; i < ctrl.buf.size(); i++) {
                    mfxU32 x = i % wMB;
                    mfxU32 y = i / wMB;
                    if ((x >= left) && (x < right) && (y >= top) && (y < bottom))
                        ctrl.buf[i] = extROI_expected.ROI[j].DeltaQP;
                }
            }

            mfxExtMBQP& mbqp = ctrl.ctrl;
            mbqp.DeltaQP = &ctrl.buf[0];
            mbqp.NumQPAlloc = mfxU32(ctrl.buf.size());

            s.Data.TimeStamp = s.Data.FrameOrder = m_fo++;

            return MFX_ERR_NONE;
        }

        mfxStatus ProcessBitstream(mfxBitstream& bs, mfxU32 nFrames) {
            mfxU32 checked = 0;

            if (bs.Data)
                set_buffer(bs.Data + bs.DataOffset, bs.DataLength + 1);

            while (checked++ < nFrames) {
                UnitType& hdr = ParseOrDie();
                AUWrap au(hdr);
                mfxU8 qp = au.NextQP();
                mfxU32 i = 0;
                mfxU32 wMB = ((m_par.mfx.FrameInfo.CropW + 15) / 16);
                mfxU32 hMB = ((m_par.mfx.FrameInfo.CropH + 15) / 16);
                mfxExtMBQP& mROI = m_ctrl[(mfxU32)bs.TimeStamp].ctrl;

                mfxU32 fieldOffset = au.IsBottomField() ? (mfxU32)(hMB / 2 * wMB) : 0;

                g_tsLog << "Frame #" << bs.TimeStamp << " QPs:\n";

                while (qp != QP_INVALID) {
                    mfxI8 expected_qp = mROI.QP[fieldOffset + i]; // +m_par.mfx.QPI;
                    if (expected_qp == QP_BY_FRM_TYPE) {
                        expected_qp = 0;
                    }

                    switch (au.GetFrameType())
                    {
                    case MFX_FRAMETYPE_I:
                        expected_qp += (mfxU8)m_par.mfx.QPI;
                        break;
                    case MFX_FRAMETYPE_P:
                        expected_qp += (mfxU8)m_par.mfx.QPP;
                        break;
                    case MFX_FRAMETYPE_B:
                        expected_qp += (mfxU8)m_par.mfx.QPB;
                        break;
                    default:
                        expected_qp += 0;
                        break;
                    }

                    if (expected_qp > 51) expected_qp = 51;
                    if (expected_qp < 0) expected_qp = 0;

                    if (qp != QP_DO_NOT_CHECK && expected_qp != qp) {
                        g_tsLog << "\nFAIL: Expected QP (" << mfxU16(expected_qp) << ") != real QP (" << mfxU16(qp) << ")\n";
                        g_tsLog << "\nERROR: Expected QP != real QP\n";
                        return MFX_ERR_ABORTED;
                    }

                    if (i++ % wMB == 0)
                        g_tsLog << "\n";

                    g_tsLog << std::setw(3) << mfxU16(qp) << " ";

                    qp = au.NextQP();
                }
                g_tsLog << "\n";
            }

            m_ctrl.erase((mfxU32)bs.TimeStamp);
            bs.DataLength = 0;

            return MFX_ERR_NONE;
        }
    };

    const tc_struct TestSuite::test_case[] =
    {
        {/*00*/{ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE },
            QUERY | INIT,{
            ROI_PARS(ROI,                0, 1, ROI_DELTAQP,   0, 0, 0, 0,    0),
            ROI_PARS(ROI_EXPECTED_QUERY, 0, 1, ROI_DELTAQP,   0, 0, 0, 0,    0)
        } },

        {/*01*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
            QUERY | INIT | ENCODE,{
            ROI_PARS(ROI,                0, 1, ROI_DELTAQP,  32, 32, 128, 128,   0),
            ROI_PARS(ROI_EXPECTED_QUERY, 0, 1, ROI_DELTAQP,  32, 32, 128, 128,   0)
        } },

        {/*02*/{ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE },
            QUERY | INIT,{
            ROI_PARS(ROI,                0, 1, ROI_DELTAQP,  64, 0, 64, 128,   0), // Right == Left
            ROI_PARS(ROI_EXPECTED_QUERY, 0, 1, ROI_DELTAQP,  64, 0,  0, 128,   0)
        } },

        {/*03*/{ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE },
            QUERY | INIT,{
            ROI_PARS(ROI,                0, 1, ROI_DELTAQP,  352, 0, 0, 128,   0), // Left == Width
            ROI_PARS(ROI_EXPECTED_QUERY, 0, 1, ROI_DELTAQP,    0, 0, 0, 128,   0)
        } },

        {/*04*/{ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE },
            QUERY | INIT,{
            ROI_PARS(ROI,                0, 1, ROI_DELTAQP,  0, 0, 367, 128,   0), // Right > Width, out of bounds
            ROI_PARS(ROI_EXPECTED_QUERY, 0, 1, ROI_DELTAQP,  0, 0,   0, 128,   0)
        } },

        {/*05*/{ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE },
            QUERY | INIT,{
            ROI_PARS(ROI,                0, 1, ROI_DELTAQP,  0, 64, 128, 64,   0), // Top == Bottom
            ROI_PARS(ROI_EXPECTED_QUERY, 0, 1, ROI_DELTAQP,  0, 64, 128,  0,   0)
        } },

        {/*06*/{ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE },
            QUERY | INIT,{
            ROI_PARS(ROI,                0, 1, ROI_DELTAQP,  0, 300, 128, 0,   0), // Top == Height
            ROI_PARS(ROI_EXPECTED_QUERY, 0, 1, ROI_DELTAQP,  0,   0, 128, 0,   0)
        } },

        {/*07*/{ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE },
            QUERY | INIT,{
            ROI_PARS(ROI,                0, 1, ROI_DELTAQP,  0, 0, 128, 303,   0), // Bottom > Height, out of bounds
            ROI_PARS(ROI_EXPECTED_QUERY, 0, 1, ROI_DELTAQP,  0, 0, 128,   0,   0)
        } },

        {/*08*/{ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE },
            QUERY | INIT /*| ENCODE*/,{
            ROI_PARS(ROI,                0, 1, ROI_DELTAQP,  1, 1, 127, 127,   0),
            ROI_PARS(ROI_EXPECTED_QUERY, 0, 1, ROI_DELTAQP,  0, 0, 128, 128,   0)
        } },

        {/*09*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
            QUERY | INIT | ENCODE,{
            ROI_PARS(ROI,                0, 1, ROI_DELTAQP,  16, 16, 128, 128,   25),
            ROI_PARS(ROI_EXPECTED_QUERY, 0, 1, ROI_DELTAQP,  16, 16, 128, 128,   25),
        } },

        {/*10*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
            QUERY | INIT | ENCODE,{
            ROI_PARS(ROI,                0, 2, ROI_DELTAQP,  16, 16, 128, 128,   25), //ROI[0]
            ROI_PARS(ROI,                1, 2, ROI_DELTAQP,   0,  0,  32,  32,   13), //ROI[1]

            ROI_PARS(ROI_EXPECTED_QUERY, 0, 2, ROI_DELTAQP,  16, 16, 128, 128,   25),
            ROI_PARS(ROI_EXPECTED_QUERY, 1, 2, ROI_DELTAQP,   0,  0,  32,  32,   13),
        } },

        {/*11*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
        QUERY | INIT | ENCODE,{
            ROI_PARS(ROI,                0, 2, ROI_DELTAQP,  16, 16, 128, 128,    51), //ROI[0]
            ROI_PARS(ROI,                1, 2, ROI_DELTAQP,   0,  0,  32,  32,   -50), //ROI[1]

            ROI_PARS(ROI_EXPECTED_QUERY, 0, 2, ROI_DELTAQP,  16, 16, 128, 128,    51),
            ROI_PARS(ROI_EXPECTED_QUERY, 1, 2, ROI_DELTAQP,   0,  0,  32,  32,   -50),
        } },

        {/*12*/{ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, MFX_ERR_NONE },
            QUERY | INIT,{
            { ROI, &tsStruct::mfxExtEncoderROI.NumROI, 300 }, // NumROI < 16
        } },

        {/*13*/{ MFX_ERR_NONE, MFX_ERR_NONE, MFX_ERR_NONE },
            ENCODE,{
            ROI_PARS(ROI,                0, 1, ROI_DELTAQP,  0, 0, 32, 32,   0),
            ROI_PARS(ROI_EXPECTED_QUERY, 0, 1, ROI_DELTAQP,  0, 0, 32, 32,   0)
        } },

        {/*14*/{ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE },
            QUERY | INIT,{
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 0 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 2000 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 0 },
            ROI_PARS(ROI,                0, 1, ROI_DELTAQP,  0, 0, 32, 32,   -52), // DeltaQP have to be in [-51..51]
            ROI_PARS(ROI_EXPECTED_QUERY, 0, 1, ROI_DELTAQP,  0, 0, 32, 32,    0)
        } },

        {/*15*/{ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE },
            QUERY | INIT,{
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 0 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 2000 },
            { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 0 },
            ROI_PARS(ROI,                0, 1, ROI_DELTAQP,  0, 0, 32, 32,    52), // DeltaQP have to be in [-51..51]
            ROI_PARS(ROI_EXPECTED_QUERY, 0, 1, ROI_DELTAQP,  0, 0, 32, 32,    0)
        } },

        // Unenable to run these tests, driver doesn't support
        //{/*16*/{ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE },
        //    QUERY | INIT,{
        //    { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
        //    { MFX_PAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 0 },
        //    { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 2000 },
        //    { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 0 },
        //    ROI_PARS(ROI,                0, 1, ROI_PRIORITY,  0, 0, 32, 32,   -4), // Priority have to be in [-3..3]
        //    ROI_PARS(ROI_EXPECTED_QUERY, 0, 1, ROI_PRIORITY,  0, 0, 32, 32,    0)
        //} },

        //{/*17*/{ MFX_ERR_UNSUPPORTED, MFX_ERR_INVALID_VIDEO_PARAM, MFX_ERR_NONE },
        //    QUERY | INIT,{
        //    { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CBR },
        //    { MFX_PAR, &tsStruct::mfxVideoParam.mfx.InitialDelayInKB, 0 },
        //    { MFX_PAR, &tsStruct::mfxVideoParam.mfx.TargetKbps, 2000 },
        //    { MFX_PAR, &tsStruct::mfxVideoParam.mfx.MaxKbps, 0 },
        //    ROI_PARS(ROI,                0, 1, ROI_PRIORITY,  0, 0, 32, 32,    4), // Priority have to be in [-3..3]
        //    ROI_PARS(ROI_EXPECTED_QUERY, 0, 1, ROI_PRIORITY,  0, 0, 32, 32,    0)
        //} },

    };
    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::test_case[0]);

    int TestSuite::RunTest(unsigned int id) {
        TS_START;
        tc_struct tc = test_case[id];

        if (g_tsOSFamily != MFX_OS_FAMILY_WINDOWS) {
            g_tsLog << "[ SKIPPED ] This test is only for windows platform\n";
            return 0;
        }

        if (g_tsImpl == MFX_IMPL_HARDWARE) {
            if (g_tsHWtype < MFX_HW_SKL) {
                g_tsLog << "SKIPPED for this platform\n";
                return 0;
            }
        }

        mfxStatus sts = MFX_ERR_NONE;
        mfxExtEncoderROI* extROI;
        tsExtBufType<mfxVideoParam> out_par;

        MFXInit();
        m_session = tsSession::m_session;

        m_reader.reset(new tsRawReader(g_tsStreamPool.Get("forBehaviorTest/foreman_cif.nv12"), m_par.mfx.FrameInfo, 1));
        g_tsStreamPool.Reg();

        SETPARS(m_par, MFX_PAR);

        m_par.AddExtBuffer<mfxExtEncoderROI>();
        extROI = m_par;

        out_par = m_par;
        m_pParOut = &out_par;

        if (tc.type & QUERY) {
            SETPARS(extROI, ROI);
            int num_roi = extROI->NumROI;
            if (num_roi == 300)
                for (int i = 0; i < 256; i++) {
                    extROI->ROI[i] = { 0, 0, 16, 16, 0 };
                }

            g_tsStatus.expect(tc.sts.query);
            SETPARS(&extROI_expected, ROI_EXPECTED_QUERY);
            Query();

            extROI = out_par;

            if (num_roi <= 32)
                for (int i = 0; i < num_roi; i++) {
                    CHECK_PARS(extROI_expected.ROI[i], extROI->ROI[i]);
                }
            else
            {
                if (extROI->NumROI < 32) {
                    extROI_expected.NumROI = extROI->NumROI; // value of extROI_expected.NumROI depends on driver
                }
                EXPECT_EQ(extROI_expected.NumROI, extROI->NumROI)
                << "ERROR: Expect num_roi = " << extROI_expected.NumROI << ", but real num_roi = " << extROI->NumROI << "!\n";
            }
        }

        if (tc.type & INIT) {
            SETPARS(extROI, ROI);
            int num_roi = extROI->NumROI;
            if (num_roi == 300)
                for (int i = 0; i < 256; i++) {
                    extROI->ROI[i] = { 0, 0, 16, 16, 0 };
                }

            g_tsStatus.expect(tc.sts.init);
            SETPARS(&extROI_expected, ROI);
            sts = Init();

            if (num_roi <= 32)
                for (int i = 0; i < num_roi; i++) {
                    CHECK_PARS(extROI_expected.ROI[i], extROI->ROI[i]);
                }
            else
            {
                if (extROI->NumROI < 32) {
                    extROI_expected.NumROI = extROI->NumROI;
                }
                EXPECT_EQ(extROI_expected.NumROI, extROI->NumROI)
                    << "ERROR: Expect num_roi = " << extROI_expected.NumROI << ", but real num_roi = " << extROI->NumROI << "!\n";
            }

            if (sts != MFX_ERR_INVALID_VIDEO_PARAM) {
                SETPARS(&extROI_expected, ROI_EXPECTED_QUERY);
                GetVideoParam(m_session, &out_par);
                extROI = out_par;

                if (num_roi <= 32)
                    for (int i = 0; i < num_roi; i++) {
                        CHECK_PARS(extROI_expected.ROI[i], extROI->ROI[i]);
                    }
                else
                {
                    if (extROI->NumROI < 32) {
                        extROI_expected.NumROI = extROI->NumROI;
                    }
                    EXPECT_EQ(extROI_expected.NumROI, extROI->NumROI)
                        << "ERROR: Expect num_roi = " << extROI_expected.NumROI << ", but real num_roi = " << extROI->NumROI << "!\n";
                }
            }
        }

        if (tc.type & ENCODE) {
            SETPARS(extROI, ROI);
            int num_roi = extROI->NumROI;

            SETPARS(&extROI_expected, ROI_EXPECTED_QUERY);
            AllocBitstream();
            EncodeFrames(1);
        }

        if (m_initialized) {
            Close();
        }

        TS_END;
        return 0;
    }
    //! \brief Regs test suite into test system
    TS_REG_TEST_SUITE_CLASS(avce_encoder_roi);
}