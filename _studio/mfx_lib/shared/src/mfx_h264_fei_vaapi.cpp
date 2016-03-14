/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2015-2016 Intel Corporation. All Rights Reserved.
//
//
//          H264 FEI VAAPI encoder
//
*/

#include "mfx_common.h"

#if defined(MFX_ENABLE_H264_VIDEO_ENCODE_HW) && defined(MFX_VA_LINUX)

#include <va/va.h>
#include <va/va_enc.h>
#include <va/va_enc_h264.h>
#include "mfxfei.h"
#include "libmfx_core_vaapi.h"
#include "mfx_h264_encode_vaapi.h"
#include "mfx_h264_encode_hw_utils.h"

#if defined(MFX_ENABLE_H264_VIDEO_FEI_PREENC)
//#include <va/vendor/va_intel_statistics.h>
#include "mfx_h264_preenc.h"
#endif // #if defined(MFX_ENABLE_H264_VIDEO_FEI_PREENC)

#if defined(_DEBUG)
#define mdprintf fprintf
//#define mdprintf(...)
#else
#define mdprintf(...)
#endif

using namespace MfxHwH264Encode;

#if defined(MFX_ENABLE_H264_VIDEO_FEI_PREENC)

VAAPIFEIPREENCEncoder::VAAPIFEIPREENCEncoder()
: VAAPIEncoder()
,m_codingFunction(0)
,m_statParamsId(VA_INVALID_ID)
{
} // VAAPIEncoder::VAAPIEncoder(VideoCORE* core)

VAAPIFEIPREENCEncoder::~VAAPIFEIPREENCEncoder()
{

    Destroy();

} // VAAPIEncoder::~VAAPIEncoder()

mfxStatus VAAPIFEIPREENCEncoder::Destroy()
{

    mfxStatus sts = MFX_ERR_NONE;

    if (VA_INVALID_ID != m_statParamsId)
        MFX_DESTROY_VABUFFER(m_statParamsId, m_vaDisplay);

    for( mfxU32 i = 0; i < m_statMVId.size(); i++ )
    {
        MFX_DESTROY_VABUFFER(m_statMVId[i], m_vaDisplay);
    }

    for( mfxU32 i = 0; i < m_statOutId.size(); i++ )
    {
        MFX_DESTROY_VABUFFER(m_statOutId[i], m_vaDisplay);
    }

    sts = VAAPIEncoder::Destroy();

    return sts;

} // VAAPIEncoder::~VAAPIEncoder()


mfxStatus VAAPIFEIPREENCEncoder::CreateAccelerationService(MfxVideoParam const & par)
{
    mfxStatus mfxSts = MFX_ERR_INVALID_VIDEO_PARAM;
    m_videoParam = par;
    m_codingFunction = 0; //Unknown function
    bool isFEI = false;
    const mfxExtFeiParam* params = GetExtBuffer(par);
    if (NULL == params)
        return MFX_ERR_INVALID_VIDEO_PARAM;
    else
    {
        isFEI = true;
        m_codingFunction = params->Func;
        if (MFX_FEI_FUNCTION_PREENC == m_codingFunction)
            mfxSts = CreatePREENCAccelerationService(par);
    }

    return mfxSts;
} // mfxStatus VAAPIFEIPREENCEncoder::CreateAccelerationService(MfxVideoParam const & par)

mfxStatus VAAPIFEIPREENCEncoder::CreatePREENCAccelerationService(MfxVideoParam const & par)
{
    MFX_CHECK(m_vaDisplay, MFX_ERR_DEVICE_FAILED);
    VAStatus vaSts;

    mfxI32 entrypointsIndx = 0;
    mfxI32 numEntrypoints = vaMaxNumEntrypoints(m_vaDisplay);
    MFX_CHECK(numEntrypoints, MFX_ERR_DEVICE_FAILED);

    std::vector<VAEntrypoint> pEntrypoints(numEntrypoints);

    bool bEncodeEnable = false;
    //entry point for preENC
    vaSts = vaQueryConfigEntrypoints(
            m_vaDisplay,
            VAProfileNone, //specific for statistic
            Begin(pEntrypoints),
            &numEntrypoints);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    for (entrypointsIndx = 0; entrypointsIndx < numEntrypoints; entrypointsIndx++) {
        if (VAEntrypointStatisticsIntel == pEntrypoints[entrypointsIndx]) {
            bEncodeEnable = true;
            break;
        }
    }

    if (!bEncodeEnable)
        return MFX_ERR_DEVICE_FAILED;

    //check attributes of the configuration
    VAConfigAttrib attrib[2];
    //attrib[0].type = VAConfigAttribRTFormat;
    attrib[0].type = (VAConfigAttribType)VAConfigAttribStatisticsIntel;
    vaSts = vaGetConfigAttributes(m_vaDisplay,
            VAProfileNone,
            (VAEntrypoint)VAEntrypointStatisticsIntel,
            &attrib[0], 1);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    //if ((attrib[0].value & VA_RT_FORMAT_YUV420) == 0)
    //    return MFX_ERR_DEVICE_FAILED;

    //attrib[0].value = VA_RT_FORMAT_YUV420;

    //check statistic attributes
    VAConfigAttribValStatisticsIntel statVal;
    statVal.value = attrib[0].value;
    //temp to be removed
    // But lets leave for a while
    mdprintf(stderr,"Statistics attributes:\n");
    mdprintf(stderr,"max_past_refs: %d\n", statVal.bits.max_num_past_references);
    mdprintf(stderr,"max_future_refs: %d\n", statVal.bits.max_num_future_references);
    mdprintf(stderr,"num_outputs: %d\n", statVal.bits.num_outputs);
    mdprintf(stderr,"interlaced: %d\n\n", statVal.bits.interlaced);
    //attrib[0].value |= ((2 - 1/*MV*/ - 1/*mb*/) << 8);

    vaSts = vaCreateConfig(
            m_vaDisplay,
            VAProfileNone,
            (VAEntrypoint)VAEntrypointStatisticsIntel,
            &attrib[0],
            1,
            &m_vaConfig); //don't configure stat attribs
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    std::vector<VASurfaceID> rawSurf;
    for (unsigned int i = 0; i < m_reconQueue.size(); i++)
        rawSurf.push_back(m_reconQueue[i].surface);


    // Encoder create
    vaSts = vaCreateContext(
            m_vaDisplay,
            m_vaConfig,
            m_width,
            m_height,
            VA_PROGRESSIVE,
            Begin(rawSurf),
            rawSurf.size(),
            &m_vaContextEncode);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    m_statOutId.resize(2);
    m_statMVId.resize(2);
    for( int i = 0; i < 2; i++ )
        m_statOutId[i] = m_statMVId[i] = VA_INVALID_ID;

    Zero(m_sps);
    Zero(m_pps);
    Zero(m_slice);
    //------------------------------------------------------------------

    FillSps(par, m_sps);
    //SetPrivateParams(par, m_vaDisplay, m_vaContextEncode, m_privateParamsId);
    //FillConstPartOfPps(par, m_pps);

    return MFX_ERR_NONE;
}


mfxStatus VAAPIFEIPREENCEncoder::Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type)
{
    std::vector<ExtVASurface> * pQueue;
    mfxStatus sts;

    if (D3DDDIFMT_INTELENCODE_BITSTREAMDATA == type)
    {
        pQueue = &m_bsQueue;
    } else
    {
        pQueue = &m_reconQueue;
    }

    {
        // we should register allocated HW bitstreams and recon surfaces
        MFX_CHECK(response.mids, MFX_ERR_NULL_PTR);

        ExtVASurface extSurf;
        VASurfaceID *pSurface = NULL;

        for (mfxU32 i = 0; i < response.NumFrameActual; i++) {

            sts = m_core->GetFrameHDL(response.mids[i], (mfxHDL *) & pSurface);
            MFX_CHECK_STS(sts);

            extSurf.number = i;
            extSurf.surface = *pSurface;

            pQueue->push_back(extSurf);
        }
    }
#if 0
    if (D3DDDIFMT_INTELENCODE_BITSTREAMDATA != type) {
        sts = CreateAccelerationService(m_videoParam);
        MFX_CHECK_STS(sts);
    }
#endif

    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type)

mfxStatus VAAPIFEIPREENCEncoder::Execute(
        mfxHDL surface,
        DdiTask const & task,
        mfxU32 fieldId,
        PreAllocatedVector const & sei)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc Execute");

    mfxStatus mfxSts = MFX_ERR_NONE;
    VAStatus vaSts;
    VAPictureFEI past_ref;
    VAPictureFEI future_ref;
    VASurfaceID *inputSurface = (VASurfaceID*) surface;

    std::vector<VABufferID> configBuffers;
    configBuffers.resize(MAX_CONFIG_BUFFERS_COUNT + m_slice.size()*2);
    for (mfxU32 ii = 0; ii < configBuffers.size(); ii++)
        configBuffers[ii]=VA_INVALID_ID;
    mfxU16 buffersCount = 0;

    //Add preENC buffers
    //preENC control
    VABufferID mvPredid = VA_INVALID_ID;
    VABufferID qpid = VA_INVALID_ID;

    /* Condition != NULL checked on VideoENC_PREENC::RunFrameVmeENCCheck */
    mfxENCInput* in = (mfxENCInput*)task.m_userData[0];
    mfxENCOutput* out = (mfxENCOutput*)task.m_userData[1];

    mfxU32 feiFieldId = fieldId;

    /*Input FEI Ext buffer sequence for BFF is: bff tff bff tff
     * So, MSDK need to swap feiFieldId values to get correct buffer */
    if (MFX_PICSTRUCT_FIELD_BFF == m_videoParam.mfx.FrameInfo.PicStruct)
    {
        if (1 == feiFieldId)
            feiFieldId = 0;
        else
            feiFieldId = 1;
    }

    //find output surfaces
    mfxExtFeiPreEncMV* mvsOut = GetExtBufferFEI(out, feiFieldId);
    mfxExtFeiPreEncMBStat* mbstatOut = GetExtBufferFEI(out, fieldId);

    //find in buffers
    mfxExtFeiPreEncCtrl* feiCtrl = GetExtBufferFEI(in, feiFieldId);
    mfxExtFeiEncQP* feiQP = GetExtBufferFEI(in, feiFieldId);
    mfxExtFeiPreEncMVPredictors* feiMVPred = GetExtBufferFEI(in, feiFieldId);

    //should be adjusted

    VAStatsStatisticsParameter16x16Intel m_statParams;
    memset(&m_statParams, 0, sizeof (VAStatsStatisticsParameter16x16Intel));
    VABufferID statParamsId = VA_INVALID_ID;

    if (feiCtrl != NULL)
    {
        m_statParams.adaptive_search = feiCtrl->AdaptiveSearch;
        m_statParams.disable_statistics_output = (mbstatOut == NULL) || feiCtrl->DisableStatisticsOutput;
        m_statParams.disable_mv_output = (mvsOut == NULL) || feiCtrl->DisableMVOutput;
        m_statParams.mb_qp = (feiQP == NULL) && feiCtrl->MBQp;
        m_statParams.mv_predictor_ctrl = (feiMVPred != NULL) ? feiCtrl->MVPredictor : 0;
        m_statParams.frame_qp = feiCtrl->Qp;
        m_statParams.ft_enable = feiCtrl->FTEnable;
        m_statParams.inter_sad = feiCtrl->InterSAD;
        m_statParams.intra_sad = feiCtrl->IntraSAD;
        m_statParams.len_sp = feiCtrl->LenSP;
        m_statParams.search_path = feiCtrl->SearchPath;
        //m_statParams.outputs = &outBuffers[0]; //bufIDs for outputs
        m_statParams.sub_pel_mode = feiCtrl->SubPelMode;
        m_statParams.sub_mb_part_mask = feiCtrl->SubMBPartMask;
        m_statParams.ref_height = feiCtrl->RefHeight;
        m_statParams.ref_width = feiCtrl->RefWidth;
        m_statParams.search_window = feiCtrl->SearchWindow;
        m_statParams.enable_8x8statistics = feiCtrl->Enable8x8Stat;
        m_statParams.intra_part_mask = feiCtrl->IntraPartMask;
    }
    else
    {
        m_statParams.adaptive_search = 0;
        m_statParams.disable_statistics_output = 1;
        m_statParams.disable_mv_output = 1;
        m_statParams.mb_qp = 0;
        m_statParams.mv_predictor_ctrl = 0;
        m_statParams.frame_qp = 26;
        m_statParams.ft_enable = 0;
        m_statParams.inter_sad = 2;
        m_statParams.intra_sad = 2;
        m_statParams.len_sp = 57;
        m_statParams.search_path = 2;
        //m_statParams.outputs = &outBuffers[0]; //bufIDs for outputs
        m_statParams.sub_pel_mode = 3;
        m_statParams.sub_mb_part_mask = 0;
        m_statParams.ref_height = 32;
        m_statParams.ref_width = 32;
        m_statParams.search_window = 5;
        m_statParams.enable_8x8statistics = 0;
        m_statParams.intra_part_mask = 0;
    } // if (feiCtrl != NULL)


    VABufferID outBuffers[2];
    int numOutBufs = 0;
    outBuffers[0] = VA_INVALID_ID;
    outBuffers[1] = VA_INVALID_ID;

    if ((m_statParams.mv_predictor_ctrl) && (feiMVPred != NULL) && (feiMVPred->MB != NULL)) {
        vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                (VABufferType)VAStatsMVPredictorBufferTypeIntel,
                sizeof (VAMotionVectorIntel) *feiMVPred->NumMBAlloc,
                1,
                feiMVPred->MB,
                &mvPredid);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        m_statParams.mv_predictor = mvPredid;
        configBuffers[buffersCount++] = mvPredid;
        mdprintf(stderr, "MVPred bufId=%d\n", mvPredid);
    }

    if ((m_statParams.mb_qp) && (feiQP != NULL) && (feiQP->QP != NULL)) {

        vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                (VABufferType)VAEncQpBufferType,
                sizeof (VAEncQpBufferH264)*feiQP->NumQPAlloc,
                1,
                feiQP->QP,
                &qpid);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        m_statParams.qp = qpid;
        configBuffers[buffersCount++] = qpid;
        mdprintf(stderr, "Qp bufId=%d\n", qpid);
    }

    if ((!m_statParams.disable_mv_output) && (VA_INVALID_ID == m_statMVId[feiFieldId]))
    {
        vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                (VABufferType)VAStatsMotionVectorBufferTypeIntel,
                sizeof (VAMotionVectorIntel)*mvsOut->NumMBAlloc * 16, //16 MV per MB
                1,
                NULL, //should be mapped later
                &m_statMVId[feiFieldId]);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        outBuffers[numOutBufs++] = m_statMVId[feiFieldId];
        mdprintf(stderr, "MV bufId=%d\n", m_statMVId[feiFieldId]);
        configBuffers[buffersCount++] = m_statMVId[feiFieldId];
    }
    /* In case of interlaced MSDK need to use one buffer for whole frame (TFF + BFF)
     * This is limitation from driver */
    if (!m_statParams.disable_statistics_output){
        if (VA_INVALID_ID == m_statOutId[0])
        {
            int numMB = m_sps.picture_height_in_mbs * m_sps.picture_width_in_mbs;
            vaSts = vaCreateBuffer(m_vaDisplay,
                    m_vaContextEncode,
                    (VABufferType)VAStatsStatisticsBufferTypeIntel,
                    sizeof (VAStatsStatistics16x16Intel) * numMB,
                    1,
                    NULL,
                    &m_statOutId[0]);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        }
        outBuffers[numOutBufs++] = m_statOutId[0];
        mdprintf(stderr, "StatOut bufId=%d\n", m_statOutId[0]);
        configBuffers[buffersCount++] = m_statOutId[0];
    }

    /* PreEnc support only 1 forward and 1 backward reference */
    /*
     * (AL): I have decided to save previous implementation of passing reference
     * via mfxENCInput structure ...
     * This is can be used in future, maybe
     *  */
    m_statParams.num_past_references = 0;
    //currently only video mem is used, all input surfaces should be in video mem
    m_statParams.past_references = NULL;
    //mdprintf(stderr,"in vaid: %x ", *inputSurface);

    /*
    if (in->NumFrameL0)
    {
        VAPictureFEI* l0surfs = new VAPictureFEI [in->NumFrameL0];
        for (int i = 0; i < in->NumFrameL0; i++)
        {
            mfxHDL handle;
            m_core->GetExternalFrameHDL(in->L0Surface[i]->Data.MemId, &handle);
            VASurfaceID* s = (VASurfaceID*) handle; //id in the memory by ptr
            l0surfs[i].picture_id = *s;

            mfxHDL handle_1;
            m_core->GetExternalFrameHDL(feiCtrl->RefFrame[0]->Data.MemId, &handle_1);
            VASurfaceID* s_1 = (VASurfaceID*) handle; //id in the memory by ptr
            l0surfs[i].picture_id = *s_1;
            if (*s != *s_1)
            {
                mdprintf(stderr,"Error in handling reference in Pre-Enc for L0\n");
            }

            if ((MFX_PICTYPE_TOPFIELD == feiCtrl->RefPictureType[0]) && (task.m_fieldPicFlag))
                l0surfs[i].flags = VA_PICTURE_FEI_TOP_FIELD;
            else if ((MFX_PICTYPE_BOTTOMFIELD == feiCtrl->RefPictureType[0]) && (task.m_fieldPicFlag))
                l0surfs[i].flags = VA_PICTURE_FEI_BOTTOM_FIELD;
            else if ((MFX_PICTYPE_FRAME == feiCtrl->RefPictureType[0]) && (!task.m_fieldPicFlag))
                l0surfs[i].flags = VA_PICTURE_FEI_PROGRESSIVE;
            else // This is disallowed to mix, surface types should be on Init() and within runtime
                return MFX_ERR_INVALID_VIDEO_PARAM;
        }
        // pass LO ref list
        m_statParams.past_references = &l0surfs[0]; //L0 refs
      //  mdprintf(stderr,"l0vaid: %x ", l0surfs[0]);
    }
    */

    if ((NULL != feiCtrl) && (NULL != feiCtrl->RefFrame[0]))
    {
        m_statParams.num_past_references = 1;
        mfxHDL handle;
        VAPictureFEI* l0surfs = &past_ref;
        mfxSts = m_core->GetExternalFrameHDL(feiCtrl->RefFrame[0]->Data.MemId, &handle);
        if (MFX_ERR_NONE != mfxSts)
            return mfxSts;
        l0surfs->picture_id = *(VASurfaceID*)handle;
        if ((MFX_PICTYPE_TOPFIELD == feiCtrl->RefPictureType[0]) && (task.m_fieldPicFlag))
            l0surfs->flags = VA_PICTURE_FEI_TOP_FIELD;
        else if ((MFX_PICTYPE_BOTTOMFIELD == feiCtrl->RefPictureType[0]) && (task.m_fieldPicFlag))
            l0surfs->flags = VA_PICTURE_FEI_BOTTOM_FIELD;
        else if ((MFX_PICTYPE_FRAME == feiCtrl->RefPictureType[0]) && (!task.m_fieldPicFlag))
            l0surfs->flags = VA_PICTURE_FEI_PROGRESSIVE;
        else // This is disallowed to mix, surface types should be on Init() and within runtime
            return MFX_ERR_INVALID_VIDEO_PARAM;
        m_statParams.past_references = l0surfs;
    }

    m_statParams.num_future_references = 0;
    m_statParams.future_references = NULL;
    /*
    if (in->NumFrameL1)
    {
        VAPictureFEI* l1surfs = new VAPictureFEI [in->NumFrameL1];
        for (int i = 0; i < in->NumFrameL1; i++)
        {
            mfxHDL handle;
            m_core->GetExternalFrameHDL(in->L1Surface[i]->Data.MemId, &handle);
            VASurfaceID* s = (VASurfaceID*) handle;
            l1surfs[i].picture_id = *s;

            mfxHDL handle_1;
            m_core->GetExternalFrameHDL(feiCtrl->RefFrame[1]->Data.MemId, &handle_1);
            VASurfaceID* s_1 = (VASurfaceID*) handle; //id in the memory by ptr
            if (*s != *s_1)
            {
                mdprintf(stderr,"Error in handling reference in Pre-Enc for L1\n");
            }
            if ((MFX_PICTYPE_TOPFIELD == feiCtrl->RefPictureType[1]) && (task.m_fieldPicFlag))
                l1surfs[i].flags = VA_PICTURE_FEI_TOP_FIELD;
            else if ((MFX_PICTYPE_BOTTOMFIELD == feiCtrl->RefPictureType[1]) && (task.m_fieldPicFlag))
                l1surfs[i].flags = VA_PICTURE_FEI_BOTTOM_FIELD;
            else if ((MFX_PICTYPE_FRAME == feiCtrl->RefPictureType[1]) && (!task.m_fieldPicFlag))
                l1surfs[i].flags = VA_PICTURE_FEI_PROGRESSIVE;
            else // This is disallowed to mix, surface types should be on Init() and within runtime
                return MFX_ERR_INVALID_VIDEO_PARAM;
        }
        //pass L1 ref list
        m_statParams.future_references = &l1surfs[0];
        //mdprintf(stderr,"l1vaid: %x", l1surfs[0]);
    }
    */

    if ((NULL != feiCtrl) && (NULL != feiCtrl->RefFrame[1]))
    {
        m_statParams.num_future_references = 1;
        mfxHDL handle;
        VAPictureFEI* l1surfs = &future_ref;
        mfxSts = m_core->GetExternalFrameHDL(feiCtrl->RefFrame[1]->Data.MemId, &handle);
        if (MFX_ERR_NONE != mfxSts)
            return mfxSts;
        l1surfs->picture_id = *(VASurfaceID*)handle;
        if ((MFX_PICTYPE_TOPFIELD == feiCtrl->RefPictureType[1]) && (task.m_fieldPicFlag))
            l1surfs->flags = VA_PICTURE_FEI_TOP_FIELD;
        else if ((MFX_PICTYPE_BOTTOMFIELD == feiCtrl->RefPictureType[1]) && (task.m_fieldPicFlag))
            l1surfs->flags = VA_PICTURE_FEI_BOTTOM_FIELD;
        else if ((MFX_PICTYPE_FRAME == feiCtrl->RefPictureType[1]) && (!task.m_fieldPicFlag))
            l1surfs->flags = VA_PICTURE_FEI_PROGRESSIVE;
        else // This is disallowed to mix, surface types should be on Init() and within runtime
            return MFX_ERR_INVALID_VIDEO_PARAM;
        m_statParams.future_references = l1surfs;
    }

    //mdprintf(stderr,"\n");
    m_statParams.input.picture_id = *inputSurface;
    /*
     * feiCtrl->PictureType - value from user in runtime
     * task.m_fieldPicFlag - actually value from mfxVideoParams from Init()
     * And this values should be matched
     * */
    if ((MFX_PICTYPE_TOPFIELD == feiCtrl->PictureType) && (task.m_fieldPicFlag))
        m_statParams.input.flags = VA_PICTURE_FEI_TOP_FIELD;
    else if ((MFX_PICTYPE_BOTTOMFIELD == feiCtrl->PictureType) && (task.m_fieldPicFlag))
        m_statParams.input.flags = VA_PICTURE_FEI_BOTTOM_FIELD;
    else if ((MFX_PICTYPE_FRAME == feiCtrl->PictureType) && (!task.m_fieldPicFlag))
        m_statParams.input.flags = VA_PICTURE_FEI_PROGRESSIVE;
    else /* This is disallowed to mix, surface types should be on Init() and within runtime */
        return MFX_ERR_INVALID_VIDEO_PARAM;
    if (!IsOff(feiCtrl->DownsampleInput))
        m_statParams.input.flags |= VA_PICTURE_FEI_CONTENT_UPDATED;

    /* Link output va buffers */
    m_statParams.outputs = &outBuffers[0]; //bufIDs for outputs

