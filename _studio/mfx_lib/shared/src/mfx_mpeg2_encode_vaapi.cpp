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


    void FillSps(
        MfxHwMpeg2Encode::ExecuteBuffers* pExecuteBuffers,
        VAEncSequenceParameterBufferMPEG2 & sps)
    {
        assert(pExecuteBuffers);
        const ENCODE_SET_SEQUENCE_PARAMETERS_MPEG2 & winSps = pExecuteBuffers->m_sps;

        sps.picture_width   = winSps.FrameWidth;
        sps.picture_height  = winSps.FrameHeight;
        sps.frame_rate      = winSps.FrameRateCode;
        sps.intra_period    = pExecuteBuffers->m_GOPPictureSize;
        sps.ip_period       = pExecuteBuffers->m_GOPRefDist;
        sps.bits_per_second = winSps.bit_rate;

        sps.vbv_buffer_size = 3; // B = 16 * 1024 * vbv_buffer_size

        int profile = 4, level = 8;

        switch (ConvertProfileTypeMFX2VAAPI(winSps.Profile)) {
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

        switch (winSps.Level) {
        case MFX_LEVEL_MPEG2_LOW:
            level = 10;
            break;
        case MFX_LEVEL_MPEG2_MAIN:
            level = 8;
            break;
        case MFX_LEVEL_MPEG2_HIGH:
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
        // sps.sequence_extension.bits.progressive_sequence = 0;
        sps.sequence_extension.bits.low_delay = 0; // FIXME

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

        pps.picture_type = ConvertCodingTypeMFX2VAAPI(winPps.picture_coding_type);
        pps.temporal_reference = winPps.temporal_reference;

//         m_CurrFrameMemID        = curr;
//         m_bExternalCurrFrame    = bExternal;
//         m_RecFrameMemID         = rec;
//         m_RefFrameMemID[0]      = ref_0;
//         m_RefFrameMemID[1]      = ref_1;

        pps.reconstructed_picture = ConvertSurfaceIdMFX2VAAPI(core, pExecuteBuffers->m_RecFrameMemID);
        // pps.forward_reference_picture = ConvertSurfaceIdMFX2VAAPI(core, pExecuteBuffers->m_RefFrameMemID[0]);
        // pps.backward_reference_picture = ConvertSurfaceIdMFX2VAAPI(core, pExecuteBuffers->m_RefFrameMemID[1]);

        pps.coded_buf = VA_INVALID_ID;        
        
        pps.f_code[0][0] = 0xf;
        pps.f_code[0][1] = 0xf;
        pps.f_code[1][0] = 0xf;
        pps.f_code[1][1] = 0xf;

        pps.picture_coding_extension.bits.intra_dc_precision = 0; /* 8bits */
        pps.picture_coding_extension.bits.picture_structure = 3; /* frame picture */
        pps.picture_coding_extension.bits.top_field_first = 0; 
        pps.picture_coding_extension.bits.frame_pred_frame_dct = 1; /* FIXME */
        pps.picture_coding_extension.bits.concealment_motion_vectors = 0;
        pps.picture_coding_extension.bits.q_scale_type = 0;
        pps.picture_coding_extension.bits.intra_vlc_format = 0;
        pps.picture_coding_extension.bits.alternate_scan = 0;
        pps.picture_coding_extension.bits.repeat_first_field = 0;
        pps.picture_coding_extension.bits.progressive_frame = 1;
        pps.picture_coding_extension.bits.composite_display_flag = 0;

        if (pps.picture_type == VAEncPictureTypeIntra) {
            pps.f_code[0][0] = 0xf;
            pps.f_code[0][1] = 0xf;
            pps.f_code[1][0] = 0xf;
            pps.f_code[1][1] = 0xf;
            pps.forward_reference_picture = VA_INVALID_SURFACE;
            pps.backward_reference_picture = VA_INVALID_SURFACE;
        } else if (pps.picture_type == VAEncPictureTypePredictive) {
            pps.f_code[0][0] = 0x1;
            pps.f_code[0][1] = 0x1;
            pps.f_code[1][0] = 0xf;
            pps.f_code[1][1] = 0xf;
            pps.forward_reference_picture = ConvertSurfaceIdMFX2VAAPI(core, pExecuteBuffers->m_RefFrameMemID[0]);
            pps.backward_reference_picture = VA_INVALID_SURFACE;
        } else if (pps.picture_type == VAEncPictureTypeBidirectional) {
            pps.f_code[0][0] = 0x1;
            pps.f_code[0][1] = 0x1;
            pps.f_code[1][0] = 0x1;
            pps.f_code[1][1] = 0x1;
            pps.forward_reference_picture = ConvertSurfaceIdMFX2VAAPI(core, pExecuteBuffers->m_RefFrameMemID[0]);
            pps.backward_reference_picture = ConvertSurfaceIdMFX2VAAPI(core, pExecuteBuffers->m_RefFrameMemID[1]);
        } else {
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
    , m_vaContextEncode(0)
    , m_vaConfig(0)
    , m_spsBufferId(VA_INVALID_ID)
    , m_ppsBufferId(VA_INVALID_ID)
    , m_qmBufferId(VA_INVALID_ID)
    , m_numSliceGroups(0)
    , m_codedbufBufferId(VA_INVALID_ID)
    , m_codedbufISize(0)
    , m_codedbufPBSize(0)
{
    Zero(m_vaSpsBuf);
    Zero(m_vaPpsBuf);
    Zero(m_sliceParam, MAX_SLICES);
    std::fill(m_sliceParamBufferId, m_sliceParamBufferId + MAX_SLICES, VA_INVALID_ID);
    Zero(m_allocResponseMB);
    Zero(m_allocResponseBS);
    memset(&m_recFrames, 0, sizeof(mfxRecFrames));
    memset(&m_rawFrames, 0, sizeof(mfxRawFrames));
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

    caps.SliceIPBOnly     = 1;
    caps.EncodeFunc       = 1;
    caps.MaxNum_Reference = 1;
    caps.MaxPicWidth      = 1920;
    caps.MaxPicHeight     = 1088;
    caps.NoInterlacedField = 1; // disable interlaced encoding
    caps.BRCReset         = 1; // no bitrate resolution control
    caps.VCMBitrateControl = 0; //Video conference mode
    caps.HeaderInsertion  = 0; // we will privide headers (SPS, PPS) in binary format to the driver
    caps.SliceStructure   = 1; // 1 - SliceDividerSnb; 2 - SliceDividerHsw; 3 - SliceDividerBluRay; the other - SliceDividerOneSlice

        
    return MFX_ERR_NONE;
} // mfxStatus VAAPIEncoder::QueryEncodeCaps(ENCODE_CAPS & caps)

mfxStatus VAAPIEncoder::Init(ExecuteBuffers* pExecuteBuffers, mfxU32 numRefFrames, mfxU32 funcId)
{
    mfxStatus sts = MFX_ERR_UNSUPPORTED; 
    assert(ENCODE_ENC_PAK_ID == funcId);
    
    sts = Init(/*DXVA2_Intel_Encode_MPEG2, */ENCODE_ENC_PAK, pExecuteBuffers);
    MFX_CHECK_STS(sts);

    sts = GetBuffersInfo();
    MFX_CHECK_STS(sts);

    sts = CreateBSBuffer(numRefFrames, pExecuteBuffers);
    MFX_CHECK_STS(sts);

    return sts;
} // mfxStatus VAAPIEncoder::Init(ExecuteBuffers* pExecuteBuffers, mfxU32 numRefFrames, mfxU32 funcId)

mfxStatus VAAPIEncoder::Init(ENCODE_FUNC func, ExecuteBuffers* pExecuteBuffers)
{
    mfxStatus   sts    = MFX_ERR_NONE;    

    mfxU16          width   = pExecuteBuffers->m_sps.FrameWidth;
    mfxU16          height  = pExecuteBuffers->m_sps.FrameHeight;
    memset (&m_rawFrames, 0, sizeof(mfxRawFrames));

    ExtVASurface cleanSurf = {0};
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

    std::vector<VASurfaceID> reconSurf;    
    for(int i = 0; i < m_recFrames.size(); i++)
        reconSurf.push_back(m_recFrames[i].surface);

    // Encoder create
    vaSts = vaCreateContext(
        m_vaDisplay,
        m_vaConfig,
        width,
        height,
        VA_PROGRESSIVE,
        &*reconSurf.begin(),
        reconSurf.size(),
        &m_vaContextEncode);
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


mfxStatus VAAPIEncoder::GetBuffersInfo()
{
    /*HRESULT hr = 0;
    ENCODE_FORMAT_COUNT encodeFormatCount = {0};
    GUID    curr_guid = m_pDevice->GetCurrentGuid();

    hr = m_pDevice->Execute(ENCODE_FORMAT_COUNT_ID, &curr_guid,sizeof(GUID),&encodeFormatCount,sizeof(encodeFormatCount));
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

    MFX_CHECK(encodeFormatCount.CompressedBufferInfoCount>0 && encodeFormatCount.CompressedBufferInfoCount>0, MFX_ERR_DEVICE_FAILED);

    m_compBufInfo.resize(encodeFormatCount.CompressedBufferInfoCount);
    m_uncompBufInfo.resize(encodeFormatCount.UncompressedFormatCount);

    ENCODE_FORMATS encodeFormats = {0};
    encodeFormats.CompressedBufferInfoSize  = encodeFormatCount.CompressedBufferInfoCount * sizeof(ENCODE_COMP_BUFFER_INFO);
    encodeFormats.UncompressedFormatSize    = encodeFormatCount.UncompressedFormatCount * sizeof(D3DDDIFORMAT);
    encodeFormats.pCompressedBufferInfo     = &m_compBufInfo[0];
    encodeFormats.pUncompressedFormats      = &m_uncompBufInfo[0];

    hr = m_pDevice->Execute(ENCODE_FORMATS_ID, 0,0,&encodeFormats,sizeof(encodeFormats));
    MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
    */
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
        pRequest->Info.Height = IPP_MAX(pRequest->Info.Height, pExecuteBuffers->m_sps.FrameHeight*3/2);
    }
    else if (type == D3DDDIFMT_INTELENCODE_BITSTREAMDATA)
    {       
        pRequest->Info.Width = IPP_MAX(pRequest->Info.Width, pExecuteBuffers->m_sps.FrameWidth);
        pRequest->Info.Height = IPP_MAX(pRequest->Info.Height, pExecuteBuffers->m_sps.FrameHeight*3/2);
//         pRequest->Info.FourCC = MFX_FOURCC_NV12;
//         pRequest->NumFrameMin = m_compBufInfo[i].NumBuffer;
//         pRequest->NumFrameSuggested = m_compBufInfo[i].NumBuffer;
    }
    else
    {
        assert(!"unknown buffer type");
    }
    
    m_codedbufISize = pExecuteBuffers->m_sps.FrameWidth*pExecuteBuffers->m_sps.FrameHeight*3/2;
    m_codedbufPBSize = 0;

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
    EmulSurfaceRegister     SurfaceReg;
    memset(&SurfaceReg, 0, sizeof(SurfaceReg));
    mfxStatus               sts         = MFX_ERR_NONE;;
    //HRESULT                 hr          = 0;

    SurfaceReg.type     = type;
    SurfaceReg.num_surf = pResponse->NumFrameActual;

    MFX_CHECK(pResponse->mids, MFX_ERR_NULL_PTR);

    for (int i = 0; i < pResponse->NumFrameActual; i++)
    {
        sts = m_core->GetFrameHDL(pResponse->mids[i], (mfxHDL *)&SurfaceReg.surface[i]);        
        MFX_CHECK_STS(sts);
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

    return MFX_ERR_NONE;

    return Register(pResponse, D3DDDIFMT_NV12);
} // mfxStatus VAAPIEncoder::RegisterRefFrames (const mfxFrameAllocResponse* pResponse)


mfxI32 VAAPIEncoder::GetRecFrameIndex (mfxMemId memID)
{
    mfxStatus sts;
    ExtVASurface extSurf;
    VASurfaceID *pSurface = NULL;
    sts = m_core->GetFrameHDL(memID, (mfxHDL *)&pSurface);
    MFX_CHECK_WITH_ASSERT(MFX_ERR_NONE == sts, -1);

    for (int i = 0; i < m_recFrames.size(); i++)
    {
        if (m_recFrames[i].surface == *pSurface)
            return i;    
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

    for (int i = 0; i < m_recFrames.size(); i++)
    {
        if (m_rawFrames[i].surface == *pSurface)
            return i;    
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
    mfxFrameAllocRequest request = {0};

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
    mfxFrameAllocRequest request = {0};

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
    mfxFrameAllocRequest request = {0};

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

    width_in_mbs = (m_vaSpsBuf.picture_width + 15) / 16;
    height_in_mbs = (m_vaSpsBuf.picture_height + 15) / 16;
    m_numSliceGroups = 1;

    for (i = 0; i < height_in_mbs; i++) {
        sliceParam = &m_sliceParam[i];
        sliceParam->macroblock_address = i * width_in_mbs;
        sliceParam->num_macroblocks = width_in_mbs;
        sliceParam->is_intra_slice = (m_vaPpsBuf.picture_type == VAEncPictureTypeIntra);
        sliceParam->quantiser_scale_code = /*ctx->qp*/ 8 / 2; // TODO: where find QP value ?
    }

    vaSts = vaCreateBuffer(m_vaDisplay,
        m_vaContextEncode,
        VAEncSliceParameterBufferType,
        sizeof(*sliceParam),
        height_in_mbs,
        m_sliceParam,
        m_sliceParamBufferId);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    return MFX_ERR_NONE;
}

mfxStatus VAAPIEncoder::Execute(ExecuteBuffers* pExecuteBuffers, mfxU32 funcId, mfxU8 *pUserData, mfxU32 userDataLen)
{
    const mfxU32    NumCompBuffer = 10;

//     ENCODE_COMPBUFFERDESC   encodeCompBufferDesc[NumCompBuffer] = {0};
//     ENCODE_PACKEDHEADER_DATA payload = {0};
// 
//     ENCODE_EXECUTE_PARAMS encodeExecuteParams = {0};
//     encodeExecuteParams.pCompressedBuffers = encodeCompBufferDesc;

    VAStatus vaSts;

    //VASurfaceID *inputSurface = (VASurfaceID*)surface;
    //VASurfaceID reconSurface;
    //VABufferID codedBuffer;
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

            configBuffers[buffersCount++] = m_qmBufferId;
        }
    }

    // TODO: fill pps only once
    Zero(m_vaPpsBuf);    
    FillPps(m_core, pExecuteBuffers, m_vaPpsBuf);

    /*
    encodeCompBufferDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTELENCODE_SLICEDATA;
    encodeCompBufferDesc[bufCnt].DataSize = sizeof(*pExecuteBuffers->m_pSlice) * pExecuteBuffers->m_pps.NumSlice;
    encodeCompBufferDesc[bufCnt].pCompBuffer = pExecuteBuffers->m_pSlice;
    bufCnt++;*/

    mfxStatus mfxSts = FillSlices(pExecuteBuffers);
    MFX_CHECK(mfxSts == MFX_ERR_NONE, MFX_ERR_DEVICE_FAILED);

    MFX_DESTROY_VABUFFER(m_codedbufBufferId, m_vaDisplay);
    int codedbuf_size = (m_vaPpsBuf.picture_type == VAEncPictureTypeIntra)
        ? m_codedbufISize
        : m_codedbufPBSize;

    vaSts = vaCreateBuffer(m_vaDisplay,
        m_vaContextEncode,
        VAEncCodedBufferType,
        codedbuf_size,
        1,
        NULL,
        &m_codedbufBufferId);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    m_vaPpsBuf.coded_buf = m_codedbufBufferId;

    MFX_DESTROY_VABUFFER(m_ppsBufferId, m_vaDisplay);
    vaSts = vaCreateBuffer(m_vaDisplay,
        m_vaContextEncode,
        VAEncPictureParameterBufferType,
        sizeof(m_vaPpsBuf),
        1,
        &m_vaPpsBuf,
        &m_ppsBufferId);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    configBuffers[buffersCount++] = m_ppsBufferId;

//     if (funcId == ENCODE_ENC_PAK_ID)
//     {
    /*encodeCompBufferDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTELENCODE_BITSTREAMDATA;
    encodeCompBufferDesc[bufCnt].DataSize = sizeof(pExecuteBuffers->m_idxBs);
    encodeCompBufferDesc[bufCnt].pCompBuffer = &pExecuteBuffers->m_idxBs;
    bufCnt++;*/

    /*
    if (userDataLen>0 && pUserData)
    {

        payload.pData = pUserData;
        payload.DataLength = payload.BufferSize = userDataLen;

        encodeCompBufferDesc[bufCnt].CompressedBufferType = D3DDDIFMT_INTELENCODE_PACKEDHEADERDATA;
        encodeCompBufferDesc[bufCnt].DataSize       = sizeof (payload);
        encodeCompBufferDesc[bufCnt].pCompBuffer    = &payload;
        bufCnt++;        
    }*/

//     }
//     else
//     {
//         return MFX_ERR_UNSUPPORTED;
//     } 

    //------------------------------------------------------------------
    // Rendering
    //------------------------------------------------------------------
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_PRIVATE, "DDI_ENC");
        MFX_LTRACE_I(MFX_TRACE_LEVEL_PRIVATE, pExecuteBuffers->m_idxMb);


        vaSts = vaBeginPicture(m_vaDisplay,
            m_vaContextEncode,
            /*pExecuteBuffers->m_pSurface*/ConvertSurfaceIdMFX2VAAPI(m_core, pExecuteBuffers->m_pSurface));
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        vaSts = vaRenderPicture(m_vaDisplay,
            m_vaContextEncode,
            &*configBuffers.begin(),
            buffersCount);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        vaSts = vaRenderPicture(m_vaDisplay,
            m_vaContextEncode,
            &m_sliceParamBufferId[0],
            m_numSliceGroups);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        vaSts = vaEndPicture(m_vaDisplay, m_vaContextEncode);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        //hr = m_pDevice->BeginFrame((IDirect3DSurface9 *)pExecuteBuffers->m_pSurface,0);
        //MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);

        //printf("nExecute %d\n", pExecuteBuffers->m_pps.StatusReportFeedbackNumber);
        //hr = m_pDevice->Execute(funcId, &encodeExecuteParams,sizeof(encodeExecuteParams));
        //MFX_CHECK(SUCCEEDED(hr), MFX_ERR_DEVICE_FAILED);
    }

    //------------------------------------------------------------------
    // PostStage
    //------------------------------------------------------------------
    // put to cache
    {
        // UMC::AutomaticUMCMutex guard(m_guard);
        ExtVASurface currentFeedback;
        currentFeedback.number  =  pExecuteBuffers->m_pps.StatusReportFeedbackNumber;//   task.m_statusReportNumber[fieldId];
        currentFeedback.surface = ConvertSurfaceIdMFX2VAAPI(m_core, pExecuteBuffers->m_pSurface);
        currentFeedback.idxBs    = pExecuteBuffers->m_idxBs;                         // task.m_idxBs[fieldId];
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

    if (m_vaContextEncode)
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
        m_vaContextEncode = 0;
    }

    if (m_vaConfig)
    {
        vaDestroyConfig(m_vaDisplay, m_vaConfig);
        m_vaConfig = 0;
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
    mfxFrameData Frame = {0};

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
        mfxFrameSurface1 src = {0};
        mfxFrameSurface1 dst = {0};

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
    mfxFrameData Frame = {0};
    
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

        return MFX_ERR_NONE;

    case VASurfaceRendering:
    case VASurfaceDisplaying:
        return MFX_WRN_DEVICE_BUSY;

    case VASurfaceSkipped:
    default:
        assert(!"bad feedback status");
        return MFX_ERR_DEVICE_FAILED;
    }



    Frame.MemId = m_allocResponseBS.mids[nBitstream];
    sts = m_core->LockFrame(Frame.MemId,&Frame);
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


/*
// (mfxExecuteBuffers& data)
mfxStatus VAAPIEncoder::Execute(
    mfxHDL          surface,
    DdiTask const & task,
    mfxU32          fieldId,
    PreAllocatedVector const & sei)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc Execute");

    VAStatus vaSts;

    VASurfaceID *inputSurface = (VASurfaceID*)surface;
    VASurfaceID reconSurface;
    VABufferID codedBuffer;
    mfxU32 i;
    VAEncPackedHeaderParameterBuffer packed_header_param_buffer;

    std::vector<VABufferID> configBuffers;
    configBuffers.resize(MAX_CONFIG_BUFFERS_COUNT + m_slice.size()*2);
    mfxU16 buffersCount = 0;

    // update params
    UpdatePPS(task, fieldId, m_pps, m_reconQueue);
    UpdateSlice(m_caps, task, fieldId, m_sps, m_pps, m_slice, m_videoParam, m_reconQueue);

    //------------------------------------------------------------------
    // find bitstream
    mfxU32 idxBs = task.m_idxBs[fieldId];
    if( idxBs < m_bsQueue.size() )
    {
        codedBuffer = m_bsQueue[idxBs].surface;
    }
    else
    {
        return MFX_ERR_UNKNOWN;
    }

    // find reconstructed surface
    int idxRecon = task.m_idxRecon;
    if( idxRecon < m_reconQueue.size())
    {
        reconSurface = m_reconQueue[ idxRecon ].surface;
    }
    else
    {
        return MFX_ERR_UNKNOWN;
    }

    m_pps.coded_buf = codedBuffer;
    m_pps.reconstructed_picture = reconSurface;

    //------------------------------------------------------------------
    // buffer creation & configuration
    //------------------------------------------------------------------
    {
        // 1. sequence level
        {
            MFX_DESTROY_VABUFFER(m_spsBufferId, m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                VAEncSequenceParameterBufferType,
                sizeof(m_sps),
                1,
                &m_sps,
                &m_spsBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            configBuffers[buffersCount++] = m_spsBufferId;
        }

        // 2. Picture level
        {
            MFX_DESTROY_VABUFFER(m_ppsBufferId, m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                VAEncPictureParameterBufferType,
                sizeof(m_pps),
                1,
                &m_pps,
                &m_ppsBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            configBuffers[buffersCount++] = m_ppsBufferId;
        }

        // 3. Slice level
        for( i = 0; i < m_slice.size(); i++ )
        {
            MFX_DESTROY_VABUFFER(m_sliceBufferId[i], m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                VAEncSliceParameterBufferType,
                sizeof(m_slice[i]),
                1,
                &m_slice[i],
                &m_sliceBufferId[i]);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        }
    }

    if (m_caps.HeaderInsertion == 1)
    {
        // SEI
        if (sei.Size() > 0)
        {
            packed_header_param_buffer.type = VAEncPackedHeaderH264_SEI;
            packed_header_param_buffer.has_emulation_bytes = 1;
            packed_header_param_buffer.bit_length = sei.Size()*8;

            MFX_DESTROY_VABUFFER(m_packedSeiHeaderBufferId, m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                VAEncPackedHeaderParameterBufferType,
                sizeof(packed_header_param_buffer),
                1,
                &packed_header_param_buffer,
                &m_packedSeiHeaderBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            MFX_DESTROY_VABUFFER(m_packedSeiBufferId, m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                VAEncPackedHeaderDataBufferType,
                sei.Size(), 1, RemoveConst(sei.Buffer()),
                &m_packedSeiBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            configBuffers[buffersCount++] = m_packedSeiHeaderBufferId;
            configBuffers[buffersCount++] = m_packedSeiBufferId;
        }
    }
    else
    {
        // AUD
        if (task.m_insertAud[fieldId])
        {
            ENCODE_PACKEDHEADER_DATA const & packedAud = m_headerPacker.PackAud(task, fieldId);

            packed_header_param_buffer.type = VAEncPackedHeaderRawData;
            packed_header_param_buffer.has_emulation_bytes = !packedAud.SkipEmulationByteCount;
            packed_header_param_buffer.bit_length = packedAud.DataLength*8;

            MFX_DESTROY_VABUFFER(m_packedAudHeaderBufferId, m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                VAEncPackedHeaderParameterBufferType,
                sizeof(packed_header_param_buffer),
                1,
                &packed_header_param_buffer,
                &m_packedAudHeaderBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            MFX_DESTROY_VABUFFER(m_packedAudBufferId, m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                VAEncPackedHeaderDataBufferType,
                packedAud.DataLength, 1, packedAud.pData,
                &m_packedAudBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            configBuffers[buffersCount++] = m_packedAudHeaderBufferId;
            configBuffers[buffersCount++] = m_packedAudBufferId;
        }
        // SPS
        if (task.m_insertSps[fieldId])
        {
            std::vector<ENCODE_PACKEDHEADER_DATA> const & packedSpsArray = m_headerPacker.GetSps();
            ENCODE_PACKEDHEADER_DATA const & packedSps = packedSpsArray[0];

            packed_header_param_buffer.type = VAEncPackedHeaderSequence;
            packed_header_param_buffer.has_emulation_bytes = !packedSps.SkipEmulationByteCount;
            packed_header_param_buffer.bit_length = packedSps.DataLength*8;

            MFX_DESTROY_VABUFFER(m_packedSpsHeaderBufferId, m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                VAEncPackedHeaderParameterBufferType,
                sizeof(packed_header_param_buffer),
                1,
                &packed_header_param_buffer,
                &m_packedSpsHeaderBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            MFX_DESTROY_VABUFFER(m_packedSpsBufferId, m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                VAEncPackedHeaderDataBufferType,
                packedSps.DataLength, 1, packedSps.pData,
                &m_packedSpsBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            configBuffers[buffersCount++] = m_packedSpsHeaderBufferId;
            configBuffers[buffersCount++] = m_packedSpsBufferId;
        }

        // PPS
        std::vector<ENCODE_PACKEDHEADER_DATA> const & packedPpsArray = m_headerPacker.GetPps();
        ENCODE_PACKEDHEADER_DATA const & packedPps = packedPpsArray[0];

        packed_header_param_buffer.type = VAEncPackedHeaderPicture;
        packed_header_param_buffer.has_emulation_bytes = !packedPps.SkipEmulationByteCount;
        packed_header_param_buffer.bit_length = packedPps.DataLength*8;

        MFX_DESTROY_VABUFFER(m_packedPpsHeaderBufferId, m_vaDisplay);
        vaSts = vaCreateBuffer(m_vaDisplay,
            m_vaContextEncode,
            VAEncPackedHeaderParameterBufferType,
            sizeof(packed_header_param_buffer),
            1,
            &packed_header_param_buffer,
            &m_packedPpsHeaderBufferId);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        MFX_DESTROY_VABUFFER(m_packedPpsBufferId, m_vaDisplay);
        vaSts = vaCreateBuffer(m_vaDisplay,
            m_vaContextEncode,
            VAEncPackedHeaderDataBufferType,
            packedPps.DataLength, 1, packedPps.pData,
            &m_packedPpsBufferId);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        configBuffers[buffersCount++] = m_packedPpsHeaderBufferId;
        configBuffers[buffersCount++] = m_packedPpsBufferId;

        // SEI
        if (sei.Size() > 0)
        {
            packed_header_param_buffer.type = VAEncPackedHeaderH264_SEI;
            packed_header_param_buffer.has_emulation_bytes = 1;
            packed_header_param_buffer.bit_length = sei.Size()*8;

            MFX_DESTROY_VABUFFER(m_packedSeiHeaderBufferId, m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                VAEncPackedHeaderParameterBufferType,
                sizeof(packed_header_param_buffer),
                1,
                &packed_header_param_buffer,
                &m_packedSeiHeaderBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            MFX_DESTROY_VABUFFER(m_packedSeiBufferId, m_vaDisplay);
            vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                VAEncPackedHeaderDataBufferType,
                sei.Size(), 1, RemoveConst(sei.Buffer()),
                &m_packedSeiBufferId);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            configBuffers[buffersCount++] = m_packedSeiHeaderBufferId;
            configBuffers[buffersCount++] = m_packedSeiBufferId;
        }

        //Slices
        if (m_core->GetHWType() >= MFX_HW_HSW)
        {
            std::vector<ENCODE_PACKEDHEADER_DATA> const & packedSlices = m_headerPacker.PackSlices(task, fieldId);
            for (size_t i = 0; i < packedSlices.size(); i++)
            {
                ENCODE_PACKEDHEADER_DATA const & packedSlice = packedSlices[i];

                packed_header_param_buffer.type = VAEncPackedHeaderH264_Slice;
                packed_header_param_buffer.has_emulation_bytes = !packedSlice.SkipEmulationByteCount;
                packed_header_param_buffer.bit_length = packedSlice.DataLength; // DataLength is already in bits !

                MFX_DESTROY_VABUFFER(m_packeSliceHeaderBufferId[i], m_vaDisplay);
                vaSts = vaCreateBuffer(m_vaDisplay,
                    m_vaContextEncode,
                    VAEncPackedHeaderParameterBufferType,
                    sizeof(packed_header_param_buffer),
                    1,
                    &packed_header_param_buffer,
                    &m_packeSliceHeaderBufferId[i]);
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

                MFX_DESTROY_VABUFFER(m_packedSliceBufferId[i], m_vaDisplay);
                vaSts = vaCreateBuffer(m_vaDisplay,
                    m_vaContextEncode,
                    VAEncPackedHeaderDataBufferType,
                    (packedSlice.DataLength + 7) / 8, 1, packedSlice.pData,
                    &m_packedSliceBufferId[i]);
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

                configBuffers[buffersCount++] = m_packeSliceHeaderBufferId[i];
                configBuffers[buffersCount++] = m_packedSliceBufferId[i];
            }
        }
    }

    configBuffers[buffersCount++] = m_hrdBufferId;
    configBuffers[buffersCount++] = m_rateParamBufferId;
    configBuffers[buffersCount++] = m_frameRateId;
    if (VA_INVALID_ID != m_privateParamsId) configBuffers[buffersCount++] = m_privateParamsId;

    assert(buffersCount <= configBuffers.size());

    //------------------------------------------------------------------
    // Rendering
    //------------------------------------------------------------------
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc vaBeginPicture");
        vaSts = vaBeginPicture(
            m_vaDisplay,
            m_vaContextEncode,
            *inputSurface);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc vaRenderPicture");
        vaSts = vaRenderPicture(
            m_vaDisplay,
            m_vaContextEncode,
            Begin(configBuffers),
            buffersCount);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        for( i = 0; i < m_slice.size(); i++)
        {
            vaSts = vaRenderPicture(
                m_vaDisplay,
                m_vaContextEncode,
                &m_sliceBufferId[i],
                1);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        }
    }
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc vaEndPicture");
        vaSts = vaEndPicture(m_vaDisplay, m_vaContextEncode);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }

    //------------------------------------------------------------------
    // PostStage
    //------------------------------------------------------------------
    // put to cache
    {
        UMC::AutomaticUMCMutex guard(m_guard);

        ExtVASurface currentFeedback;
        currentFeedback.number  = task.m_statusReportNumber[fieldId];
        currentFeedback.surface = *inputSurface;
        currentFeedback.idxBs    = task.m_idxBs[fieldId];
        m_feedbackCache.push_back( currentFeedback );
    }

    return MFX_ERR_NONE;
}
*/

#endif // (MFX_ENABLE_MPEG2_VIDEO_ENCODE_HW) && (MFX_VA_LINUX)
/* EOF */
