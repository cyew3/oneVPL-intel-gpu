/* ****************************************************************************** *\

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2007-2021 Intel Corporation. All Rights Reserved.

File Name: avce_init.cpp

\* ****************************************************************************** */

#include "ts_encoder.h"
#include "ts_struct.h"

#define INVALID_WIDTH   16384
#define INVALID_HEIGHT  16384
#define QP 26
#define ICQ_QUALITY 51

namespace avce_init
{
    inline bool IsOn(mfxU32 coding_option)
    {
        return coding_option == MFX_CODINGOPTION_ON;
    }

    class TestSuite : tsVideoEncoder
    {
    private:

        enum
        {
            MFX_PAR = 0,
            SESSION_NULL,
            PAR_NULL,
            _2_CALL,
            _2_CALL_CLOSE,
            CALL_QUERY,
            RESOLUTION,
            RESOLUTION_W,
            RESOLUTION_H,
            NOT_ALLIGNED,
            ZEROED,
            CROP,
            XY,
            WH,
            XW,
            YH,
            W,
            H,
            INVALID_MAX,
            BIG,
            IO_PATTERN,
            FRAME_RATE,
            SLICE,
            CHROMA_FORMAT,
            NONE,
            BUFFER_SIZE,
            BUFFER_SIZE_DEFAULT,
            INITIAL_DELAY_DEFAULT,
            DEFAULTS,
            EXT_CO2,
            EXT_CO3,
            Compatibility = 0x100,
            RATE_CONTROL = 0x200,
            PIC_STRUCT = 0x400,
            EXT_BUFF = 0x800
        };

        struct tc_struct
        {
            mfxStatus sts;
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

#define ALLOCATE_EXTBUFF(buff_id, buff_class)   \
{ \
                        (*buff) = &(new buff_class())->Header; \
                        memset((*buff), 0, sizeof(buff_class)); \
                        (*buff)->BufferId =  buff_id; \
                        (*buff)->BufferSz =  sizeof(buff_class); \
}

        void CreateBuffer(mfxU32 buff_id, mfxExtBuffer** buff)
        {
            switch (buff_id)
            {
            case MFX_EXTBUFF_CODING_OPTION_SPSPPS:
                ALLOCATE_EXTBUFF(buff_id, mfxExtCodingOptionSPSPPS);
                break;

            case MFX_EXTBUFF_VIDEO_SIGNAL_INFO:
                ALLOCATE_EXTBUFF(buff_id, mfxExtVideoSignalInfo);
                break;

            case MFX_EXTBUFF_CODING_OPTION:
                ALLOCATE_EXTBUFF(buff_id, mfxExtCodingOption);
                break;

            case MFX_EXTBUFF_CODING_OPTION2:
                ALLOCATE_EXTBUFF(buff_id, mfxExtCodingOption2);
                break;

            case MFX_EXTBUFF_CODING_OPTION3:
                ALLOCATE_EXTBUFF(buff_id, mfxExtCodingOption3);
                break;

            case MFX_EXTBUFF_VPP_DOUSE:
                ALLOCATE_EXTBUFF(buff_id, mfxExtVPPDoUse);
                break;

            case MFX_EXTBUFF_VPP_AUXDATA:
                ALLOCATE_EXTBUFF(buff_id, mfxExtVppAuxData);
                break;

            case MFX_EXTBUFF_AVC_REFLIST_CTRL:
                ALLOCATE_EXTBUFF(buff_id, mfxExtAVCRefListCtrl);
                break;

            case MFX_EXTBUFF_VPP_FRAME_RATE_CONVERSION:
                ALLOCATE_EXTBUFF(buff_id, mfxExtVPPFrameRateConversion);
                break;

            case MFX_EXTBUFF_PICTURE_TIMING_SEI:
                ALLOCATE_EXTBUFF(buff_id, mfxExtPictureTimingSEI);
                break;

            case MFX_EXTBUFF_ENCODER_CAPABILITY:
                ALLOCATE_EXTBUFF(buff_id, mfxExtEncoderCapability);
                break;

            case MFX_EXTBUFF_ENCODER_RESET_OPTION:
                ALLOCATE_EXTBUFF(buff_id, mfxExtEncoderResetOption);
                break;

            case NONE:
            {
                *buff = nullptr;
                break;
            }

            default: break;
            }
        }

    public:
        TestSuite() : tsVideoEncoder(MFX_CODEC_AVC) {}
        ~TestSuite() { }

