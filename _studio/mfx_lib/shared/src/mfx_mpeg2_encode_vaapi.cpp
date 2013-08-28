/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2013 Intel Corporation. All Rights Reserved.
//
//
//          MPEG2 encoder VAAPI
//
*/

#include "mfx_common.h"

#if defined(MFX_ENABLE_MPEG2_VIDEO_ENCODE_HW) && defined(MFX_VA_LINUX)

#include <va/va.h>
#include <assert.h>

#include "libmfx_core_vaapi.h"
#include "mfx_mpeg2_encode_vaapi.h"
#include "vaapi_ext_interface.h"
#include "ippi.h"

#ifndef D3DDDIFMT_NV12
#define D3DDDIFMT_NV12 (D3DDDIFORMAT)(MFX_MAKEFOURCC('N', 'V', '1', '2'))
#endif

#define MFX_CHECK_WITH_ASSERT(EXPR, ERR) { assert(EXPR); MFX_CHECK(EXPR, ERR); }

#define MFX_DESTROY_VABUFFER(vaBufferId, vaDisplay)    \
do {                                               \
    if ( vaBufferId != VA_INVALID_ID)              \
    {                                              \
        vaDestroyBuffer(vaDisplay, vaBufferId);    \
        vaBufferId = VA_INVALID_ID;                \
    }                                              \
} while (0)

#define CODEC_MPEG2_ENC_FCODE_X(width) ((width < 200) ? 3 : (width < 500) ? 4 : (width < 1400) ? 5 : 6)
#define CODEC_MPEG2_ENC_FCODE_Y(fcodeX) ((fcodeX > 5) ? 5 : fcodeX)


namespace
{

    mfxU8 ConvertRateControlMFX2VAAPI(mfxU8 rateControl)
    {
        switch (rateControl)
        {
        case MFX_RATECONTROL_CBR:  return VA_RC_CBR;
        case MFX_RATECONTROL_VBR:  return VA_RC_VBR;
        case MFX_RATECONTROL_AVBR: return VA_RC_VBR;
        case MFX_RATECONTROL_CQP:  return VA_RC_CQP;
        default: assert(!"Unsupported RateControl"); return 0;
        }

    } // mfxU8 ConvertRateControlMFX2VAAPI(mfxU8 rateControl)

    VAProfile ConvertProfileTypeMFX2VAAPI(mfxU8 type)
    {
        switch (type)
        {
            case MFX_PROFILE_MPEG2_SIMPLE: return VAProfileMPEG2Simple;
            case MFX_PROFILE_MPEG2_MAIN:   return VAProfileMPEG2Main;
            case MFX_PROFILE_MPEG2_HIGH:   assert(!"Unsupported profile type");  return VAProfileMPEG2Main;
            default: assert(!"Unsupported profile type"); return VAProfileNone;
        }

    } // VAProfile ConvertProfileTypeMFX2VAAPI(mfxU8 type)

    VAEncPictureType ConvertCodingTypeMFX2VAAPI(UCHAR codingType)
    {
        switch (codingType)
        {
            case CODING_TYPE_I: return VAEncPictureTypeIntra;
            case CODING_TYPE_P: return VAEncPictureTypePredictive;
            case CODING_TYPE_B: return VAEncPictureTypeBidirectional;
            default: assert(!"Unsupported picture coding type"); return (VAEncPictureType)-1;
        }
    } // VAEncPictureType ConvertCodingTypeMFX2VAAPI(UCHAR codingType)

    VASurfaceID ConvertSurfaceIdMFX2VAAPI(VideoCORE* core, mfxMemId id)
    {
        mfxStatus sts;
        VASurfaceID *pSurface = NULL;
        MFX_CHECK_WITH_ASSERT(core, VA_INVALID_SURFACE);
        sts = core->GetFrameHDL(id, (mfxHDL *)&pSurface);
        MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == sts, VA_INVALID_SURFACE);
        return *pSurface;
    }

    unsigned short CalculateAspectRatio(int width, int height)
    {
        unsigned short ret = 1;
        double rate;
        rate = ((double) width) / ((double)height);
        if(rate > 2.3)
            ret = 0xf;
        else if(rate > 2.2)
            ret = 4;
        else if(rate > 1.70)
            ret = 3;
        else if(rate > 1.2)
            ret = 2;
        else if(rate > 0.9)
            ret = 1;
        return ret & 0xf;
    }

    void FillSps(
        MfxHwMpeg2Encode::ExecuteBuffers* pExecuteBuffers,
        VAEncSequenceParameterBufferMPEG2 & sps)
    {
        assert(pExecuteBuffers);
        const ENCODE_SET_SEQUENCE_PARAMETERS_MPEG2 & winSps = pExecuteBuffers->m_sps;

        sps.picture_width   = winSps.FrameWidth;
        sps.picture_height  = winSps.FrameHeight;
        
        if (winSps.FrameRateCode > 0 && winSps.FrameRateCode <= 8)
        {
          const Ipp64f ratetab[8]=
            {24000.0/1001.0, 24.0, 25.0, 30000.0/1001.0, 30.0, 50.0, 60000.0/1001.0, 60.0};
          sps.frame_rate = ratetab[winSps.FrameRateCode - 1]
            * (winSps.FrameRateExtN + 1) / (winSps.FrameRateExtD + 1);
        } else
          assert(!"Unknown FrameRateCode appeared.");

        sps.aspect_ratio_information = CalculateAspectRatio(sps.picture_width, sps.picture_height);
        // sps.vbv_buffer_size = winSps.vbv_buffer_size; // B = 16 * 1024 * vbv_buffer_size 

        sps.intra_period    = pExecuteBuffers->m_GOPPictureSize; // 22??
        sps.ip_period       = pExecuteBuffers->m_GOPRefDist;
        sps.bits_per_second = winSps.bit_rate; // 104857200;
        sps.vbv_buffer_size = winSps.bit_rate * 45 / 1000;
        //sps.vbv_buffer_size = 3; // B = 16 * 1024 * vbv_buffer_size

        int profile = 4, level = 8;

        switch (ConvertProfileTypeMFX2VAAPI(winSps.Profile))
        {
        case VAProfileMPEG2Simple:
            profile = 5;
            break;
        case VAProfileMPEG2Main:
            profile = 4;
            break;
        default:
            assert(0);
            break;
        }

        switch (winSps.Level)
        {
        case MFX_LEVEL_MPEG2_LOW:
            level = 10;
            break;
        case MFX_LEVEL_MPEG2_MAIN:
            level = 8;
            break;
        case MFX_LEVEL_MPEG2_HIGH:
        case MFX_LEVEL_MPEG2_HIGH1440:
            level = 4;
            break;
        default:
            assert(0);
            break;
        }
        sps.sequence_extension.bits.profile_and_level_indication = profile << 4 | level;
        sps.sequence_extension.bits.chroma_format = 1; // CHROMA_FORMAT_420; // 4:2:0
        sps.sequence_extension.bits.frame_rate_extension_n = winSps.FrameRateExtN;
        sps.sequence_extension.bits.frame_rate_extension_d = winSps.FrameRateExtD;
        sps.sequence_extension.bits.progressive_sequence   = winSps.progressive_sequence; 
        sps.sequence_extension.bits.low_delay              = winSps.low_delay; // FIXME

        sps.gop_header.bits.time_code = (1 << 12); // bit12: marker_bit
        sps.gop_header.bits.closed_gop = 0;
        sps.gop_header.bits.broken_link = 0;  

    } // void FillSps(...)

    void FillPps(
        VideoCORE * core,
        MfxHwMpeg2Encode::ExecuteBuffers* pExecuteBuffers,
        VAEncPictureParameterBufferMPEG2 & pps)
    {
        assert(pExecuteBuffers);
        const ENCODE_SET_PICTURE_PARAMETERS_MPEG2 & winPps = pExecuteBuffers->m_pps;
        const ENCODE_SET_SEQUENCE_PARAMETERS_MPEG2 & winSps = pExecuteBuffers->m_sps;

        pps.picture_type = ConvertCodingTypeMFX2VAAPI(winPps.picture_coding_type);
        pps.temporal_reference = winPps.temporal_reference;
        pps.vbv_delay = winPps.vbv_delay;
//         m_CurrFrameMemID        = curr;
//         m_bExternalCurrFrame    = bExternal;
//         m_RecFrameMemID         = rec;
//         m_RefFrameMemID[0]      = ref_0;
//         m_RefFrameMemID[1]      = ref_1;

        pps.reconstructed_picture = ConvertSurfaceIdMFX2VAAPI(core, pExecuteBuffers->m_RecFrameMemID);
        // pps.forward_reference_picture = ConvertSurfaceIdMFX2VAAPI(core, pExecuteBuffers->m_RefFrameMemID[0]);
        // pps.backward_reference_picture = ConvertSurfaceIdMFX2VAAPI(core, pExecuteBuffers->m_RefFrameMemID[1]);

        pps.coded_buf = VA_INVALID_ID;        

        pps.f_code[0][0] = CODEC_MPEG2_ENC_FCODE_X(winSps.FrameWidth);
        pps.f_code[0][1] = CODEC_MPEG2_ENC_FCODE_Y(pps.f_code[0][0]);
        pps.f_code[1][0] = CODEC_MPEG2_ENC_FCODE_X(winSps.FrameWidth);
        pps.f_code[1][1] = CODEC_MPEG2_ENC_FCODE_Y(pps.f_code[1][0]);

//        pps.user_data_length = 0;

        pps.picture_coding_extension.bits.intra_dc_precision         = winPps.intra_dc_precision; /* 8bits */ 
        pps.picture_coding_extension.bits.picture_structure          = 3; /* frame picture */
        pps.picture_coding_extension.bits.top_field_first            = winPps.InterleavedFieldBFF == 0 ? 1 : 0; 
        pps.picture_coding_extension.bits.frame_pred_frame_dct       = winPps.frame_pred_frame_dct;//1; /* FIXME */
        pps.picture_coding_extension.bits.concealment_motion_vectors = winPps.concealment_motion_vectors;
        pps.picture_coding_extension.bits.q_scale_type               = winPps.q_scale_type;
        pps.picture_coding_extension.bits.intra_vlc_format           = winPps.intra_vlc_format;
        pps.picture_coding_extension.bits.alternate_scan             = winPps.alternate_scan;
        pps.picture_coding_extension.bits.repeat_first_field         = winPps.repeat_first_field;
        pps.picture_coding_extension.bits.progressive_frame          = (winPps.FieldCodingFlag == 0) && (winPps.FieldFrameCodingFlag == 0) ? 1 : 0;
        pps.picture_coding_extension.bits.composite_display_flag     = 0;

        if (pps.picture_type == VAEncPictureTypeIntra)
        {
            pps.f_code[0][0] = 0xf;
            pps.f_code[0][1] = 0xf;
            pps.f_code[1][0] = 0xf;
            pps.f_code[1][1] = 0xf;
            pps.forward_reference_picture = VA_INVALID_SURFACE;
            pps.backward_reference_picture = VA_INVALID_SURFACE;
        } 
        else if (pps.picture_type == VAEncPictureTypePredictive) 
        {
            pps.f_code[0][0] = CODEC_MPEG2_ENC_FCODE_X(winSps.FrameWidth);
            pps.f_code[0][1] = CODEC_MPEG2_ENC_FCODE_Y(pps.f_code[0][0]);
            pps.f_code[1][0] = 0xf;
            pps.f_code[1][1] = 0xf;
            pps.forward_reference_picture = ConvertSurfaceIdMFX2VAAPI(core, pExecuteBuffers->m_RefFrameMemID[0]);
            pps.backward_reference_picture = VA_INVALID_SURFACE;
        } 
        else if (pps.picture_type == VAEncPictureTypeBidirectional) 
        {
            pps.f_code[0][0] = CODEC_MPEG2_ENC_FCODE_X(winSps.FrameWidth);
            pps.f_code[0][1] = CODEC_MPEG2_ENC_FCODE_Y(pps.f_code[0][0]);
            pps.f_code[1][0] = CODEC_MPEG2_ENC_FCODE_X(winSps.FrameWidth);
            pps.f_code[1][1] = CODEC_MPEG2_ENC_FCODE_Y(pps.f_code[1][0]);
            pps.forward_reference_picture = ConvertSurfaceIdMFX2VAAPI(core, pExecuteBuffers->m_RefFrameMemID[0]);
            pps.backward_reference_picture = ConvertSurfaceIdMFX2VAAPI(core, pExecuteBuffers->m_RefFrameMemID[1]);
        } else 
        {
            assert(0);
        }
    } // void FillPps(...)