#if 0
    mdprintf(stderr, "\n**** stat params:\n");
    mdprintf(stderr, "input {pic_id = %d, flag = %d}\n", m_statParams.input.picture_id, m_statParams.input.flags);
    for (unsigned int ii = 0; ii < m_statParams.num_past_references; ii++)
        mdprintf(stderr, "{pic_id = %d, flag = %d}", m_statParams.past_references[ii].picture_id,
                                                     m_statParams.past_references[ii].flags);
    mdprintf(stderr, " ]\n");
    mdprintf(stderr, "num_past_references=%d\n", m_statParams.num_past_references);
    for (unsigned int ii = 0; ii < m_statParams.num_future_references; ii++)
        mdprintf(stderr, "{pic_id = %d, flag = %d}", m_statParams.future_references[ii].picture_id,
                                                 m_statParams.future_references[ii].flags);
    mdprintf(stderr, " ]\n");
    mdprintf(stderr, "num_future_references=%d\n", m_statParams.num_future_references);

    mdprintf(stderr, "outputs=0x%x [", m_statParams.outputs);
    for (int i = 0; i < 2; i++)
        mdprintf(stderr, " %d", m_statParams.outputs[i]);
    mdprintf(stderr, " ]\n");

    mdprintf(stderr, "mv_predictor=%d\n", m_statParams.mv_predictor);
    mdprintf(stderr, "qp=%d\n", m_statParams.qp);
    mdprintf(stderr, "frame_qp=%d\n", m_statParams.frame_qp);
    mdprintf(stderr, "len_sp=%d\n", m_statParams.len_sp);
    mdprintf(stderr, "search_path=%d\n", m_statParams.search_path);
    mdprintf(stderr, "reserved0=%d\n", m_statParams.reserved0);
    mdprintf(stderr, "sub_mb_part_mask=%d\n", m_statParams.sub_mb_part_mask);
    mdprintf(stderr, "sub_pel_mode=%d\n", m_statParams.sub_pel_mode);
    mdprintf(stderr, "inter_sad=%d\n", m_statParams.inter_sad);
    mdprintf(stderr, "intra_sad=%d\n", m_statParams.intra_sad);
    mdprintf(stderr, "adaptive_search=%d\n", m_statParams.adaptive_search);
    mdprintf(stderr, "mv_predictor_ctrl=%d\n", m_statParams.mv_predictor_ctrl);
    mdprintf(stderr, "mb_qp=%d\n", m_statParams.mb_qp);
    mdprintf(stderr, "ft_enable=%d\n", m_statParams.ft_enable);
    mdprintf(stderr, "reserved1=%d\n", m_statParams.reserved1);
    mdprintf(stderr, "ref_width=%d\n", m_statParams.ref_width);
    mdprintf(stderr, "ref_height=%d\n", m_statParams.ref_height);
    mdprintf(stderr, "search_window=%d\n", m_statParams.search_window);
    mdprintf(stderr, "reserved2=%d\n", m_statParams.reserved2);
    mdprintf(stderr, "disable_mv_output=%d\n", m_statParams.disable_mv_output);
    mdprintf(stderr, "disable_statistics_output=%d\n", m_statParams.disable_statistics_output);
    mdprintf(stderr, "interlaced=%d\n", m_statParams.interlaced);
    mdprintf(stderr, "reserved3=%d\n", m_statParams.reserved3);
    mdprintf(stderr, "\n");
#endif

    //MFX_DESTROY_VABUFFER(m_statParamsId, m_vaDisplay);
    vaSts = vaCreateBuffer(m_vaDisplay,
            m_vaContextEncode,
            (VABufferType)VAStatsStatisticsParameterBufferTypeIntel,
            sizeof (m_statParams),
            1,
            &m_statParams,
            &statParamsId);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    configBuffers[buffersCount++] = statParamsId;

    assert(buffersCount <= configBuffers.size());

    //------------------------------------------------------------------
    // Rendering
    //------------------------------------------------------------------
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc vaBeginPicture");
        vaSts = vaBeginPicture(
                m_vaDisplay,
                m_vaContextEncode,
                *inputSurface);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc vaRenderPicture");
        mdprintf(stderr, "va_buffers to render: [");
        for (int i = 0; i < buffersCount; i++)
            mdprintf(stderr, " %d", configBuffers[i]);
        mdprintf(stderr, "]\n");

        vaSts = vaRenderPicture(
                m_vaDisplay,
                m_vaContextEncode,
                &configBuffers[0], /* vector store leaner in memory*/
                buffersCount);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    }
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc vaEndPicture");
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
        currentFeedback.number = task.m_statusReportNumber[feiFieldId];
        currentFeedback.surface = *inputSurface;
        currentFeedback.mv = m_statMVId[feiFieldId];
        currentFeedback.mbstat = m_statOutId[0];
        m_statFeedbackCache.push_back(currentFeedback);
    }

    MFX_DESTROY_VABUFFER(mvPredid, m_vaDisplay);
    MFX_DESTROY_VABUFFER(statParamsId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(qpid, m_vaDisplay);
    /* these allocated buffers will be destroyed in QueryStatus()
     * when driver returned result of preenc processing  */
    //MFX_DESTROY_VABUFFER(statMVid, m_vaDisplay);
    //MFX_DESTROY_VABUFFER(statOUTid, m_vaDisplay);

    mdprintf(stderr, "submit_vaapi done: %d\n", task.m_frameOrder);
    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::Execute(ExecuteBuffers& data, mfxU32 fieldId)

mfxStatus VAAPIFEIPREENCEncoder::QueryStatus(
        DdiTask & task,
        mfxU32 fieldId) {
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc QueryStatus");
    VAStatus vaSts;
    mfxStatus sts = MFX_ERR_NONE;

    mdprintf(stderr, "query_vaapi status: %d\n", task.m_frameOrder);
    //------------------------------------------
    // (1) mapping feedbackNumber -> surface & bs
    bool isFound = false;
    VASurfaceID waitSurface;
    VABufferID statMVid = VA_INVALID_ID;
    VABufferID statOUTid = VA_INVALID_ID;
    mfxU32 indxSurf;

    mfxU32 feiFieldId = fieldId;
    /*Input FEI Ext buffer sequence for BFF is: bff tff bff tff
     * So, MSDK need to swap feiFieldId values to get correct buffer */
    if (MFX_PICSTRUCT_FIELD_BFF == m_videoParam.mfx.FrameInfo.PicStruct)
    {
        if (1 == feiFieldId)
            feiFieldId = 0;
        else
            feiFieldId = 1;
    }

    UMC::AutomaticUMCMutex guard(m_guard);

    for (indxSurf = 0; indxSurf < m_statFeedbackCache.size(); indxSurf++)
    {
        ExtVASurface currentFeedback = m_statFeedbackCache[ indxSurf ];

        if (currentFeedback.number == task.m_statusReportNumber[feiFieldId])
        {
            waitSurface = currentFeedback.surface;
            statMVid = currentFeedback.mv;
            //statMVid = m_statMVId.pop_front()
            statOUTid = currentFeedback.mbstat;

            isFound = true;

            break;
        }
    }

    if (!isFound) {
        return MFX_ERR_UNKNOWN;
    }

    VASurfaceStatus surfSts = VASurfaceSkipped;

    vaSts = vaSyncSurface(m_vaDisplay, waitSurface);
    // following code is workaround:
    // because of driver bug it could happen that decoding error will not be returned after decoder sync
    // and will be returned at subsequent encoder sync instead
    // just ignore VA_STATUS_ERROR_DECODING_ERROR in encoder
    if (vaSts == VA_STATUS_ERROR_DECODING_ERROR)
        vaSts = VA_STATUS_SUCCESS;
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    vaSts = vaQuerySurfaceStatus(m_vaDisplay, waitSurface, &surfSts);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    mdprintf(stderr, "sync on surface: %d\n", surfSts);

    switch (surfSts)
    {
        default: //for now driver does not return correct status
        case VASurfaceReady:
        {
            VAMotionVectorIntel* mvs;
            VAStatsStatistics16x16Intel* mbstat;

            mfxENCInput* in = (mfxENCInput*)task.m_userData[0];
            mfxENCOutput* out = (mfxENCOutput*)task.m_userData[1];

            //find control buffer
            mfxExtFeiPreEncCtrl* feiCtrl = GetExtBufferFEI(in, feiFieldId);
            //find output surfaces
            mfxExtFeiPreEncMV* mvsOut = GetExtBufferFEI(out, feiFieldId);
            mfxExtFeiPreEncMBStat* mbstatOut = GetExtBufferFEI(out, fieldId);

            if (feiCtrl && !feiCtrl->DisableMVOutput && mvsOut)
            {
                {
                    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc MV vaMapBuffer");
                    vaSts = vaMapBuffer(
                            m_vaDisplay,
                            statMVid,
                            (void **) (&mvs));
                }
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
                //copy to output in task here MVs
                memcpy_s(mvsOut->MB, sizeof (mfxExtFeiPreEncMV::mfxExtFeiPreEncMVMB) * mvsOut->NumMBAlloc,
                                mvs, 16 * sizeof (VAMotionVectorIntel) * mvsOut->NumMBAlloc);
                {
                    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc MV vaUnmapBuffer");
                    vaUnmapBuffer(m_vaDisplay, statMVid);
                }

                MFX_DESTROY_VABUFFER(statMVid, m_vaDisplay);
                m_statMVId[feiFieldId] = VA_INVALID_ID;
            }

            if (feiCtrl && !feiCtrl->DisableStatisticsOutput && mbstatOut)
            {
                {
                    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc MBStat vaMapBuffer");
                    vaSts = vaMapBuffer(
                            m_vaDisplay,
                            statOUTid,
                            (void **) (&mbstat));
                }

                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
                //copy to output in task here statistics
                /* In case of Interlaced we have one buffer for whole frame
                 * So, MSDK need to copy data for TFF,
                 *  (!) do not delete buffer (!)
                 *   and then copy data for BFF*/
                const mfxExtFeiParam* params = GetExtBuffer(m_videoParam);
                mfxU8 *mbstat_ptr = NULL;
                if ((NULL != params) && (MFX_CODINGOPTION_ON == params->SingleFieldProcessing))
                {
                    if ( ( (MFX_PICSTRUCT_FIELD_TFF == m_videoParam.mfx.FrameInfo.PicStruct) &&
                           (MFX_PICTYPE_BOTTOMFIELD == feiCtrl->PictureType)) ||
                         ( (MFX_PICSTRUCT_FIELD_BFF == m_videoParam.mfx.FrameInfo.PicStruct) &&
                           (MFX_PICTYPE_TOPFIELD == feiCtrl->PictureType)) )
                    {
                        /* In field processing mode all fields is separate,
                         * but library need to know when to remove */
                        feiFieldId = 1;
                    }
                }
                mbstat_ptr = (mfxU8*)mbstat + feiFieldId* (sizeof (VAStatsStatistics16x16Intel) * mbstatOut->NumMBAlloc);
                memcpy_s(mbstatOut->MB, sizeof (mfxExtFeiPreEncMBStat::mfxExtFeiPreEncMBStatMB) * mbstatOut->NumMBAlloc,
                        mbstat_ptr, sizeof (VAStatsStatistics16x16Intel) * mbstatOut->NumMBAlloc);
                {
                    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc MBStat vaUnmapBuffer");
                    vaUnmapBuffer(m_vaDisplay, statOUTid);
                }
                if ((MFX_PICTYPE_FRAME == feiCtrl->PictureType) || (1 == feiFieldId) /* Second field */)
                {
                    MFX_DESTROY_VABUFFER(statOUTid, m_vaDisplay);
                    m_statOutId[0] = VA_INVALID_ID;
                }
                /*WA for m_singleFieldProcessingMode*/
                if ((NULL != params) && (MFX_CODINGOPTION_ON == params->SingleFieldProcessing))
                {
                    if ( ( (MFX_PICSTRUCT_FIELD_TFF == m_videoParam.mfx.FrameInfo.PicStruct) &&
                           (MFX_PICTYPE_BOTTOMFIELD == feiCtrl->PictureType)) ||
                         ( (MFX_PICSTRUCT_FIELD_BFF == m_videoParam.mfx.FrameInfo.PicStruct) &&
                           (MFX_PICTYPE_TOPFIELD == feiCtrl->PictureType)) )
                    {
                        MFX_DESTROY_VABUFFER(statOUTid, m_vaDisplay);
                        m_statOutId[0] = VA_INVALID_ID;
                    }
                }
            }

            // remove task
            m_statFeedbackCache.erase(m_statFeedbackCache.begin() + indxSurf);
        }
            sts =  MFX_ERR_NONE;
            break;
        case VASurfaceRendering:
        case VASurfaceDisplaying:
            sts = MFX_WRN_DEVICE_BUSY;
            break;

        case VASurfaceSkipped:
            //default:
            assert(!"bad feedback status");
            sts =  MFX_ERR_DEVICE_FAILED;
            //return MFX_ERR_NONE;
            break;
    }

    mdprintf(stderr, "query_vaapi done\n");
    return sts;

} // mfxStatus VAAPIEncoder::QueryStatus(mfxU32 feedbackNumber, mfxU32& bytesWritten)
#endif

#if defined(MFX_ENABLE_H264_VIDEO_FEI_ENC)

VAAPIFEIENCEncoder::VAAPIFEIENCEncoder()
: VAAPIEncoder()
, m_codingFunction(MFX_FEI_FUNCTION_ENC)
, m_statParamsId(VA_INVALID_ID)
, m_statMVId(VA_INVALID_ID)
, m_statOutId(VA_INVALID_ID)
, m_codedBufferId(VA_INVALID_ID){
} // VAAPIEncoder::VAAPIEncoder(VideoCORE* core)

VAAPIFEIENCEncoder::~VAAPIFEIENCEncoder()
{

    Destroy();

} // VAAPIEncoder::~VAAPIEncoder()

mfxStatus VAAPIFEIENCEncoder::Destroy()
{

    mfxStatus sts = MFX_ERR_NONE;

    if (m_statParamsId != VA_INVALID_ID)
        MFX_DESTROY_VABUFFER(m_statParamsId, m_vaDisplay);
    if (m_statMVId != VA_INVALID_ID)
        MFX_DESTROY_VABUFFER(m_statMVId, m_vaDisplay);
    if (m_statOutId != VA_INVALID_ID)
        MFX_DESTROY_VABUFFER(m_statOutId, m_vaDisplay);
    if (m_codedBufferId != VA_INVALID_ID)
        MFX_DESTROY_VABUFFER(m_codedBufferId, m_vaDisplay);

    sts = VAAPIEncoder::Destroy();

    return sts;

} // VAAPIEncoder::~VAAPIEncoder()


mfxStatus VAAPIFEIENCEncoder::CreateAccelerationService(MfxVideoParam const & par)
{
    m_videoParam = par;

    //check for ext buffer for FEI
    m_codingFunction = 0; //Unknown function
    if (par.NumExtParam > 0) {
        bool isFEI = false;
        for (int i = 0; i < par.NumExtParam; i++) {
            if (par.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PARAM) {
                isFEI = true;
                const mfxExtFeiParam* params = (mfxExtFeiParam*) (par.ExtParam[i]);
                m_codingFunction = params->Func;
                break;
            }
        }
        if (!isFEI)
            return MFX_ERR_INVALID_VIDEO_PARAM;
    } else
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (m_codingFunction == MFX_FEI_FUNCTION_ENC)
        return CreateENCAccelerationService(par);

    return MFX_ERR_INVALID_VIDEO_PARAM;
} // mfxStatus VAAPIEncoder::CreateAccelerationService(MfxVideoParam const & par)

mfxStatus VAAPIFEIENCEncoder::CreateENCAccelerationService(MfxVideoParam const & par)
{
    MFX_CHECK(m_vaDisplay, MFX_ERR_DEVICE_FAILED);
    VAStatus vaSts = VA_STATUS_SUCCESS;
    VAProfile profile = ConvertProfileTypeMFX2VAAPI(m_videoParam.mfx.CodecProfile);
    mfxU8 vaRCType = ConvertRateControlMFX2VAAPI(par.mfx.RateControlMethod);
    VAEntrypoint entrypoints[5];
    int num_entrypoints,slice_entrypoint;
    VAConfigAttrib attrib[4];

    vaSts = vaQueryConfigEntrypoints( m_vaDisplay, profile,
                                entrypoints,&num_entrypoints);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    for (slice_entrypoint = 0; slice_entrypoint < num_entrypoints; slice_entrypoint++) {
        if (entrypoints[slice_entrypoint] == VAEntrypointEncFEIIntel)
            break;
    }

    if (slice_entrypoint == num_entrypoints) {
        /* not find VAEntrypointEncFEIIntel entry point */
        return MFX_ERR_DEVICE_FAILED;
    }

    /* find out the format for the render target, and rate control mode */
    attrib[0].type = VAConfigAttribRTFormat;
    attrib[1].type = VAConfigAttribRateControl;
    attrib[2].type = (VAConfigAttribType) VAConfigAttribEncFunctionTypeIntel;
    attrib[3].type = (VAConfigAttribType) VAConfigAttribEncMVPredictorsIntel;
    vaSts = vaGetConfigAttributes(m_vaDisplay, profile,
                                    (VAEntrypoint)VAEntrypointEncFEIIntel, &attrib[0], 4);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    if ((attrib[0].value & VA_RT_FORMAT_YUV420) == 0) {
        /* not find desired YUV420 RT format */
        return MFX_ERR_DEVICE_FAILED;
    }

    /* Working only with RC_CPQ */
    if (VA_RC_CQP != vaRCType)
        vaRCType = VA_RC_CQP;
    if ((attrib[1].value & VA_RC_CQP) == 0) {
        /* Can't find matched RC mode */
        printf("Can't find the desired RC mode, exit\n");
        return MFX_ERR_DEVICE_FAILED;
    }

    attrib[0].value = VA_RT_FORMAT_YUV420; /* set to desired RT format */
    attrib[1].value = VA_RC_CQP;
    attrib[2].value = VA_ENC_FUNCTION_ENC_INTEL;
    attrib[3].value = 1; /* set 0 MV Predictor */

    vaSts = vaCreateConfig(m_vaDisplay, profile,
                                (VAEntrypoint)VAEntrypointEncFEIIntel, &attrib[0], 4, &m_vaConfig);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    std::vector<VASurfaceID> rawSurf;
    for (unsigned int i = 0; i < m_reconQueue.size(); i++)
        rawSurf.push_back(m_reconQueue[i].surface);


    // Encoder create
    vaSts = vaCreateContext(
            m_vaDisplay,
            m_vaConfig,
            m_width,
            m_height,
            VA_PROGRESSIVE,
            Begin(rawSurf),
            rawSurf.size(),
            &m_vaContextEncode);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

//#endif

    m_slice.resize(par.mfx.NumSlice); // aya - it is enough for encoding
    m_sliceBufferId.resize(par.mfx.NumSlice);
    m_packeSliceHeaderBufferId.resize(par.mfx.NumSlice);
    m_packedSliceBufferId.resize(par.mfx.NumSlice);
    for (int i = 0; i < par.mfx.NumSlice; i++)
    {
        m_sliceBufferId[i] = m_packeSliceHeaderBufferId[i] = m_packedSliceBufferId[i] = VA_INVALID_ID;
    }

    /* driver need only 16 buffer for statistic */
    m_vaFeiMBStatId.resize(2);
    m_vaFeiMVOutId.resize(2);
    m_vaFeiMCODEOutId.resize(2);
    for( int i = 0; i < 2; i++ )
        m_vaFeiMBStatId[i] = m_vaFeiMVOutId[i] = m_vaFeiMCODEOutId[i] = VA_INVALID_ID;


    Zero(m_sps);
    Zero(m_pps);
    Zero(m_slice);
    //------------------------------------------------------------------

    FillSps(par, m_sps);
    SetHRD(par, m_vaDisplay, m_vaContextEncode, m_hrdBufferId);
    //SetRateControl(par, m_vaDisplay, m_vaContextEncode, m_rateParamBufferId);
    SetFrameRate(par, m_vaDisplay, m_vaContextEncode, m_frameRateId);
    //SetPrivateParams(par, m_vaDisplay, m_vaContextEncode, m_privateParamsId);
    FillConstPartOfPps(par, m_pps);

    if (m_caps.HeaderInsertion == 0)
        m_headerPacker.Init(par, m_caps);

    return MFX_ERR_NONE;
}


mfxStatus VAAPIFEIENCEncoder::Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type)
{
    std::vector<ExtVASurface> * pQueue;
    mfxStatus sts;

    if (D3DDDIFMT_INTELENCODE_BITSTREAMDATA == type)
    {
        pQueue = &m_bsQueue;
    } else
    {
        pQueue = &m_reconQueue;
    }

    {
        // we should register allocated HW bitstreams and recon surfaces
        MFX_CHECK(response.mids, MFX_ERR_NULL_PTR);

        ExtVASurface extSurf;
        VASurfaceID *pSurface = NULL;

        for (mfxU32 i = 0; i < response.NumFrameActual; i++) {

            sts = m_core->GetFrameHDL(response.mids[i], (mfxHDL *) & pSurface);
            MFX_CHECK_STS(sts);

            extSurf.number = i;
            extSurf.surface = *pSurface;

            pQueue->push_back(extSurf);
        }
    }
#if 0
    if (D3DDDIFMT_INTELENCODE_BITSTREAMDATA != type) {
        sts = CreateAccelerationService(m_videoParam);
        MFX_CHECK_STS(sts);
    }
#endif

    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type)