        template<mfxU32 fourcc>
        int RunTest_Subtype(unsigned int id);

        int RunTest(tc_struct tc, unsigned int fourcc_id);
        static const unsigned int n_cases;

        mfxStatus Init(mfxSession session)
        {
            mfxVideoParam orig_par;

            memcpy(&orig_par, m_pPar, sizeof(mfxVideoParam));

            TRACE_FUNC2(MFXVideoENCODE_Init, session, m_pPar);

            g_tsStatus.check(MFXVideoENCODE_Init(session, m_pPar));

            m_initialized = (g_tsStatus.get() >= 0);

            if (m_bCheckLowPowerAtInit && g_tsConfig.lowpower != MFX_CODINGOPTION_UNKNOWN)
            {
                EXPECT_EQ(g_tsConfig.lowpower, m_pPar->mfx.LowPower)
                    << "ERROR: external configuration of LowPower isn't equal to real value\n";
            }

            EXPECT_EQ(0, memcmp(&orig_par, m_pPar, sizeof(mfxVideoParam)))
                << "ERROR: Input parameters must not be changed in Init()";

            return g_tsStatus.get();
        }

    };

    const TestSuite::tc_struct TestSuite::test_case[] =
    {
          // PicStruct
          {/*00*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, PIC_STRUCT, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_UNKNOWN } },
          {/*01*/ MFX_ERR_INVALID_VIDEO_PARAM, PIC_STRUCT, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF } },
          {/*02*/ MFX_ERR_INVALID_VIDEO_PARAM, PIC_STRUCT, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_BFF } },
          {/*03*/ MFX_ERR_NONE, PIC_STRUCT, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_PROGRESSIVE } },
          {/*04*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, PIC_STRUCT, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.FrameInfo.PicStruct, MFX_PICSTRUCT_FIELD_TFF | MFX_PICSTRUCT_FIELD_BFF } },
          {/*05*/ MFX_ERR_NONE, EXT_BUFF, MFX_EXTBUFF_CODING_OPTION2, { EXT_CO2, &tsStruct::mfxExtCodingOption2.IntRefType, MFX_REFRESH_NO } },
          {/*06*/ MFX_ERR_NONE, EXT_BUFF, MFX_EXTBUFF_CODING_OPTION2, { EXT_CO2, &tsStruct::mfxExtCodingOption2.IntRefType, MFX_REFRESH_VERTICAL } },
          {/*07*/ MFX_ERR_NONE, EXT_BUFF, MFX_EXTBUFF_CODING_OPTION2, { EXT_CO2, &tsStruct::mfxExtCodingOption2.IntRefType, MFX_REFRESH_HORIZONTAL } },
          {/*08*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, EXT_BUFF, MFX_EXTBUFF_CODING_OPTION2, { EXT_CO2, &tsStruct::mfxExtCodingOption2.IntRefType, MFX_REFRESH_SLICE } },
          {/*09*/ MFX_ERR_INVALID_VIDEO_PARAM, EXT_BUFF, MFX_EXTBUFF_CODING_OPTION2, { EXT_CO2, &tsStruct::mfxExtCodingOption2.IntRefType, 0x100 } },
          {/*10*/ MFX_ERR_INVALID_VIDEO_PARAM, EXT_BUFF, MFX_EXTBUFF_CODING_OPTION3, { EXT_CO3, &tsStruct::mfxExtCodingOption3.FadeDetection, MFX_CODINGOPTION_ON } },
          // Rate Control Method
          {/*11*/ MFX_ERR_INVALID_VIDEO_PARAM, RATE_CONTROL, NONE, { MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_AVBR } },
          // QP range
          {/*12*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, NONE, NONE, {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
                                                                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, mfxU32(-52) },
                                                                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2 }} },
          {/*13*/ MFX_ERR_NONE, NONE, NONE, {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
                                             { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 10 },
                                             { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2 }} },
          {/*14*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, NONE, NONE, {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
                                                                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPB, mfxU32(-1) },
                                                                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 1 }} },
          {/*15*/ MFX_ERR_NONE, NONE, NONE, {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
                                             { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPP, 51 },
                                             { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 2 }} },
          {/*16*/ MFX_WRN_INCOMPATIBLE_VIDEO_PARAM, NONE, NONE, {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
                                                                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPB, 52 },
                                                                 { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 3 }} },
          {/*17*/ MFX_ERR_NONE, NONE, NONE, {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
                                             { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 30 },
                                             { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 9 }} },
          {/*18*/ MFX_ERR_NONE, NONE, NONE, {{ MFX_PAR, &tsStruct::mfxVideoParam.mfx.RateControlMethod, MFX_RATECONTROL_CQP },
                                             { MFX_PAR, &tsStruct::mfxVideoParam.mfx.QPI, 30 },
                                             { MFX_PAR, &tsStruct::mfxVideoParam.mfx.GopRefDist, 17 }} },
    };

    const unsigned int TestSuite::n_cases = sizeof(TestSuite::test_case) / sizeof(TestSuite::tc_struct);

    template<mfxU32 fourcc>
    int TestSuite::RunTest_Subtype(unsigned int id)
    {
        tc_struct tc = test_case[id];
        return RunTest(tc, fourcc);
    }

    int TestSuite::RunTest(tc_struct tc, unsigned int fourcc_id)
    {
        TS_START;

        mfxHDL hdl;
        mfxHandleType type;
        mfxExtBuffer* ext_buff = nullptr;
        mfxExtCodingOption2* cod2 = nullptr;
        mfxExtCodingOption3* cod3 = nullptr;
        mfxStatus sts = tc.sts;
        ENCODE_CAPS_AVC caps = { 0 };
        mfxU32 capSize = sizeof(ENCODE_CAPS_AVC);

        if (g_tsHWtype >= MFX_HW_DG2 && !IsOn(g_tsConfig.lowpower)) {
            g_tsLog << "[ SKIPPED ] HW Type >= DG2 thus TS_LOWPOWER environment must be set 'ON'"
                << g_tsConfig.lowpower << "\n";
            throw tsSKIP;
        }

        MFXInit();

        mfxStatus caps_sts = GetCaps(&caps, &capSize);
        if (caps_sts != MFX_ERR_UNSUPPORTED)
            g_tsStatus.check(caps_sts);

        if (fourcc_id == MFX_FOURCC_NV12)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
        }
        else if (fourcc_id == MFX_FOURCC_RGB4)
        {
            m_par.mfx.FrameInfo.FourCC = MFX_FOURCC_RGB4;
            m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV444;
            m_par.mfx.FrameInfo.BitDepthLuma = m_par.mfx.FrameInfo.BitDepthChroma = 8;
        }
        else
        {
            g_tsLog << "WARNING: invalid fourcc_id parameter: " << fourcc_id << "\n";
            return 0;
        }

        // set defaults
        m_par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
        m_par.mfx.FrameInfo.Width = 352;
        m_par.mfx.FrameInfo.Height = 320;
        m_par.mfx.FrameInfo.CropW = 352;
        m_par.mfx.FrameInfo.CropH = 288;
        m_par.mfx.FrameInfo.CropX = 0;
        m_par.mfx.FrameInfo.CropY = 0;

        SETPARS(m_pPar, MFX_PAR);

        if (!GetAllocator())
        {
            UseDefaultAllocator(
                (m_par.IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY)
                || (m_request.Type & MFX_MEMTYPE_SYSTEM_MEMORY)
                );
        }

        if (!((m_par.IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY)
            || (m_request.Type & MFX_MEMTYPE_SYSTEM_MEMORY))
            && (!m_pVAHandle))
        {
            m_pFrameAllocator = GetAllocator();
            SetFrameAllocator();
            m_pVAHandle = m_pFrameAllocator;
            m_pVAHandle->get_hdl(type, hdl);
            SetHandle(m_session, type, hdl);
            m_is_handle_set = (g_tsStatus.get() >= 0);
        }

        if (tc.type & EXT_BUFF)
        {
            CreateBuffer(tc.sub_type, &ext_buff);
            m_pPar->NumExtParam = 1;
            m_pPar->ExtParam = &ext_buff;
            if (tc.sub_type == MFX_EXTBUFF_CODING_OPTION2) {
                SETPARS(ext_buff, EXT_CO2);
                cod2 = (mfxExtCodingOption2*)m_par.ExtParam[0];
            }
            if (tc.sub_type == MFX_EXTBUFF_CODING_OPTION3) {
                SETPARS(ext_buff, EXT_CO3);
                cod3 = (mfxExtCodingOption3*)m_par.ExtParam[0];
            }
        }

        sts = tc.sts;

        if (!IsOn(g_tsConfig.lowpower) || g_tsHWtype < MFX_HW_DG2) {
            if (tc.type & Compatibility)
            {
                g_tsLog << "[ SKIPPED ] VDEnc Compatibility tests are targeted only for VDEnc!\n\n\n";
                throw tsSKIP;
            }
        }

        if (caps.SliceIPOnly) {
            m_par.mfx.GopRefDist = 1;
        }

        if (tc.type & PIC_STRUCT) {
            if (IsOn(g_tsConfig.lowpower)) {
                if (!caps.NoInterlacedField && (m_par.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_FIELD_TFF
                    || m_par.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_FIELD_BFF))
                    sts = MFX_ERR_NONE;
            }
            else { // Legacy
                switch (m_par.mfx.FrameInfo.PicStruct) {
                    case MFX_PICSTRUCT_UNKNOWN:
                    case MFX_PICSTRUCT_PROGRESSIVE:
                        sts = MFX_ERR_NONE; break;

                    case MFX_PICSTRUCT_FIELD_TFF:
                    case MFX_PICSTRUCT_FIELD_BFF:
                        if (!caps.NoInterlacedField) sts = MFX_ERR_NONE;
                        break;

                    default:
                        sts = MFX_ERR_NONE;
                }
            }
        }

        if (tc.type == RESOLUTION && tc.sub_type == INVALID_MAX) {
            m_par.mfx.FrameInfo.Width = INVALID_WIDTH;
            m_par.mfx.FrameInfo.Height = INVALID_HEIGHT;
        }
        else if (tc.type == RESOLUTION_W && tc.sub_type == INVALID_MAX) {
            m_par.mfx.FrameInfo.Width = INVALID_WIDTH;
        }
        else if (tc.type == RESOLUTION_H && tc.sub_type == INVALID_MAX) {
            m_par.mfx.FrameInfo.Height = INVALID_HEIGHT;
        }

        if (tc.type & EXT_BUFF)
        {
            if ((tc.sub_type == MFX_EXTBUFF_ENCODER_RESET_OPTION) ||
                (tc.sub_type == MFX_EXTBUFF_VIDEO_SIGNAL_INFO))
                sts = MFX_ERR_NONE;
            else if (tc.sub_type == MFX_EXTBUFF_ENCODER_CAPABILITY && g_tsOSFamily == MFX_OS_FAMILY_WINDOWS && ((g_tsImpl & MFX_IMPL_VIA_D3D11)))
                sts = MFX_ERR_NONE;

            if (g_tsHWtype < MFX_HW_DG2) {
                if ((cod2 && cod2->IntRefType == MFX_REFRESH_SLICE)
                    || (cod3 && cod3->FadeDetection == MFX_CODINGOPTION_ON))
                    sts = MFX_ERR_NONE;
            }
        }

        if (tc.type == BUFFER_SIZE && IsOn(g_tsConfig.lowpower) && g_tsOSFamily == MFX_OS_FAMILY_LINUX &&
            m_pPar->mfx.RateControlMethod == MFX_RATECONTROL_ICQ)
        {
            // ICQ is not supported on VDENC Linux
            sts = MFX_ERR_INVALID_VIDEO_PARAM;
        }

        if ((tc.type & RATE_CONTROL) || tc.type == BUFFER_SIZE)
        {
            if (m_pPar->mfx.RateControlMethod == MFX_RATECONTROL_AVBR && g_tsHWtype < MFX_HW_DG2)
                sts = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
            else if (tc.sts < MFX_ERR_NONE)
                sts = MFX_ERR_INVALID_VIDEO_PARAM;
        }

        if (tc.type == BUFFER_SIZE)
        {
            switch (tc.sub_type)
            {
            case BUFFER_SIZE_DEFAULT:
            {
                // We need to set a quite large InitialDelayInKB,
                // it should be more than bufferSizeInKB = m_par.mfx.TargetKbps / 4
                // It is the same as bufferSizeInKB = m_par.mfx.TargetKbps* DEFAULT_CPB_IN_SECONDS(=2) / 8
                // so DEFAULT_CPB_IN_SECONDS will be increased
                // to make sure that BufferSizeInKB >= InitialDelayInKB after MFXVideoENCODE_Init
                m_par.mfx.BufferSizeInKB = 0;
                m_par.mfx.InitialDelayInKB = m_par.mfx.TargetKbps * 3 / 8;
                break;
            }
            case INITIAL_DELAY_DEFAULT:
            {
                // If InitialDelayInKB == 0 it is calculated as BufferSizeInKB / 2
                // For MFX_RATECONTROL_CQP mode BufferSizeInKB should be >= rawBytes / 1000
                // Maximum of rawBytes = (w * h * 3 * BitDepthLuma + 7) / 8;
                // For others BufferSizeInKB = <valid_non_zero_value>
                m_par.mfx.BufferSizeInKB = (m_par.mfx.FrameInfo.Width * m_par.mfx.FrameInfo.Height * 3 * m_par.mfx.FrameInfo.BitDepthLuma + 7) / 8 / 1000;
                m_par.mfx.InitialDelayInKB = 0;
                break;
            }
            case DEFAULTS:
            {
                m_par.mfx.BufferSizeInKB = 0;
                m_par.mfx.InitialDelayInKB = 0;
                break;
            }
            case NONE:
            {
                m_par.mfx.BufferSizeInKB = (m_par.mfx.FrameInfo.Width * m_par.mfx.FrameInfo.Height * 3 * m_par.mfx.FrameInfo.BitDepthLuma + 7) / 8 / 1000;
                m_par.mfx.InitialDelayInKB = m_par.mfx.TargetKbps * 3 / 8;
                break;
            }
            default: break;
            }
            if (m_pPar->mfx.RateControlMethod == MFX_RATECONTROL_CQP)
            {
                m_pPar->mfx.QPI = QP;
                m_pPar->mfx.QPP = QP;
                m_pPar->mfx.QPB = QP;
            }
            if (m_pPar->mfx.RateControlMethod == MFX_RATECONTROL_ICQ)
            {
                m_pPar->mfx.ICQQuality = ICQ_QUALITY;
            }
        }

        if (tc.type == CALL_QUERY)
            Query();

        if (tc.type == PAR_NULL)
        {
            m_pPar = nullptr;
        }

        if ((tc.type == _2_CALL) || (tc.type == _2_CALL_CLOSE))
        {
            this->Init(m_session);
            if (tc.type == _2_CALL_CLOSE)
                Close();
        }

        //call tested function
        g_tsStatus.expect(sts);

        if (tc.type != SESSION_NULL)
            sts = this->Init(m_session);
        else
            sts = this->Init(NULL);

        if (tc.type & BUFFER_SIZE && sts == MFX_ERR_NONE)
        {
            mfxVideoParam get_par = {};
            GetVideoParam(m_session, &get_par);
            if (m_par.mfx.RateControlMethod != MFX_RATECONTROL_CQP &&
                m_par.mfx.RateControlMethod != MFX_RATECONTROL_AVBR &&
                m_par.mfx.RateControlMethod != MFX_RATECONTROL_ICQ &&
                m_par.mfx.RateControlMethod != MFX_RATECONTROL_LA_ICQ &&
                m_par.mfx.RateControlMethod != MFX_RATECONTROL_LA)
                //InitialDelayInKB is not used if RateControlMethod is not HRD compliant
            {
                if (tc.sub_type == NONE)
                {
                    EXPECT_EQ(get_par.mfx.BufferSizeInKB, m_par.mfx.BufferSizeInKB)
                        << "ERROR: BufferSizeInKB must not be changed";
                    EXPECT_EQ(get_par.mfx.InitialDelayInKB, m_par.mfx.InitialDelayInKB)
                        << "ERROR: InitialDelayInKB must not be changed";
                }
                else
                {
                    EXPECT_GE(get_par.mfx.BufferSizeInKB, get_par.mfx.InitialDelayInKB)
                        << "ERROR: BufferSizeInKB must not be less than InitialDelayInKB";
                }
            }
            else
            {
                EXPECT_GT(get_par.mfx.BufferSizeInKB, 0u)
                    << "ERROR: BufferSizeInKB must be greater than 0";
            }
        }

        if ((sts < MFX_ERR_NONE) && (tc.type != _2_CALL) && (tc.type != _2_CALL_CLOSE))
            g_tsStatus.disable_next_check();

        g_tsStatus.expect(MFX_ERR_NONE);
        Close();

        TS_END;
        return 0;
    }

    TS_REG_TEST_SUITE_CLASS_ROUTINE(avce_init, RunTest_Subtype<MFX_FOURCC_NV12>, n_cases);
    TS_REG_TEST_SUITE_CLASS_ROUTINE(avce_8b_444_rgb4_init, RunTest_Subtype<MFX_FOURCC_RGB4>, n_cases);
}