/*
    mfxStatus SetRateControl(
        MfxVideoParam const & par,
        VADisplay    m_vaDisplay,
        VAContextID  m_vaContextEncode,
        VABufferID & rateParamBuf_id)
    {
        VAStatus vaSts;
        VAEncMiscParameterBuffer *misc_param;
        VAEncMiscParameterRateControl *rate_param;

        if ( rateParamBuf_id != VA_INVALID_ID)
        {
            vaDestroyBuffer(m_vaDisplay, rateParamBuf_id);
        }

        vaSts = vaCreateBuffer(m_vaDisplay,
            m_vaContextEncode,
            VAEncMiscParameterBufferType,
            sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterRateControl),
            1,
            NULL,
            &rateParamBuf_id);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        vaSts = vaMapBuffer(m_vaDisplay,
            rateParamBuf_id,
            (void **)&misc_param);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        misc_param->type = VAEncMiscParameterTypeRateControl;
        rate_param = (VAEncMiscParameterRateControl *)misc_param->data;

        rate_param->bits_per_second = par.calcParam.maxKbps * 1000;
        rate_param->window_size     = par.mfx.Convergence * 100;

        if(par.calcParam.maxKbps)
            rate_param->target_percentage = (unsigned int)(100.0 * (mfxF64)par.calcParam.targetKbps / (mfxF64)par.calcParam.maxKbps);

        vaUnmapBuffer(m_vaDisplay, rateParamBuf_id);

        return MFX_ERR_NONE;
    } // void SetRateControl(...)

    mfxStatus SetFrameRate(
        MfxVideoParam const & par,
        VADisplay    m_vaDisplay,
        VAContextID  m_vaContextEncode,
        VABufferID & frameRateBuf_id)
    {
        VAStatus vaSts;
        VAEncMiscParameterBuffer *misc_param;
        VAEncMiscParameterFrameRate *frameRate_param;

        if ( frameRateBuf_id != VA_INVALID_ID)
        {
            vaDestroyBuffer(m_vaDisplay, frameRateBuf_id);
        }

        vaSts = vaCreateBuffer(m_vaDisplay,
            m_vaContextEncode,
            VAEncMiscParameterBufferType,
            sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterFrameRate),
            1,
            NULL,
            &frameRateBuf_id);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        vaSts = vaMapBuffer(m_vaDisplay,
            frameRateBuf_id,
            (void **)&misc_param);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        misc_param->type = VAEncMiscParameterTypeFrameRate;
        frameRate_param = (VAEncMiscParameterFrameRate *)misc_param->data;

        frameRate_param->framerate = (unsigned int)(100.0 * (mfxF64)par.mfx.FrameInfo.FrameRateExtN / (mfxF64)par.mfx.FrameInfo.FrameRateExtD);

        vaUnmapBuffer(m_vaDisplay, frameRateBuf_id);

        return MFX_ERR_NONE;
    } // void SetFrameRate(...)

    mfxStatus SetPrivateParams(
        MfxVideoParam const & par,
        VADisplay    m_vaDisplay,
        VAContextID  m_vaContextEncode,
        VABufferID & privateParams_id)
    {
        if (!IsSupported__VAEncMiscParameterPrivate()) return MFX_ERR_UNSUPPORTED;

        VAStatus vaSts;
        VAEncMiscParameterBuffer *misc_param;
        VAEncMiscParameterPrivate *private_param;

        if ( privateParams_id != VA_INVALID_ID)
        {
            vaDestroyBuffer(m_vaDisplay, privateParams_id);
        }

        vaSts = vaCreateBuffer(m_vaDisplay,
            m_vaContextEncode,
            VAEncMiscParameterBufferType,
            sizeof(VAEncMiscParameterBuffer) + sizeof(VAEncMiscParameterPrivate),
            1,
            NULL,
            &privateParams_id);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        vaSts = vaMapBuffer(m_vaDisplay,
            privateParams_id,
            (void **)&misc_param);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        misc_param->type = (VAEncMiscParameterType)VAEncMiscParameterTypePrivate;
        private_param = (VAEncMiscParameterPrivate *)misc_param->data;

        private_param->target_usage = (unsigned int)(par.mfx.TargetUsage);

        vaUnmapBuffer(m_vaDisplay, privateParams_id);

        return MFX_ERR_NONE;
    } // void SetPrivateParams(...)
*/
    template<class T> inline void Zero(T & obj)                { memset(&obj, 0, sizeof(obj)); }
    template<class T> inline void Zero(std::vector<T> & vec)   { memset(&vec[0], 0, sizeof(T) * vec.size()); }
    template<class T> inline void Zero(T * first, size_t cnt)  { memset(first, 0, sizeof(T) * cnt); }


} // anonymous namespace