mfxStatus VAAPIFEIENCEncoder::Execute(
        mfxHDL surface,
        DdiTask const & task,
        mfxU32 fieldId,
        PreAllocatedVector const & sei)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "FEI Enc Execute");
    mdprintf(stderr, "VAAPIFEIENCEncoder::Execute\n");

    VAStatus vaSts = VA_STATUS_SUCCESS;
    mfxStatus mfxSts = MFX_ERR_NONE;
    VAEncPackedHeaderParameterBuffer packed_header_param_buffer;

    VASurfaceID *inputSurface = (VASurfaceID*) surface;

    mfxU32 feiFieldId = fieldId;

    /*Input FEI Ext buffer sequence for BFF is: bff tff bff tff
     * So, MSDK need to swap feiFieldId values to get correct buffer */
    if (MFX_PICSTRUCT_FIELD_BFF == m_videoParam.mfx.FrameInfo.PicStruct)
    {
        if (1 == feiFieldId)
            feiFieldId = 0;
        else
            feiFieldId = 1;
    }

    std::vector<VABufferID> configBuffers;
    configBuffers.resize(MAX_CONFIG_BUFFERS_COUNT + m_slice.size()*2);
    for (mfxU32 ii = 0; ii < configBuffers.size(); ii++)
        configBuffers[ii]=VA_INVALID_ID;
    mfxU16 buffersCount = 0;

    /* hrd parameter */
    /* it was done on the init stage */
    //SetHRD(par, m_vaDisplay, m_vaContextEncode, m_hrdBufferId);
    mdprintf(stderr, "m_hrdBufferId=%d\n", m_hrdBufferId);
    configBuffers[buffersCount++] = m_hrdBufferId;

    /* frame rate parameter */
    /* it was done on the init stage */
    //SetFrameRate(par, m_vaDisplay, m_vaContextEncode, m_frameRateId);
    mdprintf(stderr, "m_frameRateId=%d\n", m_frameRateId);
    configBuffers[buffersCount++] = m_frameRateId;

    mfxENCInput* in = (mfxENCInput*)task.m_userData[0];
    mfxENCOutput* out = (mfxENCOutput*)task.m_userData[1];

    VABufferID vaFeiFrameControlId = VA_INVALID_ID;
    VABufferID vaFeiMVPredId = VA_INVALID_ID;
    VABufferID vaFeiMBControlId = VA_INVALID_ID;
    VABufferID vaFeiMBQPId = VA_INVALID_ID;

    //find ext buffers
    /* In buffers */
    const mfxEncodeCtrl& ctrl = task.m_ctrl;
    mfxExtFeiPPS *pDataPPS = GetExtBufferFEI(in,feiFieldId);
    mfxExtFeiSPS *pDataSPS = GetExtBufferFEI(in,feiFieldId);
    mfxExtFeiSliceHeader *pDataSliceHeader = GetExtBufferFEI(in,feiFieldId);
    mfxExtFeiEncMBCtrl* mbctrl = GetExtBufferFEI(in,feiFieldId);
    mfxExtFeiEncMVPredictors* mvpred = GetExtBufferFEI(in,feiFieldId);
    mfxExtFeiEncFrameCtrl* frameCtrl = GetExtBufferFEI(in,feiFieldId);
    mfxExtFeiEncQP* mbqp = GetExtBufferFEI(in,feiFieldId);
    /* Out Buffers*/
    mfxExtFeiEncMBStat* mbstat = GetExtBufferFEI(out,feiFieldId);
    mfxExtFeiEncMV* mvout = GetExtBufferFEI(out,feiFieldId);
    mfxExtFeiPakMBCtrl* mbcodeout = GetExtBufferFEI(out,feiFieldId);

    /* Lets look does re-generation of headers required */
    if ((NULL != pDataSPS) && (task.m_insertSps[fieldId]))
    {
        mfxExtSpsHeader* extSps = GetExtBuffer(m_videoParam);

        /* SPS portion */
        mdprintf(stderr,"Library's generated SPS header (it will be changed now on external one) \n");
        mdprintf(stderr,"---->extSps->seqParameterSetId = %d\n", extSps->seqParameterSetId);
        mdprintf(stderr,"---->extSps->levelIdc = %d\n", extSps->levelIdc);
        mdprintf(stderr,"---->extSps->maxNumRefFrames = %d\n", extSps->maxNumRefFrames);
        mdprintf(stderr,"---->extSps->picWidthInMbsMinus1 = %d\n", extSps->picWidthInMbsMinus1);
        mdprintf(stderr,"---->extSps->picHeightInMapUnitsMinus1 = %d\n", extSps->picHeightInMapUnitsMinus1);
        mdprintf(stderr,"---->extSps->chromaFormatIdc = %d\n", extSps->chromaFormatIdc);
        mdprintf(stderr,"---->extSps->frameMbsOnlyFlag = %d\n", extSps->frameMbsOnlyFlag);
        mdprintf(stderr,"---->extSps->mbAdaptiveFrameFieldFlag = %d\n", extSps->mbAdaptiveFrameFieldFlag);
        mdprintf(stderr,"---->extSps->direct8x8InferenceFlag = %d\n", extSps->direct8x8InferenceFlag);
        mdprintf(stderr,"---->extSps->log2MaxFrameNumMinus4 = %d\n", extSps->log2MaxFrameNumMinus4);
        mdprintf(stderr,"---->extSps->picOrderCntType = %d\n", extSps->picOrderCntType);
        mdprintf(stderr,"---->extSps->log2MaxPicOrderCntLsbMinus4 = %d\n", extSps->log2MaxPicOrderCntLsbMinus4);
        mdprintf(stderr,"---->extSps->deltaPicOrderAlwaysZeroFlag = %d\n", extSps->deltaPicOrderAlwaysZeroFlag);
        extSps->seqParameterSetId = pDataSPS->SPSId;
        extSps->levelIdc = pDataSPS->Level;
        extSps->maxNumRefFrames = pDataSPS->NumRefFrame;
        extSps->picWidthInMbsMinus1 = pDataSPS->WidthInMBs -1 ;
        extSps->picHeightInMapUnitsMinus1 = pDataSPS->HeightInMBs -1;
        extSps->chromaFormatIdc = pDataSPS->ChromaFormatIdc;
        extSps->frameMbsOnlyFlag = pDataSPS->FrameMBsOnlyFlag;
        extSps->mbAdaptiveFrameFieldFlag = pDataSPS->MBAdaptiveFrameFieldFlag;
        extSps->direct8x8InferenceFlag = pDataSPS->Direct8x8InferenceFlag;
        extSps->log2MaxFrameNumMinus4 = pDataSPS->Log2MaxFrameNum;
        extSps->picOrderCntType = pDataSPS->PicOrderCntType;
        extSps->log2MaxPicOrderCntLsbMinus4 = pDataSPS->Log2MaxPicOrderCntLsb - 4 ;
        extSps->deltaPicOrderAlwaysZeroFlag = pDataSPS->DeltaPicOrderAlwaysZeroFlag;

        mdprintf(stderr,"Applications's SPS header\n");
        mdprintf(stderr,"---->extSps->seqParameterSetId = %d\n", extSps->seqParameterSetId);
        mdprintf(stderr,"---->extSps->levelIdc = %d\n", extSps->levelIdc);
        mdprintf(stderr,"---->extSps->maxNumRefFrames = %d\n", extSps->maxNumRefFrames);
        mdprintf(stderr,"---->extSps->picWidthInMbsMinus1 = %d\n", extSps->picWidthInMbsMinus1);
        mdprintf(stderr,"---->extSps->picHeightInMapUnitsMinus1 = %d\n", extSps->picHeightInMapUnitsMinus1);
        mdprintf(stderr,"---->extSps->chromaFormatIdc = %d\n", extSps->chromaFormatIdc);
        mdprintf(stderr,"---->extSps->frameMbsOnlyFlag = %d\n", extSps->frameMbsOnlyFlag);
        mdprintf(stderr,"---->extSps->mbAdaptiveFrameFieldFlag = %d\n", extSps->mbAdaptiveFrameFieldFlag);
        mdprintf(stderr,"---->extSps->direct8x8InferenceFlag = %d\n", extSps->direct8x8InferenceFlag);
        mdprintf(stderr,"---->extSps->log2MaxFrameNumMinus4 = %d\n", extSps->log2MaxFrameNumMinus4);
        mdprintf(stderr,"---->extSps->picOrderCntType = %d\n", extSps->picOrderCntType);
        mdprintf(stderr,"---->extSps->log2MaxPicOrderCntLsbMinus4 = %d\n", extSps->log2MaxPicOrderCntLsbMinus4);
        mdprintf(stderr,"---->extSps->deltaPicOrderAlwaysZeroFlag = %d\n", extSps->deltaPicOrderAlwaysZeroFlag);
    }
    /* PPS */
    if ((NULL != pDataPPS) && (task.m_insertPps[fieldId]) )
    {
        mfxExtPpsHeader* extPps = GetExtBuffer(m_videoParam);

        /* PPS */
        mdprintf(stderr,"Library's generated PPS header (it will be changed now on external one) \n");
        mdprintf(stderr,"---->extPps->seqParameterSetId = %d\n", extPps->seqParameterSetId);
        mdprintf(stderr,"---->extPps->picParameterSetId = %d\n", extPps->picParameterSetId);
        mdprintf(stderr,"---->extPps->picInitQpMinus26 = %d\n", extPps->picInitQpMinus26);
        mdprintf(stderr,"---->extPps->numRefIdxL0DefaultActiveMinus1 = %d\n", extPps->numRefIdxL0DefaultActiveMinus1);
        mdprintf(stderr,"---->extPps->numRefIdxL1DefaultActiveMinus1 = %d\n", extPps->numRefIdxL1DefaultActiveMinus1);
        mdprintf(stderr,"---->extPps->chromaQpIndexOffset = %d\n", extPps->chromaQpIndexOffset);
        mdprintf(stderr,"---->extPps->secondChromaQpIndexOffset = %d\n", extPps->secondChromaQpIndexOffset);
        mdprintf(stderr,"---->extPps->entropyCodingModeFlag = %d\n", extPps->entropyCodingModeFlag);
        mdprintf(stderr,"---->extPps->constrainedIntraPredFlag = %d\n", extPps->constrainedIntraPredFlag);
        mdprintf(stderr,"---->extPps->transform8x8ModeFlag = %d\n", extPps->transform8x8ModeFlag);

        extPps->seqParameterSetId = pDataPPS->SPSId;
        extPps->picParameterSetId = pDataPPS->PPSId;

        //extPps->frame_num = pDataPPS->FrameNum;

        extPps->picInitQpMinus26 = pDataPPS->PicInitQP-26;
        extPps->picInitQsMinus26 = pDataPPS->PicInitQP-26;;
        extPps->numRefIdxL0DefaultActiveMinus1 = pDataPPS->NumRefIdxL0Active ? (pDataPPS->NumRefIdxL0Active - 1) : 0;
        extPps->numRefIdxL1DefaultActiveMinus1 = pDataPPS->NumRefIdxL1Active ? (pDataPPS->NumRefIdxL1Active - 1) : 0;

        extPps->chromaQpIndexOffset = pDataPPS->ChromaQPIndexOffset;
        extPps->secondChromaQpIndexOffset = pDataPPS->SecondChromaQPIndexOffset;

        //extPps->m_pps.pic_fields.bits.idr_pic_flag = pDataPPS->IDRPicFlag;
        //extPps->rm_pps.pic_fields.bits.reference_pic_flag = pDataPPS->ReferencePicFlag;
        extPps->entropyCodingModeFlag = pDataPPS->EntropyCodingModeFlag;
        extPps->constrainedIntraPredFlag = pDataPPS->ConstrainedIntraPredFlag;
        extPps->transform8x8ModeFlag = pDataPPS->Transform8x8ModeFlag;

        mdprintf(stderr,"Applications's generated PPS header\n");
        mdprintf(stderr,"---->extPps->seqParameterSetId = %d\n", extPps->seqParameterSetId);
        mdprintf(stderr,"---->extPps->picParameterSetId = %d\n", extPps->picParameterSetId);
        mdprintf(stderr,"---->extPps->picInitQpMinus26 = %d\n", extPps->picInitQpMinus26);
        mdprintf(stderr,"---->extPps->numRefIdxL0DefaultActiveMinus1 = %d\n", extPps->numRefIdxL0DefaultActiveMinus1);
        mdprintf(stderr,"---->extPps->numRefIdxL1DefaultActiveMinus1 = %d\n", extPps->numRefIdxL1DefaultActiveMinus1);
        mdprintf(stderr,"---->extPps->chromaQpIndexOffset = %d\n", extPps->chromaQpIndexOffset);
        mdprintf(stderr,"---->extPps->secondChromaQpIndexOffset = %d\n", extPps->secondChromaQpIndexOffset);
        mdprintf(stderr,"---->extPps->entropyCodingModeFlag = %d\n", extPps->entropyCodingModeFlag);
        mdprintf(stderr,"---->extPps->constrainedIntraPredFlag = %d\n", extPps->constrainedIntraPredFlag);
        mdprintf(stderr,"---->extPps->transform8x8ModeFlag = %d\n", extPps->transform8x8ModeFlag);
    }
    /* repack headers by itself */
    if ( ((NULL != pDataSPS) && (task.m_insertSps[fieldId])) ||
         ((NULL != pDataPPS) && (task.m_insertPps[fieldId])) )
    {
//        std::vector<ENCODE_PACKEDHEADER_DATA> const & packedSpsArray = m_headerPacker.GetSps();
//        ENCODE_PACKEDHEADER_DATA const & packedSps = packedSpsArray[0];
//        /* Dump SPS */
//        mdprintf(stderr,"Dump of packed SPS\n");
//        for (mfxU32 i = 0; i < packedSps.DataLength; i++)
//        {
//            mdprintf(stderr,"packedSps.pData[%u]=%u\n", i, packedSps.pData[i]);
//        }
//        std::vector<ENCODE_PACKEDHEADER_DATA> const & packedPpsArray = m_headerPacker.GetPps();
//        ENCODE_PACKEDHEADER_DATA const & packedPps = packedPpsArray[0];
//        /* Dump SPS */
//        mdprintf(stderr,"Dump of packed PPS\n");
//        for (mfxU32 i = 0; i < packedPps.DataLength; i++)
//        {
//            mdprintf(stderr,"packedPps.pData[%u]=%u\n", i, packedPps.pData[i]);
//        }
        m_headerPacker.Init(m_videoParam, m_caps);
//        /* Dump SPS */
//        mdprintf(stderr,"Dump of packed SPS\n");
//        for (mfxU32 i = 0; i < packedSps.DataLength; i++)
//        {
//            mdprintf(stderr,"packedSps.pData[%u]=%u\n", i, packedSps.pData[i]);
//        }
//        /* Dump SPS */
//        mdprintf(stderr,"Dump of packed PPS\n");
//        for (mfxU32 i = 0; i < packedPps.DataLength; i++)
//        {
//            mdprintf(stderr,"packedPps.pData[%u]=%u\n", i, packedPps.pData[i]);
//        }
    }

    // SPS
    if ((task.m_insertSps[fieldId]) || (NULL != pDataSPS) )
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

        //packedBufferIndexes.push_back(buffersCount);
        //packedDataSize += packed_header_param_buffer.bit_length;
        configBuffers[buffersCount++] = m_packedSpsHeaderBufferId;
        configBuffers[buffersCount++] = m_packedSpsBufferId;
        /* sequence parameter set */
        vaSts = vaCreateBuffer(m_vaDisplay,
                                m_vaContextEncode,
                                   VAEncSequenceParameterBufferType,
                                   sizeof(m_sps),1,
                                   &m_sps,
                                   &m_spsBufferId);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        mdprintf(stderr, "m_spsBufferId=%d\n", m_spsBufferId);
        configBuffers[buffersCount++] = m_spsBufferId;
    }

    if (frameCtrl != NULL && frameCtrl->MVPredictor && mvpred != NULL)
    {
        vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                (VABufferType) VAEncFEIMVPredictorBufferTypeIntel,
                sizeof(VAEncMVPredictorH264Intel)*mvpred->NumMBAlloc,  //limitation from driver, num elements should be 1
                1,
                mvpred->MB,
                &vaFeiMVPredId );
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        configBuffers[buffersCount++] = vaFeiMVPredId;
        mdprintf(stderr, "vaFeiMVPredId=%d\n", vaFeiMVPredId);
    }

    if (frameCtrl != NULL && frameCtrl->PerMBInput && mbctrl != NULL)
    {
        vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                (VABufferType)VAEncFEIMBControlBufferTypeIntel,
                sizeof (VAEncFEIMBControlH264Intel)*mbctrl->NumMBAlloc,  //limitation from driver, num elements should be 1
                1,
                mbctrl->MB,
                &vaFeiMBControlId);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        configBuffers[buffersCount++] = vaFeiMBControlId;
        mdprintf(stderr, "vaFeiMBControlId=%d\n", vaFeiMBControlId);
    }

    if (frameCtrl != NULL && frameCtrl->PerMBQp && mbqp != NULL)
    {
        vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                (VABufferType)VAEncQpBufferType,
                sizeof (VAEncQpBufferH264)*mbqp->NumQPAlloc, //limitation from driver, num elements should be 1
                1,
                mbqp->QP,
                &vaFeiMBQPId);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        configBuffers[buffersCount++] = vaFeiMBQPId;
        mdprintf(stderr, "vaFeiMBQPId=%d\n", vaFeiMBQPId);
    }

    //output buffer for MB distortions
    if ((NULL != mbstat) && (VA_INVALID_ID == m_vaFeiMBStatId[feiFieldId]))
    {
        vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                (VABufferType)VAEncFEIDistortionBufferTypeIntel,
                sizeof (VAEncFEIDistortionBufferH264Intel)*mbstat->NumMBAlloc,
                //limitation from driver, num elements should be 1
                1,
                NULL, //should be mapped later
                &m_vaFeiMBStatId[feiFieldId]);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        mdprintf(stderr, "MB Stat bufId=%d\n", m_vaFeiMBStatId[feiFieldId]);
    }

    //output buffer for MV
    if ((NULL != mvout) && (VA_INVALID_ID == m_vaFeiMVOutId[feiFieldId]))
    {
        vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                (VABufferType)VAEncFEIMVBufferTypeIntel,
                sizeof (VAMotionVectorIntel)*16*mvout->NumMBAlloc,
                //limitation from driver, num elements should be 1
                1,
                NULL, //should be mapped later
                &m_vaFeiMVOutId[feiFieldId]);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        mdprintf(stderr, "MV Out bufId=%d\n", m_vaFeiMVOutId[feiFieldId]);
    }

    //output buffer for MBCODE (Pak object cmds)
    if ((NULL != mbcodeout) && (VA_INVALID_ID == m_vaFeiMCODEOutId[feiFieldId]))
    {
        vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                (VABufferType)VAEncFEIModeBufferTypeIntel,
                sizeof (VAEncFEIModeBufferH264Intel)*mbcodeout->NumMBAlloc,
                //limitation from driver, num elements should be 1
                1,
                NULL, //should be mapped later
                &m_vaFeiMCODEOutId[feiFieldId]);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        mdprintf(stderr, "MCODE Out bufId=%d\n", m_vaFeiMCODEOutId[feiFieldId]);
    }

    if (frameCtrl != NULL)
    {
        VAEncMiscParameterBuffer *miscParam;
        vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                VAEncMiscParameterBufferType,
                sizeof(VAEncMiscParameterBuffer) + sizeof (VAEncMiscParameterFEIFrameControlH264Intel),
                1,
                NULL,
                &vaFeiFrameControlId);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        vaMapBuffer(m_vaDisplay,
            vaFeiFrameControlId,
            (void **)&miscParam);

        miscParam->type = (VAEncMiscParameterType)VAEncMiscParameterTypeFEIFrameControlIntel;
        VAEncMiscParameterFEIFrameControlH264Intel* vaFeiFrameControl = (VAEncMiscParameterFEIFrameControlH264Intel*)miscParam->data;
        memset(vaFeiFrameControl, 0, sizeof (VAEncMiscParameterFEIFrameControlH264Intel)); //check if we need this

        vaFeiFrameControl->function = VA_ENC_FUNCTION_ENC_INTEL;
        vaFeiFrameControl->adaptive_search = frameCtrl->AdaptiveSearch;
        vaFeiFrameControl->distortion_type = frameCtrl->DistortionType;
        vaFeiFrameControl->inter_sad = frameCtrl->InterSAD;
        vaFeiFrameControl->intra_part_mask = frameCtrl->IntraPartMask;
        /*Correction for Main and Baseline profiles: prohibited 8x8 transform */
        if ((MFX_PROFILE_AVC_BASELINE == m_videoParam.mfx.CodecProfile) ||
            (MFX_PROFILE_AVC_MAIN == m_videoParam.mfx.CodecProfile) )
        {
            vaFeiFrameControl->intra_part_mask = 0x02;
        }
        vaFeiFrameControl->intra_sad = frameCtrl->AdaptiveSearch;
        vaFeiFrameControl->intra_sad = frameCtrl->IntraSAD;
        vaFeiFrameControl->len_sp = frameCtrl->LenSP;
        vaFeiFrameControl->search_path = frameCtrl->SearchPath;

        vaFeiFrameControl->distortion = m_vaFeiMBStatId[feiFieldId];
        vaFeiFrameControl->mv_data = m_vaFeiMVOutId[feiFieldId];
        vaFeiFrameControl->mb_code_data = m_vaFeiMCODEOutId[feiFieldId];
        vaFeiFrameControl->qp = vaFeiMBQPId;
        vaFeiFrameControl->mb_ctrl = vaFeiMBControlId;
        vaFeiFrameControl->mb_input = frameCtrl->PerMBInput;
        vaFeiFrameControl->mb_qp = frameCtrl->PerMBQp;  //not supported for now
        vaFeiFrameControl->mb_size_ctrl = frameCtrl->MBSizeCtrl;
        vaFeiFrameControl->multi_pred_l0 = frameCtrl->MultiPredL0;
        vaFeiFrameControl->multi_pred_l1 = frameCtrl->MultiPredL1;
        vaFeiFrameControl->mv_predictor = vaFeiMVPredId;
        vaFeiFrameControl->mv_predictor_enable = frameCtrl->MVPredictor;
        vaFeiFrameControl->num_mv_predictors_l0 = frameCtrl->NumMVPredictors[0];
        vaFeiFrameControl->num_mv_predictors_l1 = frameCtrl->NumMVPredictors[1];
        vaFeiFrameControl->ref_height = frameCtrl->RefHeight;
        vaFeiFrameControl->ref_width = frameCtrl->RefWidth;
        vaFeiFrameControl->repartition_check_enable = frameCtrl->RepartitionCheckEnable;
        vaFeiFrameControl->search_window = frameCtrl->SearchWindow;
        vaFeiFrameControl->sub_mb_part_mask = frameCtrl->SubMBPartMask;
        vaFeiFrameControl->sub_pel_mode = frameCtrl->SubPelMode;

        vaUnmapBuffer(m_vaDisplay, vaFeiFrameControlId);  //check for deletions

        configBuffers[buffersCount++] = vaFeiFrameControlId;
        mdprintf(stderr, "vaFeiFrameControlId=%d\n", vaFeiFrameControlId);
    }

    /* Number of reference handling */
    mfxU32 ref_counter_l0 = 0, ref_counter_l1 = 0;
    mfxU32 maxNumRefL0 = 1, maxNumRefL1 = 1;
    if ((NULL != pDataSliceHeader) && (NULL != pDataSliceHeader->Slice))
    {
        maxNumRefL0 = pDataSliceHeader->Slice[0].NumRefIdxL0Active;
        maxNumRefL1 = pDataSliceHeader->Slice[0].NumRefIdxL1Active;
        if ((maxNumRefL1 > 2) && (task.GetPicStructForEncode() != MFX_PICSTRUCT_PROGRESSIVE ))
            maxNumRefL1 = 2;
    }

    UpdatePPS(task, fieldId, m_pps, m_reconQueue);

    /* to fill DPD in PPS List*/
    if ((maxNumRefL0 > 0) && (task.m_type[fieldId] & MFX_FRAMETYPE_PB))
    {
        for (ref_counter_l0 = 0; ref_counter_l0 < maxNumRefL0; ref_counter_l0++)
        {
            mfxHDL handle;
            mfxU16 indexFromPPSHeader = 0;
            indexFromPPSHeader = pDataPPS->ReferenceFrames[ref_counter_l0];
            if (0xffff != indexFromPPSHeader)
            {
                mfxSts = m_core->GetExternalFrameHDL(in->L0Surface[indexFromPPSHeader]->Data.MemId, &handle);
                MFX_CHECK(MFX_ERR_NONE == mfxSts, MFX_ERR_INVALID_HANDLE);
                VASurfaceID s = *(VASurfaceID*) handle; //id in the memory by ptr
                if (m_pps.ReferenceFrames[ref_counter_l0].picture_id != s)
                {
                    mdprintf(stderr, "!!!Warning picture_id from in->L0Surface[%u] = %u\n", ref_counter_l0, s);
                    mdprintf(stderr, "   But library's is m_pps.ReferenceFrames[%u].picture_id = %u\n", ref_counter_l0, m_pps.ReferenceFrames[ref_counter_l0].picture_id);
                    m_pps.ReferenceFrames[ref_counter_l0].picture_id = s;
                }
            } // if (VA_INVALID_ID != indexFromPPSHeader)
        } // for (ref_counter_l0 = 0; ref_counter_l0 < maxNumRefL0; ref_counter_l0++)
    }

    /* Check application's provided data */
    if (NULL != pDataPPS)
    {
        if (m_pps.pic_fields.bits.idr_pic_flag != pDataPPS->IDRPicFlag)
        {
            mdprintf(stderr, "!!!Warning pDataPPS->IDRPicFlag = %u\n", pDataPPS->IDRPicFlag);
            mdprintf(stderr, "   But library's is m_pps.pic_fields.bits.idr_pic_flag = %u\n", m_pps.pic_fields.bits.idr_pic_flag);
            m_pps.pic_fields.bits.idr_pic_flag = pDataPPS->IDRPicFlag;
        }
        if (m_pps.pic_fields.bits.reference_pic_flag != pDataPPS->ReferencePicFlag)
        {
            mdprintf(stderr, "!!!Warning pDataPPS->ReferencePicFlag = %u\n", pDataPPS->ReferencePicFlag);
            mdprintf(stderr, "   But library's is m_pps.pic_fields.bits.reference_pic_flag = %u\n", m_pps.pic_fields.bits.reference_pic_flag);
            m_pps.pic_fields.bits.reference_pic_flag = pDataPPS->ReferencePicFlag;
        }
        if (m_pps.frame_num != pDataPPS->FrameNum)
        {
            mdprintf(stderr, "!!!Warning pDataPPS->FrameNum = %u\n", pDataPPS->FrameNum);
            mdprintf(stderr, "   But library's is m_pps.frame_num = %u\n", m_pps.frame_num);
            m_pps.frame_num = pDataPPS->FrameNum;
        }
    }

    /* ENC & PAK has issue with reconstruct surface.
     * How does it work right now?
     * ENC & PAK has same surface pool, both for source and reconstructed surfaces,
     * this surface pool should be passed to component when vaCreateContext() called on Init() stage.
     * (1): Current source surface is equal to reconstructed surface. (src = recon surface)
     * and this surface should be unchanged within ENC call.
     * (2): ENC does not generate real reconstruct surface, but driver use surface ID to store
     * additional internal information.
     * (3): PAK generate real reconstruct surface. So, after PAK call content of source surfaces
     * changed, inside already reconstructed surface. For me this is incorrect behavior.
     * (4): Generated reconstructed surfaces become references and managed accordingly by application.
     * (5): Library does not manage reference by itself.
     * (6): And of course main rule: ENC (N number call) and PAK (N number call) should have same exactly
     * same reference /reconstruct list ! (else GPU hang)
     * */

    mfxHDL recon_handle;
    mfxSts = m_core->GetExternalFrameHDL(out->OutSurface->Data.MemId, &recon_handle);
    m_pps.CurrPic.picture_id = *(VASurfaceID*) recon_handle; //id in the memory by ptr
    /* Driver select progressive / interlaced based on this field */
    if (task.GetPicStructForEncode() != MFX_PICSTRUCT_PROGRESSIVE)
        m_pps.CurrPic.flags = TFIELD == fieldId ? VA_PICTURE_H264_TOP_FIELD : VA_PICTURE_H264_BOTTOM_FIELD;
    else
        m_pps.CurrPic.flags = 0;

    /* Need to allocated coded buffer: this is does not used by ENC actually
     */
    if (VA_INVALID_ID == m_codedBufferId)
    {
        int width32 = 32 * ((m_videoParam.mfx.FrameInfo.Width + 31) >> 5);
        int height32 = 32 * ((m_videoParam.mfx.FrameInfo.Height + 31) >> 5);
        int codedbuf_size = static_cast<int>((width32 * height32) * 400LL / (16 * 16)); //from libva spec

        vaSts = vaCreateBuffer(m_vaDisplay,
                                m_vaContextEncode,
                                VAEncCodedBufferType,
                                codedbuf_size,
                                1,
                                NULL,
                                &m_codedBufferId);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        //configBuffers[buffersCount++] = m_codedBufferId;
        mdprintf(stderr, "m_codedBufferId=%d\n", m_codedBufferId);
    }
    m_pps.coded_buf = m_codedBufferId;

    if ((MFX_PROFILE_AVC_BASELINE == m_videoParam.mfx.CodecProfile) ||
        (MFX_PROFILE_AVC_MAIN == m_videoParam.mfx.CodecProfile) ||
        ((frameCtrl != NULL) && (0x2 == frameCtrl->IntraPartMask)) )
    {
        m_pps.pic_fields.bits.transform_8x8_mode_flag = 0;
    }

    vaSts = vaCreateBuffer(m_vaDisplay,
                            m_vaContextEncode,
                            VAEncPictureParameterBufferType,
                            sizeof(m_pps),
                            1,
                            &m_pps,
                            &m_ppsBufferId);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    configBuffers[buffersCount++] = m_ppsBufferId;
    mdprintf(stderr, "m_ppsBufferId=%d\n", m_ppsBufferId);

    if (task.m_insertPps[fieldId])
    {
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

        //packedBufferIndexes.push_back(buffersCount);
        //packedDataSize += packed_header_param_buffer.bit_length;
        configBuffers[buffersCount++] = m_packedPpsHeaderBufferId;
        configBuffers[buffersCount++] = m_packedPpsBufferId;
    }

    /* slice parameters */
    mfxU32 numPics  = task.GetPicStructForEncode() == MFX_PICSTRUCT_PROGRESSIVE ? 1 : 2;
    mfxU32 numSlice = task.m_numSlice[feiFieldId];
    mfxU32 idx = 0, ref = 0;
    /* Application defined slice params */
    if (NULL != pDataSliceHeader)
    {
        if (pDataSliceHeader->NumSlice != pDataSliceHeader->NumSliceAlloc)
            return MFX_ERR_INVALID_VIDEO_PARAM;
        if ((pDataSliceHeader->NumSlice >=1) && (numSlice != pDataSliceHeader->NumSlice))
            numSlice = pDataSliceHeader->NumSlice;
    }

    UpdateSlice(m_caps, task, fieldId, m_sps, m_pps, m_slice, m_videoParam, m_reconQueue);

