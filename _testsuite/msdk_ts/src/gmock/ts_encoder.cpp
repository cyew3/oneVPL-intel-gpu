/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2014-2019 Intel Corporation. All Rights Reserved.
//
*/

#include "ts_encoder.h"
#include "ts_aux_dev.h"

enum eEncoderFunction
{
      INIT
    , RESET
    , QUERY
    , QUERYIOSURF
};

void SkipDecision(mfxVideoParam& par, mfxPluginUID& uid, eEncoderFunction function)
{
    if (g_tsConfig.lowpower == MFX_CODINGOPTION_ON)
    {
        if (   par.mfx.GopRefDist > 1
            || (par.mfx.FrameInfo.PicStruct & (MFX_PICSTRUCT_FIELD_BFF|MFX_PICSTRUCT_FIELD_TFF)))
        {
            g_tsStatus.disable();
            throw tsSKIP;
        }

        if (par.mfx.CodecId == MFX_CODEC_AVC)
        {
            if (   par.mfx.RateControlMethod != 0
                && par.mfx.RateControlMethod != MFX_RATECONTROL_CBR
                && par.mfx.RateControlMethod != MFX_RATECONTROL_VBR
                && par.mfx.RateControlMethod != MFX_RATECONTROL_AVBR
                && par.mfx.RateControlMethod != MFX_RATECONTROL_QVBR
                && par.mfx.RateControlMethod != MFX_RATECONTROL_VCM
                && par.mfx.RateControlMethod != MFX_RATECONTROL_CQP
                && g_tsStatus.m_expected <= MFX_ERR_NONE)
            {
                g_tsStatus.disable();
                throw tsSKIP;
            }
        }
    }

    if (   par.mfx.CodecId == MFX_CODEC_AVC && g_tsHWtype >= HWType::MFX_HW_ICL
        && function != QUERYIOSURF)
    {
        mfxExtCodingOption2* CO2 = GetExtBufferPtr(par);
        mfxExtCodingOption3* CO3 = GetExtBufferPtr(par);
        mfxStatus expect = g_tsStatus.m_expected;
        bool unsupported = false;

        if (   (CO2 && CO2->MaxSliceSize && par.mfx.LowPower == MFX_CODINGOPTION_ON)
            || (CO3 && CO3->FadeDetection == MFX_CODINGOPTION_ON))
        {
            expect = MFX_WRN_INCOMPATIBLE_VIDEO_PARAM;
            unsupported = true;
        }

        if (unsupported)
        {
            if (function != QUERY && expect == MFX_ERR_UNSUPPORTED)
                expect = MFX_ERR_INVALID_VIDEO_PARAM;
            g_tsStatus.last();
            g_tsStatus.expect(expect);
        }
    }

    if ( (par.mfx.CodecId == MFX_CODEC_HEVC) &&
         (uid.Data) && (0 == memcmp(uid.Data, MFX_PLUGINID_HEVCE_HW.Data, sizeof(MFX_PLUGINID_HEVCE_HW.Data))) )
    {
        // MFX_PLUGIN_HEVCE_HW - unsupported on platform less SKL
        if (g_tsHWtype < MFX_HW_SKL)
        {
            g_tsStatus.expect(MFX_ERR_UNSUPPORTED);
            g_tsLog << "WARNING: Unsupported HW Platform!\n";
        }

        if (g_tsConfig.lowpower == MFX_CODINGOPTION_ON && g_tsHWtype < MFX_HW_CNL) {
            g_tsLog << "\n\nWARNING: Lowpower is supported starting from Gen10!\n\n\n";
            g_tsStatus.disable();
            throw tsSKIP;
        }

        // skip all HEVCe tests with unsupported fourcc
        mfxU32 fourcc_id = par.mfx.FrameInfo.FourCC;
        if ((fourcc_id == MFX_FOURCC_P010) && (g_tsHWtype < MFX_HW_KBL))
        {
            g_tsLog << "\n\nWARNING: P010 format only supported on KBL+!\n\n\n";
            g_tsStatus.disable();
            throw tsSKIP;
        }
        if ((fourcc_id == MFX_FOURCC_Y210 || fourcc_id == MFX_FOURCC_YUY2 ||
            fourcc_id == MFX_FOURCC_AYUV || fourcc_id == MFX_FOURCC_Y410)
            && (g_tsHWtype < MFX_HW_ICL))
        {
            g_tsLog << "\n\nWARNING: RExt formats are only supported starting from ICL!\n\n\n";
            g_tsStatus.disable();
            throw tsSKIP;
        }
        if ((fourcc_id == MFX_FOURCC_P016 || fourcc_id == MFX_FOURCC_Y216 || fourcc_id == MFX_FOURCC_Y416)
            && (g_tsHWtype < MFX_HW_TGL))
        {
            g_tsLog << "\n\nWARNING: 12b formats are only supported starting from TGL!\n\n\n";
            g_tsStatus.disable();
            throw tsSKIP;
        }
        if (fourcc_id == MFX_FOURCC_Y216 && g_tsConfig.lowpower == MFX_CODINGOPTION_ON)
        {
            g_tsLog << "\n\nWARNING: Y216 format is NOT supported on VDENC!\n\n\n";
            g_tsStatus.disable();
            throw tsSKIP;
        }
        if ((fourcc_id == MFX_FOURCC_AYUV || fourcc_id == MFX_FOURCC_Y410 || fourcc_id == MFX_FOURCC_Y416)
            && (g_tsConfig.lowpower != MFX_CODINGOPTION_ON))
        {
            g_tsLog << "\n\nWARNING: 4:4:4 formats are only supported on VDENC!\n\n\n";
            g_tsStatus.disable();
            throw tsSKIP;
        }
    }

    if ( par.mfx.CodecId == MFX_CODEC_MPEG2 )
    {
        if ( g_tsImpl != MFX_IMPL_SOFTWARE && g_tsHWtype == MFX_HW_APL )
        {
            g_tsLog << "\nMPEG2 HW Encode is not supported on BXT platform\n";
            g_tsStatus.disable();
            throw tsSKIP;
        }
    }

    if (   par.mfx.LowPower == MFX_CODINGOPTION_ON && par.mfx.CodecId == MFX_CODEC_AVC
        && par.mfx.RateControlMethod != MFX_RATECONTROL_CQP
        && function != QUERYIOSURF)
    {
        mfxExtEncoderROI* roi = GetExtBufferPtr(par);
        mfxStatus expect = g_tsStatus.m_expected;

        if (roi && roi->NumROI)
        {
            switch (function)
            {
            case INIT:
                expect = MFX_ERR_INVALID_VIDEO_PARAM;
                break;
            case RESET:
                if (expect != MFX_ERR_NOT_INITIALIZED)
                    expect = MFX_ERR_INVALID_VIDEO_PARAM;
                break;
            case QUERY:
                expect = MFX_ERR_UNSUPPORTED;
                break;
            default:
                break;
            }
        }
        g_tsStatus.expect(expect);
    }

    if (function != QUERYIOSURF && g_tsOSFamily == MFX_OS_FAMILY_WINDOWS)
    {
        mfxExtCodingOption3* CO3 = GetExtBufferPtr(par);
        if (CO3 && CO3->GPB == MFX_CODINGOPTION_OFF)
        {
            //CodingOption3.GPB == OFF is not supported on Windows
            g_tsStatus.expect(MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
        }
    }
}

void SetFrameTypeIfRequired(mfxEncodeCtrl * pCtrl, mfxVideoParam * pPar, mfxFrameSurface1 * pSurf)
{
    if (!pPar || !pPar->mfx.EncodedOrder || !pCtrl || !pSurf)
    {
        // Skip if frame draining in display order
        return;
    }

    mfxU32 order = pSurf->Data.FrameOrder;
    mfxU32 goporder = order % pPar->mfx.GopPicSize;
    if (goporder == 0)
    {
        pCtrl->FrameType = MFX_FRAMETYPE_I | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_xP | MFX_FRAMETYPE_xREF;
        if (pPar->mfx.IdrInterval <= 1 || order / pPar->mfx.GopPicSize % pPar->mfx.IdrInterval == 0)
            pCtrl->FrameType |= MFX_FRAMETYPE_IDR;
    }
    else if (goporder % pPar->mfx.GopRefDist == 0) {
        pCtrl->FrameType = MFX_FRAMETYPE_P | MFX_FRAMETYPE_REF | MFX_FRAMETYPE_xP | MFX_FRAMETYPE_xREF;
    }
    else {
        pCtrl->FrameType = MFX_FRAMETYPE_B | MFX_FRAMETYPE_xB;
    }
}

tsVideoEncoder::tsVideoEncoder(mfxU32 CodecId, bool useDefaults, MsdkPluginType type)
    : m_default(useDefaults)
    , m_initialized(false)
    , m_loaded(false)
    , m_par()
    , m_bitstream()
    , m_request()
    , m_pPar(&m_par)
    , m_pParOut(&m_par)
    , m_pBitstream(&m_bitstream)
    , m_pRequest(&m_request)
    , m_pSyncPoint(&m_syncpoint)
    , m_pSurf(0)
    , m_pCtrl(&m_ctrl)
    , m_filler(0)
    , m_bs_processor(0)
    , m_frames_buffered(0)
    , m_uid(0)
    , m_single_field_processing(false)
    , m_field_processed(0)
    , m_bUseDefaultFrameType(false)
{
    m_par.mfx.CodecId = CodecId;
    if (g_tsConfig.lowpower != MFX_CODINGOPTION_UNKNOWN)
    {
        m_par.mfx.LowPower = g_tsConfig.lowpower;
    }

    if (m_par.mfx.LowPower == MFX_CODINGOPTION_ON)
    {
        m_par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
    }

    if(m_default)
    {
        //TODO: add codec specific
        m_par.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
        m_par.mfx.RateControlMethod = MFX_RATECONTROL_CQP;
        m_par.mfx.QPI = m_par.mfx.QPP = m_par.mfx.QPB = 26;

        if (CodecId == MFX_CODEC_VP8 || CodecId == MFX_CODEC_VP9)
            m_par.mfx.QPB = 0;

        m_par.mfx.FrameInfo.FourCC       = MFX_FOURCC_NV12;
        m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        if (g_tsConfig.sim) {
            m_par.mfx.FrameInfo.Width  = m_par.mfx.FrameInfo.CropW = 176;
            m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 144;
        } else {
            m_par.mfx.FrameInfo.Width  = m_par.mfx.FrameInfo.CropW = 720;
            m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 480;
        }
        m_par.mfx.FrameInfo.FrameRateExtN = 30;
        m_par.mfx.FrameInfo.FrameRateExtD = 1;

        if (   (CodecId == MFX_CODEC_AVC || CodecId == MFX_CODEC_MPEG2)
            && m_par.mfx.FrameInfo.PicStruct != MFX_PICSTRUCT_PROGRESSIVE)
            m_par.mfx.FrameInfo.Height = (m_par.mfx.FrameInfo.Height + 31) & ~31;
    }
    m_uid = g_tsPlugin.UID(MFX_PLUGINTYPE_VIDEO_ENCODE, CodecId, type);
    m_loaded = !m_uid;
}

tsVideoEncoder::tsVideoEncoder(mfxFeiFunction func, mfxU32 CodecId, bool useDefaults)
    : m_default(useDefaults)
    , m_initialized(false)
    , m_loaded(false)
    , m_par()
    , m_bitstream()
    , m_request()
    , m_pPar(&m_par)
    , m_pParOut(&m_par)
    , m_pBitstream(&m_bitstream)
    , m_pRequest(&m_request)
    , m_pSyncPoint(&m_syncpoint)
    , m_pSurf(0)
    , m_pCtrl(&m_ctrl)
    , m_filler(0)
    , m_bs_processor(0)
    , m_frames_buffered(0)
    , m_uid(0)
    , m_single_field_processing(false)
    , m_field_processed(0)
    , m_bUseDefaultFrameType(false)
{
    m_par.mfx.CodecId = CodecId;
    if (g_tsConfig.lowpower != MFX_CODINGOPTION_UNKNOWN)
    {
        m_par.mfx.LowPower = g_tsConfig.lowpower;
    }

    if(m_default)
    {
        m_par.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
        m_par.mfx.RateControlMethod = MFX_RATECONTROL_CQP;
        m_par.mfx.QPI = m_par.mfx.QPP = m_par.mfx.QPB = 26;
        m_par.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;

        m_par.mfx.FrameInfo.FourCC       = MFX_FOURCC_NV12;
        m_par.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
        m_par.mfx.FrameInfo.Width  = m_par.mfx.FrameInfo.CropW = 720;
        m_par.mfx.FrameInfo.Height = m_par.mfx.FrameInfo.CropH = 480;
        m_par.mfx.FrameInfo.FrameRateExtN = 30;
        m_par.mfx.FrameInfo.FrameRateExtD = 1;
    }


    mfxExtFeiParam& extbuffer = m_par;
    extbuffer.Func = func;

    m_loaded = true;
}

tsVideoEncoder::~tsVideoEncoder()
{
    if(m_initialized)
    {
        Close();
    }
}

mfxStatus tsVideoEncoder::Init()
{   // TODO: remove this. This function is too logic-heavy and non-portable.
    // Use more granular lib init checks/functions in the tests.
    if(m_default)
    {
        if(!m_session)
        {
            MFXInit();TS_CHECK_MFX;
        }
        if(!m_loaded)
        {
            Load();
        }
        if(     !m_pFrameAllocator
            && (   (m_request.Type & (MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET|MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET))
                || (m_par.IOPattern & MFX_IOPATTERN_IN_VIDEO_MEMORY)))
        {
            if(!GetAllocator())
            {
                if (m_pVAHandle)
                    SetAllocator(m_pVAHandle, true);
                else
                    UseDefaultAllocator(false);
            }
            m_pFrameAllocator = GetAllocator();
            SetFrameAllocator();TS_CHECK_MFX;
        }
        if(m_par.IOPattern & MFX_IOPATTERN_IN_OPAQUE_MEMORY)
        {
            QueryIOSurf();
            AllocOpaque(m_request, m_par);
        }
    }

    // Set single field processing flag
    mfxExtFeiParam* fei_ext = (mfxExtFeiParam*)m_par.GetExtBuffer(MFX_EXTBUFF_FEI_PARAM);
    if (fei_ext)
        m_single_field_processing = (fei_ext->SingleFieldProcessing == MFX_CODINGOPTION_ON);

    return Init(m_session, m_pPar);
}

mfxStatus tsVideoEncoder::Init(mfxSession session, mfxVideoParam *par)
{
    mfxVideoParam orig_par;

    if (par) memcpy(&orig_par, m_pPar, sizeof(mfxVideoParam));

    TRACE_FUNC2(MFXVideoENCODE_Init, session, par);
    if (par)
    {
        SkipDecision(*par, *m_uid, INIT);
    }
    g_tsStatus.check( MFXVideoENCODE_Init(session, par) );

    m_initialized = (g_tsStatus.get() >= 0);

    if (par)
    {
        if (g_tsConfig.lowpower != MFX_CODINGOPTION_UNKNOWN)
        {
            EXPECT_EQ(g_tsConfig.lowpower, par->mfx.LowPower)
                << "ERROR: external configuration of LowPower doesn't equal to real value\n";
        }
        EXPECT_EQ(0, memcmp(&orig_par, m_pPar, sizeof(mfxVideoParam)))
            << "ERROR: Input parameters must not be changed in Init()";
    }

    return g_tsStatus.get();
}

typedef enum tagENCODE_FUNC
{
    ENCODE_ENC = 0x0001,
    ENCODE_PAK = 0x0002,
    ENCODE_ENC_PAK = 0x0004,
    ENCODE_HybridPAK = 0x0008,
    ENCODE_WIDI = 0x8000
} ENCODE_FUNC;

// Decode Extension Functions for DXVA11 Encode
#define ENCODE_QUERY_ACCEL_CAPS_ID 0x110

#define Clip3(_min, _max, _x) BS_MIN(BS_MAX(_min, _x), _max)

#if defined(_WIN32) || defined(_WIN64)
mfxStatus tsVideoEncoder::GetGuid(GUID &guid)
{
    typedef std::map<mfxU16, const GUID> TableGuid;

    static TableGuid DXVA2_Intel_Encode_HEVC_Main_8 = {
        { MFX_CHROMAFORMAT_YUV420, { 0x28566328, 0xf041, 0x4466, { 0x8b, 0x14, 0x8f, 0x58, 0x31, 0xe7, 0x8f, 0x8b } } },
        { MFX_CHROMAFORMAT_YUV422, { 0x056a6e36, 0xf3a8, 0x4d00, { 0x96, 0x63, 0x7e, 0x94, 0x30, 0x35, 0x8b, 0xf9 } } },
        { MFX_CHROMAFORMAT_YUV444, { 0x5415a68c, 0x231e, 0x46f4, { 0x87, 0x8b, 0x5e, 0x9a, 0x22, 0xe9, 0x67, 0xe9 } } },
    };
    static TableGuid DXVA2_Intel_Encode_HEVC_Main_10 = {
        { MFX_CHROMAFORMAT_YUV420, { 0x6b4a94db, 0x54fe, 0x4ae1, { 0x9b, 0xe4, 0x7a, 0x7d, 0xad, 0x00, 0x46, 0x00 } } },
        { MFX_CHROMAFORMAT_YUV422, { 0xe139b5ca, 0x47b2, 0x40e1, { 0xaf, 0x1c, 0xad, 0x71, 0xa6, 0x7a, 0x18, 0x36 } } },
        { MFX_CHROMAFORMAT_YUV422, { 0x161be912, 0x44c2, 0x49c0, { 0xb6, 0x1e, 0xd9, 0x46, 0x85, 0x2b, 0x32, 0xa1 } } },
    };
    static TableGuid DXVA2_Intel_Encode_HEVC_Main_12 = {
        { MFX_CHROMAFORMAT_YUV420, { 0xd6d6bc4f, 0xd51a, 0x4712, { 0x97, 0xe8, 0x75, 0x9, 0x17, 0xc8, 0x60, 0xfd } } },
        { MFX_CHROMAFORMAT_YUV422, { 0x7fef652d, 0x3233, 0x44df, { 0xac, 0xf7, 0xec, 0xfb, 0x58, 0x4d, 0xab, 0x35 } } },
        { MFX_CHROMAFORMAT_YUV444, { 0xf8fa34b7, 0x93f5, 0x45a4, { 0xbf, 0xc0, 0x38, 0x17, 0xce, 0xe6, 0xbb, 0x93 } } },
    };
    static TableGuid DXVA2_Intel_LowpowerEncode_HEVC_Main_8 = {
        { MFX_CHROMAFORMAT_YUV420, { 0xb8b28e0c, 0xecab, 0x4217, { 0x8c, 0x82, 0xea, 0xaa, 0x97, 0x55, 0xaa, 0xf0 } } },
        { MFX_CHROMAFORMAT_YUV422, { 0xcee393ab, 0x1030, 0x4f7b, { 0x8d, 0xbc, 0x55, 0x62, 0x9c, 0x72, 0xf1, 0x7e } } },
        { MFX_CHROMAFORMAT_YUV444, { 0x87b2ae39, 0xc9a5, 0x4c53, { 0x86, 0xb8, 0xa5, 0x2d, 0x7e, 0xdb, 0xa4, 0x88 } } },
    };
    static TableGuid DXVA2_Intel_LowpowerEncode_HEVC_Main_10 = {
        { MFX_CHROMAFORMAT_YUV420, { 0x8732ecfd, 0x9747, 0x4897,{ 0xb4, 0x2a, 0xe5, 0x34, 0xf9, 0xff, 0x2b, 0x7a } } },
        { MFX_CHROMAFORMAT_YUV422, { 0x580da148, 0xe4bf, 0x49b1,{ 0x94, 0x3b, 0x42, 0x14, 0xab, 0x05, 0xa6, 0xff } } },
        { MFX_CHROMAFORMAT_YUV444, { 0x10e19ac8, 0xbf39, 0x4443,{ 0xbe, 0xc3, 0x1b, 0x0c, 0xbf, 0xe4, 0xc7, 0xaa } } },
    };
#if defined(MFX_ENABLE_HEVCE_SCC)
    static TableGuid DXVA2_Intel_LowpowerEncode_HEVC_SCC_Main_8 = {
        { MFX_CHROMAFORMAT_YUV420,{ 0x2dec00c7, 0x21ee, 0x4bf8,{ 0x8f, 0x0e, 0x77, 0x3f, 0x11, 0xf1, 0x26, 0xa2 } } },
        { MFX_CHROMAFORMAT_YUV444,{ 0xa33fd0ec, 0xa9d3, 0x4c21,{ 0x92, 0x76, 0xc2, 0x41, 0xcc, 0x90, 0xf6, 0xc7 } } },
    };
    static TableGuid DXVA2_Intel_LowpowerEncode_HEVC_SCC_Main_10 = {
        { MFX_CHROMAFORMAT_YUV420,{ 0xc35153a0, 0x23c0, 0x4a81,{ 0xb3, 0xbb, 0x6a, 0x13, 0x26, 0xf2, 0xb7, 0x6b } } },
        { MFX_CHROMAFORMAT_YUV444,{ 0x310e59d2, 0x7ea4, 0x47bb,{ 0xb3, 0x19, 0x50, 0x0e, 0x78, 0x85, 0x53, 0x36 } } },
    };
#endif

    static TableGuid DXVA2_Intel_LowpowerEncode_VP9_Profile = {
        { MFX_PROFILE_VP9_0,{ 0x9b31316b, 0xf204, 0x455d,{ 0x8a, 0x8c, 0x93, 0x45, 0xdc, 0xa7, 0x7c, 0x1 } } },  ///Profile 0
        { MFX_PROFILE_VP9_1,{ 0x277de9c5, 0xed83, 0x48dd,{ 0xab, 0x8f, 0xac, 0x2d, 0x24, 0xb2, 0x29, 0x43 } } }, ///Profile 1
        { MFX_PROFILE_VP9_2,{ 0xacef8bc,  0x285f, 0x415d,{ 0xab, 0x22, 0x7b, 0xf2, 0x52, 0x7a, 0x3d, 0x2e } } }, ///Profile 2 (10 bit)
        { MFX_PROFILE_VP9_3,{ 0x353aca91, 0xd945, 0x4c13,{ 0xae, 0x7e, 0x46, 0x90, 0x60, 0xfa, 0xc8, 0xd8 } } }, ///Profile 3 (10 bit)
    };

    mfxExtCodingOption3* CO3 = GetExtBufferPtr(m_par);
    switch (m_par.mfx.CodecId)
    {
    case MFX_CODEC_HEVC:
    {
        mfxU16 chromaFormat = MFX_CHROMAFORMAT_YUV420;
#if (MFX_VERSION >= 1027)
        if (CO3)
            chromaFormat = Clip3(MFX_CHROMAFORMAT_YUV420, MFX_CHROMAFORMAT_YUV444, CO3->TargetChromaFormatPlus1 - 1);
        if (g_tsHWtype < MFX_HW_ICL)
            chromaFormat = MFX_CHROMAFORMAT_YUV420;

        if (m_par.mfx.CodecProfile == MFX_PROFILE_HEVC_SCC && chromaFormat == MFX_CHROMAFORMAT_YUV422)
            chromaFormat = MFX_CHROMAFORMAT_YUV420;
#else
        chromaFormat = MFX_CHROMAFORMAT_YUV420;
#endif

        if (g_tsConfig.lowpower != MFX_CODINGOPTION_ON) ///Lowpower = OFF
        {
            guid = DXVA2_Intel_Encode_HEVC_Main_8[chromaFormat]; /// Default 8 bit
            if (g_tsHWtype < MFX_HW_KBL)
                break;

#if (MFX_VERSION >= 1027)
            if (m_par.mfx.CodecProfile == MFX_PROFILE_HEVC_MAIN10 || (CO3 && CO3->TargetBitDepthLuma == 10))
            {
                guid = DXVA2_Intel_Encode_HEVC_Main_10[chromaFormat];
            }
#if defined(PRE_SI_TARGET_PLATFORM_GEN12)
            if (CO3 && CO3->TargetBitDepthLuma == 12)
            {
                guid = DXVA2_Intel_Encode_HEVC_Main_12[chromaFormat];
            }
#endif
#else
            if (m_par.mfx.CodecProfile == MFX_PROFILE_HEVC_MAIN10 || m_par.mfx.FrameInfo.BitDepthLuma == 10 || m_par.mfx.FrameInfo.FourCC == MFX_FOURCC_P010)
            {
                guid = DXVA2_Intel_Encode_HEVC_Main_10[chromaFormat];
            }
#endif
            break;
        }
#if defined(MFX_ENABLE_HEVCE_SCC)
        else if (m_par.mfx.CodecProfile == MFX_PROFILE_HEVC_SCC)
        {
            guid = DXVA2_Intel_LowpowerEncode_HEVC_SCC_Main_8[chromaFormat]; /// Default 8 bit

#if (MFX_VERSION >= 1027)
            if (m_par.mfx.CodecProfile == MFX_PROFILE_HEVC_MAIN10 || (CO3 && CO3->TargetBitDepthLuma == 10))
            {
                guid = DXVA2_Intel_LowpowerEncode_HEVC_SCC_Main_10[chromaFormat];
            }
#else
            if (m_par.mfx.CodecProfile == MFX_PROFILE_HEVC_MAIN10 || m_par.mfx.FrameInfo.BitDepthLuma == 10 || m_par.mfx.FrameInfo.FourCC == MFX_FOURCC_P010)
            {
                guid = DXVA2_Intel_LowpowerEncode_HEVC_SCC_Main_10[chromaFormat];
            }
#endif
            break;
        }
#endif
        else ///Lowpower = ON
        {
            if (g_tsHWtype < MFX_HW_CNL)
                return MFX_ERR_UNSUPPORTED;

            guid = DXVA2_Intel_LowpowerEncode_HEVC_Main_8[chromaFormat]; /// Default 8 bit
            if (g_tsHWtype < MFX_HW_KBL)
                break;

#if (MFX_VERSION >= 1027)
            if (m_par.mfx.CodecProfile == MFX_PROFILE_HEVC_MAIN10 || (CO3 && CO3->TargetBitDepthLuma == 10))
            {
                guid = DXVA2_Intel_LowpowerEncode_HEVC_Main_10[chromaFormat];
            }
#else
            if (m_par.mfx.CodecProfile == MFX_PROFILE_HEVC_MAIN10 || m_par.mfx.FrameInfo.BitDepthLuma == 10 || m_par.mfx.FrameInfo.FourCC == MFX_FOURCC_P010)
            {
                guid = DXVA2_Intel_LowpowerEncode_HEVC_Main_10[chromaFormat];
            }
#endif
            break;
        }
    }
    case MFX_CODEC_VP9:
        if (g_tsConfig.lowpower != MFX_CODINGOPTION_ON)
            return MFX_ERR_UNSUPPORTED;
        else
        {
            if (g_tsHWtype < MFX_HW_CNL)
                return MFX_ERR_UNSUPPORTED;

            try
            {
                guid = DXVA2_Intel_LowpowerEncode_VP9_Profile.at(m_par.mfx.CodecProfile);
            }
            catch (std::out_of_range)
            {
                guid = DXVA2_Intel_LowpowerEncode_VP9_Profile[MFX_PROFILE_VP9_0]; ///Default profile
            }
        }
        break;
    default:
        return MFX_ERR_UNSUPPORTED;
    }
    return MFX_ERR_NONE;
}
#elif LIBVA_SUPPORT

VAProfile ConvertProfileMFX2VA(mfxU16 profile, mfxU32 codecId)
{
// Implemented for hevc, vp9
    if (codecId == MFX_CODEC_HEVC)
    {
        switch (profile)
        {
        case MFX_PROFILE_HEVC_MAIN:
            return VAProfileHEVCMain;
        case MFX_PROFILE_HEVC_MAIN10:
            return VAProfileHEVCMain10;
        default:
            return VAProfileHEVCMain;
        }
    }
    else if (codecId == MFX_CODEC_VP9)
    {
        switch (profile)
        {
        case MFX_PROFILE_VP9_0:
            return VAProfileVP9Profile0;
        case MFX_PROFILE_VP9_1:
            return VAProfileVP9Profile1;
        case MFX_PROFILE_VP9_2:
            return VAProfileVP9Profile2;
        case MFX_PROFILE_VP9_3:
            return VAProfileVP9Profile3;
        default:
            return VAProfileVP9Profile0;
        }
    }
    return VAProfileNone;
}

mfxStatus tsVideoEncoder::IsModeSupported(VADisplay& device, mfxU16 codecProfile, mfxU32 lowpower)
{
    std::vector<VAProfile> profile_list(vaMaxNumProfiles(device), VAProfileNone);
    mfxI32 num_profiles = 0;

    mfxStatus sts = va_to_mfx_status(
        vaQueryConfigProfiles(
            device,
            profile_list.data(),
            &num_profiles));
    MFX_CHECK(sts == MFX_ERR_NONE, sts);

    VAProfile currentProfile = ConvertProfileMFX2VA(codecProfile, m_par.mfx.CodecId);
    MFX_CHECK(
        std::find(std::begin(profile_list), std::end(profile_list), currentProfile) != std::end(profile_list),
        MFX_ERR_UNSUPPORTED);

    std::vector<VAEntrypoint> entrypoint_list(vaMaxNumEntrypoints(device));
    mfxI32 num_entrypoints = 0;

    sts = va_to_mfx_status(
        vaQueryConfigEntrypoints(
            device,
            currentProfile,
            entrypoint_list.data(),
            &num_entrypoints));
    MFX_CHECK(sts == MFX_ERR_NONE, sts);

    VAEntrypoint currentEntryPoint = lowpower == MFX_CODINGOPTION_ON ? VAEntrypointEncSliceLP : VAEntrypointEncSlice;
    MFX_CHECK(
        std::find(std::begin(entrypoint_list), std::end(entrypoint_list), currentEntryPoint) != std::end(entrypoint_list),
        MFX_ERR_UNSUPPORTED);

    return MFX_ERR_NONE;
}

mfxStatus tsVideoEncoder::GetVACaps(VADisplay& device, void *pCaps, mfxU32 *pCapsSize)
{
    mfxStatus sts = MFX_ERR_UNSUPPORTED;

    VAProfile vaProfile = ConvertProfileMFX2VA(m_par.mfx.CodecProfile, m_par.mfx.CodecId);

    if (m_par.mfx.CodecId == MFX_CODEC_HEVC)
    {
        ENCODE_CAPS_HEVC* hevc_caps = (ENCODE_CAPS_HEVC*)pCaps;
        memset(hevc_caps, 0, *pCapsSize);

        hevc_caps->BRCReset = 1;

        std::map<VAConfigAttribType, int> idx_map;
        VAConfigAttribType attr_types[] = {
            VAConfigAttribRTFormat,
            VAConfigAttribRateControl,
            VAConfigAttribEncQuantization,
            VAConfigAttribEncIntraRefresh,
            VAConfigAttribMaxPictureHeight,
            VAConfigAttribMaxPictureWidth,
            VAConfigAttribEncParallelRateControl,
            VAConfigAttribEncMaxRefFrames,
            VAConfigAttribEncSliceStructure,
            VAConfigAttribEncROI,
            VAConfigAttribEncDirtyRect,
            VAConfigAttribFrameSizeToleranceSupport
        };
        std::vector<VAConfigAttrib> attrs;
        attrs.reserve(sizeof(attr_types) / sizeof(attr_types[0]));

        for (size_t i = 0; i < sizeof(attr_types) / sizeof(attr_types[0]); i++) {
            attrs.push_back({ attr_types[i], 0 });
            idx_map[attr_types[i]] = i;
        }

        VAEntrypoint entryPoint = (m_par.mfx.LowPower == MFX_CODINGOPTION_ON) ?
            VAEntrypointEncSliceLP : VAEntrypointEncSlice;

        sts = va_to_mfx_status(
            vaGetConfigAttributes(device,
                vaProfile,
                entryPoint,
                attrs.data(),
                (int)attrs.size()));
        MFX_CHECK(sts == MFX_ERR_NONE, sts);

        //SKL:
        hevc_caps->BitDepth8Only = 1;
        hevc_caps->MaxEncodedBitDepth = 0;

        if (g_tsHWtype >= MFX_HW_KBL)
        {
            hevc_caps->BitDepth8Only = 0;
            hevc_caps->MaxEncodedBitDepth = 1;
        }
        if (g_tsHWtype >= MFX_HW_CNL)
        {
            if (m_par.mfx.LowPower == MFX_CODINGOPTION_ON)
            {
                hevc_caps->LCUSizeSupported = (64 >> 4);
            }
            else
            {
                hevc_caps->LCUSizeSupported = (32 >> 4) | (64 >> 4);
            }
        }
        else
        {
            hevc_caps->LCUSizeSupported = (32 >> 4);
        }

        hevc_caps->BlockSize = 2;

        hevc_caps->VCMBitRateControl =
            attrs[idx_map[VAConfigAttribRateControl]].value & VA_RC_VCM ? 1 : 0; //Video conference mode
        hevc_caps->RollingIntraRefresh = 0; /* (attrs[3].value & (~VA_ATTRIB_NOT_SUPPORTED))  ? 1 : 0*/
        if (attrs[idx_map[VAConfigAttribFrameSizeToleranceSupport]].value != VA_ATTRIB_NOT_SUPPORTED)
            hevc_caps->UserMaxFrameSizeSupport = attrs[idx_map[VAConfigAttribFrameSizeToleranceSupport]].value;
        hevc_caps->MBBRCSupport = 1;
        hevc_caps->MbQpDataSupport = 1;
        hevc_caps->TUSupport = 73;


        if (attrs[idx_map[VAConfigAttribRTFormat]].value == VA_RT_FORMAT_YUV420)
        {
            hevc_caps->Color420Only = 1;
        }
        else
        {
            hevc_caps->YUV422ReconSupport = attrs[idx_map[VAConfigAttribRTFormat]].value & VA_RT_FORMAT_YUV422 ? 1 : 0;
            hevc_caps->YUV444ReconSupport = attrs[idx_map[VAConfigAttribRTFormat]].value & VA_RT_FORMAT_YUV444 ? 1 : 0;
        }

        if (attrs[idx_map[VAConfigAttribMaxPictureWidth]].value != VA_ATTRIB_NOT_SUPPORTED)
            hevc_caps->MaxPicWidth = attrs[idx_map[VAConfigAttribMaxPictureWidth]].value;

        if (attrs[idx_map[VAConfigAttribMaxPictureHeight]].value != VA_ATTRIB_NOT_SUPPORTED)
            hevc_caps->MaxPicHeight = attrs[idx_map[VAConfigAttribMaxPictureHeight]].value;

        if (attrs[idx_map[VAConfigAttribEncSliceStructure]].value != VA_ATTRIB_NOT_SUPPORTED)
        {
            switch (attrs[idx_map[VAConfigAttribEncSliceStructure]].value)
            {
            case VA_ENC_SLICE_STRUCTURE_POWER_OF_TWO_ROWS:
                hevc_caps->SliceStructure = 1;
                break;
            case VA_ENC_SLICE_STRUCTURE_ARBITRARY_MACROBLOCKS:
                hevc_caps->SliceStructure = 4;
                break;
            case VA_ENC_SLICE_STRUCTURE_EQUAL_ROWS:
                hevc_caps->SliceStructure = 2;
                break;
            case VA_ENC_SLICE_STRUCTURE_ARBITRARY_ROWS:
                hevc_caps->SliceStructure = 3;
                break;
            default:
                hevc_caps->SliceStructure = 0;
            }
        }

        if (attrs[idx_map[VAConfigAttribEncMaxRefFrames]].value != VA_ATTRIB_NOT_SUPPORTED)
        {
            hevc_caps->MaxNum_Reference0 =
                attrs[idx_map[VAConfigAttribEncMaxRefFrames]].value & 0xffff;
            hevc_caps->MaxNum_Reference1 =
                (attrs[idx_map[VAConfigAttribEncMaxRefFrames]].value >> 16) & 0xffff;
        }
        else
        {
            hevc_caps->MaxNum_Reference0 = 3;
            hevc_caps->MaxNum_Reference1 = 1;
        }

        if (attrs[idx_map[VAConfigAttribEncROI]].value != VA_ATTRIB_NOT_SUPPORTED) // VAConfigAttribEncROI
        {
            VAConfigAttribValEncROI *VaEncROIValPtr = reinterpret_cast<VAConfigAttribValEncROI *>(&attrs[idx_map[VAConfigAttribEncROI]].value);

            assert(VaEncROIValPtr->bits.num_roi_regions < 32);
            hevc_caps->MaxNumOfROI = VaEncROIValPtr->bits.num_roi_regions;
            hevc_caps->ROIBRCPriorityLevelSupport = VaEncROIValPtr->bits.roi_rc_priority_support;
            hevc_caps->ROIDeltaQPSupport = VaEncROIValPtr->bits.roi_rc_qp_delta_support;
        }
        else
        {
            hevc_caps->MaxNumOfROI = 0;
        }

        if (attrs[idx_map[VAConfigAttribEncDirtyRect]].value != VA_ATTRIB_NOT_SUPPORTED &&
            attrs[idx_map[VAConfigAttribEncDirtyRect]].value != 0)
        {
            hevc_caps->DirtyRectSupport = 1;
            hevc_caps->MaxNumOfDirtyRect = attrs[idx_map[VAConfigAttribEncDirtyRect]].value;
        }


        sts = MFX_ERR_NONE;
    }
    else if (m_par.mfx.CodecId == MFX_CODEC_VP9)
    {
        ENCODE_CAPS_VP9* vp9e_caps = (ENCODE_CAPS_VP9*)pCaps;
        memset(vp9e_caps, 0, *pCapsSize);

        std::map<VAConfigAttribType, int> idx_map;
        VAConfigAttribType attr_types[] = {
            VAConfigAttribRTFormat,
            VAConfigAttribEncDirtyRect,
            VAConfigAttribMaxPictureWidth,
            VAConfigAttribMaxPictureHeight,
            VAConfigAttribEncTileSupport,
            VAConfigAttribEncRateControlExt,
            VAConfigAttribEncParallelRateControl,
            VAConfigAttribFrameSizeToleranceSupport,
            VAConfigAttribProcessingRate,
            VAConfigAttribEncDynamicScaling,
        };
        std::vector<VAConfigAttrib> attrs;
        attrs.reserve(sizeof(attr_types) / sizeof(attr_types[0]));

        for (size_t i = 0; i < sizeof(attr_types) / sizeof(attr_types[0]); i++) {
            attrs.push_back({ attr_types[i], 0 });
            idx_map[attr_types[i]] = i;
        }

        MFX_CHECK(m_par.mfx.LowPower == MFX_CODINGOPTION_ON, MFX_ERR_UNSUPPORTED);
        VAEntrypoint entryPoint = VAEntrypointEncSliceLP; // VP9e + lowpower == OFF is not supported

        sts = va_to_mfx_status(
            vaGetConfigAttributes(device,
                vaProfile,
                entryPoint,
                attrs.data(),
                (int)attrs.size()));
        MFX_CHECK(sts == MFX_ERR_NONE, sts);

        vp9e_caps->CodingLimitSet = 1;
        vp9e_caps->Color420Only = 1; // See DDI

        if (g_tsHWtype >= MFX_HW_ICL)
        {
            vp9e_caps->MaxEncodedBitDepth = 1; //0: 8bit, 1: 8 and 10 bit; TO DO: 10bit also must be supported
            vp9e_caps->NumScalablePipesMinus1 = 1;
        }
        if (attrs[idx_map[VAConfigAttribRTFormat]].value != VA_ATTRIB_NOT_SUPPORTED)
        {
            vp9e_caps->YUV422ReconSupport = attrs[idx_map[VAConfigAttribRTFormat]].value & VA_RT_FORMAT_YUV422 ? 1 : 0;
            vp9e_caps->YUV444ReconSupport = attrs[idx_map[VAConfigAttribRTFormat]].value & VA_RT_FORMAT_YUV444 ? 1 : 0;
        }

        if (attrs[idx_map[VAConfigAttribEncDirtyRect]].value != VA_ATTRIB_NOT_SUPPORTED &&
            attrs[idx_map[VAConfigAttribEncDirtyRect]].value != 0)
        {
            vp9e_caps->DirtyRectSupport = 1;
            vp9e_caps->MaxNumOfDirtyRect = attrs[idx_map[VAConfigAttribEncDirtyRect]].value;
        }

        if (attrs[idx_map[VAConfigAttribMaxPictureWidth]].value != VA_ATTRIB_NOT_SUPPORTED)
            vp9e_caps->MaxPicWidth = attrs[idx_map[VAConfigAttribMaxPictureWidth]].value;

        if (attrs[idx_map[VAConfigAttribMaxPictureHeight]].value != VA_ATTRIB_NOT_SUPPORTED)
            vp9e_caps->MaxPicHeight = attrs[idx_map[VAConfigAttribMaxPictureHeight]].value;

        if (attrs[idx_map[VAConfigAttribEncTileSupport]].value != VA_ATTRIB_NOT_SUPPORTED)
            vp9e_caps->TileSupport = attrs[idx_map[VAConfigAttribEncTileSupport]].value;

        if (attrs[idx_map[VAConfigAttribEncRateControlExt]].value != VA_ATTRIB_NOT_SUPPORTED)
        {
            VAConfigAttribValEncRateControlExt rateControlConf;
            rateControlConf.value = attrs[idx_map[VAConfigAttribEncRateControlExt]].value;
            vp9e_caps->TemporalLayerRateCtrl = rateControlConf.bits.max_num_temporal_layers_minus1;
        }

        vp9e_caps->ForcedSegmentationSupport = 1;
        vp9e_caps->AutoSegmentationSupport = 1;

        if (attrs[idx_map[VAConfigAttribEncMacroblockInfo]].value != VA_ATTRIB_NOT_SUPPORTED &&
            attrs[idx_map[VAConfigAttribEncMacroblockInfo]].value)
            vp9e_caps->SegmentFeatureSupport &= 0b0001;

        if (attrs[idx_map[VAConfigAttribEncMaxRefFrames]].value != VA_ATTRIB_NOT_SUPPORTED &&
            attrs[idx_map[VAConfigAttribEncMaxRefFrames]].value)
            vp9e_caps->SegmentFeatureSupport &= 0b0100;

        if (attrs[idx_map[VAConfigAttribEncSkipFrame]].value != VA_ATTRIB_NOT_SUPPORTED &&
            attrs[idx_map[VAConfigAttribEncSkipFrame]].value)
            vp9e_caps->SegmentFeatureSupport &= 0b1000;

        if (attrs[idx_map[VAConfigAttribEncDynamicScaling]].value != VA_ATTRIB_NOT_SUPPORTED)
        {
            vp9e_caps->DynamicScaling = attrs[idx_map[VAConfigAttribEncDynamicScaling]].value;
        }

        if (attrs[idx_map[VAConfigAttribFrameSizeToleranceSupport]].value != VA_ATTRIB_NOT_SUPPORTED)
            vp9e_caps->UserMaxFrameSizeSupport = attrs[idx_map[VAConfigAttribFrameSizeToleranceSupport]].value;

        if (attrs[idx_map[VAConfigAttribProcessingRate]].value != VA_ATTRIB_NOT_SUPPORTED)
        {
            vp9e_caps->FrameLevelRateCtrl = attrs[idx_map[VAConfigAttribProcessingRate]].value == VA_PROCESSING_RATE_ENCODE;
            vp9e_caps->BRCReset = attrs[idx_map[VAConfigAttribProcessingRate]].value == VA_PROCESSING_RATE_ENCODE;
        }
        vp9e_caps->EncodeFunc = 1;
        vp9e_caps->HybridPakFunc = 1;

        sts = MFX_ERR_NONE;
    }
    else
        sts = MFX_ERR_UNSUPPORTED;


    return sts;
}
#endif

mfxStatus tsVideoEncoder::GetCaps(void *pCaps, mfxU32 *pCapsSize)
{
    TRACE_FUNC2(GetCaps, pCaps, pCapsSize);
    mfxHandleType hdl_type;
    mfxHDL hdl;
    mfxU32 count = 0;
    mfxStatus sts = MFX_ERR_UNSUPPORTED;

#if defined(_WIN32) || defined(_WIN64)
    static const GUID DXVA_NoEncrypt = { 0x1b81beD0, 0xa0c7, 0x11d3, { 0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5 } };

    HRESULT hr;
    const mfxPluginUID * pluginId;

    if (m_par.mfx.CodecId == MFX_CODEC_HEVC)
        pluginId = &MFX_PLUGINID_HEVCE_HW;
    else if (m_par.mfx.CodecId == MFX_CODEC_VP9)
        pluginId = &MFX_PLUGINID_VP9E_HW;
    else
        return MFX_ERR_UNSUPPORTED;

    if ((0 != memcmp(m_uid->Data, pluginId->Data, sizeof(pluginId->Data)))) {
        if (pCapsSize) {
            memset(pCaps, 0, *pCapsSize);
            pCapsSize[0] = 0;
        }
        return MFX_ERR_UNSUPPORTED;
    }

    GUID guid;
    sts = GetGuid(guid);
    MFX_CHECK(sts == MFX_ERR_NONE, MFX_ERR_UNSUPPORTED);

    if (!m_is_handle_set && g_tsImpl != MFX_IMPL_SOFTWARE)
    {
        if (m_initialized)
        {
            g_tsLog << "\nERROR: Handle can't be set if encoder is already initialized!!!\n\n";
            return MFX_ERR_INVALID_HANDLE;
        }

        if (!m_pVAHandle)
        {
            m_pVAHandle = new frame_allocator(
                    (g_tsImpl & MFX_IMPL_VIA_D3D11) ? frame_allocator::HARDWARE_DX11 : frame_allocator::HARDWARE,
                    frame_allocator::ALLOC_MAX,
                    frame_allocator::ENABLE_ALL,
                    frame_allocator::ALLOC_EMPTY);
        }
        m_pVAHandle->get_hdl(hdl_type, hdl);
        SetHandle(m_session, hdl_type, hdl);
        m_is_handle_set = (g_tsStatus.get() >= 0);
    }
    if (g_tsImpl & MFX_IMPL_VIA_D3D11) {
        ID3D11Device* device;
        D3D11_VIDEO_DECODER_DESC    desc = {};
        D3D11_VIDEO_DECODER_CONFIG  config = {};
        hdl_type = mfxHandleType::MFX_HANDLE_D3D11_DEVICE;
        sts = MFXVideoCORE_GetHandle(m_session, hdl_type, (mfxHDL*)&device);
        MFX_CHECK((sts == MFX_ERR_NONE), MFX_ERR_DEVICE_FAILED);

        CComPtr<ID3D11DeviceContext>                m_context;
        CComPtr<ID3D11VideoDecoder>                 m_vdecoder;
        CComQIPtr<ID3D11VideoDevice>                m_vdevice;
        CComQIPtr<ID3D11VideoContext>               m_vcontext;

        device->GetImmediateContext(&m_context);
        MFX_CHECK(m_context, MFX_ERR_DEVICE_FAILED);

        m_vdevice = device;
        m_vcontext = m_context;
        MFX_CHECK(m_vdevice, MFX_ERR_DEVICE_FAILED);
        MFX_CHECK(m_vcontext, MFX_ERR_DEVICE_FAILED);

        // Query supported decode profiles
        {
            bool isFound = false;

            UINT profileCount = m_vdevice->GetVideoDecoderProfileCount();
            assert(profileCount > 0);

            for (UINT i = 0; i < profileCount; i++)
            {
                GUID profileGuid;
                hr = m_vdevice->GetVideoDecoderProfile(i, &profileGuid);
                MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

                if (guid == profileGuid)
                {
                    isFound = true;
                    break;
                }
            }
            MFX_CHECK(isFound, MFX_ERR_UNSUPPORTED);
        }
        mfxU16 width = m_pPar->mfx.FrameInfo.Width ? m_pPar->mfx.FrameInfo.Width : 720;
        mfxU16 height = m_pPar->mfx.FrameInfo.Height ? m_pPar->mfx.FrameInfo.Height : 480;
        // Query the supported encode functions
        {
            desc.SampleWidth = width;
            desc.SampleHeight = height;
            desc.OutputFormat = DXGI_FORMAT_NV12;
            desc.Guid = guid;

            hr = m_vdevice->GetVideoDecoderConfigCount(&desc, &count);
            MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
        }

        // CreateVideoDecoder
        {
            desc.SampleWidth = width;
            desc.SampleHeight = height;
            desc.OutputFormat = DXGI_FORMAT_NV12;
            desc.Guid = guid;

            config.ConfigDecoderSpecific = ENCODE_ENC_PAK;
            config.guidConfigBitstreamEncryption = DXVA_NoEncrypt;
            if (!!m_vdecoder)
                m_vdecoder.Release();

            hr = m_vdevice->CreateVideoDecoder(&desc, &config, &m_vdecoder);
            MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
        }

        // Query the encoding device capabilities
        {
            D3D11_VIDEO_DECODER_EXTENSION ext = {};
            ext.Function = ENCODE_QUERY_ACCEL_CAPS_ID;
            ext.pPrivateInputData = 0;
            ext.PrivateInputDataSize = 0;
            ext.pPrivateOutputData = pCaps;
            ext.PrivateOutputDataSize = *pCapsSize;
            ext.ResourceCount = 0;
            ext.ppResourceList = 0;

            HRESULT hr;
            hr = m_vcontext->DecoderExtension(m_vdecoder, &ext);
            MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
        }

    } else if (g_tsImpl != MFX_IMPL_SOFTWARE) {
        IDirect3DDeviceManager9 *device = 0;
        hdl_type = mfxHandleType::MFX_HANDLE_D3D9_DEVICE_MANAGER;
        sts = MFXVideoCORE_GetHandle(m_session, hdl_type, (mfxHDL*)&device);
        MFX_CHECK((sts == MFX_ERR_NONE), MFX_ERR_DEVICE_FAILED);

        std::auto_ptr<AuxiliaryDevice> auxDevice(new AuxiliaryDevice());
        sts = auxDevice->Initialize(device);
        MFX_CHECK((sts == MFX_ERR_NONE), MFX_ERR_DEVICE_FAILED);

        sts = auxDevice->IsAccelerationServiceExist(guid);
        MFX_CHECK((sts == MFX_ERR_NONE), MFX_ERR_DEVICE_FAILED);
        sts = auxDevice->QueryAccelCaps(&guid, pCaps, pCapsSize);
        MFX_CHECK((sts == MFX_ERR_NONE), MFX_ERR_DEVICE_FAILED);
    }
#elif LIBVA_SUPPORT

    MFX_CHECK(g_tsImpl != MFX_IMPL_SOFTWARE, MFX_ERR_UNSUPPORTED);

    CLibVA* libVa = CreateLibVA();

    VADisplay device = libVa->GetVADisplay();

    MFX_CHECK(IsModeSupported(device, m_par.mfx.CodecProfile, m_par.mfx.LowPower) == MFX_ERR_NONE, MFX_ERR_UNSUPPORTED);

    sts = GetVACaps(device, pCaps, pCapsSize);
    MFX_CHECK(sts == MFX_ERR_NONE, sts);

#endif

    return sts;
}

mfxStatus tsVideoEncoder::Close()
{
    Close(m_session);

    //free the surfaces in pool
    tsSurfacePool::FreeSurfaces();

    return g_tsStatus.get();
}

mfxStatus tsVideoEncoder::Close(mfxSession session)
{
    TRACE_FUNC1(MFXVideoENCODE_Close, session);
    g_tsStatus.check( MFXVideoENCODE_Close(session) );

    m_initialized = false;
    m_frames_buffered = 0;

    return g_tsStatus.get();
}

mfxStatus tsVideoEncoder::Query()
{
    if(m_default)
    {
        if(!m_session)
        {
            MFXInit();
        }
        if(!m_loaded)
        {
            Load();
        }
    }
    return Query(m_session, m_pPar, m_pParOut);
}

mfxStatus tsVideoEncoder::Query(mfxSession session, mfxVideoParam *in, mfxVideoParam *out)
{
    TRACE_FUNC3(MFXVideoENCODE_Query, session, in, out);
    if (in)
    {
        SkipDecision(*in, *m_uid, QUERY);
    }
    g_tsStatus.check( MFXVideoENCODE_Query(session, in, out) );
    TS_TRACE(out);

    return g_tsStatus.get();
}

mfxStatus tsVideoEncoder::QueryIOSurf()
{
    if(m_default)
    {
        if(!m_session)
        {
            MFXInit();
        }
        if(!m_loaded)
        {
            Load();
        }
    }
    return QueryIOSurf(m_session, m_pPar, m_pRequest);
}

mfxStatus tsVideoEncoder::QueryIOSurf(mfxSession session, mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    TRACE_FUNC3(MFXVideoENCODE_QueryIOSurf, session, par, request);
    if (par)
    {
        SkipDecision(*par, *m_uid, QUERYIOSURF);
    }
    g_tsStatus.check( MFXVideoENCODE_QueryIOSurf(session, par, request) );
    TS_TRACE(request);

    return g_tsStatus.get();
}

mfxStatus tsVideoEncoder::Reset()
{
    return Reset(m_session, m_pPar);
}

mfxStatus tsVideoEncoder::Reset(mfxSession session, mfxVideoParam *par)
{
    TRACE_FUNC2(MFXVideoENCODE_Reset, session, par);
    if (par)
    {
        SkipDecision(*par, *m_uid, RESET);
    }
    g_tsStatus.check( MFXVideoENCODE_Reset(session, par) );

    //m_frames_buffered = 0;

    return g_tsStatus.get();
}

mfxStatus tsVideoEncoder::GetVideoParam()
{
    if(m_default && !m_initialized)
    {
        Init();TS_CHECK_MFX;
    }
    return GetVideoParam(m_session, m_pPar);
}

mfxStatus tsVideoEncoder::GetVideoParam(mfxSession session, mfxVideoParam *par)
{
    TRACE_FUNC2(MFXVideoENCODE_GetVideoParam, session, par);
    g_tsStatus.check( MFXVideoENCODE_GetVideoParam(session, par) );
    TS_TRACE(par);

    return g_tsStatus.get();
}

mfxStatus tsVideoEncoder::EncodeFrameAsync()
{
    mfxEncodeCtrl* prevCtrl;
    bool restoreCtrl = false;

    if(m_default)
    {
        if(!PoolSize())
        {
            if(m_pFrameAllocator && !GetAllocator())
            {
                SetAllocator(m_pFrameAllocator, true);
            }
            AllocSurfaces();TS_CHECK_MFX;
            if(!m_pFrameAllocator && (m_request.Type & (MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET|MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET)))
            {
                m_pFrameAllocator = GetAllocator();
                SetFrameAllocator();TS_CHECK_MFX;
            }
        }
        if(!m_initialized)
        {
            Init();TS_CHECK_MFX;
        }
        if(!m_bitstream.MaxLength)
        {
            AllocBitstream();TS_CHECK_MFX;
        }
        if (m_field_processed == 0) {
            //if SingleFieldProcessing is enabled, then don't get new surface if the second field to be processed.
            //if SingleFieldProcessing is not enabled, m_field_processed == 0 always.
            m_pSurf = GetSurface(); TS_CHECK_MFX;
        }

        if(m_filler)
        {
            m_pSurf = m_filler->ProcessSurface(m_pSurf, m_pFrameAllocator);
        }

        if (m_ctrl_next.Get())
        {
            prevCtrl = m_pCtrl;
            restoreCtrl = true;
            m_pCtrl = m_ctrl_next.Get();
        }
    }

    mfxEncodeCtrl * cur_ctrl = m_pSurf ? m_pCtrl : NULL;
    if (m_bUseDefaultFrameType)
    {
        SetFrameTypeIfRequired(cur_ctrl, m_pPar, m_pSurf);
    }

    mfxStatus mfxRes = EncodeFrameAsync(m_session, cur_ctrl, m_pSurf, m_pBitstream, m_pSyncPoint);

    if (m_single_field_processing)
    {
        //m_field_processed would be use as indicator in ProcessBitstream(),
        //so increase it in advance here.
        m_field_processed = 1 - m_field_processed;
    }

    if (m_default)
    {
        if (restoreCtrl)
            m_pCtrl = prevCtrl;

        if (m_ctrl_next.Get())
        {
            if (   mfxRes == MFX_ERR_MORE_DATA
                || mfxRes == MFX_ERR_NONE)
            {
                m_ctrl_next.m_fo = m_pSurf ? m_pSurf->Data.FrameOrder : 0;
                m_ctrl_reorder_buffer.push_back(m_ctrl_next);
                m_ctrl_next.Reset();
            }

            if (mfxRes == MFX_ERR_NONE)
            {
                for (auto& p : m_ctrl_reorder_buffer)
                {
                    p.m_sp = *m_pSyncPoint;
                    p.m_lockCnt = m_request.NumFrameMin;
                }

                m_ctrl_list.splice(m_ctrl_list.end(), m_ctrl_reorder_buffer);
            }
        }
    }

    return mfxRes;
}

mfxStatus tsVideoEncoder::DrainEncodedBitstream()
{
    mfxStatus mfxRes = MFX_ERR_NONE;

    while (mfxRes == MFX_ERR_NONE)
    {
        mfxRes = EncodeFrameAsync(m_session, 0, NULL, m_pBitstream, m_pSyncPoint);
        if (mfxRes == MFX_ERR_NONE)
        {
            mfxStatus mfxResSync = SyncOperation(*m_pSyncPoint);
            if (mfxResSync != MFX_ERR_NONE)
            {
                return mfxResSync;
            }
        }
    }

    if (mfxRes == MFX_ERR_MORE_DATA)
    {
        // MORE_DATA code means everything was drained from the encoder
        return MFX_ERR_NONE;
    }
    else
    {
        return mfxRes;
    }
}

mfxStatus tsVideoEncoder::EncodeFrameAsync(mfxSession session, mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs, mfxSyncPoint *syncp)
{
    TRACE_FUNC5(MFXVideoENCODE_EncodeFrameAsync, session, ctrl, surface, bs, syncp);
    mfxStatus mfxRes = MFXVideoENCODE_EncodeFrameAsync(session, ctrl, surface, bs, syncp);
    TS_TRACE(mfxRes);
    TS_TRACE(ctrl);
    TS_TRACE(surface);
    TS_TRACE(bs);
    TS_TRACE(syncp);

    m_frames_buffered += (mfxRes >= 0);

    return g_tsStatus.m_status = mfxRes;
}

mfxStatus tsVideoEncoder::AllocBitstream(mfxU32 size)
{
    if(!size)
    {
        if(m_par.mfx.CodecId == MFX_CODEC_JPEG)
        {
            size = TS_MAX((m_par.mfx.FrameInfo.Width*m_par.mfx.FrameInfo.Height), 1000000);
        }
        else
        {
            if(!m_par.mfx.BufferSizeInKB)
            {
                GetVideoParam();TS_CHECK_MFX;
            }
            size = m_par.mfx.BufferSizeInKB * TS_MAX(m_par.mfx.BRCParamMultiplier, 1) * 1000 * TS_MAX(m_par.AsyncDepth, 1);
        }
    }

    g_tsLog << "ALLOC BITSTREAM OF SIZE " << size << "\n";

    mfxMemId mid = 0;
    TRACE_FUNC4((*m_buffer_allocator.Alloc), &m_buffer_allocator, size, (MFX_MEMTYPE_SYSTEM_MEMORY|MFX_MEMTYPE_FROM_ENCODE), &mid);
    g_tsStatus.check((*m_buffer_allocator.Alloc)(&m_buffer_allocator, size, (MFX_MEMTYPE_SYSTEM_MEMORY|MFX_MEMTYPE_FROM_ENCODE), &mid));
    TRACE_FUNC3((*m_buffer_allocator.Lock), &m_buffer_allocator, mid, &m_bitstream.Data);
    g_tsStatus.check((*m_buffer_allocator.Lock)(&m_buffer_allocator, mid, &m_bitstream.Data));
    m_bitstream.MaxLength = size;

    return g_tsStatus.get();
}

mfxStatus tsVideoEncoder::AllocSurfaces()
{
    if(m_default && !m_request.NumFrameMin)
    {
        QueryIOSurf();TS_CHECK_MFX;
    }
    return tsSurfacePool::AllocSurfaces(m_request);
}

mfxStatus tsVideoEncoder::SyncOperation()
{
    return SyncOperation(m_syncpoint);
}

mfxStatus tsVideoEncoder::SyncOperation(mfxSyncPoint syncp)
{
    mfxU32 nFrames = m_frames_buffered;
    mfxStatus res = SyncOperation(m_session, syncp, MFX_INFINITE);

    if (m_default && m_bs_processor && g_tsStatus.get() == MFX_ERR_NONE)
    {
        g_tsStatus.check(m_bs_processor->ProcessBitstream(m_pBitstream ? *m_pBitstream : m_bitstream, nFrames));
        TS_CHECK_MFX;
    }

    return g_tsStatus.m_status = res;
}

mfxStatus tsVideoEncoder::SyncOperation(mfxSession session,  mfxSyncPoint syncp, mfxU32 wait)
{
    m_frames_buffered = 0;
    tsSession::SyncOperation(session, syncp, wait);

    if (!g_tsStatus.get())
    {
        for (auto& p : m_ctrl_list)
        {
            if (p.m_sp == syncp)
                p.m_unlock = true;

            if (p.m_unlock && p.m_lockCnt)
                p.m_lockCnt--;
        }

        m_ctrl_list.remove_if([](tsSharedCtrl& p) { return (p.m_unlock && !p.m_lockCnt); });
    }

    return g_tsStatus.get();
}

mfxStatus tsVideoEncoder::EncodeFrames(mfxU32 n, bool check)
{
    mfxU32 encoded = 0;
    mfxU32 submitted = 0;
    mfxU32 async = TS_MAX(1, m_par.AsyncDepth);
    mfxSyncPoint sp;

    mfxExtFeiParam* fei_ext= (mfxExtFeiParam*)m_par.GetExtBuffer(MFX_EXTBUFF_FEI_PARAM);
    if (fei_ext)
        m_single_field_processing = (fei_ext->SingleFieldProcessing == MFX_CODINGOPTION_ON);

    async = TS_MIN(n, async - 1);

    while(encoded < n)
    {
        mfxStatus sts = EncodeFrameAsync();

        if (sts == MFX_ERR_MORE_DATA)
        {
            if(!m_pSurf)
            {
                if(submitted)
                {
                    encoded += submitted;
                    SyncOperation(sp);
                }
                break;
            }

            continue;
        }

        g_tsStatus.check(); TS_CHECK_MFX;
        sp = m_syncpoint;

        //For FEI, AsyncDepth = 1, so every time one frame is submitted for encoded, it will
        //be synced immediately afterwards.
        if(++submitted >= async)
        {
            SyncOperation();TS_CHECK_MFX;

            //If SingleFieldProcessing is enabled, and the first field is processed, continue
            if (m_single_field_processing)
            {
                if (m_field_processed)
                {
                    //for the second field, no new surface submitted.
                    submitted--;
                    continue;
                }
            }

            encoded += submitted;
            submitted = 0;
            async = TS_MIN(async, (n - encoded));
        }
    }

    g_tsLog << encoded << " FRAMES ENCODED\n";

    if (check && (encoded != n))
        return MFX_ERR_UNKNOWN;

    return g_tsStatus.get();
}

mfxStatus tsVideoEncoder::InitAndSetAllocator()
{
    mfxHDL hdl;
    mfxHandleType type;

    bool bNeedSystemMemory = (m_par.IOPattern & MFX_IOPATTERN_IN_SYSTEM_MEMORY)
                             || (m_request.Type & MFX_MEMTYPE_SYSTEM_MEMORY);
    if (!GetAllocator())
    {
        if (!bNeedSystemMemory && m_pVAHandle != nullptr)
        {
            SetAllocator(m_pVAHandle, true); // This allocator stems from the tsSession::MFXInit,
                                             // so it is external to the tsVideoEncoder
        }
        else
        {
            UseDefaultAllocator(bNeedSystemMemory);
        }
    }

    //set handle
    if (!bNeedSystemMemory && m_pVAHandle == nullptr)
    {
        m_pFrameAllocator = GetAllocator();
        SetFrameAllocator();
        m_pVAHandle = m_pFrameAllocator;
        m_pVAHandle->get_hdl(type, hdl);
        SetHandle(m_session, type, hdl);
        m_is_handle_set = (g_tsStatus.get() >= 0);
    }

    return g_tsStatus.get();
}