using namespace MfxHwMpeg2Encode;

VAAPIEncoder::VAAPIEncoder(VideoCORE* core)
    : m_core(core)
    , m_vaDisplay(0)
    , m_vaContextEncode(VA_INVALID_ID)
    , m_vaConfig(VA_INVALID_ID)
    , m_spsBufferId(VA_INVALID_ID)
    , m_ppsBufferId(VA_INVALID_ID)
    , m_qmBufferId(VA_INVALID_ID)
    , m_numSliceGroups(0)
    , m_codedbufISize(0)
    , m_codedbufPBSize(0)
    , m_pMiscParamsFps(0)
    , m_pMiscParamsPrivate(0)
    , m_miscParamFpsId(VA_INVALID_ID)
    , m_miscParamPrivateId(VA_INVALID_ID)
    , m_vbvBufSize(0)
    , m_initFrameWidth(0)
    , m_initFrameHeight(0)
{
    Zero(m_vaSpsBuf);
    Zero(m_vaPpsBuf);
    Zero(m_sliceParam, MAX_SLICES);
    std::fill(m_sliceParamBufferId, m_sliceParamBufferId + MAX_SLICES, VA_INVALID_ID);
    Zero(m_allocResponseMB);
    Zero(m_allocResponseBS);

    m_pMiscParamsFps = (VAEncMiscParameterBuffer*)new mfxU8[sizeof(VAEncMiscParameterFrameRate) + sizeof(VAEncMiscParameterBuffer)];
    Zero((VAEncMiscParameterFrameRate &)m_pMiscParamsFps->data);
    m_pMiscParamsFps->type = VAEncMiscParameterTypeFrameRate;
    
    m_pMiscParamsPrivate = (VAEncMiscParameterBuffer*)new mfxU8[sizeof(VAEncMiscParameterPrivate) + sizeof(VAEncMiscParameterBuffer)];
    Zero((VAEncMiscParameterPrivate &)m_pMiscParamsPrivate->data);
    m_pMiscParamsPrivate->type = (VAEncMiscParameterType)VAEncMiscParameterTypePrivate;
}

VAAPIEncoder::~VAAPIEncoder()
{
    Close();
}

mfxStatus VAAPIEncoder::QueryEncodeCaps(ENCODE_CAPS & caps)
{
    MFX_CHECK_NULL_PTR1(m_core);

    VAAPIVideoCORE * hwcore = dynamic_cast<VAAPIVideoCORE *>(m_core);
    MFX_CHECK_WITH_ASSERT(hwcore != 0, MFX_ERR_DEVICE_FAILED);

    if (hwcore)
    {
        mfxStatus mfxSts = hwcore->GetVAService(&m_vaDisplay);
        MFX_CHECK_STS(mfxSts);
    }

    // m_width  = width;
    // m_height = height;

    memset(&caps, 0, sizeof(caps));

    caps.EncodeFunc       = 1;
    caps.BRCReset         = 1; // no bitrate resolution control
    caps.VCMBitrateControl = 0; //Video conference mode
    caps.HeaderInsertion  = 0; // we will privide headers (SPS, PPS) in binary format to the driver

    VAStatus vaSts;
    vaExtQueryEncCapabilities pfnVaExtQueryCaps = NULL;
    pfnVaExtQueryCaps = (vaExtQueryEncCapabilities)vaGetLibFunc(m_vaDisplay,VPG_EXT_QUERY_ENC_CAPS);
    if (pfnVaExtQueryCaps)
    {
        VAEncQueryCapabilities VaEncCaps;
        memset(&VaEncCaps, 0, sizeof(VaEncCaps));
        VaEncCaps.size = sizeof(VAEncQueryCapabilities);
        vaSts = pfnVaExtQueryCaps(m_vaDisplay, VAProfileH264Baseline, &VaEncCaps);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        caps.MaxPicWidth  = VaEncCaps.MaxPicWidth;
        caps.MaxPicHeight = VaEncCaps.MaxPicHeight;
        caps.SliceStructure = VaEncCaps.EncLimits.bits.SliceStructure;
        caps.NoInterlacedField = VaEncCaps.EncLimits.bits.NoInterlacedField;
        caps.MaxNum_Reference = VaEncCaps.MaxNum_ReferenceL0;
        caps.MaxNum_Reference1 = VaEncCaps.MaxNum_ReferenceL1;       
    }
    else
    {
        caps.SliceIPBOnly     = 1;
        caps.MaxNum_Reference = 1;
        caps.MaxPicWidth      = 1920;
        caps.MaxPicHeight     = 1088;
        caps.NoInterlacedField = 0; // enable interlaced encoding
        caps.SliceStructure   = 1; // 1 - SliceDividerSnb; 2 - SliceDividerHsw; 3 - SliceDividerBluRay; the other - SliceDividerOneSlice
    }

        
    return MFX_ERR_NONE;
} // mfxStatus VAAPIEncoder::QueryEncodeCaps(ENCODE_CAPS & caps)

mfxStatus VAAPIEncoder::Init(ExecuteBuffers* pExecuteBuffers, mfxU32 numRefFrames, mfxU32 funcId)
{
    mfxStatus sts = MFX_ERR_UNSUPPORTED; 
    assert(ENCODE_ENC_PAK_ID == funcId);
    
    sts = Init(ENCODE_ENC_PAK, pExecuteBuffers);
    MFX_CHECK_STS(sts);

    sts = GetBuffersInfo();
    MFX_CHECK_STS(sts);

    return sts;
} // mfxStatus VAAPIEncoder::Init(ExecuteBuffers* pExecuteBuffers, mfxU32 numRefFrames, mfxU32 funcId)

mfxStatus VAAPIEncoder::Init(ENCODE_FUNC func, ExecuteBuffers* pExecuteBuffers)
{
    mfxStatus   sts    = MFX_ERR_NONE;    

    m_initFrameWidth   = pExecuteBuffers->m_sps.FrameWidth;
    m_initFrameHeight  = pExecuteBuffers->m_sps.FrameHeight;
    memset (&m_rawFrames, 0, sizeof(mfxRawFrames));

    ExtVASurface cleanSurf = {VA_INVALID_ID, 0, 0};
    std::fill(m_feedback.begin(), m_feedback.end(), cleanSurf);

    VAAPIVideoCORE * hwcore = dynamic_cast<VAAPIVideoCORE *>(m_core);
    if(hwcore)
    {
        mfxStatus mfxSts = hwcore->GetVAService(&m_vaDisplay);
        MFX_CHECK_STS(mfxSts);
    }

    VAStatus vaSts;
    VAProfile mpegProfile = ConvertProfileTypeMFX2VAAPI(pExecuteBuffers->m_sps.Profile);
    // should be moved to core->IsGuidSupported()
    {
        VAEntrypoint* pEntrypoints = NULL;
        mfxI32 entrypointsCount = 0, entrypointsIndx = 0;
        mfxI32 maxNumEntrypoints   = vaMaxNumEntrypoints(m_vaDisplay);

        if(maxNumEntrypoints)
            pEntrypoints = (VAEntrypoint*)ippsMalloc_8u(maxNumEntrypoints*sizeof(VAEntrypoint));
        else
            return MFX_ERR_DEVICE_FAILED;
        
        vaSts = vaQueryConfigEntrypoints(
            m_vaDisplay,
            mpegProfile,
            pEntrypoints,
            &entrypointsCount);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        bool bEncodeEnable = false;
        for( entrypointsIndx = 0; entrypointsIndx < entrypointsCount; entrypointsIndx++ )
        {
            if( VAEntrypointEncSlice == pEntrypoints[entrypointsIndx] )
            {
                bEncodeEnable = true;
                break;
            }
        }
        ippsFree(pEntrypoints);
        if( !bEncodeEnable )
        {
            return MFX_ERR_DEVICE_FAILED;// unsupport?
        }
    }
    // IsGuidSupported()

    // Configuration
    VAConfigAttrib attrib[2];

    attrib[0].type = VAConfigAttribRTFormat;
    attrib[1].type = VAConfigAttribRateControl;
    vaGetConfigAttributes(m_vaDisplay,
        ConvertProfileTypeMFX2VAAPI(pExecuteBuffers->m_sps.Profile),
        VAEntrypointEncSlice,
        &attrib[0], 2);

    if ((attrib[0].value & VA_RT_FORMAT_YUV420) == 0)
        return MFX_ERR_DEVICE_FAILED;    

    mfxU8 vaRCType = ConvertRateControlMFX2VAAPI(pExecuteBuffers->m_sps.RateControlMethod);

    if ((attrib[1].value & vaRCType) == 0)
        return MFX_ERR_DEVICE_FAILED;

    attrib[0].value = VA_RT_FORMAT_YUV420;
    attrib[1].value = vaRCType;

    vaSts = vaCreateConfig(
        m_vaDisplay,
        mpegProfile,
        VAEntrypointEncSlice,
        attrib,
        2,
        &m_vaConfig);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    return MFX_ERR_NONE;

#ifdef MPEG2_ENC_HW_PERF

    vm_time_init (&copy_MB_data_time[0]);
    vm_time_init (&copy_MB_data_time[1]);
    vm_time_init (&copy_MB_data_time[2]);

    vm_time_init ( &lock_MB_data_time[0]);
    vm_time_init ( &lock_MB_data_time[1]);
    vm_time_init ( &lock_MB_data_time[2]);

#endif 

    return sts;
} // mfxStatus VAAPIEncoder::Init(ENCODE_FUNC func,ExecuteBuffers* pExecuteBuffers)