#if (mdprintf == fprintf)
    mdprintf(stderr, "LIBRRAY's ref list-----\n");
    for ( mfxU32 i = 0; i < 32; i++)
    {
        if (VA_INVALID_ID != m_slice[0].RefPicList0[i].picture_id)
        {
            mdprintf(stderr, " ------%d-----\n", i);
            mdprintf(stderr, "m_slice[0].RefPicList0[%d].picture_id=%d\n", i, m_slice[0].RefPicList0[i].picture_id);
            mdprintf(stderr, "m_slice[0].RefPicList0[%d].frame_idx=%d\n", i, m_slice[0].RefPicList0[i].frame_idx);
            mdprintf(stderr, "m_slice[0].RefPicList0[%d].flags=%d\n", i, m_slice[0].RefPicList0[i].flags);
            mdprintf(stderr, "m_slice[0].RefPicList0[%d].TopFieldOrderCnt=%d\n", i, m_slice[0].RefPicList0[i].TopFieldOrderCnt);
            mdprintf(stderr, "m_slice[0].RefPicList0[%d].BottomFieldOrderCnt=%d\n", i, m_slice[0].RefPicList0[i].BottomFieldOrderCnt);
        }
    }
    for ( mfxU32 i = 0; i < 32; i++)
    {
        if (VA_INVALID_ID != m_slice[0].RefPicList1[i].picture_id)
        {
            mdprintf(stderr, " ------%d-----\n", i);
            mdprintf(stderr, "m_slice[0].RefPicList1[%d].picture_id=%d\n", i, m_slice[0].RefPicList1[i].picture_id);
            mdprintf(stderr, "m_slice[0].RefPicList1[%d].frame_idx=%d\n", i, m_slice[0].RefPicList1[i].frame_idx);
            mdprintf(stderr, "m_slice[0].RefPicList1[%d].flags=%d\n", i, m_slice[0].RefPicList1[i].flags);
            mdprintf(stderr, "m_slice[0].RefPicList1[%d].TopFieldOrderCnt=%d\n", i, m_slice[0].RefPicList1[i].TopFieldOrderCnt);
            mdprintf(stderr, "m_slice[0].RefPicList1[%d].BottomFieldOrderCnt=%d\n", i, m_slice[0].RefPicList1[i].BottomFieldOrderCnt);
        }
    }
#endif //(mdprintf == fprintf)

    for( mfxU32 i = 0; i < numSlice; i++ )
    {
        /* Small correction: legacy use 5,6,7 type values, but for FEI 0,1,2 */
        m_slice[i].slice_type = m_slice[i].slice_type - 5;
        if ( NULL != pDataSliceHeader )
        {
            mdprintf(stderr,"Library's generated Slice header (it will be changed now on external one) \n");
            mdprintf(stderr,"---->m_slice[%u].macroblock_address = %d\n", i, m_slice[i].macroblock_address);
            mdprintf(stderr,"---->m_slice[%u].num_macroblocks = %d\n", i, m_slice[i].num_macroblocks);
            mdprintf(stderr,"---->m_slice[%u].slice_type = %d\n", i, m_slice[i].slice_type);
            mdprintf(stderr,"---->m_slice[%u].idr_pic_id = %d\n", i, m_slice[i].idr_pic_id);
            mdprintf(stderr,"---->m_slice[%u].cabac_init_idc = %d\n", i, m_slice[i].cabac_init_idc);
            mdprintf(stderr,"---->m_slice[%u].slice_qp_delta = %d\n", i, m_slice[i].slice_qp_delta);
            mdprintf(stderr,"---->m_slice[%u].disable_deblocking_filter_idc = %d\n", i, m_slice[i].disable_deblocking_filter_idc);
            mdprintf(stderr,"---->m_slice[%u].slice_alpha_c0_offset_div2 = %d\n", i, m_slice[i].slice_alpha_c0_offset_div2);
            mdprintf(stderr,"---->m_slice[%u].slice_beta_offset_div2 = %d\n", i, m_slice[i].slice_beta_offset_div2);

            if (m_slice[i].macroblock_address != pDataSliceHeader->Slice[i].MBAaddress)
            {
                mdprintf(stderr, "!!!Warning pDataSliceHeader->Slice[%u].MBAaddress = %u\n", i, pDataSliceHeader->Slice[i].MBAaddress);
                mdprintf(stderr, "   But library's is m_slice[%u].macroblock_address = %u\n", i, m_slice[i].macroblock_address);
                m_slice[i].macroblock_address = pDataSliceHeader->Slice[i].MBAaddress;

            }
            if (m_slice[i].num_macroblocks != pDataSliceHeader->Slice[i].NumMBs)
            {
                mdprintf(stderr, "!!!Warning pDataSliceHeader->Slice[%u].NumMBs = %u\n", i, pDataSliceHeader->Slice[i].NumMBs);
                mdprintf(stderr, "   But library's is m_slice[%u].num_macroblocks = %u\n", i, m_slice[i].num_macroblocks);
                m_slice[i].num_macroblocks = pDataSliceHeader->Slice[i].NumMBs;
            }
            if (m_slice[i].slice_type != pDataSliceHeader->Slice[i].SliceType)
            {
                mdprintf(stderr, "!!!Warning pDataSliceHeader->Slice[%u].SliceType = %u\n", i, pDataSliceHeader->Slice[i].SliceType);
                mdprintf(stderr, "   But library's is m_slice[%u].slice_type = %u\n", i, m_slice[i].slice_type);
                m_slice[i].slice_type = pDataSliceHeader->Slice[i].SliceType;
            }
            if (pDataSliceHeader->Slice[i].PPSId != m_pps.pic_parameter_set_id)
            {
                /*May be bug in application */
                mdprintf(stderr,"pDataSliceHeader->Slice[%u].PPSId = %u\n", i, pDataSliceHeader->Slice[i].PPSId);
                mdprintf(stderr,"m_pps.pic_parameter_set_id = %u\n", m_pps.pic_parameter_set_id);
            }
            if (m_slice[i].idr_pic_id != pDataSliceHeader->Slice[i].IdrPicId)
            {
                mdprintf(stderr, "!!!Warning pDataSliceHeader->Slice[%u].IdrPicId = %u\n", i, pDataSliceHeader->Slice[i].IdrPicId);
                mdprintf(stderr, "   But library's is m_slice[%u].idr_pic_id = %u\n", i, m_slice[i].idr_pic_id);
                m_slice[i].idr_pic_id  = pDataSliceHeader->Slice[i].IdrPicId;
            }
            if (m_slice[i].cabac_init_idc != pDataSliceHeader->Slice[i].CabacInitIdc)
            {
                mdprintf(stderr, "!!!Warning pDataSliceHeader->Slice[%u].CabacInitIdc = %u\n", i, pDataSliceHeader->Slice[i].CabacInitIdc);
                mdprintf(stderr, "   But library's is m_slice[%u].cabac_init_idc = %u\n", i, m_slice[i].cabac_init_idc);
                m_slice[i].cabac_init_idc = pDataSliceHeader->Slice[i].CabacInitIdc;
            }
            if (m_slice[i].slice_qp_delta != pDataSliceHeader->Slice[i].SliceQPDelta)
            {
                mdprintf(stderr, "!!!Warning pDataSliceHeader->Slice[%u].SliceQPDelta = %u\n", i, pDataSliceHeader->Slice[i].SliceQPDelta);
                mdprintf(stderr, "   But library's is m_slice[%u].slice_qp_delta = %u\n", i, m_slice[i].slice_qp_delta);
                m_slice[i].slice_qp_delta = pDataSliceHeader->Slice[i].SliceQPDelta;
            }

            if (m_slice[i].disable_deblocking_filter_idc != pDataSliceHeader->Slice[i].DisableDeblockingFilterIdc)
            {
                mdprintf(stderr, "!!!Warning pDataSliceHeader->Slice[%u].DisableDeblockingFilterIdc = %u\n", i, pDataSliceHeader->Slice[i].DisableDeblockingFilterIdc);
                mdprintf(stderr, "   But library's is m_slice[%u].disable_deblocking_filter_idc = %u\n", i, m_slice[i].disable_deblocking_filter_idc);
                m_slice[i].disable_deblocking_filter_idc = pDataSliceHeader->Slice[i].DisableDeblockingFilterIdc;
            }
            if (m_slice[i].slice_alpha_c0_offset_div2 != pDataSliceHeader->Slice[i].SliceAlphaC0OffsetDiv2)
            {
                mdprintf(stderr, "!!!Warning pDataSliceHeader->Slice[%u].SliceAlphaC0OffsetDiv2 = %u\n", i, pDataSliceHeader->Slice[i].SliceAlphaC0OffsetDiv2);
                mdprintf(stderr, "   But library's is m_slice[%u].slice_alpha_c0_offset_div2 = %u\n", i, m_slice[i].slice_alpha_c0_offset_div2);
                m_slice[i].slice_alpha_c0_offset_div2 = pDataSliceHeader->Slice[i].SliceAlphaC0OffsetDiv2;
            }

            if (m_slice[i].slice_beta_offset_div2 != pDataSliceHeader->Slice[i].SliceBetaOffsetDiv2)
            {
                mdprintf(stderr, "!!!Warning pDataSliceHeader->Slice[%u].SliceBetaOffsetDiv2 = %u\n", i, pDataSliceHeader->Slice[i].SliceBetaOffsetDiv2);
                mdprintf(stderr, "   But library's is m_slice[%u].slice_beta_offset_div2 = %u\n", i, m_slice[i].slice_beta_offset_div2);
                m_slice[i].slice_beta_offset_div2 = pDataSliceHeader->Slice[i].SliceBetaOffsetDiv2;
            }

            mdprintf(stderr,"Application's generated Slice header \n");
            mdprintf(stderr,"---->m_slice[%u].macroblock_address = %d\n", i, m_slice[i].macroblock_address);
            mdprintf(stderr,"---->m_slice[%u].num_macroblocks = %d\n", i, m_slice[i].num_macroblocks);
            mdprintf(stderr,"---->m_slice[%u].slice_type = %d\n", i, m_slice[i].slice_type);
            mdprintf(stderr,"---->m_slice[%u].idr_pic_id = %d\n", i, m_slice[i].idr_pic_id);
            mdprintf(stderr,"---->m_slice[%u].cabac_init_idc = %d\n", i, m_slice[i].cabac_init_idc);
            mdprintf(stderr,"---->m_slice[%u].slice_qp_delta = %d\n", i, m_slice[i].slice_qp_delta);
            mdprintf(stderr,"---->m_slice[%u].disable_deblocking_filter_idc = %d\n", i, m_slice[i].disable_deblocking_filter_idc);
            mdprintf(stderr,"---->m_slice[%u].slice_alpha_c0_offset_div2 = %d\n", i, m_slice[i].slice_alpha_c0_offset_div2);
            mdprintf(stderr,"---->m_slice[%u].slice_beta_offset_div2 = %d\n", i, m_slice[i].slice_beta_offset_div2);
        }

        if ((m_slice[i].slice_type == SLICE_TYPE_P) || (m_slice[i].slice_type == SLICE_TYPE_B))
        {
            for (ref_counter_l0 = 0; ref_counter_l0 < maxNumRefL0; ref_counter_l0++)
            {
                mfxHDL handle;
                mfxU16 indexFromSliceHeader = 0;
                if (NULL != pDataSliceHeader)
                {
                    indexFromSliceHeader = pDataSliceHeader->Slice[i].RefL0[ref_counter_l0].Index;
                    mfxSts = m_core->GetExternalFrameHDL(in->L0Surface[indexFromSliceHeader]->Data.MemId, &handle);
                    MFX_CHECK(MFX_ERR_NONE == mfxSts, MFX_ERR_INVALID_HANDLE);
                    VASurfaceID s = *(VASurfaceID*) handle; //id in the memory by ptr
                    if (m_slice[i].RefPicList0[ref_counter_l0].picture_id != s)
                    {
                        mdprintf(stderr, "!!!Warning picture_id from pDataSliceHeader->Slice[%u].RefL0[%u] = %u\n", i, ref_counter_l0, s);
                        mdprintf(stderr, "   But library's is m_slice[%u].RefPicList0[%u].picture_id = %u\n",
                                i, ref_counter_l0, m_slice[i].RefPicList0[ref_counter_l0].picture_id);
                        m_slice[i].RefPicList0[ref_counter_l0].picture_id = s;
                    }
                    if (m_slice[i].RefPicList0[ref_counter_l0].flags != pDataSliceHeader->Slice[i].RefL0[ref_counter_l0].PictureType)
                    {
                        mdprintf(stderr, "!!!Warning pDataSliceHeader->Slice[%u].RefL0[%u].PictureType = %u\n",
                                i, ref_counter_l0, pDataSliceHeader->Slice[i].RefL0[ref_counter_l0].PictureType);
                        mdprintf(stderr, "   But library's is m_slice[%u].RefPicList0[%u].flag = %u\n",
                                i, ref_counter_l0, m_slice[i].RefPicList0[ref_counter_l0].flags & 0x0f);
                        m_slice[i].RefPicList0[ref_counter_l0].flags = pDataSliceHeader->Slice[i].RefL0[ref_counter_l0].PictureType;
                    }
                }
            } /*for (ref_counter_l0 = 0; ref_counter_l0 < in->NumFrameL0; ref_counter_l0++)*/
            for ( ; ref_counter_l0 < 32; ref_counter_l0++)
            {
                m_slice[i].RefPicList0[ref_counter_l0].picture_id = VA_INVALID_ID;
                m_slice[i].RefPicList0[ref_counter_l0].flags = VA_PICTURE_H264_INVALID;
            }
        }
        if (m_slice[i].slice_type == SLICE_TYPE_B)
        {
            for (ref_counter_l1 = 0; ref_counter_l1 < maxNumRefL1; ref_counter_l1++)
            {
                mfxHDL handle;
                mfxU16 indexFromSliceHeader = 0;
                if (NULL != pDataSliceHeader)
                {
                    indexFromSliceHeader = pDataSliceHeader->Slice[i].RefL1[ref_counter_l1].Index;
                    mfxSts = m_core->GetExternalFrameHDL(in->L0Surface[indexFromSliceHeader]->Data.MemId, &handle);
                    MFX_CHECK(MFX_ERR_NONE == mfxSts, MFX_ERR_INVALID_HANDLE);
                    //m_slice[i].RefPicList1[ref_counter_l1].picture_id = *(VASurfaceID*) handle;
                    VASurfaceID s = *(VASurfaceID*) handle; //id in the memory by ptr
                    if (m_slice[i].RefPicList1[ref_counter_l1].picture_id != s)
                    {
                        //m_slice[i].RefPicList1[ref_counter_l1].picture_id = s;
                        mdprintf(stderr, "!!!Warning picture_id from pDataSliceHeader->Slice[%u].RefL1[%u] = %u\n", i, ref_counter_l1, s);
                        mdprintf(stderr, "   But library's is m_slice[%u].RefPicList0[%u].picture_id = %u\n",
                                i, ref_counter_l1, m_slice[i].RefPicList1[ref_counter_l1].picture_id);
                    }
                    if (m_slice[i].RefPicList1[ref_counter_l1].flags != pDataSliceHeader->Slice[i].RefL1[ref_counter_l1].PictureType)
                    {
                        //m_slice[i].RefPicList1[ref_counter_l1].flags = pDataSliceHeader->Slice[i].RefL1[ref_counter_l1].PictureType;
                        mdprintf(stderr, "!!!Warning pDataSliceHeader->Slice[%u].RefL1[%u].PictureType = %u\n",
                                i, ref_counter_l0, pDataSliceHeader->Slice[i].RefL1[ref_counter_l1].PictureType);
                        mdprintf(stderr, "   But library's is m_slice[%u].RefPicList1[%u].flag = %u\n",
                                i, ref_counter_l1, m_slice[i].RefPicList1[ref_counter_l1].flags);
                    }
                }
            } /* for (ref_counter_l1 = 0; ref_counter_l1 < in->NumFrameL1; ref_counter_l1++) */
            for ( ; ref_counter_l1 < 32; ref_counter_l1++)
            {
                m_slice[i].RefPicList1[ref_counter_l1].picture_id = VA_INVALID_ID;
                m_slice[i].RefPicList1[ref_counter_l1].flags = VA_PICTURE_H264_INVALID;
            }
        } // if ( (in->NumFrameL1) && (m_slice[i].slice_type == SLICE_TYPE_B) )
    } // for( size_t i = 0; i < m_slice.size(); i++ )

    mfxU32 prefix_bytes = (task.m_AUStartsFromSlice[fieldId] + 8) * m_headerPacker.isSvcPrefixUsed();
    std::vector<ENCODE_PACKEDHEADER_DATA> const & packedSliceArray = m_headerPacker.GetSlices();
    ENCODE_PACKEDHEADER_DATA const & packedSlice = packedSliceArray[0];
//    /* Dump Slice Header */
//    mdprintf(stderr,"Dump of packed Slice Header (Before)\n");
//    for (mfxU32 i = 0; i < (packedSlice.DataLength+7)/8; i++)
//    {
//        mdprintf(stderr,"packedSlice.pData[%u]=%u\n", i, packedSlice.pData[i]);
//    }

    //Slice headers only
    std::vector<ENCODE_PACKEDHEADER_DATA> const & packedSlices = m_headerPacker.PackSlices(task, fieldId);
//    mdprintf(stderr,"Dump of packed Slice Header (AFTER)\n");
//    for (mfxU32 i = 0; i < (packedSlice.DataLength+7)/8; i++)
//    {
//        mdprintf(stderr,"packedSlice.pData[%u]=%u\n", i, packedSlice.pData[i]);
//    }

    for (size_t i = 0; i < packedSlices.size(); i++)
    {
        ENCODE_PACKEDHEADER_DATA const & packedSlice = packedSlices[i];

        if (prefix_bytes)
        {
            packed_header_param_buffer.type = VAEncPackedHeaderRawData;
            packed_header_param_buffer.has_emulation_bytes = 1;
            packed_header_param_buffer.bit_length = (prefix_bytes * 8);

            vaSts = vaCreateBuffer(m_vaDisplay,
                    m_vaContextEncode,
                    VAEncPackedHeaderParameterBufferType,
                    sizeof(packed_header_param_buffer),
                    1,
                    &packed_header_param_buffer,
                    &m_packedSvcPrefixHeaderBufferId[i]);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            vaSts = vaCreateBuffer(m_vaDisplay,
                                m_vaContextEncode,
                                VAEncPackedHeaderDataBufferType,
                                 prefix_bytes, 1, packedSlice.pData,
                                &m_packedSvcPrefixBufferId[i]);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            configBuffers[buffersCount++] = m_packedSvcPrefixHeaderBufferId[i];
            configBuffers[buffersCount++] = m_packedSvcPrefixBufferId[i];
        }

        packed_header_param_buffer.type = VAEncPackedHeaderH264_Slice;
        packed_header_param_buffer.has_emulation_bytes = 0;
        packed_header_param_buffer.bit_length = packedSlice.DataLength - (prefix_bytes * 8); // DataLength is already in bits !

        //MFX_DESTROY_VABUFFER(m_packeSliceHeaderBufferId[i], m_vaDisplay);
        vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                VAEncPackedHeaderParameterBufferType,
                sizeof(packed_header_param_buffer),
                1,
                &packed_header_param_buffer,
                &m_packeSliceHeaderBufferId[i]);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        //MFX_DESTROY_VABUFFER(m_packedSliceBufferId[i], m_vaDisplay);
        vaSts = vaCreateBuffer(m_vaDisplay,
                            m_vaContextEncode,
                            VAEncPackedHeaderDataBufferType,
                            (packedSlice.DataLength + 7) / 8 - prefix_bytes, 1, packedSlice.pData + prefix_bytes,
                            &m_packedSliceBufferId[i]);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        configBuffers[buffersCount++] = m_packeSliceHeaderBufferId[i];
        configBuffers[buffersCount++] = m_packedSliceBufferId[i];
    }

    for( size_t i = 0; i < m_slice.size(); i++ )
    {
        //MFX_DESTROY_VABUFFER(m_sliceBufferId[i], m_vaDisplay);
        vaSts = vaCreateBuffer(m_vaDisplay,
                                m_vaContextEncode,
                                VAEncSliceParameterBufferType,
                                sizeof(m_slice[i]),
                                1,
                                &m_slice[i],
                                &m_sliceBufferId[i]);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        //configBuffers[buffersCount++] = m_sliceBufferId[i];
        mdprintf(stderr, "m_sliceBufferId[%zu]=%d\n", i, m_sliceBufferId[i]);
    }

    assert(buffersCount <= configBuffers.size());

    //------------------------------------------------------------------
    // Rendering
    //------------------------------------------------------------------
    /* Below log only for debug  */
#if (mdprintf == fprintf)

    mdprintf(stderr, "task.m_frameNum=%d\n", task.m_frameNum);
    mdprintf(stderr, "inputSurface=%d\n", *inputSurface);
    mdprintf(stderr, "m_pps.CurrPic.picture_id=%d\n", m_pps.CurrPic.picture_id);
    mdprintf(stderr, "m_pps.CurrPic.frame_idx=%d\n", m_pps.CurrPic.frame_idx);
    mdprintf(stderr, "m_pps.CurrPic.flags=%d\n",m_pps.CurrPic.flags);
    mdprintf(stderr, "m_pps.CurrPic.TopFieldOrderCnt=%d\n", m_pps.CurrPic.TopFieldOrderCnt);
    mdprintf(stderr, "m_pps.CurrPic.BottomFieldOrderCnt=%d\n", m_pps.CurrPic.BottomFieldOrderCnt);

    mdprintf(stderr, "-------------------------------------------\n");

    int i = 0;

    for ( i = 0; i < 16; i++)
    {
        if (VA_INVALID_ID != m_pps.ReferenceFrames[i].picture_id)
        {
            mdprintf(stderr, " ------%d-----\n", i);
            mdprintf(stderr, "m_pps.ReferenceFrames[%d].picture_id=%d\n", i, m_pps.ReferenceFrames[i].picture_id);
            mdprintf(stderr, "m_pps.ReferenceFrames[%d].frame_idx=%d\n", i, m_pps.ReferenceFrames[i].frame_idx);
            mdprintf(stderr, "m_pps.ReferenceFrames[%d].flags=%d\n", i, m_pps.ReferenceFrames[i].flags);
            mdprintf(stderr, "m_pps.ReferenceFrames[%d].TopFieldOrderCnt=%d\n", i, m_pps.ReferenceFrames[i].TopFieldOrderCnt);
            mdprintf(stderr, "m_pps.ReferenceFrames[%d].BottomFieldOrderCnt=%d\n", i, m_pps.ReferenceFrames[i].BottomFieldOrderCnt);
        }
    }

    for ( i = 0; i < 32; i++)
    {
        if (VA_INVALID_ID != m_slice[0].RefPicList0[i].picture_id)
        {
            mdprintf(stderr, " ------%d-----\n", i);
            mdprintf(stderr, "m_slice[0].RefPicList0[%d].picture_id=%d\n", i, m_slice[0].RefPicList0[i].picture_id);
            mdprintf(stderr, "m_slice[0].RefPicList0[%d].frame_idx=%d\n", i, m_slice[0].RefPicList0[i].frame_idx);
            mdprintf(stderr, "m_slice[0].RefPicList0[%d].flags=%d\n", i, m_slice[0].RefPicList0[i].flags);
            mdprintf(stderr, "m_slice[0].RefPicList0[%d].TopFieldOrderCnt=%d\n", i, m_slice[0].RefPicList0[i].TopFieldOrderCnt);
            mdprintf(stderr, "m_slice[0].RefPicList0[%d].BottomFieldOrderCnt=%d\n", i, m_slice[0].RefPicList0[i].BottomFieldOrderCnt);
        }
    }
    for ( i = 0; i < 32; i++)
    {
        if (VA_INVALID_ID != m_slice[0].RefPicList1[i].picture_id)
        {
            mdprintf(stderr, " ------%d-----\n", i);
            mdprintf(stderr, "m_slice[0].RefPicList1[%d].picture_id=%d\n", i, m_slice[0].RefPicList1[i].picture_id);
            mdprintf(stderr, "m_slice[0].RefPicList1[%d].frame_idx=%d\n", i, m_slice[0].RefPicList1[i].frame_idx);
            mdprintf(stderr, "m_slice[0].RefPicList1[%d].flags=%d\n", i, m_slice[0].RefPicList1[i].flags);
            mdprintf(stderr, "m_slice[0].RefPicList1[%d].TopFieldOrderCnt=%d\n", i, m_slice[0].RefPicList1[i].TopFieldOrderCnt);
            mdprintf(stderr, "m_slice[0].RefPicList1[%d].BottomFieldOrderCnt=%d\n", i, m_slice[0].RefPicList1[i].BottomFieldOrderCnt);
        }
    }
#endif // #if (mdprintf == fprintf)
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc vaBeginPicture");
        vaSts = vaBeginPicture(
                m_vaDisplay,
                m_vaContextEncode,
                *inputSurface);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        mdprintf(stderr,"inputSurface = %d\n",*inputSurface);
    }
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc vaRenderPicture");
        mdprintf(stderr, "va_buffers to render: [");
        for (int i = 0; i < buffersCount; i++)
            mdprintf(stderr, " %d", configBuffers[i]);
        mdprintf(stderr, "]\n");

        vaSts = vaRenderPicture(
                m_vaDisplay,
                m_vaContextEncode,
                &configBuffers[0], /* vector store leaner in memory*/
                buffersCount);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        for(size_t i = 0; i < m_slice.size(); i++)
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
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc vaEndPicture");
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
        currentFeedback.number = task.m_statusReportNumber[feiFieldId];
        currentFeedback.surface = *inputSurface;
        currentFeedback.mv        = m_vaFeiMVOutId[feiFieldId];
        currentFeedback.mbstat    = m_vaFeiMBStatId[feiFieldId];
        currentFeedback.mbcode    = m_vaFeiMCODEOutId[feiFieldId];
        m_statFeedbackCache.push_back(currentFeedback);
    }

    MFX_DESTROY_VABUFFER(vaFeiFrameControlId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(vaFeiMVPredId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(vaFeiMBControlId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(vaFeiMBQPId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_packedSpsHeaderBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_packedSpsBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_spsBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_ppsBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_packedPpsHeaderBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_packedPpsBufferId, m_vaDisplay);

    for(size_t i = 0; i < m_slice.size(); i++)
    {
        MFX_DESTROY_VABUFFER(m_sliceBufferId[i], m_vaDisplay);
        MFX_DESTROY_VABUFFER(m_packeSliceHeaderBufferId[i], m_vaDisplay);
        MFX_DESTROY_VABUFFER(m_packedSliceBufferId[i], m_vaDisplay);
    }

    mdprintf(stderr, "submit_vaapi done: %d\n", task.m_frameOrder);
    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::Execute(ExecuteBuffers& data, mfxU32 fieldId)

mfxStatus VAAPIFEIENCEncoder::QueryStatus(
        DdiTask & task,
        mfxU32 fieldId) {
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc QueryStatus");
    VAStatus vaSts;
    mfxStatus sts = MFX_ERR_NONE;

    mdprintf(stderr, "query_vaapi status: %d\n", task.m_frameOrder);
    //------------------------------------------
    // (1) mapping feedbackNumber -> surface & bs
    bool isFound = false;
    VASurfaceID waitSurface;
    VABufferID vaFeiMBStatId = VA_INVALID_ID;
    VABufferID vaFeiMBCODEOutId = VA_INVALID_ID;
    VABufferID vaFeiMVOutId = VA_INVALID_ID;
    mfxU32 indxSurf;

    mfxU32 feiFieldId = fieldId;

    /*Input FEI Ext buffer sequence for BFF is: bff tff bff tff
     * So, MSDK need to swap feiFieldId values to get correct buffer */
    if (MFX_PICSTRUCT_FIELD_BFF == m_videoParam.mfx.FrameInfo.PicStruct)
    {
        if (1 == feiFieldId)
            feiFieldId = 0;
        else
            feiFieldId = 1;
    }

    UMC::AutomaticUMCMutex guard(m_guard);

    for (indxSurf = 0; indxSurf < m_statFeedbackCache.size(); indxSurf++)
    {
        ExtVASurface currentFeedback = m_statFeedbackCache[ indxSurf ];

        if (currentFeedback.number == task.m_statusReportNumber[feiFieldId])
        {
            waitSurface = currentFeedback.surface;
            vaFeiMBStatId = currentFeedback.mbstat;
            vaFeiMBCODEOutId = currentFeedback.mbcode;
            vaFeiMVOutId = currentFeedback.mv;
            isFound = true;

            break;
        }
    }

    if (!isFound) {
        return MFX_ERR_UNKNOWN;
    }

    VASurfaceStatus surfSts = VASurfaceSkipped;

    vaSts = vaSyncSurface(m_vaDisplay, waitSurface);
    // following code is workaround:
    // because of driver bug it could happen that decoding error will not be returned after decoder sync
    // and will be returned at subsequent encoder sync instead
    // just ignore VA_STATUS_ERROR_DECODING_ERROR in encoder
    if (vaSts == VA_STATUS_ERROR_DECODING_ERROR)
        vaSts = VA_STATUS_SUCCESS;
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    vaSts = vaQuerySurfaceStatus(m_vaDisplay, waitSurface, &surfSts);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    mdprintf(stderr, "sync on surface: %d\n", surfSts);

    switch (surfSts)
    {
        default: //for now driver does not return correct status
        case VASurfaceReady:
        {
            mfxENCInput* in = (mfxENCInput*)task.m_userData[0];
            mfxENCOutput* out = (mfxENCOutput*)task.m_userData[1];
            mfxExtFeiEncMBStat* mbstat = GetExtBufferFEI(out,feiFieldId);
            mfxExtFeiEncMV* mvout = GetExtBufferFEI(out,feiFieldId);
            mfxExtFeiPakMBCtrl* mbcodeout = GetExtBufferFEI(out,feiFieldId);

            /* NO Bitstream in ENC */
            task.m_bsDataLength[feiFieldId] = 0;

            if (mbstat != NULL && vaFeiMBStatId != VA_INVALID_ID)
            {
                VAEncFEIDistortionBufferH264Intel* mbs;
                {
                    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "MB vaMapBuffer");
                    vaSts = vaMapBuffer(
                            m_vaDisplay,
                            vaFeiMBStatId,
                            (void **) (&mbs));
                }
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
                //copy to output in task here MVs
                memcpy_s(mbstat->MB, sizeof (VAEncFEIDistortionBufferH264Intel) * mbstat->NumMBAlloc,
                               mbs, sizeof (VAEncFEIDistortionBufferH264Intel) * mbstat->NumMBAlloc);
                {
                    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc MV vaUnmapBuffer");
                    vaUnmapBuffer(m_vaDisplay, vaFeiMBStatId);
                    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
                }
                mdprintf(stderr, "Destroy - Distortion bufId=%d\n", m_vaFeiMBStatId[feiFieldId]);
                MFX_DESTROY_VABUFFER(m_vaFeiMBStatId[feiFieldId],m_vaDisplay);
            }

            if (mvout != NULL && vaFeiMVOutId != VA_INVALID_ID)
            {
                VAMotionVectorIntel* mvs;
                {
                    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "MB vaMapBuffer");
                    vaSts = vaMapBuffer(
                            m_vaDisplay,
                            vaFeiMVOutId,
                            (void **) (&mvs));
                }

                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
                //copy to output in task here MVs
                memcpy_s(mvout->MB, sizeof (VAMotionVectorIntel) * 16 * mvout->NumMBAlloc,
                               mvs, sizeof (VAMotionVectorIntel) * 16 * mvout->NumMBAlloc);
                    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc MV vaUnmapBuffer");
                    vaUnmapBuffer(m_vaDisplay, vaFeiMVOutId);
                    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
                    mdprintf(stderr, "Destroy - MV bufId=%d\n", m_vaFeiMVOutId[feiFieldId]);
                    MFX_DESTROY_VABUFFER(m_vaFeiMVOutId[feiFieldId],m_vaDisplay);
            }

            if (mbcodeout != NULL && vaFeiMBCODEOutId != VA_INVALID_ID)
            {
                VAEncFEIModeBufferH264Intel* mbcs;
                {
                    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "MB vaMapBuffer");
                    vaSts = vaMapBuffer(
                            m_vaDisplay,
                            vaFeiMBCODEOutId,
                            (void **) (&mbcs));
                }
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
                //copy to output in task here MVs
                memcpy_s(mbcodeout->MB, sizeof (VAEncFEIModeBufferH264Intel) * mbcodeout->NumMBAlloc,
                                  mbcs, sizeof (VAEncFEIModeBufferH264Intel) * mbcodeout->NumMBAlloc);
                {
                    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc MV vaUnmapBuffer");
                    vaUnmapBuffer(m_vaDisplay, vaFeiMBCODEOutId);
                }
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
                mdprintf(stderr, "Destroy - MBCODE bufId=%d\n", m_vaFeiMCODEOutId[feiFieldId]);
                MFX_DESTROY_VABUFFER(m_vaFeiMCODEOutId[feiFieldId],m_vaDisplay);
            }

            // remove task
            m_statFeedbackCache.erase(m_statFeedbackCache.begin() + indxSurf);
        }
            sts = MFX_ERR_NONE;
            break;
        case VASurfaceRendering:
        case VASurfaceDisplaying:
            sts = MFX_WRN_DEVICE_BUSY;
            break;

        case VASurfaceSkipped:
            //default:
            assert(!"bad feedback status");
            sts =  MFX_ERR_DEVICE_FAILED;
            //return MFX_ERR_NONE;
            break;
    }

    mdprintf(stderr, "query_vaapi done\n");

    return sts;
} // mfxStatus VAAPIFEIENCEncoder::QueryStatus(mfxU32 feedbackNumber, mfxU32& bytesWritten)
#endif //#if defined(MFX_ENABLE_H264_VIDEO_FEI_ENC)


#if defined(MFX_ENABLE_H264_VIDEO_FEI_ENC) && defined(MFX_ENABLE_H264_VIDEO_FEI_PAK)

VAAPIFEIPAKEncoder::VAAPIFEIPAKEncoder()
: VAAPIEncoder()
, m_codingFunction(MFX_FEI_FUNCTION_PAK)
, m_statParamsId(VA_INVALID_ID)
, m_statMVId(VA_INVALID_ID)
, m_statOutId(VA_INVALID_ID)
//, m_codedBufferId[0](VA_INVALID_ID)
{
    m_codedBufferId[0] = m_codedBufferId[1] = VA_INVALID_ID;
} // VAAPIEncoder::VAAPIEncoder(VideoCORE* core)

VAAPIFEIPAKEncoder::~VAAPIFEIPAKEncoder()
{

    Destroy();

} // VAAPIEncoder::~VAAPIEncoder()

mfxStatus VAAPIFEIPAKEncoder::Destroy()
{

    mfxStatus sts = MFX_ERR_NONE;

    if (m_statParamsId != VA_INVALID_ID)
        MFX_DESTROY_VABUFFER(m_statParamsId, m_vaDisplay);
    if (m_statMVId != VA_INVALID_ID)
        MFX_DESTROY_VABUFFER(m_statMVId, m_vaDisplay);
    if (m_statOutId != VA_INVALID_ID)
        MFX_DESTROY_VABUFFER(m_statOutId, m_vaDisplay);
    if (m_codedBufferId[0] != VA_INVALID_ID)
        MFX_DESTROY_VABUFFER(m_codedBufferId[0], m_vaDisplay);
    if (m_codedBufferId[1] != VA_INVALID_ID)
        MFX_DESTROY_VABUFFER(m_codedBufferId[1], m_vaDisplay);


    sts = VAAPIEncoder::Destroy();

    return sts;

} // VAAPIEncoder::~VAAPIEncoder()


mfxStatus VAAPIFEIPAKEncoder::CreateAccelerationService(MfxVideoParam const & par)
{
    m_videoParam = par;

    //check for ext buffer for FEI
    m_codingFunction = 0; //Unknown function
    if (par.NumExtParam > 0) {
        bool isFEI = false;
        for (int i = 0; i < par.NumExtParam; i++) {
            if (par.ExtParam[i]->BufferId == MFX_EXTBUFF_FEI_PARAM) {
                isFEI = true;
                const mfxExtFeiParam* params = (mfxExtFeiParam*) (par.ExtParam[i]);
                m_codingFunction = params->Func;
                break;
            }
        }
        if (!isFEI)
            return MFX_ERR_INVALID_VIDEO_PARAM;
    } else
        return MFX_ERR_INVALID_VIDEO_PARAM;

    if (m_codingFunction == MFX_FEI_FUNCTION_PAK)
        return CreatePAKAccelerationService(par);

    return MFX_ERR_INVALID_VIDEO_PARAM;
} // mfxStatus VAAPIEncoder::CreateAccelerationService(MfxVideoParam const & par)