mfxStatus VAAPIEncoder::CreateContext(ExecuteBuffers* pExecuteBuffers, mfxU32 numRefFrames, mfxU32 funcId)
{
    assert (m_vaContextEncode == VA_INVALID_ID);
    
    mfxStatus sts;
    VAStatus vaSts;
    std::vector<VASurfaceID> reconSurf;

    //for(size_t i = 0; i < 4; i++)
    //    reconSurf.push_back((VASurfaceID)i);


    for(size_t i = 0; i < m_recFrames.size(); i++)
        reconSurf.push_back(m_recFrames[i].surface);

    // Encoder create
    vaSts = vaCreateContext(
        m_vaDisplay,
        m_vaConfig,
        m_initFrameWidth,
        m_initFrameHeight,
        VA_PROGRESSIVE,
        &*reconSurf.begin(),
        reconSurf.size(),
        &m_vaContextEncode);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    sts = CreateBSBuffer(numRefFrames, pExecuteBuffers);
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;
} // mfxStatus VAAPIEncoder::CreateContext(ExecuteBuffers* pExecuteBuffers, mfxU32 numRefFrames, mfxU32 funcId)

mfxStatus VAAPIEncoder::GetBuffersInfo()
{
    return MFX_ERR_NONE;
} // mfxStatus VAAPIEncoder::GetBuffersInfo() 


mfxStatus VAAPIEncoder::QueryMbDataLayout()
{
    // HRESULT hr = 0;

    memset(&m_layout, 0, sizeof(m_layout));

    /*
    hr = m_pDevice->Execute(MBDATA_LAYOUT_ID, 0, 0,&m_layout,sizeof(m_layout));
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
    */
    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::QueryMbDataLayout()


mfxStatus VAAPIEncoder::QueryCompBufferInfo(D3DDDIFORMAT type, mfxFrameAllocRequest* pRequest, ExecuteBuffers* pExecuteBuffers)
{
    /*
    size_t i = 0;
    for (; i < m_compBufInfo.size(); i++)
    {
        if (m_compBufInfo[i].Type == type)
        {
            break;
        }
    }
    if (i == m_compBufInfo.size())
        return MFX_ERR_UNSUPPORTED;

    pRequest->Info.Width = m_compBufInfo[i].CreationWidth;
    pRequest->Info.Height = m_compBufInfo[i].CreationHeight;
    pRequest->Info.FourCC = MFX_FOURCC_NV12;
    pRequest->NumFrameMin = m_compBufInfo[i].NumBuffer;
    pRequest->NumFrameSuggested = m_compBufInfo[i].NumBuffer;
    */

    if (type == D3DDDIFMT_INTELENCODE_MBDATA)
    {
        pRequest->Info.Width = IPP_MAX(pRequest->Info.Width, pExecuteBuffers->m_sps.FrameWidth);
        pRequest->Info.Height = IPP_MAX(pRequest->Info.Height, pExecuteBuffers->m_sps.FrameHeight);
    }
    else if (type == D3DDDIFMT_INTELENCODE_BITSTREAMDATA)
    {       
        pRequest->Info.Width = IPP_MAX(pRequest->Info.Width, pExecuteBuffers->m_sps.FrameWidth);
        pRequest->Info.Height = IPP_MAX(pRequest->Info.Height, pExecuteBuffers->m_sps.FrameHeight);
//         pRequest->Info.FourCC = MFX_FOURCC_NV12;
//         pRequest->NumFrameMin = m_compBufInfo[i].NumBuffer;
//         pRequest->NumFrameSuggested = m_compBufInfo[i].NumBuffer;
    }
    else
    {
        assert(!"unknown buffer type");
    }
    
    m_codedbufISize = pExecuteBuffers->m_sps.FrameWidth*pExecuteBuffers->m_sps.FrameHeight*3/2;
    m_codedbufPBSize =  pExecuteBuffers->m_sps.FrameWidth*pExecuteBuffers->m_sps.FrameHeight*3/2;

    // request linear buffer
    pRequest->Info.FourCC = MFX_FOURCC_P8;

    // context_id required for allocation video memory (tmp solution)
    pRequest->reserved[0] = m_vaContextEncode;

    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::QueryCompBufferInfo(D3DDDIFORMAT type, mfxFrameAllocRequest* pRequest, ExecuteBuffers * pExecuteBuffers)


mfxStatus VAAPIEncoder::Register(
    const mfxFrameAllocResponse* pResponse,
    D3DDDIFORMAT type)
{
    std::vector<ExtVASurface> * pQueue = (D3DDDIFMT_INTELENCODE_BITSTREAMDATA == type)
        ? &m_bsQueue
        : &m_reconQueue;
    
    mfxStatus sts = MFX_ERR_NONE;

    MFX_CHECK(pResponse->mids, MFX_ERR_NULL_PTR);

    for (int i = 0; i < pResponse->NumFrameActual; i++)
    {
        ExtVASurface extSurf = {};
        VASurfaceID *pSurface = 0;
        
        sts = m_core->GetFrameHDL(pResponse->mids[i], (mfxHDL *)&pSurface);
        MFX_CHECK_STS(sts);

        extSurf.number  = i;
        extSurf.surface = *pSurface;

        pQueue->push_back( extSurf );
    }

    //hr = m_pDevice->BeginFrame(SurfaceReg.surface[0], &SurfaceReg); 
    //MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    //hr = m_pDevice->EndFrame(0);
    //MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    return MFX_ERR_NONE;
} // mfxStatus VAAPIEncoder::Register(const mfxFrameAllocResponse* pResponse, D3DDDIFORMAT type)

mfxStatus VAAPIEncoder::RegisterRefFrames (const mfxFrameAllocResponse* pResponse)
{ 
    MFX_CHECK(pResponse->mids, MFX_ERR_NULL_PTR);    
    MFX_CHECK(pResponse->NumFrameActual < NUM_FRAMES, MFX_ERR_UNSUPPORTED);
    
    m_recFrames.resize(pResponse->NumFrameActual);

    mfxStatus sts;
    VAStatus vaSts;
    ExtVASurface extSurf;
    VASurfaceID *pSurface = NULL;
    

    // we should register allocated HW bitstreams and recon surfaces
    for (int i = 0; i < pResponse->NumFrameActual; i++)
    {
        sts = m_core->GetFrameHDL(pResponse->mids[i], (mfxHDL *)&pSurface);
        MFX_CHECK_STS(sts);

        m_recFrames[i].surface = *pSurface;
        m_recFrames[i].number = i;
    }

    //return MFX_ERR_NONE;

    sts = Register(pResponse, D3DDDIFMT_NV12);
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;
} // mfxStatus VAAPIEncoder::RegisterRefFrames (const mfxFrameAllocResponse* pResponse)


mfxI32 VAAPIEncoder::GetRecFrameIndex (mfxMemId memID)
{
    mfxStatus sts;
    ExtVASurface extSurf;
    VASurfaceID *pSurface = NULL;
    sts = m_core->GetFrameHDL(memID, (mfxHDL *)&pSurface);
    MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == sts, -1);

    for (size_t i = 0; i < m_recFrames.size(); i++)
    {
        if (m_recFrames[i].surface == *pSurface)
            return static_cast<mfxI32>(i);
    }

    return -1;
} // mfxI32 VAAPIEncoder::GetRecFrameIndex (mfxMemId memID)


mfxI32 VAAPIEncoder::GetRawFrameIndex (mfxMemId memID, bool bAddFrames)
{
    mfxStatus sts;
    ExtVASurface extSurf;
    VASurfaceID *pSurface = NULL;
    sts = m_core->GetFrameHDL(memID, (mfxHDL *)&pSurface);
    MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == sts, -1);

    for (size_t i = 0; i < m_recFrames.size(); i++)
    {
        if (m_rawFrames[i].surface == *pSurface)
            return static_cast<mfxI32>(i);
    }

    if (bAddFrames && m_rawFrames.size() < NUM_FRAMES)
    {
        ExtVASurface surf = {*pSurface, 0, 0};
        m_rawFrames.push_back(surf);
        return m_rawFrames.size() - 1;
    }

    return -1;
} // mfxI32 VAAPIEncoder::GetRawFrameIndex (mfxMemId memID, bool bAddFrames)