mfxStatus VAAPIFEIPAKEncoder::CreatePAKAccelerationService(MfxVideoParam const & par)
{
    MFX_CHECK(m_vaDisplay, MFX_ERR_DEVICE_FAILED);
    VAStatus vaSts = VA_STATUS_SUCCESS;
    VAProfile profile = ConvertProfileTypeMFX2VAAPI(m_videoParam.mfx.CodecProfile);
    mfxU8 vaRCType = ConvertRateControlMFX2VAAPI(par.mfx.RateControlMethod);
    VAEntrypoint entrypoints[5];
    int num_entrypoints,slice_entrypoint;
    VAConfigAttrib attrib[4];

    vaSts = vaQueryConfigEntrypoints( m_vaDisplay, profile,
                                entrypoints,&num_entrypoints);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    for (slice_entrypoint = 0; slice_entrypoint < num_entrypoints; slice_entrypoint++) {
        if (entrypoints[slice_entrypoint] == VAEntrypointEncFEIIntel)
            break;
    }

    if (slice_entrypoint == num_entrypoints) {
        /* not find VAEntrypointEncFEIIntel entry point */
        return MFX_ERR_DEVICE_FAILED;
    }

    /* find out the format for the render target, and rate control mode */
    attrib[0].type = VAConfigAttribRTFormat;
    attrib[1].type = VAConfigAttribRateControl;
    attrib[2].type = (VAConfigAttribType) VAConfigAttribEncFunctionTypeIntel;
    attrib[3].type = (VAConfigAttribType) VAConfigAttribEncMVPredictorsIntel;
    vaSts = vaGetConfigAttributes(m_vaDisplay, profile,
                                    (VAEntrypoint)VAEntrypointEncFEIIntel, &attrib[0], 4);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    if ((attrib[0].value & VA_RT_FORMAT_YUV420) == 0) {
        /* not find desired YUV420 RT format */
        return MFX_ERR_DEVICE_FAILED;
    }

    /* Working only with RC_CPQ */
    if (VA_RC_CQP != vaRCType)
        vaRCType = VA_RC_CQP;
    if ((attrib[1].value & VA_RC_CQP) == 0) {
        /* Can't find matched RC mode */
        printf("Can't find the desired RC mode, exit\n");
        return MFX_ERR_DEVICE_FAILED;
    }

    attrib[0].value = VA_RT_FORMAT_YUV420; /* set to desired RT format */
    attrib[1].value = VA_RC_CQP;
    attrib[2].value = VA_ENC_FUNCTION_PAK_INTEL;
    attrib[3].value = 1; /* set 0 MV Predictor */

    vaSts = vaCreateConfig(m_vaDisplay, profile,
                                (VAEntrypoint)VAEntrypointEncFEIIntel, &attrib[0], 4, &m_vaConfig);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    std::vector<VASurfaceID> rawSurf;
    for (unsigned int i = 0; i < m_reconQueue.size(); i++)
        rawSurf.push_back(m_reconQueue[i].surface);


    // Encoder create
    vaSts = vaCreateContext(
            m_vaDisplay,
            m_vaConfig,
            m_width,
            m_height,
            VA_PROGRESSIVE,
            Begin(rawSurf),
            rawSurf.size(),
            &m_vaContextEncode);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    m_slice.resize(par.mfx.NumSlice); // aya - it is enough for encoding
    m_sliceBufferId.resize(par.mfx.NumSlice);
    m_packeSliceHeaderBufferId.resize(par.mfx.NumSlice);
    m_packedSliceBufferId.resize(par.mfx.NumSlice);
    for (int i = 0; i < par.mfx.NumSlice; i++)
    {
        m_sliceBufferId[i] = m_packeSliceHeaderBufferId[i] = m_packedSliceBufferId[i] = VA_INVALID_ID;
    }

    /* driver need only 16 buffer for statistic */
    m_vaFeiMBStatId.resize(2);
    m_vaFeiMVOutId.resize(2);
    m_vaFeiMCODEOutId.resize(2);
    for( int i = 0; i < 2; i++ )
        m_vaFeiMBStatId[i] = m_vaFeiMVOutId[i] = m_vaFeiMCODEOutId[i] = VA_INVALID_ID;

    Zero(m_sps);
    Zero(m_pps);
    Zero(m_slice);
    //------------------------------------------------------------------

    FillSps(par, m_sps);
    SetHRD(par, m_vaDisplay, m_vaContextEncode, m_hrdBufferId);
    //SetRateControl(par, m_vaDisplay, m_vaContextEncode, m_rateParamBufferId);
    SetFrameRate(par, m_vaDisplay, m_vaContextEncode, m_frameRateId);
    //SetPrivateParams(par, m_vaDisplay, m_vaContextEncode, m_privateParamsId);
    FillConstPartOfPps(par, m_pps);

    if (m_caps.HeaderInsertion == 0)
        m_headerPacker.Init(par, m_caps);

    return MFX_ERR_NONE;
}


mfxStatus VAAPIFEIPAKEncoder::Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type)
{
    std::vector<ExtVASurface> * pQueue;
    mfxStatus sts;

    if (D3DDDIFMT_INTELENCODE_BITSTREAMDATA == type)
    {
        pQueue = &m_bsQueue;
    } else
    {
        pQueue = &m_reconQueue;
    }

    {
        // we should register allocated HW bitstreams and recon surfaces
        MFX_CHECK(response.mids, MFX_ERR_NULL_PTR);

        ExtVASurface extSurf;
        VASurfaceID *pSurface = NULL;

        for (mfxU32 i = 0; i < response.NumFrameActual; i++) {

            sts = m_core->GetFrameHDL(response.mids[i], (mfxHDL *) & pSurface);
            MFX_CHECK_STS(sts);

            extSurf.number = i;
            extSurf.surface = *pSurface;

            pQueue->push_back(extSurf);
        }
    }
#if 0
    if (D3DDDIFMT_INTELENCODE_BITSTREAMDATA != type) {
        sts = CreateAccelerationService(m_videoParam);
        MFX_CHECK_STS(sts);
    }
#endif

    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::Register(mfxFrameAllocResponse& response, D3DDDIFORMAT type)


mfxStatus VAAPIFEIPAKEncoder::Execute(
        mfxHDL surface,
        DdiTask const & task,
        mfxU32 fieldId,
        PreAllocatedVector const & sei)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "FEI Enc Execute");

    VAStatus vaSts = VA_STATUS_SUCCESS;

    mfxStatus mfxSts = MFX_ERR_NONE;

    VAEncPackedHeaderParameterBuffer packed_header_param_buffer;

    //VASurfaceID *inputSurface = (VASurfaceID*) surface;
    VASurfaceID *inputSurface = NULL;

    mfxU32 feiFieldId = fieldId;

    /*Input FEI Ext buffer sequence for BFF is: bff tff bff tff
     * So, MSDK need to swap feiFieldId values to get correct buffer */
    if (MFX_PICSTRUCT_FIELD_BFF == m_videoParam.mfx.FrameInfo.PicStruct)
    {
        if (1 == feiFieldId)
            feiFieldId = 0;
        else
            feiFieldId = 1;
    }

    mdprintf(stderr, "VAAPIFEIPAKEncoder::Execute\n");

    std::vector<VABufferID> configBuffers;
    configBuffers.resize(MAX_CONFIG_BUFFERS_COUNT + m_slice.size()*2);
    for (mfxU32 ii = 0; ii < configBuffers.size(); ii++)
        configBuffers[ii]=VA_INVALID_ID;
    mfxU16 buffersCount = 0;

    /* hrd parameter */
    /* it was done on the init stage */
    //SetHRD(par, m_vaDisplay, m_vaContextEncode, m_hrdBufferId);
    mdprintf(stderr, "m_hrdBufferId=%d\n", m_hrdBufferId);
    configBuffers[buffersCount++] = m_hrdBufferId;

    /* frame rate parameter */
    /* it was done on the init stage */
    //SetFrameRate(par, m_vaDisplay, m_vaContextEncode, m_frameRateId);
    mdprintf(stderr, "m_frameRateId=%d\n", m_frameRateId);
    configBuffers[buffersCount++] = m_frameRateId;

    // Input PAK buffers
    mfxPAKInput* in = (mfxPAKInput*)task.m_userData[0];
    mfxPAKOutput* out = (mfxPAKOutput*)task.m_userData[1];

    VABufferID vaFeiFrameControlId = VA_INVALID_ID;
    VABufferID vaFeiMVPredId = VA_INVALID_ID;
    VABufferID vaFeiMBControlId = VA_INVALID_ID;
    VABufferID vaFeiMBQPId = VA_INVALID_ID;

    //find ext buffers
    const mfxEncodeCtrl& ctrl = task.m_ctrl;
    mfxExtFeiEncFrameCtrl* frameCtrl = GetExtBufferFEI(out,feiFieldId);
    mfxExtFeiSPS *pDataSPS = GetExtBufferFEI(out,feiFieldId);
    mfxExtFeiPPS *pDataPPS = GetExtBufferFEI(out,feiFieldId);
    mfxExtFeiSliceHeader *pDataSliceHeader = GetExtBufferFEI(out,feiFieldId);

    /**/
    mfxExtFeiEncMV* mvout = GetExtBufferFEI(in,feiFieldId);
    mfxExtFeiPakMBCtrl* mbcodeout = GetExtBufferFEI(in,feiFieldId);

    /* Lets look does re-generation of headers required */
    if ((NULL != pDataSPS) && (task.m_insertSps[fieldId]))
    {
        mfxExtSpsHeader* extSps = GetExtBuffer(m_videoParam);

        /* SPS portion */
        mdprintf(stderr,"Library's generated SPS header (it will be changed now on external one) \n");
        mdprintf(stderr,"---->extSps->seqParameterSetId = %d\n", extSps->seqParameterSetId);
        mdprintf(stderr,"---->extSps->levelIdc = %d\n", extSps->levelIdc);
        mdprintf(stderr,"---->extSps->maxNumRefFrames = %d\n", extSps->maxNumRefFrames);
        mdprintf(stderr,"---->extSps->picWidthInMbsMinus1 = %d\n", extSps->picWidthInMbsMinus1);
        mdprintf(stderr,"---->extSps->picHeightInMapUnitsMinus1 = %d\n", extSps->picHeightInMapUnitsMinus1);
        mdprintf(stderr,"---->extSps->chromaFormatIdc = %d\n", extSps->chromaFormatIdc);
        mdprintf(stderr,"---->extSps->frameMbsOnlyFlag = %d\n", extSps->frameMbsOnlyFlag);
        mdprintf(stderr,"---->extSps->mbAdaptiveFrameFieldFlag = %d\n", extSps->mbAdaptiveFrameFieldFlag);
        mdprintf(stderr,"---->extSps->direct8x8InferenceFlag = %d\n", extSps->direct8x8InferenceFlag);
        mdprintf(stderr,"---->extSps->log2MaxFrameNumMinus4 = %d\n", extSps->log2MaxFrameNumMinus4);
        mdprintf(stderr,"---->extSps->picOrderCntType = %d\n", extSps->picOrderCntType);
        mdprintf(stderr,"---->extSps->log2MaxPicOrderCntLsbMinus4 = %d\n", extSps->log2MaxPicOrderCntLsbMinus4);
        mdprintf(stderr,"---->extSps->deltaPicOrderAlwaysZeroFlag = %d\n", extSps->deltaPicOrderAlwaysZeroFlag);
        extSps->seqParameterSetId = pDataSPS->SPSId;
        extSps->levelIdc = pDataSPS->Level;
        extSps->maxNumRefFrames = pDataSPS->NumRefFrame;
        extSps->picWidthInMbsMinus1 = pDataSPS->WidthInMBs -1 ;
        extSps->picHeightInMapUnitsMinus1 = pDataSPS->HeightInMBs -1;
        extSps->chromaFormatIdc = pDataSPS->ChromaFormatIdc;
        extSps->frameMbsOnlyFlag = pDataSPS->FrameMBsOnlyFlag;
        extSps->mbAdaptiveFrameFieldFlag = pDataSPS->MBAdaptiveFrameFieldFlag;
        extSps->direct8x8InferenceFlag = pDataSPS->Direct8x8InferenceFlag;
        extSps->log2MaxFrameNumMinus4 = pDataSPS->Log2MaxFrameNum;
        extSps->picOrderCntType = pDataSPS->PicOrderCntType;
        extSps->log2MaxPicOrderCntLsbMinus4 = pDataSPS->Log2MaxPicOrderCntLsb - 4 ;
        extSps->deltaPicOrderAlwaysZeroFlag = pDataSPS->DeltaPicOrderAlwaysZeroFlag;

        mdprintf(stderr,"Applications's SPS header\n");
        mdprintf(stderr,"---->extSps->seqParameterSetId = %d\n", extSps->seqParameterSetId);
        mdprintf(stderr,"---->extSps->levelIdc = %d\n", extSps->levelIdc);
        mdprintf(stderr,"---->extSps->maxNumRefFrames = %d\n", extSps->maxNumRefFrames);
        mdprintf(stderr,"---->extSps->picWidthInMbsMinus1 = %d\n", extSps->picWidthInMbsMinus1);
        mdprintf(stderr,"---->extSps->picHeightInMapUnitsMinus1 = %d\n", extSps->picHeightInMapUnitsMinus1);
        mdprintf(stderr,"---->extSps->chromaFormatIdc = %d\n", extSps->chromaFormatIdc);
        mdprintf(stderr,"---->extSps->frameMbsOnlyFlag = %d\n", extSps->frameMbsOnlyFlag);
        mdprintf(stderr,"---->extSps->mbAdaptiveFrameFieldFlag = %d\n", extSps->mbAdaptiveFrameFieldFlag);
        mdprintf(stderr,"---->extSps->direct8x8InferenceFlag = %d\n", extSps->direct8x8InferenceFlag);
        mdprintf(stderr,"---->extSps->log2MaxFrameNumMinus4 = %d\n", extSps->log2MaxFrameNumMinus4);
        mdprintf(stderr,"---->extSps->picOrderCntType = %d\n", extSps->picOrderCntType);
        mdprintf(stderr,"---->extSps->log2MaxPicOrderCntLsbMinus4 = %d\n", extSps->log2MaxPicOrderCntLsbMinus4);
        mdprintf(stderr,"---->extSps->deltaPicOrderAlwaysZeroFlag = %d\n", extSps->deltaPicOrderAlwaysZeroFlag);
    }
    /* PPS */
    if ((NULL != pDataPPS) && (task.m_insertPps[fieldId]))
    {
        mfxExtPpsHeader* extPps = GetExtBuffer(m_videoParam);

        /* PPS */
        mdprintf(stderr,"Library's generated PPS header (it will be changed now on external one) \n");
        mdprintf(stderr,"---->extPps->seqParameterSetId = %d\n", extPps->seqParameterSetId);
        mdprintf(stderr,"---->extPps->picParameterSetId = %d\n", extPps->picParameterSetId);
        mdprintf(stderr,"---->extPps->picInitQpMinus26 = %d\n", extPps->picInitQpMinus26);
        mdprintf(stderr,"---->extPps->numRefIdxL0DefaultActiveMinus1 = %d\n", extPps->numRefIdxL0DefaultActiveMinus1);
        mdprintf(stderr,"---->extPps->numRefIdxL1DefaultActiveMinus1 = %d\n", extPps->numRefIdxL1DefaultActiveMinus1);
        mdprintf(stderr,"---->extPps->chromaQpIndexOffset = %d\n", extPps->chromaQpIndexOffset);
        mdprintf(stderr,"---->extPps->secondChromaQpIndexOffset = %d\n", extPps->secondChromaQpIndexOffset);
        mdprintf(stderr,"---->extPps->entropyCodingModeFlag = %d\n", extPps->entropyCodingModeFlag);
        mdprintf(stderr,"---->extPps->constrainedIntraPredFlag = %d\n", extPps->constrainedIntraPredFlag);
        mdprintf(stderr,"---->extPps->transform8x8ModeFlag = %d\n", extPps->transform8x8ModeFlag);

        extPps->seqParameterSetId = pDataPPS->SPSId;
        extPps->picParameterSetId = pDataPPS->PPSId;

        //extPps->frame_num = pDataPPS->FrameNum;

        extPps->picInitQpMinus26 = pDataPPS->PicInitQP-26;
        extPps->picInitQsMinus26 = pDataPPS->PicInitQP-26;;
        extPps->numRefIdxL0DefaultActiveMinus1 = pDataPPS->NumRefIdxL0Active ? (pDataPPS->NumRefIdxL0Active - 1) : 0;
        extPps->numRefIdxL1DefaultActiveMinus1 = pDataPPS->NumRefIdxL1Active ? (pDataPPS->NumRefIdxL1Active - 1) : 0;

        extPps->chromaQpIndexOffset = pDataPPS->ChromaQPIndexOffset;
        extPps->secondChromaQpIndexOffset = pDataPPS->SecondChromaQPIndexOffset;

        //extPps->m_pps.pic_fields.bits.idr_pic_flag = pDataPPS->IDRPicFlag;
        //extPps->rm_pps.pic_fields.bits.reference_pic_flag = pDataPPS->ReferencePicFlag;
        extPps->entropyCodingModeFlag = pDataPPS->EntropyCodingModeFlag;
        extPps->constrainedIntraPredFlag = pDataPPS->ConstrainedIntraPredFlag;
        extPps->transform8x8ModeFlag = pDataPPS->Transform8x8ModeFlag;

        mdprintf(stderr,"Applications's generated PPS header\n");
        mdprintf(stderr,"---->extPps->seqParameterSetId = %d\n", extPps->seqParameterSetId);
        mdprintf(stderr,"---->extPps->picParameterSetId = %d\n", extPps->picParameterSetId);
        mdprintf(stderr,"---->extPps->picInitQpMinus26 = %d\n", extPps->picInitQpMinus26);
        mdprintf(stderr,"---->extPps->numRefIdxL0DefaultActiveMinus1 = %d\n", extPps->numRefIdxL0DefaultActiveMinus1);
        mdprintf(stderr,"---->extPps->numRefIdxL1DefaultActiveMinus1 = %d\n", extPps->numRefIdxL1DefaultActiveMinus1);
        mdprintf(stderr,"---->extPps->chromaQpIndexOffset = %d\n", extPps->chromaQpIndexOffset);
        mdprintf(stderr,"---->extPps->secondChromaQpIndexOffset = %d\n", extPps->secondChromaQpIndexOffset);
        mdprintf(stderr,"---->extPps->entropyCodingModeFlag = %d\n", extPps->entropyCodingModeFlag);
        mdprintf(stderr,"---->extPps->constrainedIntraPredFlag = %d\n", extPps->constrainedIntraPredFlag);
        mdprintf(stderr,"---->extPps->transform8x8ModeFlag = %d\n", extPps->transform8x8ModeFlag);
    }
    /* repack headers by itself */
    if ( ((NULL != pDataSPS) && (task.m_insertSps[fieldId])) ||
         ((NULL != pDataPPS) && (task.m_insertPps[fieldId])) )
    {
//        std::vector<ENCODE_PACKEDHEADER_DATA> const & packedSpsArray = m_headerPacker.GetSps();
//        ENCODE_PACKEDHEADER_DATA const & packedSps = packedSpsArray[0];
//        /* Dump SPS */
//        mdprintf(stderr,"Dump of packed SPS\n");
//        for (mfxU32 i = 0; i < packedSps.DataLength; i++)
//        {
//            mdprintf(stderr,"packedSps.pData[%u]=%u\n", i, packedSps.pData[i]);
//        }
//        std::vector<ENCODE_PACKEDHEADER_DATA> const & packedPpsArray = m_headerPacker.GetPps();
//        ENCODE_PACKEDHEADER_DATA const & packedPps = packedPpsArray[0];
//        /* Dump SPS */
//        mdprintf(stderr,"Dump of packed PPS\n");
//        for (mfxU32 i = 0; i < packedPps.DataLength; i++)
//        {
//            mdprintf(stderr,"packedPps.pData[%u]=%u\n", i, packedPps.pData[i]);
//        }
        m_headerPacker.Init(m_videoParam, m_caps);
//        /* Dump SPS */
//        mdprintf(stderr,"Dump of packed SPS\n");
//        for (mfxU32 i = 0; i < packedSps.DataLength; i++)
//        {
//            mdprintf(stderr,"packedSps.pData[%u]=%u\n", i, packedSps.pData[i]);
//        }
//        /* Dump SPS */
//        mdprintf(stderr,"Dump of packed PPS\n");
//        for (mfxU32 i = 0; i < packedPps.DataLength; i++)
//        {
//            mdprintf(stderr,"packedPps.pData[%u]=%u\n", i, packedPps.pData[i]);
//        }
    }

    /* Driver need both buffer to generate bitstream
     * So, if no buffers- this is fatal error
     * */
    if ((NULL == mvout) || (NULL == mbcodeout))
        return MFX_ERR_INVALID_VIDEO_PARAM;

    // SPS
    if ((task.m_insertSps[fieldId]) || (NULL != pDataSPS) )
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

        //packedBufferIndexes.push_back(buffersCount);
        //packedDataSize += packed_header_param_buffer.bit_length;
        configBuffers[buffersCount++] = m_packedSpsHeaderBufferId;
        configBuffers[buffersCount++] = m_packedSpsBufferId;
        /* sequence parameter set */
        vaSts = vaCreateBuffer(m_vaDisplay,
                                m_vaContextEncode,
                                   VAEncSequenceParameterBufferType,
                                   sizeof(m_sps),1,
                                   &m_sps,
                                   &m_spsBufferId);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        mdprintf(stderr, "m_spsBufferId=%d\n", m_spsBufferId);
        configBuffers[buffersCount++] = m_spsBufferId;
    }

    // allocate all MV buffers
    if ((NULL != mvout) && (VA_INVALID_ID == m_vaFeiMVOutId[feiFieldId]))
    {
        vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                (VABufferType)VAEncFEIMVBufferTypeIntel,
                sizeof (VAMotionVectorIntel)*16*mvout->NumMBAlloc,
                //limitation from driver, num elements should be 1
                1,
                NULL, //will be mapped later
                &m_vaFeiMVOutId[feiFieldId]);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        mdprintf(stderr, "MV Out bufId=%d\n", m_vaFeiMVOutId[feiFieldId]);
    }
    /* Copy input data into MV buffer */
    if ( (mvout != NULL) && (m_vaFeiMVOutId[feiFieldId] != VA_INVALID_ID))
    {
        VAMotionVectorIntel* mvs;
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "MB vaMapBuffer");
            vaSts = vaMapBuffer(
                    m_vaDisplay,
                    m_vaFeiMVOutId[feiFieldId],
                    (void **) (&mvs));
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        }

        //copy input MV data to buffer
        memcpy_s(mvs, sizeof (VAMotionVectorIntel) * 16 * mvout->NumMBAlloc,
                mvout->MB, sizeof (VAMotionVectorIntel) * 16 * mvout->NumMBAlloc);
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc MV vaUnmapBuffer");
            vaSts = vaUnmapBuffer(m_vaDisplay, m_vaFeiMVOutId[feiFieldId]);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        }
    }

    // Allocate all output buffer for MBCODE (Pak object cmds)
    if ((NULL != mbcodeout) && (VA_INVALID_ID == m_vaFeiMCODEOutId[feiFieldId]) )
    {
        vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                (VABufferType)VAEncFEIModeBufferTypeIntel,
                sizeof (VAEncFEIModeBufferH264Intel)*mbcodeout->NumMBAlloc,
                //limitation from driver, num elements should be 1
                1,
                NULL, //will be mapped later
                &m_vaFeiMCODEOutId[feiFieldId]);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        mdprintf(stderr, "MCODE Out bufId=%d\n", m_vaFeiMCODEOutId[feiFieldId]);
    }
    /* Copy input data into MB CODE buffer */
    if ( (mbcodeout != NULL) && (m_vaFeiMCODEOutId[feiFieldId] != VA_INVALID_ID) )
    {
        VAEncFEIModeBufferH264Intel* mbcs;
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "MB vaMapBuffer");
            vaSts = vaMapBuffer(
                    m_vaDisplay,
                    m_vaFeiMCODEOutId[feiFieldId],
                    (void **) (&mbcs));
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        }
        //copy to output in task here MVs
        memcpy_s(mbcs, sizeof (VAEncFEIModeBufferH264Intel) * mbcodeout->NumMBAlloc,
                mbcodeout->MB, sizeof (VAEncFEIModeBufferH264Intel) * mbcodeout->NumMBAlloc);
        {
            MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc MV vaUnmapBuffer");
            vaUnmapBuffer(m_vaDisplay, m_vaFeiMCODEOutId[feiFieldId]);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        }
    }

    if (frameCtrl != NULL)
    {
        VAEncMiscParameterBuffer *miscParam;
        vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                VAEncMiscParameterBufferType,
                sizeof(VAEncMiscParameterBuffer) + sizeof (VAEncMiscParameterFEIFrameControlH264Intel),
                1,
                NULL,
                &vaFeiFrameControlId);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        mdprintf(stderr, "vaFeiFrameControlId=%d\n", vaFeiFrameControlId);

        vaMapBuffer(m_vaDisplay,
            vaFeiFrameControlId,
            (void **)&miscParam);

        miscParam->type = (VAEncMiscParameterType)VAEncMiscParameterTypeFEIFrameControlIntel;
        VAEncMiscParameterFEIFrameControlH264Intel* vaFeiFrameControl = (VAEncMiscParameterFEIFrameControlH264Intel*)miscParam->data;
        memset(vaFeiFrameControl, 0, sizeof (VAEncMiscParameterFEIFrameControlH264Intel)); //check if we need this

        vaFeiFrameControl->function = VA_ENC_FUNCTION_PAK_INTEL;
        vaFeiFrameControl->adaptive_search = frameCtrl->AdaptiveSearch;
        vaFeiFrameControl->distortion_type = frameCtrl->DistortionType;
        vaFeiFrameControl->inter_sad = frameCtrl->InterSAD;
        vaFeiFrameControl->intra_part_mask = frameCtrl->IntraPartMask;
        /*Correction for Main and Baseline profiles: prohibited 8x8 transform */
        if ((MFX_PROFILE_AVC_BASELINE == m_videoParam.mfx.CodecProfile) ||
            (MFX_PROFILE_AVC_MAIN == m_videoParam.mfx.CodecProfile) )
        {
            vaFeiFrameControl->intra_part_mask = 0x02;
        }
        vaFeiFrameControl->intra_sad = frameCtrl->AdaptiveSearch;
        vaFeiFrameControl->intra_sad = frameCtrl->IntraSAD;
        vaFeiFrameControl->len_sp = frameCtrl->LenSP;
        vaFeiFrameControl->search_path = frameCtrl->SearchPath;

        vaFeiFrameControl->distortion = VA_INVALID_ID;//m_vaFeiMBStatId[feiFieldId];
        vaFeiFrameControl->mv_data = m_vaFeiMVOutId[feiFieldId];
        vaFeiFrameControl->mb_code_data = m_vaFeiMCODEOutId[feiFieldId];
        vaFeiFrameControl->qp = vaFeiMBQPId;
        vaFeiFrameControl->mb_ctrl = vaFeiMBControlId;
        /* MB Ctrl is only for ENC */
        vaFeiFrameControl->mb_input = 0;
        /* MBQP is only for ENC */
        vaFeiFrameControl->mb_qp = 0;
        vaFeiFrameControl->mb_size_ctrl = frameCtrl->MBSizeCtrl;
        vaFeiFrameControl->multi_pred_l0 = frameCtrl->MultiPredL0;
        vaFeiFrameControl->multi_pred_l1 = frameCtrl->MultiPredL1;
        /* MV Predictors are only for ENC */
        vaFeiFrameControl->mv_predictor = VA_INVALID_ID;
        vaFeiFrameControl->mv_predictor_enable = 0;
        vaFeiFrameControl->num_mv_predictors_l0 = 0;
        vaFeiFrameControl->num_mv_predictors_l1 = 0;
        vaFeiFrameControl->ref_height = frameCtrl->RefHeight;
        vaFeiFrameControl->ref_width = frameCtrl->RefWidth;
        vaFeiFrameControl->repartition_check_enable = frameCtrl->RepartitionCheckEnable;
        vaFeiFrameControl->search_window = frameCtrl->SearchWindow;
        vaFeiFrameControl->sub_mb_part_mask = frameCtrl->SubMBPartMask;
        vaFeiFrameControl->sub_pel_mode = frameCtrl->SubPelMode;

        vaUnmapBuffer(m_vaDisplay, vaFeiFrameControlId);  //check for deletions

        configBuffers[buffersCount++] = vaFeiFrameControlId;
    }

    /* Number of reference handling */
    mfxU32 ref_counter_l0 = 0, ref_counter_l1 = 0;
    mfxU32 maxNumRefL0 = 1, maxNumRefL1 = 1;
    if ((NULL != pDataSliceHeader) && (NULL != pDataSliceHeader->Slice))
    {
        maxNumRefL0 = pDataSliceHeader->Slice[0].NumRefIdxL0Active;
        maxNumRefL1 = pDataSliceHeader->Slice[0].NumRefIdxL1Active;
        if ((maxNumRefL1 > 2) && (task.GetPicStructForEncode() != MFX_PICSTRUCT_PROGRESSIVE ))
            maxNumRefL1 = 2;
    }

    UpdatePPS(task, fieldId, m_pps, m_reconQueue);

    /* to fill DPD in PPS List*/
    if ((maxNumRefL0 > 0) && (task.m_type[fieldId] & MFX_FRAMETYPE_PB))
    {
        for (ref_counter_l0 = 0; ref_counter_l0 < maxNumRefL0; ref_counter_l0++)
        {
            mfxHDL handle;
            mfxU16 indexFromPPSHeader = 0;
            indexFromPPSHeader = pDataPPS->ReferenceFrames[ref_counter_l0];
            if (0xffff != indexFromPPSHeader)
            {
                mfxSts = m_core->GetExternalFrameHDL(in->L0Surface[indexFromPPSHeader]->Data.MemId, &handle);
                MFX_CHECK(MFX_ERR_NONE == mfxSts, MFX_ERR_INVALID_HANDLE);
                VASurfaceID s = *(VASurfaceID*) handle; //id in the memory by ptr
                if (m_pps.ReferenceFrames[ref_counter_l0].picture_id != s)
                {
                    mdprintf(stderr, "!!!Warning picture_id from in->L0Surface[%u] = %u\n", ref_counter_l0, s);
                    mdprintf(stderr, "   But library's is m_pps.ReferenceFrames[%u].picture_id = %u\n", ref_counter_l0, m_pps.ReferenceFrames[ref_counter_l0].picture_id);
                    m_pps.ReferenceFrames[ref_counter_l0].picture_id = s;
                }
            } // if (VA_INVALID_ID != indexFromPPSHeader)
        } // for (ref_counter_l0 = 0; ref_counter_l0 < maxNumRefL0; ref_counter_l0++)
    }

    /* Check application's provided data */
    if (NULL != pDataPPS)
    {
        if (m_pps.pic_fields.bits.idr_pic_flag != pDataPPS->IDRPicFlag)
        {
            mdprintf(stderr, "!!!Warning pDataPPS->IDRPicFlag = %u\n", pDataPPS->IDRPicFlag);
            mdprintf(stderr, "   But library's is m_pps.pic_fields.bits.idr_pic_flag = %u\n", m_pps.pic_fields.bits.idr_pic_flag);
            m_pps.pic_fields.bits.idr_pic_flag = pDataPPS->IDRPicFlag;
        }
        if (m_pps.pic_fields.bits.reference_pic_flag != pDataPPS->ReferencePicFlag)
        {
            mdprintf(stderr, "!!!Warning pDataPPS->ReferencePicFlag = %u\n", pDataPPS->ReferencePicFlag);
            mdprintf(stderr, "   But library's is m_pps.pic_fields.bits.reference_pic_flag = %u\n", m_pps.pic_fields.bits.reference_pic_flag);
            m_pps.pic_fields.bits.reference_pic_flag = pDataPPS->ReferencePicFlag;
        }
        if (m_pps.frame_num != pDataPPS->FrameNum)
        {
            mdprintf(stderr, "!!!Warning pDataPPS->FrameNum = %u\n", pDataPPS->FrameNum);
            mdprintf(stderr, "   But library's is m_pps.frame_num = %u\n", m_pps.frame_num);
            m_pps.frame_num = pDataPPS->FrameNum;
        }
    }

    /* ENC & PAK has issue with reconstruct surface.
     * How does it work right now?
     * ENC & PAK has same surface pool, both for source and reconstructed surfaces,
     * this surface pool should be passed to component when vaCreateContext() called on Init() stage.
     * (1): Current source surface is equal to reconstructed surface. (src = recon surface)
     * and this surface should be unchanged within ENC call.
     * (2): ENC does not generate real reconstruct surface, but driver use surface ID to store
     * additional internal information.
     * (3): PAK generate real reconstruct surface. So, after PAK call content of source surfaces
     * changed, inside already reconstructed surface. For me this is incorrect behavior.
     * (4): Generated reconstructed surfaces become references and managed accordingly by application.
     * (5): Library does not manage reference by itself.
     * (6): And of course main rule: ENC (N number call) and PAK (N number call) should have same exactly
     * same reference /reconstruct list ! (else GPU hang)
     * */

    mfxHDL recon_handle;
    mfxSts = m_core->GetExternalFrameHDL(out->OutSurface->Data.MemId, &recon_handle);
    m_pps.CurrPic.picture_id = *(VASurfaceID*) recon_handle; //id in the memory by ptr
    /* Driver select progressive / interlaced based on this field */
    if (task.GetPicStructForEncode() != MFX_PICSTRUCT_PROGRESSIVE)
        m_pps.CurrPic.flags = TFIELD == fieldId ? VA_PICTURE_H264_TOP_FIELD : VA_PICTURE_H264_BOTTOM_FIELD;
    else
        m_pps.CurrPic.flags = 0;

    /* Need to allocated coded buffer
     */
    if (VA_INVALID_ID == m_codedBufferId[feiFieldId])
    {
        int width32 = 32 * ((m_videoParam.mfx.FrameInfo.Width + 31) >> 5);
        int height32 = 32 * ((m_videoParam.mfx.FrameInfo.Height + 31) >> 5);
        int codedbuf_size = static_cast<int>((width32 * height32) * 400LL / (16 * 16)); //from libva spec

        vaSts = vaCreateBuffer(m_vaDisplay,
                                m_vaContextEncode,
                                VAEncCodedBufferType,
                                codedbuf_size,
                                1,
                                NULL,
                                &m_codedBufferId[feiFieldId]);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        //configBuffers[buffersCount++] = m_codedBufferId;
        mdprintf(stderr, "m_codedBufferId=%d\n", m_codedBufferId[feiFieldId]);
    }
    m_pps.coded_buf = m_codedBufferId[feiFieldId];

    if ((MFX_PROFILE_AVC_BASELINE == m_videoParam.mfx.CodecProfile) ||
        (MFX_PROFILE_AVC_MAIN == m_videoParam.mfx.CodecProfile) ||
        ((frameCtrl != NULL) && (0x2 == frameCtrl->IntraPartMask)) )
    {
        m_pps.pic_fields.bits.transform_8x8_mode_flag = 0;
    }

    vaSts = vaCreateBuffer(m_vaDisplay,
                            m_vaContextEncode,
                            VAEncPictureParameterBufferType,
                            sizeof(m_pps),
                            1,
                            &m_pps,
                            &m_ppsBufferId);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
    configBuffers[buffersCount++] = m_ppsBufferId;
    mdprintf(stderr, "m_ppsBufferId=%d\n", m_ppsBufferId);

    if (task.m_insertPps[fieldId])
    {
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
        mdprintf(stderr, "m_packedPpsHeaderBufferId=%d\n", m_packedPpsHeaderBufferId);

        MFX_DESTROY_VABUFFER(m_packedPpsBufferId, m_vaDisplay);
        vaSts = vaCreateBuffer(m_vaDisplay,
                            m_vaContextEncode,
                            VAEncPackedHeaderDataBufferType,
                            packedPps.DataLength, 1, packedPps.pData,
                            &m_packedPpsBufferId);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        mdprintf(stderr, "m_packedPpsBufferId=%d\n", m_packedPpsBufferId);

        //packedBufferIndexes.push_back(buffersCount);
        //packedDataSize += packed_header_param_buffer.bit_length;
        configBuffers[buffersCount++] = m_packedPpsHeaderBufferId;
        configBuffers[buffersCount++] = m_packedPpsBufferId;
    }

    /* slice parameters */
    mfxU32 numPics  = task.GetPicStructForEncode() == MFX_PICSTRUCT_PROGRESSIVE ? 1 : 2;
    mfxU32 numSlice = task.m_numSlice[fieldId];
    mfxU32 idx = 0, ref = 0;
    /* Application defined slice params */
    if (NULL != pDataSliceHeader)
    {
        if (pDataSliceHeader->NumSlice != pDataSliceHeader->NumSliceAlloc)
            return MFX_ERR_INVALID_VIDEO_PARAM;
        if ((pDataSliceHeader->NumSlice >=1) && (numSlice != pDataSliceHeader->NumSlice))
            numSlice = pDataSliceHeader->NumSlice;
    }

    UpdateSlice(m_caps, task, fieldId, m_sps, m_pps, m_slice, m_videoParam, m_reconQueue);