mfxStatus VAAPIEncoder::CreateMBDataBuffer(mfxU32 numRefFrames)
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxFrameAllocRequest request = {};

    // Buffer for MB INFO
    sts = QueryCompBufferInfo(D3DDDIFMT_INTELENCODE_MBDATA, &request, NULL);
    MFX_CHECK_STS(sts);

    request.NumFrameMin = (request.NumFrameMin < numRefFrames)? (mfxU16)numRefFrames:request.NumFrameMin;
    request.NumFrameSuggested = (request.NumFrameSuggested < request.NumFrameMin)? request.NumFrameMin:request.NumFrameSuggested;

    if (m_allocResponseMB.NumFrameActual == 0)
    {
        request.Info.FourCC = MFX_FOURCC_P8; //D3DFMT_P8;
        request.Type = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_DXVA2_DECODER_TARGET|MFX_MEMTYPE_INTERNAL_FRAME;
        sts = m_core->AllocFrames(&request, &m_allocResponseMB);
        MFX_CHECK_STS(sts);
    }
    else
    {
        if (m_allocResponseMB.NumFrameActual < request.NumFrameMin)
        {
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM; 
        }
    }
    sts = Register(&m_allocResponseMB, D3DDDIFMT_INTELENCODE_MBDATA);
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;
} // mfxStatus VAAPIEncoder::CreateMBDataBuffer(mfxU32 numRefFrames)

mfxStatus VAAPIEncoder::CreateCompBuffers(ExecuteBuffers* pExecuteBuffers, mfxU32 numRefFrames)
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxFrameAllocRequest request = {};

    // Buffer for MB INFO
    sts = QueryCompBufferInfo(D3DDDIFMT_INTELENCODE_MBDATA, &request, pExecuteBuffers);
    MFX_CHECK_STS(sts);

    request.NumFrameMin = (request.NumFrameMin < numRefFrames) ? (mfxU16)numRefFrames : request.NumFrameMin;
    request.NumFrameSuggested = (request.NumFrameSuggested < request.NumFrameMin) ? request.NumFrameMin : request.NumFrameSuggested;

    if (m_allocResponseMB.NumFrameActual == 0)
    {
        request.Info.FourCC = MFX_FOURCC_P8;//D3DFMT_P8;
        request.Type = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_DXVA2_DECODER_TARGET|MFX_MEMTYPE_INTERNAL_FRAME;
        sts = m_core->AllocFrames(&request, &m_allocResponseMB);
        MFX_CHECK_STS(sts);
    }
    else
    {
        if (m_allocResponseMB.NumFrameActual < request.NumFrameMin)
        {
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM; 
        }
    }
    sts = Register(&m_allocResponseMB, D3DDDIFMT_INTELENCODE_MBDATA);
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;
} // mfxStatus VAAPIEncoder::CreateCompBuffers(ExecuteBuffers* pExecuteBuffers, mfxU32 numRefFrames)


mfxStatus VAAPIEncoder::CreateBSBuffer(mfxU32 numRefFrames, ExecuteBuffers* pExecuteBuffers)
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxFrameAllocRequest request = {};

    // Buffer for MB INFO
    sts = QueryCompBufferInfo(D3DDDIFMT_INTELENCODE_BITSTREAMDATA, &request, pExecuteBuffers);
    MFX_CHECK_STS(sts);

    request.NumFrameMin = (request.NumFrameMin < numRefFrames) ? (mfxU16)numRefFrames : request.NumFrameMin;
    request.NumFrameSuggested = (request.NumFrameSuggested < request.NumFrameMin) ? request.NumFrameMin : request.NumFrameSuggested;

    if (m_allocResponseMB.NumFrameActual == 0)
    {
        request.Info.FourCC = MFX_FOURCC_P8;//D3DFMT_P8;
        request.Type = MFX_MEMTYPE_FROM_ENCODE | MFX_MEMTYPE_DXVA2_DECODER_TARGET|MFX_MEMTYPE_INTERNAL_FRAME;
        sts = m_core->AllocFrames(&request, &m_allocResponseBS);
        MFX_CHECK_STS(sts);
    }
    else
    {
        if (m_allocResponseBS.NumFrameActual < request.NumFrameMin)
        {
            return MFX_ERR_INCOMPATIBLE_VIDEO_PARAM; 
        }
    }
    sts = Register(&m_allocResponseBS, D3DDDIFMT_INTELENCODE_BITSTREAMDATA);
    MFX_CHECK_STS(sts);

    return MFX_ERR_NONE;
} // mfxStatus VAAPIEncoder::CreateBSBuffer(mfxU32 numRefFrames)


mfxStatus VAAPIEncoder::FillSlices(ExecuteBuffers* pExecuteBuffers)
{
    VAEncSliceParameterBufferMPEG2 *sliceParam;
    VAStatus vaSts;
    int i, width_in_mbs, height_in_mbs;

    assert(m_vaPpsBuf.picture_coding_extension.bits.q_scale_type == 0);

    width_in_mbs = (m_vaSpsBuf.picture_width + 15) >> 4;
    height_in_mbs = (m_vaSpsBuf.picture_height + 15) >> 4;
    m_numSliceGroups = 1;

#if 1                   //multiple slice
    for (i = 0; i < height_in_mbs; i++) {
        ENCODE_SET_SLICE_HEADER_MPEG2&  ddiSlice = pExecuteBuffers->m_pSlice[i];
        assert(ddiSlice.NumMbsForSlice == width_in_mbs);                
        sliceParam = &m_sliceParam[i];
        //sliceParam->macroblock_address = i * width_in_mbs;
        sliceParam->macroblock_address = i;
        sliceParam->num_macroblocks      = ddiSlice.NumMbsForSlice;
        sliceParam->is_intra_slice       = ddiSlice.IntraSlice;
        sliceParam->quantiser_scale_code = ddiSlice.quantiser_scale_code; 
        //sliceParam->quantiser_scale_code = 4; /*ctx->qp*/ // TODO: where find QP value ?
    }

    vaSts = vaCreateBuffer(m_vaDisplay,
        m_vaContextEncode,
        VAEncSliceParameterBufferType,
        sizeof(*sliceParam),
        height_in_mbs,
        m_sliceParam,
        m_sliceParamBufferId);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

#else       //sigle slice
    sliceParam = &m_sliceParam[0];
    sliceParam->macroblock_address = 0;
    sliceParam->num_macroblocks = width_in_mbs * height_in_mbs;
    sliceParam->is_intra_slice = (m_vaPpsBuf.picture_type == VAEncPictureTypeIntra);
    sliceParam->quantiser_scale_code = /*ctx->qp*/ 8 / 2; // TODO: where find QP value ?


    vaSts = vaCreateBuffer(m_vaDisplay,
        m_vaContextEncode,
        VAEncSliceParameterBufferType,
        sizeof(*sliceParam),
        height_in_mbs,
        m_sliceParam,
        m_sliceParamBufferId);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
#endif

    return MFX_ERR_NONE;
}