#if (mdprintf == fprintf)
    mdprintf(stderr, "LIBRRAY's ref list-----\n");
    for ( mfxU32 i = 0; i < 32; i++)
    {
        if (VA_INVALID_ID != m_slice[0].RefPicList0[i].picture_id)
        {
            mdprintf(stderr, " ------%d-----\n", i);
            mdprintf(stderr, "m_slice[0].RefPicList0[%d].picture_id=%d\n", i, m_slice[0].RefPicList0[i].picture_id);
            mdprintf(stderr, "m_slice[0].RefPicList0[%d].frame_idx=%d\n", i, m_slice[0].RefPicList0[i].frame_idx);
            mdprintf(stderr, "m_slice[0].RefPicList0[%d].flags=%d\n", i, m_slice[0].RefPicList0[i].flags);
            mdprintf(stderr, "m_slice[0].RefPicList0[%d].TopFieldOrderCnt=%d\n", i, m_slice[0].RefPicList0[i].TopFieldOrderCnt);
            mdprintf(stderr, "m_slice[0].RefPicList0[%d].BottomFieldOrderCnt=%d\n", i, m_slice[0].RefPicList0[i].BottomFieldOrderCnt);
        }
    }
    for ( mfxU32 i = 0; i < 32; i++)
    {
        if (VA_INVALID_ID != m_slice[0].RefPicList1[i].picture_id)
        {
            mdprintf(stderr, " ------%d-----\n", i);
            mdprintf(stderr, "m_slice[0].RefPicList1[%d].picture_id=%d\n", i, m_slice[0].RefPicList1[i].picture_id);
            mdprintf(stderr, "m_slice[0].RefPicList1[%d].frame_idx=%d\n", i, m_slice[0].RefPicList1[i].frame_idx);
            mdprintf(stderr, "m_slice[0].RefPicList1[%d].flags=%d\n", i, m_slice[0].RefPicList1[i].flags);
            mdprintf(stderr, "m_slice[0].RefPicList1[%d].TopFieldOrderCnt=%d\n", i, m_slice[0].RefPicList1[i].TopFieldOrderCnt);
            mdprintf(stderr, "m_slice[0].RefPicList1[%d].BottomFieldOrderCnt=%d\n", i, m_slice[0].RefPicList1[i].BottomFieldOrderCnt);
        }
    }
#endif //(mdprintf == fprintf)

    for( mfxU32 i = 0; i < numSlice; i++)
    {
        /* Small correction: legacy use 5,6,7 type values, but for FEI 0,1,2 */
        m_slice[i].slice_type = m_slice[i].slice_type - 5;

        if (NULL != pDataSliceHeader)
        {
            mdprintf(stderr,"Library's generated Slice header (it will be changed now on external one) \n");
            mdprintf(stderr,"---->m_slice[%u].macroblock_address = %d\n", i, m_slice[i].macroblock_address);
            mdprintf(stderr,"---->m_slice[%u].num_macroblocks = %d\n", i, m_slice[i].num_macroblocks);
            mdprintf(stderr,"---->m_slice[%u].slice_type = %d\n", i, m_slice[i].slice_type);
            mdprintf(stderr,"---->m_slice[%u].idr_pic_id = %d\n", i, m_slice[i].idr_pic_id);
            mdprintf(stderr,"---->m_slice[%u].cabac_init_idc = %d\n", i, m_slice[i].cabac_init_idc);
            mdprintf(stderr,"---->m_slice[%u].slice_qp_delta = %d\n", i, m_slice[i].slice_qp_delta);
            mdprintf(stderr,"---->m_slice[%u].disable_deblocking_filter_idc = %d\n", i, m_slice[i].disable_deblocking_filter_idc);
            mdprintf(stderr,"---->m_slice[%u].slice_alpha_c0_offset_div2 = %d\n", i, m_slice[i].slice_alpha_c0_offset_div2);
            mdprintf(stderr,"---->m_slice[%u].slice_beta_offset_div2 = %d\n", i, m_slice[i].slice_beta_offset_div2);

            if (m_slice[i].macroblock_address != pDataSliceHeader->Slice[i].MBAaddress)
            {
                mdprintf(stderr, "!!!Warning pDataSliceHeader->Slice[%u].MBAaddress = %u\n", i, pDataSliceHeader->Slice[i].MBAaddress);
                mdprintf(stderr, "   But library's is m_slice[%u].macroblock_address = %u\n", i, m_slice[i].macroblock_address);
                m_slice[i].macroblock_address = pDataSliceHeader->Slice[i].MBAaddress;
            }
            if (m_slice[i].num_macroblocks != pDataSliceHeader->Slice[i].NumMBs)
            {
                mdprintf(stderr, "!!!Warning pDataSliceHeader->Slice[%u].NumMBs = %u\n", i, pDataSliceHeader->Slice[i].NumMBs);
                mdprintf(stderr, "   But library's is m_slice[%u].num_macroblocks = %u\n", i, m_slice[i].num_macroblocks);
                m_slice[i].num_macroblocks = pDataSliceHeader->Slice[i].NumMBs;
            }
            if (m_slice[i].slice_type != pDataSliceHeader->Slice[i].SliceType)
            {
                mdprintf(stderr, "!!!Warning pDataSliceHeader->Slice[%u].SliceType = %u\n", i, pDataSliceHeader->Slice[i].SliceType);
                mdprintf(stderr, "   But library's is m_slice[%u].slice_type = %u\n", i, m_slice[i].slice_type);
                m_slice[i].slice_type = pDataSliceHeader->Slice[i].SliceType;
            }
            if (pDataSliceHeader->Slice[i].PPSId != m_pps.pic_parameter_set_id)
            {
                /*May be bug in application */
                mdprintf(stderr,"pDataSliceHeader->Slice[%u].PPSId = %u\n", i, pDataSliceHeader->Slice[i].PPSId);
                mdprintf(stderr,"m_pps.pic_parameter_set_id = %u\n", m_pps.pic_parameter_set_id);
            }
            if (m_slice[i].idr_pic_id != pDataSliceHeader->Slice[i].IdrPicId)
            {
                mdprintf(stderr, "!!!Warning pDataSliceHeader->Slice[%u].IdrPicId = %u\n", i, pDataSliceHeader->Slice[i].IdrPicId);
                mdprintf(stderr, "   But library's is m_slice[%u].idr_pic_id = %u\n", i, m_slice[i].idr_pic_id);
                m_slice[i].idr_pic_id  = pDataSliceHeader->Slice[i].IdrPicId;
            }
            if (m_slice[i].cabac_init_idc != pDataSliceHeader->Slice[i].CabacInitIdc)
            {
                mdprintf(stderr, "!!!Warning pDataSliceHeader->Slice[%u].CabacInitIdc = %u\n", i, pDataSliceHeader->Slice[i].CabacInitIdc);
                mdprintf(stderr, "   But library's is m_slice[%u].cabac_init_idc = %u\n", i, m_slice[i].cabac_init_idc);
                m_slice[i].cabac_init_idc = pDataSliceHeader->Slice[i].CabacInitIdc;
            }
            if (m_slice[i].slice_qp_delta != pDataSliceHeader->Slice[i].SliceQPDelta)
            {
                mdprintf(stderr, "!!!Warning pDataSliceHeader->Slice[%u].SliceQPDelta = %u\n", i, pDataSliceHeader->Slice[i].SliceQPDelta);
                mdprintf(stderr, "   But library's is m_slice[%u].slice_qp_delta = %u\n", i, m_slice[i].slice_qp_delta);
                m_slice[i].slice_qp_delta = pDataSliceHeader->Slice[i].SliceQPDelta;
            }

            if (m_slice[i].disable_deblocking_filter_idc != pDataSliceHeader->Slice[i].DisableDeblockingFilterIdc)
            {
                mdprintf(stderr, "!!!Warning pDataSliceHeader->Slice[%u].DisableDeblockingFilterIdc = %u\n", i, pDataSliceHeader->Slice[i].DisableDeblockingFilterIdc);
                mdprintf(stderr, "   But library's is m_slice[%u].disable_deblocking_filter_idc = %u\n", i, m_slice[i].disable_deblocking_filter_idc);
                m_slice[i].disable_deblocking_filter_idc = pDataSliceHeader->Slice[i].DisableDeblockingFilterIdc;
            }
            if (m_slice[i].slice_alpha_c0_offset_div2 != pDataSliceHeader->Slice[i].SliceAlphaC0OffsetDiv2)
            {
                mdprintf(stderr, "!!!Warning pDataSliceHeader->Slice[%u].SliceAlphaC0OffsetDiv2 = %u\n", i, pDataSliceHeader->Slice[i].SliceAlphaC0OffsetDiv2);
                mdprintf(stderr, "   But library's is m_slice[%u].slice_alpha_c0_offset_div2 = %u\n", i, m_slice[i].slice_alpha_c0_offset_div2);
                m_slice[i].slice_alpha_c0_offset_div2 = pDataSliceHeader->Slice[i].SliceAlphaC0OffsetDiv2;
            }
            if (m_slice[i].slice_beta_offset_div2 != pDataSliceHeader->Slice[i].SliceBetaOffsetDiv2)
            {
                mdprintf(stderr, "!!!Warning pDataSliceHeader->Slice[%u].SliceBetaOffsetDiv2 = %u\n", i, pDataSliceHeader->Slice[i].SliceBetaOffsetDiv2);
                mdprintf(stderr, "   But library's is m_slice[%u].slice_beta_offset_div2 = %u\n", i, m_slice[i].slice_beta_offset_div2);
                m_slice[i].slice_beta_offset_div2 = pDataSliceHeader->Slice[i].SliceBetaOffsetDiv2;
            }
            mdprintf(stderr,"Application's generated Slice header \n");
            mdprintf(stderr,"---->m_slice[%u].macroblock_address = %d\n", i, m_slice[i].macroblock_address);
            mdprintf(stderr,"---->m_slice[%u].num_macroblocks = %d\n", i, m_slice[i].num_macroblocks);
            mdprintf(stderr,"---->m_slice[%u].slice_type = %d\n", i, m_slice[i].slice_type);
            mdprintf(stderr,"---->m_slice[%u].idr_pic_id = %d\n", i, m_slice[i].idr_pic_id);
            mdprintf(stderr,"---->m_slice[%u].cabac_init_idc = %d\n", i, m_slice[i].cabac_init_idc);
            mdprintf(stderr,"---->m_slice[%u].slice_qp_delta = %d\n", i, m_slice[i].slice_qp_delta);
            mdprintf(stderr,"---->m_slice[%u].disable_deblocking_filter_idc = %d\n", i, m_slice[i].disable_deblocking_filter_idc);
            mdprintf(stderr,"---->m_slice[%u].slice_alpha_c0_offset_div2 = %d\n", i, m_slice[i].slice_alpha_c0_offset_div2);
            mdprintf(stderr,"---->m_slice[%u].slice_beta_offset_div2 = %d\n", i, m_slice[i].slice_beta_offset_div2);
        }

        if ((m_slice[i].slice_type == SLICE_TYPE_P) || (m_slice[i].slice_type == SLICE_TYPE_B))
        {
            for (ref_counter_l0 = 0; ref_counter_l0 < maxNumRefL0; ref_counter_l0++)
            {
                mfxHDL handle;
                mfxU16 indexFromSliceHeader = 0;
                if (NULL != pDataSliceHeader)
                {
                    indexFromSliceHeader = pDataSliceHeader->Slice[i].RefL0[ref_counter_l0].Index;
                    mfxSts = m_core->GetExternalFrameHDL(in->L0Surface[indexFromSliceHeader]->Data.MemId, &handle);
                    MFX_CHECK(MFX_ERR_NONE == mfxSts, MFX_ERR_INVALID_HANDLE);
                    //m_slice[i].RefPicList0[ref_counter_l0].picture_id = *(VASurfaceID*) handle;
                    VASurfaceID s = *(VASurfaceID*) handle; //id in the memory by ptr
                    if (m_slice[i].RefPicList0[ref_counter_l0].picture_id != s)
                    {
                        mdprintf(stderr, "!!!Warning picture_id from pDataSliceHeader->Slice[%u].RefL0[%u] = %u\n", i, ref_counter_l0, s);
                        mdprintf(stderr, "   But library's is m_slice[%u].RefPicList0[%u].picture_id = %u\n",
                                i, ref_counter_l0, m_slice[i].RefPicList0[ref_counter_l0].picture_id);
                        m_slice[i].RefPicList0[ref_counter_l0].picture_id = s;
                    }
                    if (m_slice[i].RefPicList0[ref_counter_l0].flags != pDataSliceHeader->Slice[i].RefL0[ref_counter_l0].PictureType)
                    {

                        mdprintf(stderr, "!!!Warning pDataSliceHeader->Slice[%u].RefL0[%u].PictureType = %u\n",
                                i, ref_counter_l0, pDataSliceHeader->Slice[i].RefL0[ref_counter_l0].PictureType);
                        mdprintf(stderr, "   But library's is m_slice[%u].RefPicList0[%u].flag = %u\n",
                                i, ref_counter_l0, m_slice[i].RefPicList0[ref_counter_l0].flags & 0x0f);
                        m_slice[i].RefPicList0[ref_counter_l0].flags = pDataSliceHeader->Slice[i].RefL0[ref_counter_l0].PictureType;
                    }
                }
            } /*for (ref_counter_l0 = 0; ref_counter_l0 < in->NumFrameL0; ref_counter_l0++)*/
            for ( ; ref_counter_l0 < 32; ref_counter_l0++)
            {
                m_slice[i].RefPicList0[ref_counter_l0].picture_id = VA_INVALID_ID;
                m_slice[i].RefPicList0[ref_counter_l0].flags = VA_PICTURE_H264_INVALID;
            }
        } // if ((in->NumFrameL0) &&
        if (m_slice[i].slice_type == SLICE_TYPE_B)
        {
            for (ref_counter_l1 = 0; ref_counter_l1 < maxNumRefL1; ref_counter_l1++)
            {
                mfxHDL handle;
                mfxU16 indexFromSliceHeader = 0;
                if (NULL != pDataSliceHeader)
                {
                    indexFromSliceHeader = pDataSliceHeader->Slice[i].RefL1[ref_counter_l1].Index;
                    mfxSts = m_core->GetExternalFrameHDL(in->L0Surface[indexFromSliceHeader]->Data.MemId, &handle);
                    MFX_CHECK(MFX_ERR_NONE == mfxSts, MFX_ERR_INVALID_HANDLE);
                    //m_slice[i].RefPicList1[ref_counter_l1].picture_id = *(VASurfaceID*) handle;
                    VASurfaceID s = *(VASurfaceID*) handle; //id in the memory by ptr
                    if (m_slice[i].RefPicList1[ref_counter_l1].picture_id != s)
                    {
                        //m_slice[i].RefPicList1[ref_counter_l1].picture_id = s;
                        mdprintf(stderr, "!!!Warning picture_id from pDataSliceHeader->Slice[%u].RefL1[%u] = %u\n", i, ref_counter_l1, s);
                        mdprintf(stderr, "   But library's is m_slice[%u].RefPicList0[%u].picture_id = %u\n",
                                i, ref_counter_l1, m_slice[i].RefPicList1[ref_counter_l1].picture_id);
                    }
                    if (m_slice[i].RefPicList1[ref_counter_l1].flags != pDataSliceHeader->Slice[i].RefL1[ref_counter_l1].PictureType)
                    {
                        //m_slice[i].RefPicList1[ref_counter_l1].flags = pDataSliceHeader->Slice[i].RefL1[ref_counter_l1].PictureType;
                        mdprintf(stderr, "!!!Warning pDataSliceHeader->Slice[%u].RefL1[%u].PictureType = %u\n",
                                i, ref_counter_l0, pDataSliceHeader->Slice[i].RefL1[ref_counter_l1].PictureType);
                        mdprintf(stderr, "   But library's is m_slice[%u].RefPicList1[%u].flag = %u\n",
                                i, ref_counter_l1, m_slice[i].RefPicList1[ref_counter_l1].flags);
                    }
                }
            } /* for (ref_counter_l1 = 0; ref_counter_l1 < in->NumFrameL1; ref_counter_l1++) */
            for ( ; ref_counter_l1 < 32; ref_counter_l1++)
            {
                m_slice[i].RefPicList1[ref_counter_l1].picture_id = VA_INVALID_ID;
                m_slice[i].RefPicList1[ref_counter_l1].flags = VA_PICTURE_H264_INVALID;
            }
        } //if ( (in->NumFrameL1) && (m_slice[i].slice_type == SLICE_TYPE_B) )
    } // for( size_t i = 0; i < m_slice.size(); ++i, divider.Next() )

    mfxU32 prefix_bytes = (task.m_AUStartsFromSlice[fieldId] + 8) * m_headerPacker.isSvcPrefixUsed();