mfxStatus VAAPIEncoder::FillMiscParameterBuffer(ExecuteBuffers* pExecuteBuffers)
{
    VAEncMiscParameterFrameRate & miscFps      = (VAEncMiscParameterFrameRate &)m_pMiscParamsFps->data;
    VAEncMiscParameterPrivate   & miscPrivate  = (VAEncMiscParameterPrivate &)m_pMiscParamsPrivate->data;
    VAStatus                      vaSts;

    miscFps.framerate            = m_vaSpsBuf.frame_rate * 100; // TODO: fixme
    miscPrivate.target_usage     = pExecuteBuffers->m_sps.TargetUsage;
    
    vaSts = vaCreateBuffer(m_vaDisplay,
        m_vaContextEncode,
        VAEncMiscParameterBufferType,
        sizeof(sizeof(VAEncMiscParameterFrameRate) + sizeof(VAEncMiscParameterBuffer)),
        1,
        m_pMiscParamsFps,
        &m_miscParamFpsId);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    vaSts = vaCreateBuffer(m_vaDisplay,
        m_vaContextEncode,
        VAEncMiscParameterBufferType,
        sizeof(sizeof(VAEncMiscParameterPrivate) + sizeof(VAEncMiscParameterBuffer)),
        1,
        m_pMiscParamsPrivate,
        &m_miscParamPrivateId);
    
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    return MFX_ERR_NONE;
}

mfxStatus VAAPIEncoder::Execute(ExecuteBuffers* pExecuteBuffers, mfxU32 funcId, mfxU8 *pUserData, mfxU32 userDataLen)
{
    const mfxU32    NumCompBuffer = 10;

    VAStatus vaSts;
    mfxU32 i;
    //VAEncPackedHeaderParameterBuffer packed_header_param_buffer;

    std::vector<VABufferID> configBuffers(NumCompBuffer, VA_INVALID_ID);
    
    mfxU16 buffersCount = 0;

    if (pExecuteBuffers->m_bAddSPS)
    {    
        // TODO: fill sps only once
        Zero(m_vaSpsBuf);
        FillSps(pExecuteBuffers, m_vaSpsBuf);

        
        MFX_DESTROY_VABUFFER(m_spsBufferId, m_vaDisplay);
        vaSts = vaCreateBuffer(m_vaDisplay,
            m_vaContextEncode,
            VAEncSequenceParameterBufferType,
            sizeof(m_vaSpsBuf),
            1,
            &m_vaSpsBuf,
            &m_spsBufferId);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        if (m_spsBufferId != VA_INVALID_ID)
            configBuffers[buffersCount++] = m_spsBufferId;

        pExecuteBuffers->m_bAddSPS = 0;

        if (funcId == ENCODE_ENC_PAK_ID &&  
            (pExecuteBuffers->m_quantMatrix.load_intra_quantiser_matrix || 
             pExecuteBuffers->m_quantMatrix.load_non_intra_quantiser_matrix || 
             pExecuteBuffers->m_quantMatrix.load_chroma_intra_quantiser_matrix || 
             pExecuteBuffers->m_quantMatrix.load_chroma_non_intra_quantiser_matrix))
        {

            MFX_DESTROY_VABUFFER(m_qmBufferId, m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                VAQMatrixBufferType,
                sizeof(pExecuteBuffers->m_quantMatrix),
                1,
                &pExecuteBuffers->m_quantMatrix,
                &m_qmBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            if (m_qmBufferId != VA_INVALID_ID)
                configBuffers[buffersCount++] = m_qmBufferId;
        }
    }

    mfxStatus mfxSts;

    // TODO: fill pps only once
    Zero(m_vaPpsBuf);    
    FillPps(m_core, pExecuteBuffers, m_vaPpsBuf);

    mfxSts = FillMiscParameterBuffer(pExecuteBuffers);
    MFX_CHECK(mfxSts == MFX_ERR_NONE, MFX_ERR_DEVICE_FAILED);

    mfxSts = FillSlices(pExecuteBuffers);
    MFX_CHECK(mfxSts == MFX_ERR_NONE, MFX_ERR_DEVICE_FAILED);

    MFX_CHECK_WITH_ASSERT(pExecuteBuffers->m_idxBs >= 0  && pExecuteBuffers->m_idxBs < m_bsQueue.size(), MFX_ERR_DEVICE_FAILED);
    m_vaPpsBuf.coded_buf = m_bsQueue[pExecuteBuffers->m_idxBs].surface;

    MFX_DESTROY_VABUFFER(m_ppsBufferId, m_vaDisplay);
    vaSts = vaCreateBuffer(m_vaDisplay,
        m_vaContextEncode,
        VAEncPictureParameterBufferType,
        sizeof(m_vaPpsBuf),
        1,
        &m_vaPpsBuf,
        &m_ppsBufferId);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    if (m_ppsBufferId != VA_INVALID_ID)
        configBuffers[buffersCount++] = m_ppsBufferId;

    if (m_miscParamFpsId != VA_INVALID_ID)
        configBuffers[buffersCount++] = m_miscParamFpsId;

    if (m_miscParamPrivateId != VA_INVALID_ID)
        configBuffers[buffersCount++] = m_miscParamPrivateId;


    //------------------------------------------------------------------
    // Rendering
    //------------------------------------------------------------------
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_PRIVATE, "DDI_ENC");
        MFX_LTRACE_I(MFX_TRACE_LEVEL_PRIVATE, pExecuteBuffers->m_idxMb);


        //TODO: external frame HDL??

        vaSts = vaBeginPicture(m_vaDisplay,
            m_vaContextEncode,
            *(VASurfaceID*)pExecuteBuffers->m_pSurface);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        vaSts = vaRenderPicture(m_vaDisplay,
            m_vaContextEncode,
            &configBuffers[0],
            buffersCount);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        vaSts = vaRenderPicture(m_vaDisplay,
            m_vaContextEncode,
            &m_sliceParamBufferId[0],
            m_numSliceGroups);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        vaSts = vaEndPicture(m_vaDisplay, m_vaContextEncode);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        
        vaSts = vaSyncSurface(m_vaDisplay, *(VASurfaceID*)pExecuteBuffers->m_pSurface);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    }

    //------------------------------------------------------------------
    // PostStage
    //------------------------------------------------------------------
    // put to cache
    {
        //UMC::AutomaticUMCMutex guard(m_guard);
        ExtVASurface currentFeedback = {
            *(VASurfaceID*)pExecuteBuffers->m_pSurface,
            pExecuteBuffers->m_pps.StatusReportFeedbackNumber,            
            pExecuteBuffers->m_idxBs
        };
        //currentFeedback.number  =  pExecuteBuffers->m_pps.StatusReportFeedbackNumber;//   task.m_statusReportNumber[fieldId];
        //currentFeedback.surface = *(VASurfaceID*)pExecuteBuffers->m_pSurface; //ConvertSurfaceIdMFX2VAAPI(m_core, pExecuteBuffers->m_pSurface);
        //currentFeedback.idxBs    = pExecuteBuffers->m_idxBs;                         // task.m_idxBs[fieldId];
        m_feedback.push_back( currentFeedback );
    }

    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::Execute(ExecuteBuffers* pExecuteBuffers, mfxU32 funcId)

mfxStatus VAAPIEncoder::SetFrames (ExecuteBuffers* pExecuteBuffers)
{
    mfxStatus sts = MFX_ERR_NONE;
    mfxI32 ind = 0;

    if (pExecuteBuffers->m_RecFrameMemID)
    {
        ind = GetRecFrameIndex(pExecuteBuffers->m_RecFrameMemID);
    }
    else
    {
        ind = 0xff;
    }

    pExecuteBuffers->m_pps.CurrReconstructedPic.Index7Bits     =  mfxU8(ind < 0 ? 0 : ind);
    pExecuteBuffers->m_pps.CurrReconstructedPic.AssociatedFlag =  0;
    pExecuteBuffers->m_idxMb = (DWORD)ind;
    pExecuteBuffers->m_idxBs = (DWORD)ind;

    if (pExecuteBuffers->m_bUseRawFrames)
    {
        ind = GetRawFrameIndex(pExecuteBuffers->m_CurrFrameMemID,true);
        MFX_CHECK(ind>=0,MFX_ERR_NOT_FOUND);
    }
    //else CurrOriginalPic == CurrReconstructedPic 

    pExecuteBuffers->m_pps.CurrOriginalPic.Index7Bits     =  mfxU8(ind);
    pExecuteBuffers->m_pps.CurrOriginalPic.AssociatedFlag =  0;

    if (pExecuteBuffers->m_RefFrameMemID[0])
    {
        ind =  GetRecFrameIndex(pExecuteBuffers->m_RefFrameMemID[0]);
        MFX_CHECK(ind>=0,MFX_ERR_NOT_FOUND);
        pExecuteBuffers->m_pps.RefFrameList[0].Index7Bits = mfxU8(ind);
        pExecuteBuffers->m_pps.RefFrameList[0].AssociatedFlag = 0;
    }
    else
    {
        pExecuteBuffers->m_pps.RefFrameList[0].bPicEntry = 0xff;
    }

    if (pExecuteBuffers->m_RefFrameMemID[1])
    {
        ind = GetRecFrameIndex(pExecuteBuffers->m_RefFrameMemID[1]);
        MFX_CHECK(ind>=0,MFX_ERR_NOT_FOUND);
        pExecuteBuffers->m_pps.RefFrameList[1].Index7Bits = mfxU8(ind);
        pExecuteBuffers->m_pps.RefFrameList[1].AssociatedFlag = 0;
    }
    else
    {
        pExecuteBuffers->m_pps.RefFrameList[1].bPicEntry = 0xff;
    }
    if (pExecuteBuffers->m_bExternalCurrFrame)
    {
        sts = m_core->GetExternalFrameHDL(pExecuteBuffers->m_CurrFrameMemID,(mfxHDL *)&pExecuteBuffers->m_pSurface);
    }
    else
    {
        sts = m_core->GetFrameHDL(pExecuteBuffers->m_CurrFrameMemID,(mfxHDL *)&pExecuteBuffers->m_pSurface);    
    }
    MFX_CHECK_STS(sts);

    /*printf("CurrOriginalPic %d, CurrReconstructedPic %d, RefFrameList[0] %d, RefFrameList[1] %d\n",
            pExecuteBuffers->m_pps.CurrOriginalPic.Index7Bits,
            pExecuteBuffers->m_pps.CurrReconstructedPic.Index7Bits,
            pExecuteBuffers->m_pps.RefFrameList[0].Index7Bits,
            pExecuteBuffers->m_pps.RefFrameList[1].Index7Bits);*/

    return sts;
} // mfxStatus VAAPIEncoder::SetFrames (ExecuteBuffers* pExecuteBuffers)


mfxStatus VAAPIEncoder::Execute(ExecuteBuffers* pExecuteBuffers, mfxU8 *pUserData, mfxU32 userDataLen)
{
    MFX::AutoTimer timer(__FUNCTION__);

    mfxStatus sts = MFX_ERR_NONE;

    sts = Execute(pExecuteBuffers, ENCODE_ENC_PAK_ID, pUserData, userDataLen);
    MFX_CHECK_STS(sts);    

    return sts;

} // mfxStatus VAAPIEncoder::Execute(ExecuteBuffers* pExecuteBuffers, mfxU8 *pUserData, mfxU32 userDataLen)


mfxStatus VAAPIEncoder::Close()
{
    mfxStatus sts = MFX_ERR_NONE;

    delete [] (mfxU8 *)m_pMiscParamsFps;
    m_pMiscParamsFps = 0;
    delete [] (mfxU8 *)m_pMiscParamsPrivate;
    m_pMiscParamsPrivate = 0;

    if (m_vaContextEncode != VA_INVALID_ID)
    {
#ifdef MPEG2_ENC_HW_PERF
        FILE* f = fopen ("mpeg2_ENK_hw_perf_ex.txt","a+");
        fprintf(f,"%d\t%d\t%d\t%d\t%d\t%d\t%d\n",
            (int)lock_MB_data_time[0].diff,
            (int)lock_MB_data_time[1].diff,
            (int)lock_MB_data_time[2].diff,
            (int)copy_MB_data_time[0].diff,
            (int)copy_MB_data_time[1].diff,
            (int)copy_MB_data_time[2].diff,
            (int)lock_MB_data_time[0].freq);
        fclose(f);
#endif 

        vaDestroyContext(m_vaDisplay, m_vaContextEncode);
        m_vaContextEncode = VA_INVALID_ID;
    }

    if (m_vaConfig != VA_INVALID_ID)
    {
        vaDestroyConfig(m_vaDisplay, m_vaConfig);
        m_vaConfig = VA_INVALID_ID;
    }

    MFX_DESTROY_VABUFFER(m_spsBufferId, m_vaDisplay);
//    MFX_DESTROY_VABUFFER(m_hrdBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_qmBufferId, m_vaDisplay);
//    MFX_DESTROY_VABUFFER(m_frameRateId, m_vaDisplay);
//    MFX_DESTROY_VABUFFER(m_privateParamsId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_ppsBufferId, m_vaDisplay);
    for (mfxU32 i = 0; i < MAX_SLICES; i++)
    {
        MFX_DESTROY_VABUFFER(m_sliceParamBufferId[i], m_vaDisplay);
    }

    MFX_DESTROY_VABUFFER(m_miscParamFpsId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_miscParamPrivateId, m_vaDisplay);

//    MFX_DESTROY_VABUFFER(m_packedAudHeaderBufferId, m_vaDisplay);
//    MFX_DESTROY_VABUFFER(m_packedAudBufferId, m_vaDisplay);
//    MFX_DESTROY_VABUFFER(m_packedSpsHeaderBufferId, m_vaDisplay);
//    MFX_DESTROY_VABUFFER(m_packedSpsBufferId, m_vaDisplay);
//    MFX_DESTROY_VABUFFER(m_packedPpsHeaderBufferId, m_vaDisplay);
//    MFX_DESTROY_VABUFFER(m_packedPpsBufferId, m_vaDisplay);
//    MFX_DESTROY_VABUFFER(m_packedSeiHeaderBufferId, m_vaDisplay);
//    MFX_DESTROY_VABUFFER(m_packedSeiBufferId, m_vaDisplay);

    if (m_allocResponseMB.NumFrameActual != 0)
    {
        m_core->FreeFrames(&m_allocResponseMB);
        Zero(m_allocResponseMB);
    }
    if (m_allocResponseBS.NumFrameActual != 0)
    {
        m_core->FreeFrames(&m_allocResponseBS);
        Zero(m_allocResponseBS);
    }        
    return MFX_ERR_NONE;
} // mfxStatus VAAPIEncoder::Close()


mfxStatus VAAPIEncoder::FillMBBufferPointer(ExecuteBuffers* pExecuteBuffers)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "CopyMB");
    mfxStatus sts = MFX_ERR_NONE;
    mfxFrameData Frame = {};

    //pExecuteBuffers->m_idxMb = 0;
    if (pExecuteBuffers->m_idxMb >= DWORD(m_allocResponseMB.NumFrameActual))
    {
        return MFX_ERR_UNSUPPORTED;
    }

    Frame.MemId = m_allocResponseMB.mids[pExecuteBuffers->m_idxMb];
#ifdef MPEG2_ENC_HW_PERF
    if (pExecuteBuffers->m_pps.picture_coding_type == CODING_TYPE_I)
    {
        vm_time_start (0,&lock_MB_data_time[0]);
    }
    else if (pExecuteBuffers->m_pps.picture_coding_type == CODING_TYPE_P)
    {
        vm_time_start (0,&lock_MB_data_time[1]);        
    }
    else if (pExecuteBuffers->m_pps.picture_coding_type == CODING_TYPE_B)
    {
        vm_time_start (0,&lock_MB_data_time[2]);        
    }
#endif
    sts = m_core->LockFrame(Frame.MemId,&Frame);
    MFX_CHECK_STS(sts);

#ifdef MPEG2_ENC_HW_PERF
    if (pExecuteBuffers->m_pps.picture_coding_type == CODING_TYPE_I)
    {
        vm_time_stop (0,&lock_MB_data_time[0]);
    }
    else if (pExecuteBuffers->m_pps.picture_coding_type == CODING_TYPE_P)
    {
        vm_time_stop (0,&lock_MB_data_time[1]);        
    }
    else if (pExecuteBuffers->m_pps.picture_coding_type == CODING_TYPE_B)
    {
        vm_time_stop (0,&lock_MB_data_time[2]);        
    }
#endif
    int numMB = 0;
    for (int i = 0; i<(int)pExecuteBuffers->m_pps.NumSlice;i++)
    {
        numMB = numMB + (int)pExecuteBuffers->m_pSlice[i].NumMbsForSlice;
    }
    //{
    //    FILE* f = fopen("MB data.txt","ab+");
    //    fwrite(Frame.Y,1,Frame.Pitch*numMB,f);
    //    fclose(f);
    //}

    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "CopyMBData");