//    std::vector<ENCODE_PACKEDHEADER_DATA> const & packedSliceArray = m_headerPacker.GetSlices();
//    ENCODE_PACKEDHEADER_DATA const & packedSlice = packedSliceArray[0];
//    /* Dump SPS */
//    mdprintf(stderr,"Dump of packed Slice Header (Before)\n");
//    for (mfxU32 i = 0; i < (packedSlice.DataLength+7)/8; i++)
//    {
//        mdprintf(stderr,"packedSlice.pData[%u]=%u\n", i, packedSlice.pData[i]);
//    }

//    //Slice headers only
    std::vector<ENCODE_PACKEDHEADER_DATA> const & packedSlices = m_headerPacker.PackSlices(task, fieldId);
//    mdprintf(stderr,"Dump of packed Slice Header (AFTER)\n");
//    for (mfxU32 i = 0; i < (packedSlice.DataLength+7)/8; i++)
//    {
//        mdprintf(stderr,"packedSlice.pData[%u]=%u\n", i, packedSlice.pData[i]);
//    }

    for (size_t i = 0; i < packedSlices.size(); i++)
    {
        ENCODE_PACKEDHEADER_DATA const & packedSlice = packedSlices[i];

        if (prefix_bytes)
        {
            packed_header_param_buffer.type = VAEncPackedHeaderRawData;
            packed_header_param_buffer.has_emulation_bytes = 1;
            packed_header_param_buffer.bit_length = (prefix_bytes * 8);

            vaSts = vaCreateBuffer(m_vaDisplay,
                    m_vaContextEncode,
                    VAEncPackedHeaderParameterBufferType,
                    sizeof(packed_header_param_buffer),
                    1,
                    &packed_header_param_buffer,
                    &m_packedSvcPrefixHeaderBufferId[i]);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            vaSts = vaCreateBuffer(m_vaDisplay,
                                m_vaContextEncode,
                                VAEncPackedHeaderDataBufferType,
                                 prefix_bytes, 1, packedSlice.pData,
                                &m_packedSvcPrefixBufferId[i]);
            MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

            configBuffers[buffersCount++] = m_packedSvcPrefixHeaderBufferId[i];
            configBuffers[buffersCount++] = m_packedSvcPrefixBufferId[i];
        }

        packed_header_param_buffer.type = VAEncPackedHeaderH264_Slice;
        packed_header_param_buffer.has_emulation_bytes = 0;
        packed_header_param_buffer.bit_length = packedSlice.DataLength - (prefix_bytes * 8); // DataLength is already in bits !

        //MFX_DESTROY_VABUFFER(m_packeSliceHeaderBufferId[i], m_vaDisplay);
        vaSts = vaCreateBuffer(m_vaDisplay,
                m_vaContextEncode,
                VAEncPackedHeaderParameterBufferType,
                sizeof(packed_header_param_buffer),
                1,
                &packed_header_param_buffer,
                &m_packeSliceHeaderBufferId[i]);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        //MFX_DESTROY_VABUFFER(m_packedSliceBufferId[i], m_vaDisplay);
        vaSts = vaCreateBuffer(m_vaDisplay,
                            m_vaContextEncode,
                            VAEncPackedHeaderDataBufferType,
                            (packedSlice.DataLength + 7) / 8 - prefix_bytes, 1, packedSlice.pData + prefix_bytes,
                            &m_packedSliceBufferId[i]);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

        configBuffers[buffersCount++] = m_packeSliceHeaderBufferId[i];
        configBuffers[buffersCount++] = m_packedSliceBufferId[i];
    }

    for( size_t i = 0; i < m_slice.size(); i++ )
    {
        //MFX_DESTROY_VABUFFER(m_sliceBufferId[i], m_vaDisplay);
        vaSts = vaCreateBuffer(m_vaDisplay,
                                m_vaContextEncode,
                                VAEncSliceParameterBufferType,
                                sizeof(m_slice[i]),
                                1,
                                &m_slice[i],
                                &m_sliceBufferId[i]);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        //configBuffers[buffersCount++] = m_sliceBufferId[i];
        mdprintf(stderr, "m_sliceBufferId[%zu]=%d\n", i, m_sliceBufferId[i]);
    }

    assert(buffersCount <= configBuffers.size());

    //------------------------------------------------------------------
    // Rendering
    //------------------------------------------------------------------

    mfxHDL handle_1;
    mfxSts = m_core->GetExternalFrameHDL(in->InSurface->Data.MemId, &handle_1);
    MFX_CHECK(MFX_ERR_NONE == mfxSts, MFX_ERR_INVALID_HANDLE);
    inputSurface = (VASurfaceID*) handle_1; //id in the memory by ptr
    /* Below log only for debug  */
#if (mdprintf == fprintf)

    mdprintf(stderr, "task.m_frameNum=%d\n", task.m_frameNum);
    mdprintf(stderr, "inputSurface=%d\n", *inputSurface);
    mdprintf(stderr, "m_pps.CurrPic.picture_id=%d\n", m_pps.CurrPic.picture_id);
    mdprintf(stderr, "m_pps.CurrPic.frame_idx=%d\n", m_pps.CurrPic.frame_idx);
    mdprintf(stderr, "m_pps.CurrPic.flags=%d\n",m_pps.CurrPic.flags);
    mdprintf(stderr, "m_pps.CurrPic.TopFieldOrderCnt=%d\n", m_pps.CurrPic.TopFieldOrderCnt);
    mdprintf(stderr, "m_pps.CurrPic.BottomFieldOrderCnt=%d\n", m_pps.CurrPic.BottomFieldOrderCnt);

    mdprintf(stderr, "-------------------------------------------\n");

    int i = 0;

    for ( i = 0; i < 16; i++)
    {
        if (VA_INVALID_ID != m_pps.ReferenceFrames[i].picture_id)
        {
            mdprintf(stderr, " ------%d-----\n", i);
            mdprintf(stderr, "m_pps.ReferenceFrames[%d].picture_id=%d\n", i, m_pps.ReferenceFrames[i].picture_id);
            mdprintf(stderr, "m_pps.ReferenceFrames[%d].frame_idx=%d\n", i, m_pps.ReferenceFrames[i].frame_idx);
            mdprintf(stderr, "m_pps.ReferenceFrames[%d].flags=%d\n", i, m_pps.ReferenceFrames[i].flags);
            mdprintf(stderr, "m_pps.ReferenceFrames[%d].TopFieldOrderCnt=%d\n", i, m_pps.ReferenceFrames[i].TopFieldOrderCnt);
            mdprintf(stderr, "m_pps.ReferenceFrames[%d].BottomFieldOrderCnt=%d\n", i, m_pps.ReferenceFrames[i].BottomFieldOrderCnt);
        }
    }

    for ( i = 0; i < 32; i++)
    {
        if (VA_INVALID_ID != m_slice[0].RefPicList0[i].picture_id)
        {
            mdprintf(stderr, " ------%d-----\n", i);
            mdprintf(stderr, "m_slice[0].RefPicList0[%d].picture_id=%d\n", i, m_slice[0].RefPicList0[i].picture_id);
            mdprintf(stderr, "m_slice[0].RefPicList0[%d].frame_idx=%d\n", i, m_slice[0].RefPicList0[i].frame_idx);
            mdprintf(stderr, "m_slice[0].RefPicList0[%d].flags=%d\n", i, m_slice[0].RefPicList0[i].flags);
            mdprintf(stderr, "m_slice[0].RefPicList0[%d].TopFieldOrderCnt=%d\n", i, m_slice[0].RefPicList0[i].TopFieldOrderCnt);
            mdprintf(stderr, "m_slice[0].RefPicList0[%d].BottomFieldOrderCnt=%d\n", i, m_slice[0].RefPicList0[i].BottomFieldOrderCnt);
        }
    }

    for ( i = 0; i < 32; i++)
    {
        if (VA_INVALID_ID != m_slice[0].RefPicList1[i].picture_id)
        {
            mdprintf(stderr, " ------%d-----\n", i);
            mdprintf(stderr, "m_slice[0].RefPicList1[%d].picture_id=%d\n", i, m_slice[0].RefPicList1[i].picture_id);
            mdprintf(stderr, "m_slice[0].RefPicList1[%d].frame_idx=%d\n", i, m_slice[0].RefPicList1[i].frame_idx);
            mdprintf(stderr, "m_slice[0].RefPicList1[%d].flags=%d\n", i, m_slice[0].RefPicList1[i].flags);
            mdprintf(stderr, "m_slice[0].RefPicList1[%d].TopFieldOrderCnt=%d\n", i, m_slice[0].RefPicList1[i].TopFieldOrderCnt);
            mdprintf(stderr, "m_slice[0].RefPicList1[%d].BottomFieldOrderCnt=%d\n", i, m_slice[0].RefPicList1[i].BottomFieldOrderCnt);
        }
    }
#endif // #if (mdprintf == fprintf)

    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc vaBeginPicture");
        vaSts = vaBeginPicture(
                m_vaDisplay,
                m_vaContextEncode,
                *inputSurface);

        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        mdprintf(stderr,"inputSurface = %d\n",*inputSurface);
    }
    {
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc vaRenderPicture");
        mdprintf(stderr, "va_buffers to render: [");
        for (int i = 0; i < buffersCount; i++)
            mdprintf(stderr, " %d", configBuffers[i]);
        mdprintf(stderr, "]\n");

        vaSts = vaRenderPicture(
                m_vaDisplay,
                m_vaContextEncode,
                &configBuffers[0], /* vector store leaner in memory*/
                buffersCount);
        MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
        for(size_t i = 0; i < m_slice.size(); i++)
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
        MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "PreEnc vaEndPicture");
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
        currentFeedback.number = task.m_statusReportNumber[feiFieldId];
        currentFeedback.surface = *inputSurface;
        currentFeedback.mv        = m_vaFeiMVOutId[feiFieldId];
        //currentFeedback.mbstat    = vaFeiMBStatId;
        currentFeedback.mbcode    = m_vaFeiMCODEOutId[feiFieldId];
        m_statFeedbackCache.push_back(currentFeedback);
        //m_feedbackCache.push_back(currentFeedback);
    }

    MFX_DESTROY_VABUFFER(vaFeiFrameControlId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(vaFeiMVPredId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(vaFeiMBControlId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(vaFeiMBQPId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_packedSpsHeaderBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_packedSpsBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_spsBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_ppsBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_packedPpsHeaderBufferId, m_vaDisplay);
    MFX_DESTROY_VABUFFER(m_packedPpsBufferId, m_vaDisplay);

    for( size_t i = 0; i < m_slice.size(); i++ )
    {
        MFX_DESTROY_VABUFFER(m_sliceBufferId[i], m_vaDisplay);
        MFX_DESTROY_VABUFFER(m_packeSliceHeaderBufferId[i], m_vaDisplay);
        MFX_DESTROY_VABUFFER(m_packedSliceBufferId[i], m_vaDisplay);
    }

    mdprintf(stderr, "submit_vaapi done: %d\n", task.m_frameOrder);
    return MFX_ERR_NONE;

} // mfxStatus VAAPIEncoder::Execute(ExecuteBuffers& data, mfxU32 fieldId)


mfxStatus VAAPIFEIPAKEncoder::QueryStatus(
        DdiTask & task,
        mfxU32 fieldId)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc QueryStatus");
    VAStatus vaSts;
    mfxStatus sts = MFX_ERR_NONE;

    mdprintf(stderr, "query_vaapi status: %d\n", task.m_frameOrder);
    //------------------------------------------
    // (1) mapping feedbackNumber -> surface & bs
    bool isFound = false;
    VASurfaceID waitSurface;
    VABufferID vaFeiMBStatId = VA_INVALID_ID;
    VABufferID vaFeiMBCODEOutId = VA_INVALID_ID;
    VABufferID vaFeiMVOutId = VA_INVALID_ID;
    mfxU32 indxSurf;

    mfxU32 feiFieldId = fieldId;

    /*Input FEI Ext buffer sequence for BFF is: bff tff bff tff
     * So, MSDK need to swap feiFieldId values to get correct buffer */
    if (MFX_PICSTRUCT_FIELD_BFF == m_videoParam.mfx.FrameInfo.PicStruct)
    {
        if (1 == feiFieldId)
            feiFieldId = 0;
        else
            feiFieldId = 1;
    }

    UMC::AutomaticUMCMutex guard(m_guard);

    for (indxSurf = 0; indxSurf < m_statFeedbackCache.size(); indxSurf++)
    {
        ExtVASurface currentFeedback = m_statFeedbackCache[ indxSurf ];

        if (currentFeedback.number == task.m_statusReportNumber[feiFieldId])
        {
            waitSurface = currentFeedback.surface;
            vaFeiMBCODEOutId = currentFeedback.mbcode;
            vaFeiMVOutId = currentFeedback.mv;
            isFound = true;

            break;
        }
    }

    if (!isFound) {
        return MFX_ERR_UNKNOWN;
    }

    VASurfaceStatus surfSts = VASurfaceSkipped;

    vaSts = vaSyncSurface(m_vaDisplay, waitSurface);
    // following code is workaround:
    // because of driver bug it could happen that decoding error will not be returned after decoder sync
    // and will be returned at subsequent encoder sync instead
    // just ignore VA_STATUS_ERROR_DECODING_ERROR in encoder
    if (vaSts == VA_STATUS_ERROR_DECODING_ERROR)
        vaSts = VA_STATUS_SUCCESS;
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    vaSts = vaQuerySurfaceStatus(m_vaDisplay, waitSurface, &surfSts);
    MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);

    mdprintf(stderr, "sync on surface: %d\n", surfSts);

    switch (surfSts)
    {
        default: //for now driver does not return correct status
        case VASurfaceReady:
        {
            VACodedBufferSegment *codedBufferSegment;
            {
                MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc vaMapBuffer");
                vaSts = vaMapBuffer(
                    m_vaDisplay,
                    m_codedBufferId[feiFieldId],
                    (void **)(&codedBufferSegment));
                MFX_CHECK_WITH_ASSERT(VA_STATUS_SUCCESS == vaSts, MFX_ERR_DEVICE_FAILED);
            }

            task.m_bsDataLength[feiFieldId] = codedBufferSegment->size;
            memcpy_s(task.m_bs->Data + task.m_bs->DataLength, codedBufferSegment->size,
                                     codedBufferSegment->buf, codedBufferSegment->size);
            task.m_bs->DataLength += codedBufferSegment->size;
            {
                MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_SCHED, "Enc vaUnmapBuffer");
                vaUnmapBuffer( m_vaDisplay, m_codedBufferId[feiFieldId] );
                // ??? may affect for performance
                MFX_DESTROY_VABUFFER(m_codedBufferId[feiFieldId],m_vaDisplay);
            }

            MFX_DESTROY_VABUFFER(m_vaFeiMVOutId[feiFieldId],m_vaDisplay);
            MFX_DESTROY_VABUFFER(m_vaFeiMCODEOutId[feiFieldId],m_vaDisplay);
            // remove task
            m_statFeedbackCache.erase(m_statFeedbackCache.begin() + indxSurf);
        }
            sts = MFX_ERR_NONE;
            break;
        case VASurfaceRendering:
        case VASurfaceDisplaying:
            sts = MFX_WRN_DEVICE_BUSY;
            break;

        case VASurfaceSkipped:
            //default:
            assert(!"bad feedback status");
            sts =MFX_ERR_DEVICE_FAILED;
            //return MFX_ERR_NONE;
            break;
    }

    mdprintf(stderr, "query_vaapi done\n");
    return sts;
} // mfxStatus VAAPIFEIENCEncoder::QueryStatus(mfxU32 feedbackNumber, mfxU32& bytesWritten)

#endif //defined(MFX_ENABLE_H264_VIDEO_FEI_ENC) && defined(MFX_ENABLE_H264_VIDEO_FEI_PAK)

#endif // (MFX_ENABLE_H264_VIDEO_ENCODE) && (MFX_VA_LINUX)
/* EOF */