#if 0

        for (mfxI32 i = 0; i < numMB; i++)
        {
            memcpy(
                pExecuteBuffers->m_pMBs + i,
                Frame.Y + m_layout.MB_CODE_offset + m_layout.MB_CODE_stride * i,
                sizeof(ENCODE_ENC_MB_DATA_MPEG2));
        }



        //memcpy(pExecuteBuffers->m_pMBs, Frame.Y, numMB * sizeof(ENCODE_ENC_MB_DATA_MPEG2));
#else
#ifdef MPEG2_ENC_HW_PERF
        if (pExecuteBuffers->m_pps.picture_coding_type == CODING_TYPE_I)
        {
            vm_time_start (0,&copy_MB_data_time[0]);
        }
        else if (pExecuteBuffers->m_pps.picture_coding_type == CODING_TYPE_P)
        {
            vm_time_start (0,&copy_MB_data_time[1]);        
        }
        else if (pExecuteBuffers->m_pps.picture_coding_type == CODING_TYPE_B)
        {
            vm_time_start (0,&copy_MB_data_time[2]);        
        }
#endif
        mfxFrameSurface1 src = {};
        mfxFrameSurface1 dst = {};

        src.Data        = Frame;
        src.Data.Y     += m_layout.MB_CODE_offset;
        src.Data.Pitch  = mfxU16(m_layout.MB_CODE_stride);
        src.Info.Width  = mfxU16(sizeof(ENCODE_ENC_MB_DATA_MPEG2));
        src.Info.Height = mfxU16(numMB);
        src.Info.FourCC = MFX_FOURCC_P8;
        dst.Data.Y      = (mfxU8 *)pExecuteBuffers->m_pMBs;
        dst.Data.Pitch  = mfxU16(sizeof(ENCODE_ENC_MB_DATA_MPEG2));
        dst.Info.Width  = mfxU16(sizeof(ENCODE_ENC_MB_DATA_MPEG2));
        dst.Info.Height = mfxU16(numMB);
        dst.Info.FourCC = MFX_FOURCC_P8;

        sts = m_core->DoFastCopy(&dst, &src);
        MFX_CHECK_STS(sts);
#ifdef MPEG2_ENC_HW_PERF
        if (pExecuteBuffers->m_pps.picture_coding_type == CODING_TYPE_I)
        {
            vm_time_stop (0,&copy_MB_data_time[0]);
        }
        else if (pExecuteBuffers->m_pps.picture_coding_type == CODING_TYPE_P)
        {
            vm_time_stop (0,&copy_MB_data_time[1]);        
        }
        else if (pExecuteBuffers->m_pps.picture_coding_type == CODING_TYPE_B)
        {
            vm_time_stop (0,&copy_MB_data_time[2]);        
        }
#endif
#endif
    }

    sts = m_core->UnlockFrame(Frame.MemId);
    MFX_CHECK_STS(sts);

    return sts;

} // mfxStatus VAAPIEncoder::FillMBBufferPointer(ExecuteBuffers* pExecuteBuffers)


mfxStatus VAAPIEncoder::FillBSBuffer(mfxU32 nFeedback,mfxU32 nBitstream, mfxBitstream* pBitstream, Encryption *pEncrypt)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "FillBSBuffer");

    mfxStatus sts = MFX_ERR_NONE;
    mfxFrameData Frame = {};
    
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc QueryStatus");
    VAStatus vaSts;

    //------------------------------------------
    // (1) mapping feedbackNumber -> surface & bs
    bool isFound = false;
    VASurfaceID waitSurface;
    mfxU32 waitIdxBs;
    mfxU32 indxSurf;
    mfxU32 bitstreamSize = 0;

    //UMC::AutomaticUMCMutex guard(m_guard);

    for (indxSurf = 0; indxSurf < m_feedback.size(); indxSurf++)
    {
        ExtVASurface currentFeedback = m_feedback[ indxSurf ];

        if (currentFeedback.number == nFeedback)
        {
            waitSurface = currentFeedback.surface;
            waitIdxBs   = currentFeedback.idxBs;
            isFound  = true;

            break;
        }
    }

    if( !isFound )
    {
        return MFX_ERR_UNKNOWN;
    }

    // find used bitstream
    VABufferID codedBuffer;
    if( waitIdxBs < m_bsQueue.size())
    {
        codedBuffer = m_bsQueue[waitIdxBs].surface;
    }
    else
    {
        return MFX_ERR_UNKNOWN;
    }

    VASurfaceStatus surfSts = VASurfaceSkipped;

    vaSts = vaQuerySurfaceStatus(m_vaDisplay, waitSurface, &surfSts);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    switch (surfSts)
    {
    case VASurfaceReady:
        break;
        // return MFX_ERR_NONE;

    case VASurfaceRendering:
    case VASurfaceDisplaying:
        return MFX_WRN_DEVICE_BUSY;

    case VASurfaceSkipped:
    default:
        assert(!"bad feedback status");
        return MFX_ERR_DEVICE_FAILED;
    }



    Frame.MemId = m_allocResponseBS.mids[nBitstream];
    sts = m_core->LockFrame(Frame.MemId, &Frame);
    MFX_CHECK_STS(sts);


#ifdef PAVP_SUPPORT
    if (pEncrypt->m_bEncryptionMode)
    {
        MFX_CHECK_NULL_PTR1(pBitstream->EncryptedData);
        MFX_CHECK_NULL_PTR1(pBitstream->EncryptedData->Data);
        MFX_CHECK(pBitstream->EncryptedData->DataLength + pBitstream->EncryptedData->DataOffset + queryStatus.bitstreamSize < pBitstream->EncryptedData->MaxLength, MFX_ERR_NOT_ENOUGH_BUFFER);
        memcpy_s(pBitstream->EncryptedData->Data + pBitstream->EncryptedData->DataLength + pBitstream->EncryptedData->DataOffset, pBitstream->EncryptedData->MaxLength, Frame.Y, queryStatus.bitstreamSize);
        pBitstream->EncryptedData->DataLength += queryStatus.bitstreamSize;
        pBitstream->EncryptedData->CipherCounter.IV = pEncrypt->m_aesCounter.IV;
        pBitstream->EncryptedData->CipherCounter.Count = pEncrypt->m_aesCounter.Count;
        pEncrypt->m_aesCounter.Increment();
        if (pEncrypt->m_aesCounter.IsWrapped())
        {
            pEncrypt->m_aesCounter.ResetWrappedFlag();
        }            
    }
    else
#endif
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_INTERNAL, "CopyBitsream");

        VACodedBufferSegment *codedBufferSegment;
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc vaMapBuffer");
            vaSts = vaMapBuffer(
                m_vaDisplay,
                codedBuffer,
                (void **)(&codedBufferSegment));
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        }
        bitstreamSize = codedBufferSegment->size;
        //pBitstream->DataLength = codedBufferSegment->size;
        //task.m_bsDataLength[fieldId] = codedBufferSegment->size;

        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc vaUnmapBuffer");
            vaUnmapBuffer( m_vaDisplay, codedBuffer );
        }

        // remove task
        m_feedback.erase( m_feedback.begin() + indxSurf );

        MFX_CHECK(pBitstream->DataLength + pBitstream->DataOffset + bitstreamSize < pBitstream->MaxLength, MFX_ERR_NOT_ENOUGH_BUFFER);

        IppiSize roi = {bitstreamSize, 1};

        IppStatus ret = ippStsNoErr;

        ret = ippiCopyManaged_8u_C1R(Frame.Y, bitstreamSize,
            pBitstream->Data + pBitstream->DataLength + pBitstream->DataOffset, 
            bitstreamSize,
            roi, IPP_NONTEMPORAL_LOAD);

        MFX_CHECK(ret == ippStsNoErr, MFX_ERR_UNDEFINED_BEHAVIOR);

        //memcpy(pBitstream->Data + pBitstream->DataLength + pBitstream->DataOffset, Frame.Y, queryStatus.bitstreamSize);
        pBitstream->DataLength += bitstreamSize;
    }


    sts = m_core->UnlockFrame(Frame.MemId);
    MFX_CHECK_STS(sts);

    return sts;
} // mfxStatus VAAPIEncoder::FillBSBuffer(mfxU32 nFeedback,mfxU32 nBitstream, mfxBitstream* pBitstream, Encryption *pEncrypt)

#endif // (MFX_ENABLE_MPEG2_VIDEO_ENCODE_HW) && (MFX_VA_LINUX)
/* EOF */
