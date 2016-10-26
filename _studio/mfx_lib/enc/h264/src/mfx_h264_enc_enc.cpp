//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2012 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H264_VIDEO_ENC)

#include "mfx_h264_enc_common.h"
#include "mfx_h264_enc_enc.h"
#include "mfx_h264_ex_param_buf.h"
#include <umc_video_processing.h>

#define TMP
#ifdef TMP
typedef struct _mfxExtCUCRefList_o {
    mfxU32 CucId;
    mfxU32 CucSz;
    mfxFrameCUC* Refs[20];
} mfxExtCUCRefList_o;
#endif

static Ipp32s sub8x8[4] = {0,2,8,10};

#define CHECK_VERSION(ver)
#define CHECK_CODEC_ID(id, myid)
#define CHECK_FUNCTION_ID(id, myid)

MFXVideoEncH264* CreateMFXVideoEncH264(VideoCORE *core, mfxStatus *status)
{
    return (MFXVideoEncH264*)new MFXVideoEncH264(core, status);
}


MFXVideoEncH264::MFXVideoEncH264(VideoCORE *core, mfxStatus *status)
:  VideoENC()
{
    Status res = UMC_OK;
    *status = MFX_ERR_NONE;

    ippStaticInit();
    m_pMFXCore  = core;
    m_InitFlag = 0;

#define H264ENC_MAKE_NAME(NAME) NAME##_8u16s
    H264ENC_CALL_NEW(res, H264CoreEncoder, enc);
#undef H264ENC_MAKE_NAME
    if (res != UMC_OK) *status = MFX_ERR_NOT_INITIALIZED;

    enc->m_pCurrentFrame = NULL;
    enc->m_pReconstructFrame = NULL;
    memset(m_refs, 0, sizeof(m_refs));
}

MFXVideoEncH264::~MFXVideoEncH264(void)
{
    Close();
    if( enc != NULL ){
        H264CoreEncoder_Destroy_8u16s(enc);
        H264_Free(enc);
        enc = NULL;
    }
}

mfxStatus MFXVideoEncH264::Reset(mfxVideoParam *par)
{
    if (par == NULL)
        return MFX_ERR_NULL_PTR;
    Close();
    return Init(par);
}

void MFXVideoEncH264::ConvertCUC_ToInternalStructure( mfxFrameCUC *cuc )
{
    //uNumSlices = cuc->NumSlice;
    enc->m_total_bits_encoded = 0; //Reset for current frame
    //m_PicType =

    const mfxFrameParam* fp = cuc->FrameParam;
    const mfxSliceParam* sp = cuc->SliceParam;

    enc->m_info.rate_controls.quantI =
    enc->m_info.rate_controls.quantP =
    enc->m_info.rate_controls.quantB = fp->AVC.PicInitQpMinus26 + 26;// + cuc->SliceParam->SliceQpDelta; //TO BE FIXED

    enc->m_WidthInMBs = fp->AVC.FrameWinMbMinus1 + 1;
    enc->m_HeightInMBs = fp->AVC.FrameHinMbMinus1 + 1;
    enc->m_info.chroma_format_idc = fp->AVC.ChromaFormatIdc;

    //Init picture parameters
    enc->m_PicParamSet.pic_parameter_set_id = 0;
    enc->m_PicParamSet.seq_parameter_set_id = 0;
    enc->m_PicParamSet.entropy_coding_mode = fp->AVC.EntropyCodingModeFlag;
    enc->m_PicParamSet.pic_order_present_flag = fp->AVC.PicOrderPresent;
    enc->m_PicParamSet.num_slice_groups = fp->AVC.NumSliceGroupMinus1 + 1;
    enc->m_PicParamSet.num_ref_idx_l0_active = fp->AVC.NumRefIdxL0Minus1 + 1;
    enc->m_PicParamSet.num_ref_idx_l1_active = fp->AVC.NumRefIdxL1Minus1 + 1;
    enc->m_PicParamSet.weighted_pred_flag = fp->AVC.WeightedPredFlag;
    enc->m_PicParamSet.weighted_bipred_idc = fp->AVC.WeightedBipredIdc;
    enc->m_PicParamSet.pic_init_qp = fp->AVC.PicInitQpMinus26 + 26;
    enc->m_PicParamSet.pic_init_qs = fp->AVC.PicInitQsMinus26 + 26;
    enc->m_PicParamSet.chroma_qp_index_offset = fp->AVC.ChromaQp1stOffset;
//    m_PicParamSet.deblocking_filter_variables_present_flag =
//    m_PicParamSet.constrained_intra_pred_flag = fp->ConstraintIntraPredFlag;
    enc->m_PicParamSet.redundant_pic_cnt_present_flag = fp->AVC.RedundantPicCntPresent;
    enc->m_PicParamSet.transform_8x8_mode_flag = enc->m_info.transform_8x8_mode_flag = fp->AVC.Transform8x8Flag;
    enc->m_PicParamSet.pic_scaling_matrix_present_flag = fp->AVC.ScalingListPresent;
    enc->m_PicParamSet.second_chroma_qp_index_offset = fp->AVC.ChromaQp2ndOffset;

//Sequence parameters
    enc->m_SeqParamSet.profile_idc = enc->m_info.profile_idc;
    enc->m_SeqParamSet.level_idc = enc->m_info.level_idc;
    //m_SeqParamSet.constraint_set0_flag;
    //m_SeqParamSet.constraint_set1_flag;
    //m_SeqParamSet.constraint_set2_flag;
    //m_SeqParamSet.constraint_set3_flag;

    //m_SeqParamSet.seq_parameter_set_id;
    enc->m_SeqParamSet.chroma_format_idc = fp->AVC.ChromaFormatIdc;
    enc->m_SeqParamSet.residual_colour_transform_flag = fp->AVC.ResColorTransFlag;
    enc->m_SeqParamSet.bit_depth_luma = fp->AVC.BitDepthLumaMinus8 + 8;
    enc->m_SeqParamSet.bit_depth_chroma = fp->AVC.BitDepthChromaMinus8 + 8;
    //m_SeqParamSet.qpprime_y_zero_transform_bypass_flag;
    //m_SeqParamSet.seq_scaling_matrix_present_flag;
    if( fp->AVC.PicOrderCntType == 2 ){
          enc->m_SeqParamSet.pic_order_cnt_type = 2;
          enc->m_SeqParamSet.log2_max_pic_order_cnt_lsb = 0;
    }else{
          enc->m_SeqParamSet.pic_order_cnt_type = 0;
          enc->m_SeqParamSet.log2_max_pic_order_cnt_lsb = cuc->FrameParam->AVC.Log2MaxPicOrdCntMinus4 + 4;
    }
    enc->m_SeqParamSet.log2_max_frame_num = fp->AVC.Log2MaxFrameCntMinus4 + 4;
    enc->m_info.num_ref_frames = enc->m_SeqParamSet.num_ref_frames = fp->AVC.NumRefFrame;
    enc->m_SeqParamSet.gaps_in_frame_num_value_allowed_flag = fp->AVC.GapsInFrameNumAllowed;
    enc->m_SeqParamSet.frame_width_in_mbs = fp->AVC.FrameWinMbMinus1 + 1;
    enc->m_SeqParamSet.frame_height_in_mbs = fp->AVC.FrameHinMbMinus1 + 1;
    enc->m_SeqParamSet.frame_mbs_only_flag = fp->AVC.FrameMbsOnlyFlag;
    enc->m_SeqParamSet.mb_adaptive_frame_field_flag = fp->AVC.MbaffFrameFlag;
    enc->m_SeqParamSet.direct_8x8_inference_flag = fp->AVC.Direct8x8InferenceFlag;
    //m_SeqParamSet.frame_cropping_flag;
    //m_SeqParamSet.frame_crop_left_offset;
    //m_SeqParamSet.frame_crop_right_offset;
    //m_SeqParamSet.frame_crop_top_offset;
    //m_SeqParamSet.frame_crop_bottom_offset;
    enc->m_SeqParamSet.vui_parameters_present_flag = 0; //Set 0 for now

//    fp->SliceGroupMapType;
//    fp->SliceGroupXRateMinus1;
    //;
    //fp->MaxFrameSize;
    //fp->FieldPicFlag;
    //fp->SPforSwitchFlag;
//    fp->RefPicFlag;
    //fp->mbsConsecutiveFlag;
    //;
    //fp->NoMinorBipredFlag;
    //fp->IntraPicFlag;
    //fp->TransCoeffFlag;
    //fp->ZeroDeltaPicOrderFlag;
    //fp->SliceGroupMapPresent; //ext buffer
    //fp->ILDBControlPresent; //ext buffer
    //fp-> MBCodePresent?
    //fp->ResDataPresent?

//Common slice parameters
    //m_SliceHeader.pic_parameter_set_id;
//    m_SliceHeader.frame_num = ;
    enc->m_SliceHeader.field_pic_flag;
    enc->m_SliceHeader.bottom_field_flag;
    enc->m_SliceHeader.idr_pic_id = sp->AVC.IdrPictureId;
    enc->m_SliceHeader.pic_order_cnt_lsb = cuc->FrameSurface->Data[cuc->FrameParam->AVC.CurrFrameLabel]->FrameOrder;
    enc->m_SliceHeader.delta_pic_order_cnt_bottom;
    enc->m_SliceHeader.redundant_pic_cnt = sp->AVC.RedundantPicCnt;
    enc->m_SliceHeader.direct_spatial_mv_pred_flag = sp->AVC.DirectPredType;
    enc->m_SliceHeader.adaptive_ref_pic_marking_mode_flag;
    enc->m_SliceHeader.sp_for_switch_flag;
    enc->m_SliceHeader.slice_qs_delta = sp->AVC.SliceQsDelta;
}



mfxStatus MFXVideoEncH264::AllocateFrames( mfxVideoParam* parMFX )
{
    mfxI32 i,w,h;
    UMC::ColorFormat cf = UMC::NONE;

    w = GetWidth( parMFX );
    h = GetHeight( parMFX );
    IppiSize padded_size;
    padded_size.width  = (w  + 15) & ~15;
    padded_size.height = (h + (16<<(1 - 1/*m_SeqParamSet.frame_mbs_only_flag*/)) - 1) & ~((16<<(1 - 1/*m_SeqParamSet.frame_mbs_only_flag*/)) - 1);

    if( parMFX->mfx.FrameInfo.FourCC == MFX_FOURCC_NV12 ){
        cf = UMC::NV12;
    }else{
        cf = ConvertColorFormatToUMC( parMFX->mfx.FrameInfo.ChromaFormat );
    }

    VideoData rFrame;
    rFrame.Init( w, h,  cf, 8 );

    for( i = 0; i<MAX_FRAMES; i++ ){
        m_refs[i] = (H264EncoderFrame_8u16s*)H264_Malloc(sizeof(H264EncoderFrame_8u16s));
        if (!m_refs[i]) return MFX_ERR_MEMORY_ALLOC;

        H264EncoderFrame_Create_8u16s( m_refs[i], &rFrame, &m_Allocator, 0, 0 );
        if (H264EncoderFrame_allocateParsedFrameData_8u16s(m_refs[i], padded_size, parMFX->mfx.NumSlice) != UMC::UMC_OK)
            return MFX_ERR_MEMORY_ALLOC;

        m_refs[i]->m_wasEncoded = true;
    }

    //Allocate current frame
    enc->m_pCurrentFrame = (H264EncoderFrame_8u16s*)ippsMalloc_8u(sizeof(H264EncoderFrame_8u16s));
    if (!enc->m_pCurrentFrame) return MFX_ERR_MEMORY_ALLOC;
    H264EncoderFrame_Create_8u16s( enc->m_pCurrentFrame, &rFrame, &m_Allocator, 0, 0 );
    if (H264EncoderFrame_allocateParsedFrameData_8u16s(enc->m_pCurrentFrame, padded_size, parMFX->mfx.NumSlice) != UMC::UMC_OK)
        return MFX_ERR_MEMORY_ALLOC;

    //Allocate reconstruct frame
    enc->m_pReconstructFrame = (H264EncoderFrame_8u16s*)ippsMalloc_8u(sizeof(H264EncoderFrame_8u16s));
    if (!enc->m_pReconstructFrame) return MFX_ERR_MEMORY_ALLOC;
    H264EncoderFrame_Create_8u16s( enc->m_pReconstructFrame, &rFrame, &m_Allocator, 0, 0 );
    if( H264EncoderFrame_allocate_8u16s(enc->m_pReconstructFrame, padded_size, parMFX->mfx.NumSlice) != UMC_OK ) return MFX_ERR_MEMORY_ALLOC;

    return MFX_ERR_NONE;
}

void MFXVideoEncH264::DeallocateFrames()
{
    H264EncoderFrameList_clearFrameList_8u16s(&enc->m_dpb);
    H264EncoderFrameList_clearFrameList_8u16s(&enc->m_cpb);
    if( enc->m_pReconstructFrame != NULL ){
        H264EncoderFrame_Destroy_8u16s(enc->m_pReconstructFrame);
        ippFree( enc->m_pReconstructFrame );
        enc->m_pReconstructFrame = NULL;
    }
    if( enc->m_pCurrentFrame != NULL ){
        H264EncoderFrame_Destroy_8u16s(enc->m_pCurrentFrame);
        ippFree( enc->m_pCurrentFrame );
        enc->m_pCurrentFrame = NULL;
    }
    for( mfxU32 i = 0; i<MAX_FRAMES; i++ ){
        if( m_refs[i] ){
            H264EncoderFrame_Destroy_8u16s(m_refs[i]);
            ippFree( m_refs[i] );
            m_refs[i] = NULL;
        }
    }
}

mfxStatus MFXVideoEncH264::Init(mfxVideoParam *parMFX)
{
    UMC::H264EncoderParams h264_VideoParams;
    UMC::Status stsUMC;
    mfxStatus st;

    if (!m_pMFXCore || !parMFX) return MFX_ERR_MEMORY_ALLOC;
    MFX_CHECK_COND(parMFX->mfx.CodecId == MFX_CODEC_AVC);

    Close();
    UMC::MemoryAllocatorParams MAparams;
    if ( m_Allocator.InitMem(&MAparams, m_pMFXCore) != UMC::UMC_OK) return MFX_ERR_MEMORY_ALLOC;

    UMC::H264EncoderParams parUMC;
    st = ConvertVideoParam_H264enc(parMFX, &parUMC);
    MFX_CHECK_STS(st);
    m_VideoParam = *parMFX;
    stsUMC = H264CoreEncoder_Init_8u16s(enc, &parUMC, &m_Allocator);
    H264CoreEncoder_SetPictureParameters_8u16s(enc);

    if( parMFX->mfx.NumSlice > 0 && enc->m_info.num_slices != parMFX->mfx.NumSlice ) return MFX_ERR_UNSUPPORTED;

/* Allocate buffers for frames */
    if( stsUMC == UMC::UMC_OK ){
        mfxStatus status = AllocateFrames(parMFX);
        m_InitFlag = 1;
        if(status != MFX_ERR_NONE ){
           Close();
        }
        return status;
    }else
        Close();

    return ConvertStatus_H264enc(stsUMC);
}

mfxStatus MFXVideoEncH264::Close(void)
{
    MFX_CHECK_INIT(m_InitFlag);
    DeallocateFrames();
    H264CoreEncoder_Close_8u16s(enc);
    m_Allocator.Close();
    m_InitFlag = 0;
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoEncH264::GetVideoParam(mfxVideoParam *par)
{
  MFX_CHECK_NULL_PTR1(par)
  CHECK_VERSION(par->Version);
  MFX_CHECK_INIT(m_InitFlag);
  *par = m_VideoParam;
  return MFX_ERR_NONE;
}

mfxStatus MFXVideoEncH264::GetFrameParam(mfxFrameParam *par)
{
    MFX_CHECK_NULL_PTR1(par)
    MFX_CHECK_INIT(m_InitFlag);
    *par = m_FrameParam;
    return MFX_ERR_NONE;
}


mfxStatus MFXVideoEncH264::GetSliceParam(mfxSliceParam *par)
{
    MFX_CHECK_NULL_PTR1(par)
    MFX_CHECK_INIT(m_InitFlag);
    *par = m_SliceParam;
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoEncH264::RunSliceVmeENC(mfxFrameCUC *cuc)
{
    UMC::Status stsUMC;
    mfxStatus   stsMFX;
    const mfxFrameParam* fp = cuc->FrameParam;
    const mfxFrameSurface* surface = cuc->FrameSurface;

    MFX_CHECK_NULL_PTR1(cuc);
    MFX_CHECK_NULL_PTR3(cuc->FrameSurface, cuc->FrameParam, cuc->MbParam);
    MFX_CHECK_COND(cuc->FourCC == MFX_FOURCC_YV12);
    MFX_CHECK_COND(cuc->NumMb == ((cuc->FrameSurface->Info.Width + 15) >> 4) * ((cuc->FrameSurface->Info.Height + 15) >> 4));
    if (m_VideoParam.mfx.NumSlice > 0)
        MFX_CHECK_COND(cuc->NumSlice == m_VideoParam.mfx.NumSlice);

cuc->FieldId;
cuc->SliceId;

    MFX_CHECK_COND(cuc->FrameParam->AVC.CucId == MFX_CUC_AVC_FRAMEPARAM);
    MFX_CHECK_COND(cuc->FrameParam->AVC.CucSz == sizeof(mfxFrameParam));
    MFX_CHECK_COND(cuc->FrameParam->AVC.ChromaFormatIdc ==  ConvertColorFormat_H264enc(m_VideoParam.mfx.FrameInfo.ChromaFormat));
    MFX_CHECK_COND(cuc->FrameParam->AVC.BitDepthLumaMinus8 == 0);
    MFX_CHECK_COND(cuc->FrameParam->AVC.BitDepthChromaMinus8 == 0);
    MFX_CHECK_COND(cuc->FrameParam->AVC.NumMb == cuc->NumMb);
    MFX_CHECK_COND(cuc->FrameParam->AVC.FrameWinMbMinus1 == (((cuc->FrameSurface->Info.Width + 15) >> 4) - 1));
    MFX_CHECK_COND(cuc->FrameParam->AVC.FrameHinMbMinus1 == (((cuc->FrameSurface->Info.Height + 15) >> 4) - 1));
    MFX_CHECK_COND(cuc->FrameParam->AVC.MbaffFrameFlag == 0);

    MFX_CHECK_COND(cuc->MbParam->CucId == MFX_CUC_AVC_MBPARAM);
    MFX_CHECK_COND(cuc->MbParam->CucSz == sizeof(mfxMbParam));
    MFX_CHECK_COND(cuc->MbParam->NumMb == cuc->FrameParam->AVC.NumMb);

    MFX_CHECK_NULL_PTR1(cuc->FrameSurface->Data);
    MFX_CHECK_COND(cuc->FrameSurface->Info.FourCC == MFX_FOURCC_YV12);
    MFX_CHECK_COND(cuc->FrameSurface->Info.Width == m_VideoParam.mfx.FrameInfo.Width);
    MFX_CHECK_COND(cuc->FrameSurface->Info.Height == m_VideoParam.mfx.FrameInfo.Height);
    MFX_CHECK_COND(ConvertPicStruct_H264enc(cuc->FrameSurface->Info.PicStruct) == ConvertPicStruct_H264enc(m_VideoParam.mfx.FrameInfo.PicStruct));
    if( cuc->FrameSurface->Info.PicStruct == MFX_PICSTRUCT_PROGRESSIVE ){
        MFX_CHECK_COND(!cuc->FrameParam->AVC.FieldPicFlag);
    }else{
        MFX_CHECK_COND(cuc->FrameParam->AVC.FieldPicFlag);
        MFX_CHECK_COND(cuc->FrameParam->AVC.FrameMbsOnlyFlag);
    }
    // Transform8x8Flag should be the same for all slices of frame
    MFX_CHECK_COND(cuc->SliceId == 0 || cuc->FrameParam->AVC.Transform8x8Flag == m_FrameParam.AVC.Transform8x8Flag);

    cuc->FrameSurface->Data[cuc->FrameParam->AVC.CurrFrameLabel]->Locked++;
    IncrementLocked incrLocked(*cuc->FrameSurface->Data[cuc->FrameParam->AVC.CurrFrameLabel]);
    MultiFrameLocker frameLocker(*m_pMFXCore, *cuc->FrameSurface);
    frameLocker.Lock(cuc->FrameParam->AVC.CurrFrameLabel);
    frameLocker.Lock(cuc->FrameParam->AVC.RecFrameLabel);

    m_FrameParam = *cuc->FrameParam;

    //m_PicParamSet.weighted_pred_flag = cuc->FrameParam->AVC.WeightedPredFlag;
    //m_PicParamSet.weighted_bipred_idc = cuc->FrameParam->AVC.WeightedBipredIdc;
    //m_PicParamSet.constrained_intra_pred_flag = cuc->FrameParam->AVC.ConstraintIntraFlag;
    enc->m_info.transform_8x8_mode_flag = enc->m_PicParamSet.transform_8x8_mode_flag = cuc->FrameParam->AVC.Transform8x8Flag;
    enc->m_info.entropy_coding_mode = enc->m_PicParamSet.entropy_coding_mode = cuc->FrameParam->AVC.EntropyCodingModeFlag;

    //update flags here
    enc->m_Analyse &= ~(ANALYSE_I_8x8 | ANALYSE_RD_MODE | ANALYSE_RD_OPT | ANALYSE_B_RD_OPT);
    if (enc->m_info.transform_8x8_mode_flag)
        enc->m_Analyse |= ANALYSE_I_8x8;
    if ((enc->m_info.m_QualitySpeed >= 2) && enc->m_info.entropy_coding_mode)
        enc->m_Analyse |= ANALYSE_RD_MODE;
    if ((enc->m_info.m_QualitySpeed >= 3) && enc->m_info.entropy_coding_mode)
        enc->m_Analyse |= ANALYSE_RD_OPT | ANALYSE_B_RD_OPT;

    enc->m_PicParamSet.pic_init_qp = cuc->FrameParam->AVC.PicInitQpMinus26 + 26;
    enc->m_PicParamSet.pic_init_qs = cuc->FrameParam->AVC.PicInitQsMinus26 + 26;
    enc->m_PicParamSet.chroma_qp_index_offset = cuc->FrameParam->AVC.ChromaQp1stOffset;
    enc->m_PicParamSet.second_chroma_qp_index_offset = cuc->FrameParam->AVC.ChromaQp2ndOffset;

    H264CoreEncoder_SetPictureParameters_8u16s(enc);

    ConvertCUC_ToInternalStructure( cuc );
    //convert frames via VideoData

    SetPlanePointers(MFX_FOURCC_NV12, *surface->Data[cuc->FrameParam->AVC.CurrFrameLabel], *enc->m_pCurrentFrame);
    //SetPlanePointers(enc->m_pReconstructFrame, fi, surface->Data[cuc->FrameParam->AVC.RecFrameLabel]);

    SetCurrentFrameParameters( cuc );
    enc->m_pCurrentFrame->m_PicCodType = ConvertPicType_H264enc(cuc->FrameParam->AVC.FrameType);
    MFX_CHECK_COND(enc->m_pCurrentFrame->m_PicCodType != (EnumPicCodType)-1);

   mfxExtAvcRefFrameParam* params = (mfxExtAvcRefFrameParam*)GetExtBuffer(cuc->ExtBuffer, cuc->NumExtBuffer, MFX_CUC_AVC_FRAMEPARAM);
   enc->m_pCurrentFrame->m_FrameNum = params->FrameNum;

//   enc->m_pCurrentFrame->m_PicOrderCnt[0] = enc->m_pCurrentFrame->m_PicOrderCnt[1] = cuc->FrameSurface->Data[cuc->FrameParam->AVC.CurrFrameLabel]->FrameOrder/2;


    if( !cuc->FrameParam->AVC.IntraPicFlag ){
          mfxExtAvcRefList* refList = (mfxExtAvcRefList*)GetExtBuffer(cuc->ExtBuffer, cuc->NumExtBuffer, MFX_CUC_AVC_REFLIST);
          H264EncoderFrameList_removeAllRef_8u16s( &enc->m_dpb );
          switch( cuc->FrameParam->AVC.FrameType & 0xf){
              case MFX_FRAMETYPE_P:
                  {
                   H264EncoderFrame_8u16s* rF[MAX_NUM_REF_FRAMES];
                   mfxI32 POCs[MAX_NUM_REF_FRAMES];
                   //FIXME get list len from first slice
                   for( mfxI32 i = 0; i<cuc->SliceParam->AVC.NumRefIdxL0Minus1+1; i++ ){
                            H264EncoderFrame_8u16s* dpbFrame = H264EncoderFrameList_findNextDisposable_8u16s(&enc->m_dpb);
                            mfxExtAvcRefFrameParam* mfxFrame = refList->RefList[fp->AVC.RefFrameListP[i]];

                            frameLocker.Lock(fp->AVC.RefFrameListP[i]);
                            mfxFrameData* mfxData = cuc->FrameSurface->Data[fp->AVC.RefFrameListP[i]];

                            dpbFrame->m_data.SetPlanePointer(mfxData->Y, 0);
                            dpbFrame->m_data.SetPlanePitch(mfxData->Pitch, 0);
                            dpbFrame->m_data.SetPlanePointer(mfxData->Cb, 1);
                            dpbFrame->m_data.SetPlanePitch(mfxData->Pitch >> 1, 1);
                            dpbFrame->m_data.SetPlanePointer(mfxData->Cr, 2);
                            dpbFrame->m_data.SetPlanePitch(mfxData->Pitch >> 1, 2);
#if 0
                            //Do padding: Correct for fields
                            Ipp32s shift_x, shift_y;

                            switch( enc->m_PicParamSet.chroma_format_idc ){
                                default:
                                case 1:
                                    shift_x=shift_y=1;
                                    break;
                                case 2:
                                    shift_x=1;
                                    shift_y=0;
                                    break;
                                case 3:
                                    shift_x=shift_y=0;
                                    break;
                            }

                            ExpandPlane_8u16s( dpbFrame->m_pYPlane,
                                    enc->m_WidthInMBs*16,
                                    enc->m_HeightInMBs*16,
                                    dpbFrame->m_pitchPixels, 48);            // was 16
                            if(enc->m_PicParamSet.chroma_format_idc){
                                ExpandPlane_8u16s( dpbFrame->m_pUPlane,
                                    enc->m_WidthInMBs*16>>shift_x,
                                    enc->m_HeightInMBs*16>>shift_y,
                                    dpbFrame->m_pitchPixels, 32);            // was 8
                                ExpandPlane_8u16s( dpbFrame->m_pVPlane,
                                    enc->m_WidthInMBs*16>>shift_x,
                                    enc->m_HeightInMBs*16>>shift_y,
                                    dpbFrame->m_pitchPixels, 32);            // was 8
                            }
#endif
                            rF[i] = dpbFrame;
                            POCs[i] = mfxFrame->PicInfo[0].FieldOrderCnt[0];
                            SetUMCFrame( dpbFrame, mfxFrame );
                        }

                   for( Ipp32s slice = 0; slice<cuc->NumSlice; slice++ ){
                        mfxSliceParam* sp = &cuc->SliceParam[slice];
                        H264EncoderFrame_8u16s **pRefPicList0 = enc->m_pCurrentFrame->m_pRefPicList[slice].m_RefPicListL0.m_RefPicList;
                        H264Slice_8u16s* curr_slice = &enc->m_Slices[slice];
                        Ipp32s* pPOC0 = enc->m_pCurrentFrame->m_pRefPicList[slice].m_RefPicListL0.m_POC;
                        Ipp8s*  pFields0 = enc->m_pCurrentFrame->m_pRefPicList[slice].m_RefPicListL0.m_Prediction;
                        pFields0[0] = 0;

                        for (mfxI32 i=0; i<MAX_NUM_REF_FRAMES; i++) pRefPicList0[i] = NULL;
                        for( mfxI32 i = 0; i<sp->AVC.NumRefIdxL0Minus1+1; i++ ){
                            pPOC0[i] = POCs[i];
                            pRefPicList0[i] = rF[i];
                        }

                        //Copy frame params
                        for (mfxI32 i=0;i<MAX_NUM_REF_FRAMES;i++){
                                curr_slice->m_TempRefPicList[0][0].m_RefPicListL0.m_RefPicList[i]=
                                curr_slice->m_TempRefPicList[1][0].m_RefPicListL0.m_RefPicList[i]=
                                curr_slice->m_TempRefPicList[0][1].m_RefPicListL0.m_RefPicList[i]=
                                curr_slice->m_TempRefPicList[1][1].m_RefPicListL0.m_RefPicList[i]= pRefPicList0[i];
                                curr_slice->m_TempRefPicList[0][0].m_RefPicListL0.m_Prediction[i]=
                                curr_slice->m_TempRefPicList[1][0].m_RefPicListL0.m_Prediction[i]=
                                curr_slice->m_TempRefPicList[0][1].m_RefPicListL0.m_Prediction[i]=
                                curr_slice->m_TempRefPicList[1][1].m_RefPicListL0.m_Prediction[i]= pFields0[i];
                                curr_slice->m_TempRefPicList[0][0].m_RefPicListL0.m_POC[i]=
                                curr_slice->m_TempRefPicList[1][0].m_RefPicListL0.m_POC[i]=
                                curr_slice->m_TempRefPicList[0][1].m_RefPicListL0.m_POC[i]=
                                curr_slice->m_TempRefPicList[1][1].m_RefPicListL0.m_POC[i]= pPOC0[i];
                        }
                   }
                  }
                  break;
              case MFX_FRAMETYPE_B:
                  {
                    H264EncoderFrame_8u16s *rFL0[MAX_NUM_REF_FRAMES], *rFL1[MAX_NUM_REF_FRAMES];
                    mfxI32 POCsL0[MAX_NUM_REF_FRAMES], POCsL1[MAX_NUM_REF_FRAMES];
                        //L0
                        for( mfxI32 i = 0; i<cuc->SliceParam->AVC.NumRefIdxL0Minus1+1; i++ ){
                            H264EncoderFrame_8u16s* dpbFrame = H264EncoderFrameList_findNextDisposable_8u16s(&enc->m_dpb);
                            mfxExtAvcRefFrameParam* mfxFrame = refList->RefList[fp->AVC.RefFrameListB[0][i]];

                            frameLocker.Lock(fp->AVC.RefFrameListB[0][i]);
                            mfxFrameData* mfxData = cuc->FrameSurface->Data[fp->AVC.RefFrameListB[0][i]];

                            dpbFrame->m_data.SetPlanePointer(mfxData->Y, 0);
                            dpbFrame->m_data.SetPlanePitch(mfxData->Pitch, 0);
                            dpbFrame->m_data.SetPlanePointer(mfxData->Cb, 1);
                            dpbFrame->m_data.SetPlanePitch(mfxData->Pitch >> 1, 1);
                            dpbFrame->m_data.SetPlanePointer(mfxData->Cr, 2);
                            dpbFrame->m_data.SetPlanePitch(mfxData->Pitch >> 1, 2);

                            //Do padding: Correct for fields
                            Ipp32s shift_x, shift_y;

                            switch( enc->m_PicParamSet.chroma_format_idc ){
                                default:
                                case 1:
                                    shift_x=shift_y=1;
                                    break;
                                case 2:
                                    shift_x=1;
                                    shift_y=0;
                                    break;
                                case 3:
                                    shift_x=shift_y=0;
                                    break;
                            }
#if 0
                            ExpandPlane_8u16s( dpbFrame->m_pYPlane,
                                    enc->m_WidthInMBs*16,
                                    enc->m_HeightInMBs*16,
                                    dpbFrame->m_pitchPixels, 48);            // was 16
                            if(enc->m_PicParamSet.chroma_format_idc){
                                ExpandPlane_8u16s( dpbFrame->m_pUPlane,
                                    enc->m_WidthInMBs*16>>shift_x,
                                    enc->m_HeightInMBs*16>>shift_y,
                                    dpbFrame->m_pitchPixels, 32);            // was 8
                                ExpandPlane_8u16s(dpbFrame->m_pVPlane,
                                    enc->m_WidthInMBs*16>>shift_x,
                                    enc->m_HeightInMBs*16>>shift_y,
                                    dpbFrame->m_pitchPixels, 32);            // was 8
                            }
#endif
                            //pRefPicList0[i]
                            rFL0[i] = dpbFrame;
                            //pPOC0[i]
                            POCsL0[i] = mfxFrame->PicInfo[0].FieldOrderCnt[0];

                            SetUMCFrame( dpbFrame, mfxFrame );
                        }

                        //L1
                        for( mfxI32 i = 0; i<cuc->SliceParam->AVC.NumRefIdxL1Minus1+1; i++ ){
                            H264EncoderFrame_8u16s* dpbFrame = H264EncoderFrameList_findNextDisposable_8u16s(&enc->m_dpb);
                            mfxExtAvcRefFrameParam* mfxFrame = refList->RefList[fp->AVC.RefFrameListB[1][i]];

                            frameLocker.Lock(fp->AVC.RefFrameListB[1][i]);
                            mfxFrameData* mfxData = cuc->FrameSurface->Data[fp->AVC.RefFrameListB[1][i]];

                            dpbFrame->m_data.SetPlanePointer(mfxData->Y, 0);
                            dpbFrame->m_data.SetPlanePitch(mfxData->Pitch, 0);
                            dpbFrame->m_data.SetPlanePointer(mfxData->Cb, 1);
                            dpbFrame->m_data.SetPlanePitch(mfxData->Pitch >> 1, 1);
                            dpbFrame->m_data.SetPlanePointer(mfxData->Cr, 2);
                            dpbFrame->m_data.SetPlanePitch(mfxData->Pitch >> 1, 2);

                            //Do padding: Correct for fields
                            Ipp32s shift_x, shift_y;

                            switch( enc->m_PicParamSet.chroma_format_idc ){
                                default:
                                case 1:
                                    shift_x=shift_y=1;
                                    break;
                                case 2:
                                    shift_x=1;
                                    shift_y=0;
                                    break;
                                case 3:
                                    shift_x=shift_y=0;
                                    break;
                            }
#if 0
                            ExpandPlane_8u16s( dpbFrame->m_pYPlane,
                                    enc->m_WidthInMBs*16,
                                    enc->m_HeightInMBs*16,
                                    dpbFrame->m_pitchPixels, 48);            // was 16
                            if(enc->m_PicParamSet.chroma_format_idc){
                                ExpandPlane_8u16s( dpbFrame->m_pUPlane,
                                    enc->m_WidthInMBs*16>>shift_x,
                                    enc->m_HeightInMBs*16>>shift_y,
                                    dpbFrame->m_pitchPixels, 32);            // was 8
                                ExpandPlane_8u16s( dpbFrame->m_pVPlane,
                                    enc->m_WidthInMBs*16>>shift_x,
                                    enc->m_HeightInMBs*16>>shift_y,
                                    dpbFrame->m_pitchPixels, 32);            // was 8
                            }
#endif
                            //pRefPicList1[i]
                            rFL1[i] = dpbFrame;
                            //pPOC1[i]
                            POCsL1[i] = mfxFrame->PicInfo[0].FieldOrderCnt[0];
                            //Copy frame MVs and Refs
                            SetUMCFrame( dpbFrame, mfxFrame );
                        }

                   for( Ipp32s slice = 0; slice<cuc->NumSlice; slice++ ){
                        mfxSliceParam* sp = &cuc->SliceParam[slice];
                        H264EncoderFrame_8u16s **pRefPicList0 = enc->m_pCurrentFrame->m_pRefPicList[slice].m_RefPicListL0.m_RefPicList;
                        H264EncoderFrame_8u16s **pRefPicList1 = enc->m_pCurrentFrame->m_pRefPicList[slice].m_RefPicListL1.m_RefPicList;
                        H264Slice_8u16s* curr_slice = &enc->m_Slices[slice];
                        Ipp32s* pPOC0 = enc->m_pCurrentFrame->m_pRefPicList[slice].m_RefPicListL0.m_POC;
                        Ipp8s*  pFields0 = enc->m_pCurrentFrame->m_pRefPicList[slice].m_RefPicListL0.m_Prediction;
                        Ipp32s* pPOC1 = enc->m_pCurrentFrame->m_pRefPicList[slice].m_RefPicListL1.m_POC;
                        Ipp8s*  pFields1 = enc->m_pCurrentFrame->m_pRefPicList[slice].m_RefPicListL1.m_Prediction;
                        pFields0[0] = 0;
                        pFields1[0] = 0;

                        for (mfxI32 i=0; i<MAX_NUM_REF_FRAMES; i++){
                            pRefPicList0[i] = NULL;
                            pRefPicList1[i] = NULL;
                        }

                        for( mfxI32 i = 0; i<sp->AVC.NumRefIdxL0Minus1+1; i++ ){
                            pPOC0[i] = POCsL0[i];
                            pRefPicList0[i] = rFL0[i];
                        }

                        for( mfxI32 i = 0; i<sp->AVC.NumRefIdxL1Minus1+1; i++ ){
                            pPOC1[i] = POCsL1[i];
                            pRefPicList1[i] = rFL1[i];
                        }

                        for (mfxI32 i=0;i<MAX_NUM_REF_FRAMES;i++){
                                curr_slice->m_TempRefPicList[0][0].m_RefPicListL0.m_RefPicList[i]=
                                curr_slice->m_TempRefPicList[1][0].m_RefPicListL0.m_RefPicList[i]=
                                curr_slice->m_TempRefPicList[0][1].m_RefPicListL0.m_RefPicList[i]=
                                curr_slice->m_TempRefPicList[1][1].m_RefPicListL0.m_RefPicList[i]= pRefPicList0[i];
                                curr_slice->m_TempRefPicList[0][0].m_RefPicListL0.m_Prediction[i]=
                                curr_slice->m_TempRefPicList[1][0].m_RefPicListL0.m_Prediction[i]=
                                curr_slice->m_TempRefPicList[0][1].m_RefPicListL0.m_Prediction[i]=
                                curr_slice->m_TempRefPicList[1][1].m_RefPicListL0.m_Prediction[i]= pFields0[i];
                                curr_slice->m_TempRefPicList[0][0].m_RefPicListL0.m_POC[i]=
                                curr_slice->m_TempRefPicList[1][0].m_RefPicListL0.m_POC[i]=
                                curr_slice->m_TempRefPicList[0][1].m_RefPicListL0.m_POC[i]=
                                curr_slice->m_TempRefPicList[1][1].m_RefPicListL0.m_POC[i]= pPOC0[i];
                        }
                        for (mfxI32 i=0;i<MAX_NUM_REF_FRAMES;i++){
                            curr_slice->m_TempRefPicList[0][0].m_RefPicListL1.m_RefPicList[i]=
                            curr_slice->m_TempRefPicList[1][0].m_RefPicListL1.m_RefPicList[i]=
                            curr_slice->m_TempRefPicList[0][1].m_RefPicListL1.m_RefPicList[i]=
                            curr_slice->m_TempRefPicList[1][1].m_RefPicListL1.m_RefPicList[i]= pRefPicList1[i];
                            curr_slice->m_TempRefPicList[0][0].m_RefPicListL1.m_Prediction[i]=
                            curr_slice->m_TempRefPicList[1][0].m_RefPicListL1.m_Prediction[i]=
                            curr_slice->m_TempRefPicList[0][1].m_RefPicListL1.m_Prediction[i]=
                            curr_slice->m_TempRefPicList[1][1].m_RefPicListL1.m_Prediction[i]= pFields1[i];
                            curr_slice->m_TempRefPicList[0][0].m_RefPicListL1.m_POC[i]=
                            curr_slice->m_TempRefPicList[1][0].m_RefPicListL1.m_POC[i]=
                            curr_slice->m_TempRefPicList[0][1].m_RefPicListL1.m_POC[i]=
                            curr_slice->m_TempRefPicList[1][1].m_RefPicListL1.m_POC[i]= pPOC1[i];
                      }
                   }

                   //Get POC of RefL1[0]
                   mfxExtAvcRefFrameParam* l1cuc0 = refList->RefList[fp->AVC.RefFrameListB[1][0]];

                   //Prepare MapColMBToList0 needed for B_DIRECT
                   for( Ipp32s slice = 0; slice<cuc->NumSlice; slice++ ){
                    H264Slice_8u16s* curr_slice = &enc->m_Slices[slice];
                    mfxSliceParam* sp = &cuc->SliceParam[slice];
                    for(mfxI32 ref=0; ref<16; ref++ ){
                        mfxI32 POC = l1cuc0->PicInfo[0].FieldOrderCntList[ref][0];
                        mfxI32 val = -1;
                        for( mfxI32 i = 0; i <= sp->AVC.NumRefIdxL0Minus1; i++ ) if( POC == refList->RefList[fp->AVC.RefFrameListB[0][i]]->PicInfo[0].FieldOrderCnt[0] ){ val = i; break;}
                        curr_slice->MapColMBToList0[ref][LIST_0] = val;

                        POC = params->PicInfo[0].FieldOrderCntList[ref][1];
                        val = -1;
                        for( mfxI32 i = 0; i <= sp->AVC.NumRefIdxL0Minus1; i++ ) if( POC == refList->RefList[fp->AVC.RefFrameListB[0][i]]->PicInfo[0].FieldOrderCnt[0] ){ val = i; break;}
                        curr_slice->MapColMBToList0[ref][LIST_1] = val;
                    }

                    //Prepare DistScale for BIDIR
                    mfxExtAvcRefFrameParam* params = (mfxExtAvcRefFrameParam*)GetExtBuffer(cuc->ExtBuffer, cuc->NumExtBuffer, MFX_CUC_AVC_FRAMEPARAM);

                    mfxI32 L0Index,L1Index;
                    Ipp32u picCntRef0 = 0;
                    Ipp32u picCntRef1 = 0;
                    Ipp32u picCntCur;
                    Ipp32s DistScaleFactor;

                    Ipp32s tb;
                    Ipp32s td;
                    Ipp32s tx;

                    //picCntCur = m_pCurrentFrame->PicOrderCnt(m_field_index);//m_PicOrderCnt;  Current POC
                    picCntCur = cuc->FrameSurface->Data[cuc->FrameParam->AVC.CurrFrameLabel]->FrameOrder;
                    for( L1Index = 0; L1Index <= fp->AVC.NumRefIdxL1Minus1; L1Index++ ){
                        if( fp->AVC.FieldPicFlag ){
//                            Ipp8u RefField=pRefPicList1[L1Index]->GetNumberByParity(GetReferenceField(pFields1,L1Index));
//                            picCntRef1 = pRefPicList1[L1Index]->PicOrderCnt(RefField);
                        }else{
                            picCntRef1 = params->PicInfo[0].FieldOrderCntList[L1Index][1];
                        }

                        for (L0Index=0; L0Index <= sp->AVC.NumRefIdxL0Minus1; L0Index++){
                            if( fp->AVC.FieldPicFlag ){
                           //     Ipp8s RefFieldTop;
                                //ref0
                           //     RefFieldTop  = GetReferenceField(pFields0,L0Index);
                           //     VM_ASSERT(pRefPicList0[L0Index]);
                           //     picCntRef0 = pRefPicList0[L0Index]->PicOrderCnt(pRefPicList1[L1Index]->GetNumberByParity(RefFieldTop));
                            }else{
                                picCntRef0 = params->PicInfo[0].FieldOrderCntList[L0Index][0];
                            }

                            tb = picCntCur - picCntRef0;    /* distance from previous */
                            td = picCntRef1 - picCntRef0;    /* distance between ref0 and ref1 */

                            if (td == 0 /*|| pRefPicList0[index]->isLongTermRef(m_field_index)*/){
                                curr_slice->DistScaleFactor[L0Index][L1Index] = 128;
                                curr_slice->DistScaleFactorMV[L0Index][L1Index] = 256;
                            }else{
                                tb = MAX(-128,tb);
                                tb = MIN(127,tb);
                                td = MAX(-128,td);
                                td = MIN(127,td);
                                VM_ASSERT(td != 0);
                                tx = (16384 + abs(td/2))/td;
                                DistScaleFactor = (tb*tx + 32)>>6;
                                DistScaleFactor = MAX(-1024, DistScaleFactor);
                                DistScaleFactor = MIN(1023, DistScaleFactor);

                                if (DistScaleFactor < -256 || DistScaleFactor > 512)
                                    curr_slice->DistScaleFactor[L0Index][L1Index] = 128;
                                else
                                    curr_slice->DistScaleFactor[L0Index][L1Index] = DistScaleFactor;

                                curr_slice->DistScaleFactorMV[L0Index][L1Index] = DistScaleFactor;
                            }
                        }
                      }
                   }
                  }
                  break;
          }
    }

    EnumPicCodType picType = ConvertPicType_H264enc(cuc->FrameParam->AVC.FrameType);
    MFX_CHECK_COND(picType != (EnumPicCodType)-1);
    EnumPicClass picClass = ConvertPicClass_H264enc(cuc->FrameParam->AVC.FrameType);
    MFX_CHECK_COND(picClass != (EnumPicClass)-1);

    if( enc->m_Analyse & ANALYSE_RECODE_FRAME /*&& m_pReconstructFrame == NULL //could be size change*/){ //make init reconstruct buffer
        enc->m_pReconstructFrame->m_bottom_field_flag[0] = enc->m_pCurrentFrame->m_bottom_field_flag[0];
        enc->m_pReconstructFrame->m_bottom_field_flag[1] = enc->m_pCurrentFrame->m_bottom_field_flag[1];
    }

    enc->m_field_index = 0; //FIXME FOR FIELDS
    enc->m_is_cur_pic_afrm = (Ipp32s)(enc->m_pCurrentFrame->m_PictureStructureForDec==AFRM_STRUCTURE);
//    for (m_field_index = 0; m_field_index <= (Ipp8u)(m_pCurrentFrame->m_PictureStructureForDec < FRM_STRUCTURE); m_field_index ++) {
        if (picClass == IDR_PIC) {
            if (enc->m_field_index) {
                picClass = REFERENCE_PIC;
            } else {
                H264CoreEncoder_SetSequenceParameters_8u16s(enc);
                if( cuc->FrameParam->AVC.PicOrderCntType == 2 ){
                    enc->m_SeqParamSet.pic_order_cnt_type = 2;
                    enc->m_SeqParamSet.log2_max_pic_order_cnt_lsb = 0;
                }else{
                    enc->m_SeqParamSet.pic_order_cnt_type = 0;
                    enc->m_SeqParamSet.log2_max_pic_order_cnt_lsb = cuc->FrameParam->AVC.Log2MaxPicOrdCntMinus4 + 4;
                }
                enc->m_SeqParamSet.log2_max_frame_num = cuc->FrameParam->AVC.Log2MaxFrameCntMinus4 + 4;
                enc->m_SeqParamSet.direct_8x8_inference_flag = cuc->FrameParam->AVC.Direct8x8InferenceFlag;
                H264CoreEncoder_SetPictureParameters_8u16s(enc);
            }
        }
        enc->m_PicType = picType;
        enc->m_PicParamSet.chroma_format_idc = enc->m_SeqParamSet.chroma_format_idc;
        enc->m_PicParamSet.bit_depth_luma = enc->m_SeqParamSet.bit_depth_luma;
        //if (!(enc->m_Analyse & ANALYSE_RECODE_FRAME))   enc->m_pReconstructFrame = enc->m_pCurrentFrame;

//        H264CoreEncoder_SetSliceHeaderCommon_8u16s(enc, enc->m_pCurrentFrame);

        enc->m_SliceHeader.frame_num = enc->m_pCurrentFrame->m_FrameNum % (1 << enc->m_SeqParamSet->log2_max_frame_num);
        // Assumes there is only one picture parameter set to choose from
        enc->m_SliceHeader.pic_parameter_set_id = enc->m_PicParamSet.pic_parameter_set_id;

        enc->m_SliceHeader.field_pic_flag = (Ipp8u)(enc->m_pCurrentFrame->m_PictureStructureForDec<FRM_STRUCTURE);
        enc->m_SliceHeader.bottom_field_flag = (Ipp8u)(enc->m_pCurrentFrame->m_bottom_field_flag[enc->m_field_index]);
        enc->m_SliceHeader.MbaffFrameFlag = (enc->m_SeqParamSet.mb_adaptive_frame_field_flag)&&(!enc->m_SliceHeader.field_pic_flag);
        enc->m_SliceHeader.delta_pic_order_cnt_bottom = enc->m_SeqParamSet.frame_mbs_only_flag == 0;


        if (enc->m_SeqParamSet.pic_order_cnt_type == 0) {
            enc->m_SliceHeader.pic_order_cnt_lsb = H264EncoderFrame_PicOrderCnt_8u16s(
                enc->m_pCurrentFrame, enc->m_field_index,
                    0) & ~(0xffffffff << enc->m_SeqParamSet.log2_max_pic_order_cnt_lsb);
        }
        enc->m_SliceHeader.adaptive_ref_pic_marking_mode_flag = 0;

        stsUMC = H264CoreEncoder_Start_Picture_8u16s(enc,&picClass, picType);
        if (stsUMC != UMC_OK) return ConvertStatus_H264enc(stsUMC);
//        UpdateRefPicListCommon();

        stsMFX = ENC_Slice(cuc, cuc->SliceId, enc->m_Slices + cuc->SliceId, 0);
        MFX_CHECK_STS(stsMFX);

        if (picClass != DISPOSABLE_PIC) {
//            End_Picture();
//            UpdateRefPicMarking();
        }
//    }

        /*
    if( enc->m_Analyse & ANALYSE_RECODE_FRAME ){
        H264EncoderFrame_exchangeFrameYUVPointers_8u16s(enc->m_pReconstructFrame, enc->m_pCurrentFrame);
        ippsFree(enc->m_pReconstructFrame);
        enc->m_pReconstructFrame = NULL;
    }
    */
//    m_uFrames_Num ++;
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoEncH264::RunFrameVmeENC(mfxFrameCUC *cuc)
{
    const mfxFrameParam* fp = cuc->FrameParam;
    const mfxFrameSurface* surface = cuc->FrameSurface;

    MFX_CHECK_NULL_PTR1(cuc);
    MFX_CHECK_NULL_PTR3(cuc->FrameSurface, cuc->FrameParam, cuc->MbParam);
    MFX_CHECK_COND(cuc->NumMb == ((cuc->FrameSurface->Info.Width + 15) >> 4) * ((cuc->FrameSurface->Info.Height + 15) >> 4));
    if (m_VideoParam.mfx.NumSlice > 0)
        MFX_CHECK_COND(cuc->NumSlice == m_VideoParam.mfx.NumSlice);

    MFX_CHECK_COND(cuc->FrameParam->AVC.CucId == MFX_CUC_AVC_FRAMEPARAM);
    MFX_CHECK_COND(cuc->FrameParam->AVC.CucSz == sizeof(mfxFrameParam));
    MFX_CHECK_COND(cuc->FrameParam->AVC.ChromaFormatIdc ==  ConvertColorFormat_H264enc(m_VideoParam.mfx.FrameInfo.ChromaFormat));
    MFX_CHECK_COND(cuc->FrameParam->AVC.BitDepthLumaMinus8 == 0);
    MFX_CHECK_COND(cuc->FrameParam->AVC.BitDepthChromaMinus8 == 0);
    MFX_CHECK_COND(cuc->FrameParam->AVC.NumMb == cuc->NumMb);
    MFX_CHECK_COND(cuc->FrameParam->AVC.FrameWinMbMinus1 == (((cuc->FrameSurface->Info.Width + 15) >> 4) - 1));
    MFX_CHECK_COND(cuc->FrameParam->AVC.FrameHinMbMinus1 == (((cuc->FrameSurface->Info.Height + 15) >> 4) - 1));
    MFX_CHECK_COND(cuc->FrameParam->AVC.MbaffFrameFlag == 0);

    MFX_CHECK_COND(cuc->MbParam->CucId == MFX_CUC_AVC_MBPARAM);
    MFX_CHECK_COND(cuc->MbParam->CucSz == sizeof(mfxMbParam));
    MFX_CHECK_COND(cuc->MbParam->NumMb == cuc->FrameParam->AVC.NumMb);

    MFX_CHECK_NULL_PTR1(cuc->FrameSurface->Data);
    MFX_CHECK_COND(cuc->FrameSurface->Info.Width == m_VideoParam.mfx.FrameInfo.Width);
    MFX_CHECK_COND(cuc->FrameSurface->Info.Height == m_VideoParam.mfx.FrameInfo.Height);
    MFX_CHECK_COND(ConvertPicStruct_H264enc(cuc->FrameSurface->Info.PicStruct) == ConvertPicStruct_H264enc(m_VideoParam.mfx.FrameInfo.PicStruct));
    if( cuc->FrameSurface->Info.PicStruct == MFX_PICSTRUCT_PROGRESSIVE ){
        MFX_CHECK_COND(!cuc->FrameParam->AVC.FieldPicFlag);
    }else{
        MFX_CHECK_COND(cuc->FrameParam->AVC.FieldPicFlag);
        MFX_CHECK_COND(cuc->FrameParam->AVC.FrameMbsOnlyFlag);
    }
//    MFX_CHECK_COND(cuc->FrameSurface->Info.NumFrameData < (cuc->FrameParam->NumRefFrame + 1));

    cuc->FrameSurface->Data[cuc->FrameParam->AVC.CurrFrameLabel]->Locked++;
    IncrementLocked incrLocked(*cuc->FrameSurface->Data[cuc->FrameParam->AVC.CurrFrameLabel]);
    MultiFrameLocker frameLocker(*m_pMFXCore, *cuc->FrameSurface);
    frameLocker.Lock(cuc->FrameParam->AVC.CurrFrameLabel);
    frameLocker.Lock(cuc->FrameParam->AVC.RecFrameLabel);

    m_FrameParam = *cuc->FrameParam;

    //m_PicParamSet.weighted_pred_flag = cuc->FrameParam->WeightedPredFlag;
    //m_PicParamSet.weighted_bipred_idc = cuc->FrameParam->WeightedBipredIdc;
    //m_PicParamSet.constrained_intra_pred_flag = cuc->FrameParam->ConstraintIntraFlag;
    enc->m_info.transform_8x8_mode_flag = enc->m_PicParamSet.transform_8x8_mode_flag = cuc->FrameParam->AVC.Transform8x8Flag;
    enc->m_info.entropy_coding_mode = enc->m_PicParamSet.entropy_coding_mode = cuc->FrameParam->AVC.EntropyCodingModeFlag;

    //update flags here
    enc->m_Analyse &= ~(ANALYSE_I_8x8 | ANALYSE_RD_MODE | ANALYSE_RD_OPT | ANALYSE_B_RD_OPT);
    if (enc->m_info.transform_8x8_mode_flag)
        enc->m_Analyse |= ANALYSE_I_8x8;
    if ((enc->m_info.m_QualitySpeed >= 2) && enc->m_info.entropy_coding_mode)
        enc->m_Analyse |= ANALYSE_RD_MODE;
    if ((enc->m_info.m_QualitySpeed >= 3) && enc->m_info.entropy_coding_mode)
        enc->m_Analyse |= ANALYSE_RD_OPT | ANALYSE_B_RD_OPT;

    enc->m_PicParamSet.pic_init_qp = cuc->FrameParam->AVC.PicInitQpMinus26 + 26;
    enc->m_PicParamSet.pic_init_qs = cuc->FrameParam->AVC.PicInitQsMinus26 + 26;
    enc->m_PicParamSet.chroma_qp_index_offset = cuc->FrameParam->AVC.ChromaQp1stOffset;
    enc->m_PicParamSet.second_chroma_qp_index_offset = cuc->FrameParam->AVC.ChromaQp2ndOffset;

    H264CoreEncoder_SetPictureParameters_8u16s(enc);

    ConvertCUC_ToInternalStructure( cuc );
    //convert frames via VideoData
    mfxU8 curPic = cuc->FrameParam->AVC.CurrFrameLabel;
    SetPlanePointers(MFX_FOURCC_NV12, *surface->Data[curPic], *enc->m_pCurrentFrame);

    SetCurrentFrameParameters( cuc );
    enc->m_pCurrentFrame->m_PicCodType = ConvertPicType_H264enc(cuc->FrameParam->AVC.FrameType);
    MFX_CHECK_COND(enc->m_pCurrentFrame->m_PicCodType != (EnumPicCodType)-1);

   mfxExtAvcRefFrameParam* params = (mfxExtAvcRefFrameParam*)GetExtBuffer(cuc->ExtBuffer, cuc->NumExtBuffer, MFX_CUC_AVC_FRAMEPARAM);
   enc->m_pCurrentFrame->m_FrameNum = params->FrameNum;

   enc->m_pCurrentFrame->m_PicOrderCnt[0] = enc->m_pCurrentFrame->m_PicOrderCnt[1] = surface->Data[curPic]->FrameOrder;

    //Special case for non Intra frames, make ref lists
    if( !cuc->FrameParam->AVC.IntraPicFlag ){
          mfxExtAvcRefList* refList = (mfxExtAvcRefList*)GetExtBuffer(cuc->ExtBuffer, cuc->NumExtBuffer, MFX_CUC_AVC_REFLIST);
          H264EncoderFrameList_removeAllRef_8u16s( &enc->m_dpb );
          switch( cuc->FrameParam->AVC.FrameType & 0xf){
              case MFX_FRAMETYPE_P:
                  {
                   H264EncoderFrame_8u16s* rF[MAX_NUM_REF_FRAMES];
                   mfxI32 POCs[MAX_NUM_REF_FRAMES];
                   //FIXME get list len from first slice
                   for( mfxI32 i = 0; i<cuc->SliceParam->AVC.NumRefIdxL0Minus1+1; i++ ){
                            H264EncoderFrame_8u16s* dpbFrame = m_refs[fp->AVC.RefFrameListP[i]];
                            mfxExtAvcRefFrameParam* mfxFrame = refList->RefList[fp->AVC.RefFrameListP[i]];

                            frameLocker.Lock(fp->AVC.RefFrameListP[i]);
                            mfxFrameData* mfxData = cuc->FrameSurface->Data[fp->AVC.RefFrameListP[i]];

                            SetPlanePointers(MFX_FOURCC_NV12, *mfxData, *dpbFrame);

                            rF[i] = dpbFrame;
                            POCs[i] = mfxData->FrameOrder;
                            SetUMCFrame( dpbFrame, mfxFrame );
                        }

                   for( Ipp32s slice = 0; slice<cuc->NumSlice; slice++ ){
                        mfxSliceParam* sp = &cuc->SliceParam[slice];
                        H264EncoderFrame_8u16s **pRefPicList0 = enc->m_pCurrentFrame->m_pRefPicList[slice].m_RefPicListL0.m_RefPicList;
                        H264Slice_8u16s* curr_slice = &enc->m_Slices[slice];
                        Ipp32s* pPOC0 = enc->m_pCurrentFrame->m_pRefPicList[slice].m_RefPicListL0.m_POC;
                        Ipp8s*  pFields0 = enc->m_pCurrentFrame->m_pRefPicList[slice].m_RefPicListL0.m_Prediction;
                        pFields0[0] = 0;

                        for (mfxI32 i=0; i<MAX_NUM_REF_FRAMES; i++) pRefPicList0[i] = NULL;
                        for( mfxI32 i = 0; i<sp->AVC.NumRefIdxL0Minus1+1; i++ ){
                            pPOC0[i] = POCs[i];
                            pRefPicList0[i] = rF[i];
                        }

                        //Copy frame params
                        for (mfxI32 i=0;i<MAX_NUM_REF_FRAMES;i++){
                                curr_slice->m_TempRefPicList[0][0].m_RefPicListL0.m_RefPicList[i]=
                                curr_slice->m_TempRefPicList[1][0].m_RefPicListL0.m_RefPicList[i]=
                                curr_slice->m_TempRefPicList[0][1].m_RefPicListL0.m_RefPicList[i]=
                                curr_slice->m_TempRefPicList[1][1].m_RefPicListL0.m_RefPicList[i]= pRefPicList0[i];
                                curr_slice->m_TempRefPicList[0][0].m_RefPicListL0.m_Prediction[i]=
                                curr_slice->m_TempRefPicList[1][0].m_RefPicListL0.m_Prediction[i]=
                                curr_slice->m_TempRefPicList[0][1].m_RefPicListL0.m_Prediction[i]=
                                curr_slice->m_TempRefPicList[1][1].m_RefPicListL0.m_Prediction[i]= pFields0[i];
                                curr_slice->m_TempRefPicList[0][0].m_RefPicListL0.m_POC[i]=
                                curr_slice->m_TempRefPicList[1][0].m_RefPicListL0.m_POC[i]=
                                curr_slice->m_TempRefPicList[0][1].m_RefPicListL0.m_POC[i]=
                                curr_slice->m_TempRefPicList[1][1].m_RefPicListL0.m_POC[i]= pPOC0[i];
                        }
                   }
                  }
                  break;
              case MFX_FRAMETYPE_B:
                  {
                    H264EncoderFrame_8u16s *rFL0[MAX_NUM_REF_FRAMES], *rFL1[MAX_NUM_REF_FRAMES];
                    mfxI32 POCsL0[MAX_NUM_REF_FRAMES], POCsL1[MAX_NUM_REF_FRAMES];
                        //L0
                        for( mfxI32 i = 0; i<cuc->SliceParam->AVC.NumRefIdxL0Minus1+1; i++ ){
                            H264EncoderFrame_8u16s* dpbFrame = m_refs[fp->AVC.RefFrameListB[0][i]];
                            mfxExtAvcRefFrameParam* mfxFrame = refList->RefList[fp->AVC.RefFrameListB[0][i]];

                            frameLocker.Lock(fp->AVC.RefFrameListB[0][i]);
                            mfxFrameData* mfxData = cuc->FrameSurface->Data[fp->AVC.RefFrameListB[0][i]];

                            SetPlanePointers(MFX_FOURCC_NV12, *mfxData, *dpbFrame);

                            rFL0[i] = dpbFrame;
                            POCsL0[i] = mfxData->FrameOrder;
                            SetUMCFrame( dpbFrame, mfxFrame );
                        }

                        //L1
                        for( mfxI32 i = 0; i<cuc->SliceParam->AVC.NumRefIdxL1Minus1+1; i++ ){
                            H264EncoderFrame_8u16s* dpbFrame = m_refs[fp->AVC.RefFrameListB[1][i]];
                            mfxExtAvcRefFrameParam* mfxFrame = refList->RefList[fp->AVC.RefFrameListB[1][i]];

                            frameLocker.Lock(fp->AVC.RefFrameListB[1][i]);
                            mfxFrameData* mfxData = cuc->FrameSurface->Data[fp->AVC.RefFrameListB[1][i]];

                            SetPlanePointers(MFX_FOURCC_NV12, *mfxData, *dpbFrame);

                            rFL1[i] = dpbFrame;
                            POCsL1[i] = mfxData->FrameOrder;
                            SetUMCFrame( dpbFrame, mfxFrame );
                        }

                   for( Ipp32s slice = 0; slice<cuc->NumSlice; slice++ ){
                        mfxSliceParam* sp = &cuc->SliceParam[slice];
                        H264EncoderFrame_8u16s **pRefPicList0 = enc->m_pCurrentFrame->m_pRefPicList[slice].m_RefPicListL0.m_RefPicList;
                        H264EncoderFrame_8u16s **pRefPicList1 = enc->m_pCurrentFrame->m_pRefPicList[slice].m_RefPicListL1.m_RefPicList;
                        H264Slice_8u16s* curr_slice = &enc->m_Slices[slice];
                        Ipp32s* pPOC0 = enc->m_pCurrentFrame->m_pRefPicList[slice].m_RefPicListL0.m_POC;
                        Ipp8s*  pFields0 = enc->m_pCurrentFrame->m_pRefPicList[slice].m_RefPicListL0.m_Prediction;
                        Ipp32s* pPOC1 = enc->m_pCurrentFrame->m_pRefPicList[slice].m_RefPicListL1.m_POC;
                        Ipp8s*  pFields1 = enc->m_pCurrentFrame->m_pRefPicList[slice].m_RefPicListL1.m_Prediction;
                        pFields0[0] = 0;
                        pFields1[0] = 0;

                        for (mfxI32 i=0; i<MAX_NUM_REF_FRAMES; i++){
                            pRefPicList0[i] = NULL;
                            pRefPicList1[i] = NULL;
                        }

                        for( mfxI32 i = 0; i<sp->AVC.NumRefIdxL0Minus1+1; i++ ){
                            pPOC0[i] = POCsL0[i];
                            pRefPicList0[i] = rFL0[i];
                        }

                        for( mfxI32 i = 0; i<sp->AVC.NumRefIdxL1Minus1+1; i++ ){
                            pPOC1[i] = POCsL1[i];
                            pRefPicList1[i] = rFL1[i];
                        }

                        for (mfxI32 i=0;i<MAX_NUM_REF_FRAMES;i++){
                                curr_slice->m_TempRefPicList[0][0].m_RefPicListL0.m_RefPicList[i]=
                                curr_slice->m_TempRefPicList[1][0].m_RefPicListL0.m_RefPicList[i]=
                                curr_slice->m_TempRefPicList[0][1].m_RefPicListL0.m_RefPicList[i]=
                                curr_slice->m_TempRefPicList[1][1].m_RefPicListL0.m_RefPicList[i]= pRefPicList0[i];
                                curr_slice->m_TempRefPicList[0][0].m_RefPicListL0.m_Prediction[i]=
                                curr_slice->m_TempRefPicList[1][0].m_RefPicListL0.m_Prediction[i]=
                                curr_slice->m_TempRefPicList[0][1].m_RefPicListL0.m_Prediction[i]=
                                curr_slice->m_TempRefPicList[1][1].m_RefPicListL0.m_Prediction[i]= pFields0[i];
                                curr_slice->m_TempRefPicList[0][0].m_RefPicListL0.m_POC[i]=
                                curr_slice->m_TempRefPicList[1][0].m_RefPicListL0.m_POC[i]=
                                curr_slice->m_TempRefPicList[0][1].m_RefPicListL0.m_POC[i]=
                                curr_slice->m_TempRefPicList[1][1].m_RefPicListL0.m_POC[i]= pPOC0[i];
                        }
                        for (mfxI32 i=0;i<MAX_NUM_REF_FRAMES;i++){
                            curr_slice->m_TempRefPicList[0][0].m_RefPicListL1.m_RefPicList[i]=
                            curr_slice->m_TempRefPicList[1][0].m_RefPicListL1.m_RefPicList[i]=
                            curr_slice->m_TempRefPicList[0][1].m_RefPicListL1.m_RefPicList[i]=
                            curr_slice->m_TempRefPicList[1][1].m_RefPicListL1.m_RefPicList[i]= pRefPicList1[i];
                            curr_slice->m_TempRefPicList[0][0].m_RefPicListL1.m_Prediction[i]=
                            curr_slice->m_TempRefPicList[1][0].m_RefPicListL1.m_Prediction[i]=
                            curr_slice->m_TempRefPicList[0][1].m_RefPicListL1.m_Prediction[i]=
                            curr_slice->m_TempRefPicList[1][1].m_RefPicListL1.m_Prediction[i]= pFields1[i];
                            curr_slice->m_TempRefPicList[0][0].m_RefPicListL1.m_POC[i]=
                            curr_slice->m_TempRefPicList[1][0].m_RefPicListL1.m_POC[i]=
                            curr_slice->m_TempRefPicList[0][1].m_RefPicListL1.m_POC[i]=
                            curr_slice->m_TempRefPicList[1][1].m_RefPicListL1.m_POC[i]= pPOC1[i];
                      }
                   }

                   //Get POC of RefL1[0]
                   mfxExtAvcRefFrameParam* l1cuc0 = refList->RefList[fp->AVC.RefFrameListB[1][0]];

                   //Prepare MapColMBToList0 needed for B_DIRECT
                   for( Ipp32s slice = 0; slice<cuc->NumSlice; slice++ ){
                    H264Slice_8u16s* curr_slice = &enc->m_Slices[slice];
                    mfxSliceParam* sp = &cuc->SliceParam[slice];
                    for(mfxI32 ref=0; ref<16; ref++ ){
                        mfxI32 POC = l1cuc0->PicInfo[0].FieldOrderCntList[ref][0];
                        mfxI32 val = -1;
                        for( mfxI32 i = 0; i <= sp->AVC.NumRefIdxL0Minus1; i++ ) if( POC == refList->RefList[fp->AVC.RefFrameListB[0][i]]->PicInfo[0].FieldOrderCnt[0] ){ val = i; break;}
                        curr_slice->MapColMBToList0[ref][LIST_0] = val;

                        POC = params->PicInfo[0].FieldOrderCntList[ref][1];
                        val = -1;
                        for( mfxI32 i = 0; i <= sp->AVC.NumRefIdxL0Minus1; i++ ) if( POC == refList->RefList[fp->AVC.RefFrameListB[0][i]]->PicInfo[0].FieldOrderCnt[0] ){ val = i; break;}
                        curr_slice->MapColMBToList0[ref][LIST_1] = val;
                    }

                    //Prepare DistScale for BIDIR
                    mfxExtAvcRefFrameParam* params = (mfxExtAvcRefFrameParam*)GetExtBuffer(cuc->ExtBuffer, cuc->NumExtBuffer, MFX_CUC_AVC_FRAMEPARAM);

                    mfxI32 L0Index,L1Index;
                    Ipp32u picCntRef0 = 0;
                    Ipp32u picCntRef1 = 0;
                    Ipp32u picCntCur;
                    Ipp32s DistScaleFactor;

                    Ipp32s tb;
                    Ipp32s td;
                    Ipp32s tx;

                    //picCntCur = m_pCurrentFrame->PicOrderCnt(m_field_index);//m_PicOrderCnt;  Current POC
                    picCntCur = cuc->FrameSurface->Data[cuc->FrameParam->AVC.CurrFrameLabel]->FrameOrder;
                    for( L1Index = 0; L1Index <= fp->AVC.NumRefIdxL1Minus1; L1Index++ ){
                        if( fp->AVC.FieldPicFlag ){
//                            Ipp8u RefField=pRefPicList1[L1Index]->GetNumberByParity(GetReferenceField(pFields1,L1Index));
//                            picCntRef1 = pRefPicList1[L1Index]->PicOrderCnt(RefField);
                        }else{
                            picCntRef1 = params->PicInfo[0].FieldOrderCntList[L1Index][1];
                        }

                        for (L0Index=0; L0Index <= sp->AVC.NumRefIdxL0Minus1; L0Index++){
                            if( fp->AVC.FieldPicFlag ){
                           //     Ipp8s RefFieldTop;
                                //ref0
                           //     RefFieldTop  = GetReferenceField(pFields0,L0Index);
                           //     VM_ASSERT(pRefPicList0[L0Index]);
                           //     picCntRef0 = pRefPicList0[L0Index]->PicOrderCnt(pRefPicList1[L1Index]->GetNumberByParity(RefFieldTop));
                            }else{
                                picCntRef0 = params->PicInfo[0].FieldOrderCntList[L0Index][0];
                            }

                            tb = picCntCur - picCntRef0;    /* distance from previous */
                            td = picCntRef1 - picCntRef0;    /* distance between ref0 and ref1 */

                            if (td == 0 /*|| pRefPicList0[index]->isLongTermRef(m_field_index)*/){
                                curr_slice->DistScaleFactor[L0Index][L1Index] = 128;
                                curr_slice->DistScaleFactorMV[L0Index][L1Index] = 256;
                            }else{
                                tb = MAX(-128,tb);
                                tb = MIN(127,tb);
                                td = MAX(-128,td);
                                td = MIN(127,td);
                                VM_ASSERT(td != 0);
                                tx = (16384 + abs(td/2))/td;
                                DistScaleFactor = (tb*tx + 32)>>6;
                                DistScaleFactor = MAX(-1024, DistScaleFactor);
                                DistScaleFactor = MIN(1023, DistScaleFactor);

                                if (DistScaleFactor < -256 || DistScaleFactor > 512)
                                    curr_slice->DistScaleFactor[L0Index][L1Index] = 128;
                                else
                                    curr_slice->DistScaleFactor[L0Index][L1Index] = DistScaleFactor;

                                curr_slice->DistScaleFactorMV[L0Index][L1Index] = DistScaleFactor;
                            }
                        }
                      }
                   }
                  }
                  break;
          }
    }

    ENC_Frame(cuc, 0);
    return MFX_ERR_NONE;
}

void MFXVideoEncH264::SetCurrentFrameParameters( mfxFrameCUC* cuc )
{
    enc->m_pCurrentFrame->m_PicCodType = enc->m_PicType;
    enc->m_pCurrentFrame->m_wasEncoded = false;

    if (cuc->FrameParam->AVC.FrameType & MFX_FRAMETYPE_IDR){
//        m_PicOrderCnt_Accu += m_PicOrderCnt;
//        enc->m_PicOrderCnt_LastIDR = 0; //TEMP!!!
//        enc->m_PicOrderCnt = 0;
        enc->m_PicOrderCnt_LastIDR = enc->m_pCurrentFrame->m_PicOrderCounterAccumulated;
    }

//    enc->m_pCurrentFrame->m_PicOrderCnt[0] = enc->m_PicOrderCnt*2;
//    enc->m_pCurrentFrame->m_PicOrderCnt[1] = enc->m_PicOrderCnt*2 + 1;
//    enc->m_pCurrentFrame->m_PicOrderCounterAccumulated = 2*(enc->m_PicOrderCnt + enc->m_PicOrderCnt_LastIDR);
    enc->m_pCurrentFrame->m_PicOrderCnt[0] = enc->m_pCurrentFrame->m_PicOrderCounterAccumulated;
    enc->m_pCurrentFrame->m_PicOrderCnt[1] = enc->m_pCurrentFrame->m_PicOrderCounterAccumulated + 1;
    if( cuc->FrameParam->AVC.FieldPicFlag ){
        if( cuc->FrameParam->AVC.MbaffFrameFlag ){
          enc->m_pCurrentFrame->m_PictureStructureForDec = enc->m_pCurrentFrame->m_PictureStructureForRef = AFRM_STRUCTURE;
          enc->m_pCurrentFrame->m_bottom_field_flag[0] = 0;
          enc->m_pCurrentFrame->m_bottom_field_flag[1] = 0;
        }else{
          enc->m_pCurrentFrame->m_PictureStructureForDec = enc->m_pCurrentFrame->m_PictureStructureForRef = FLD_STRUCTURE;
          enc->m_pCurrentFrame->m_bottom_field_flag[0] = 0;
          enc->m_pCurrentFrame->m_bottom_field_flag[1] = 1;
        }
    }else{
          enc->m_pCurrentFrame->m_PictureStructureForDec = enc->m_pCurrentFrame->m_PictureStructureForRef = FRM_STRUCTURE;
          enc->m_pCurrentFrame->m_bottom_field_flag[0] = 0;
          enc->m_pCurrentFrame->m_bottom_field_flag[1] = 0;
    }
//    m_pCurrentFrame->InitRefPicListResetCount(0);
//    m_pCurrentFrame->InitRefPicListResetCount(1);
//      m_pCurrentFrame->m_PicNum

    enc->m_pCurrentFrame->m_PicOrderCounterAccumulated += 2;
//      m_PicOrderCnt++;
cuc->FrameParam->AVC.CurrFrameLabel;
cuc->FrameParam->AVC.NumRefFrame;
//cuc->FrameParam->RefLabelListP[16];
//cuc->FrameParam->RefLabelListB[2][8];
cuc->FrameParam->AVC.NumRefIdxL0Minus1;
cuc->FrameParam->AVC.NumRefIdxL1Minus1;
cuc->FrameParam->AVC.Log2MaxFrameCntMinus4;
cuc->FrameParam->AVC.Log2MaxPicOrdCntMinus4;

/*
cuc->FrameParam->ResColorTransFlag :1; // = 0 (<= no 4:4:4)
cuc->FrameParam->SPforSwitchFlag :1; // = 0 (<= no SP)
cuc->FrameParam->MbsConsecutiveFlag :1; // = 1
cuc->FrameParam->NoMinorBipredFlag :1;
cuc->FrameParam->IntraPicFlag :1;
cuc->FrameParam->TransCoeffFlag :1;
cuc->FrameParam->ZeroDeltaPicOrderFlag :1;
cuc->FrameParam->GapsInFrameNumAllowed :1;
cuc->FrameParam->PicOrderPresent :1;
cuc->FrameParam->RedundantPicCntPresent :1;
cuc->FrameParam->ScalingListPresent :1;
cuc->FrameParam->SliceGroupMapPresent :1;
cuc->FrameParam->ILDBControlPresent :1;
cuc->FrameParam->MbCodePresent :1;
cuc->FrameParam->MvDataPresent :1;
cuc->FrameParam->ResDataPresent :1;
cuc->FrameParam->SliceGroupMapType;
cuc->FrameParam->SliceGroupXRateMinus1;
cuc->FrameParam->MinFrameSize;
cuc->FrameParam->MaxFrameSize;
*/

}

#include "umc_h264_bme.h"

mfxStatus MFXVideoEncH264::ENC_Slice(mfxFrameCUC *cuc, Ipp32s slice_num, H264Slice_8u16s *curr_slice, Ipp32s level)
{
    H264CurrentMacroblockDescriptor_8u16s* cur_mb = &curr_slice->m_cur_mb;
    Ipp32s MBYAdjust = 0;
    if (enc->m_field_index)
        MBYAdjust = enc->m_HeightInMBs;

    MFX_CHECK_COND(cuc->SliceParam[slice_num].AVC.CucId == MFX_CUC_AVC_SLICEPARAM);
    MFX_CHECK_COND(cuc->SliceParam[slice_num].AVC.CucSz == sizeof(mfxSliceParam));
    InitSliceFromCUC(slice_num, cuc, curr_slice );
    curr_slice->m_slice_number = slice_num;
    enc->m_NeedToCheckMBSliceEdges = enc->m_info.num_slices > 1 || enc->m_field_index >0;

    switch (cuc->SliceParam[slice_num].AVC.SliceType) {
        case MFX_SLICETYPE_I :
            curr_slice->m_slice_type = INTRASLICE;
            break;
        case MFX_SLICETYPE_P :
            curr_slice->m_slice_type = PREDSLICE;
            break;
        case MFX_SLICETYPE_B :
            curr_slice->m_slice_type = BPREDSLICE;
            break;
        default :
            return MFX_ERR_UNSUPPORTED;
    }
    curr_slice->m_slice_qp_delta = cuc->SliceParam[slice_num].AVC.SliceQpDelta;
    curr_slice->m_cabac_init_idc = cuc->SliceParam[slice_num].AVC.CabacInitIdc;
    enc->m_SliceHeader.luma_log2_weight_denom = cuc->SliceParam[slice_num].AVC.Log2WeightDenomLuma;
    enc->m_SliceHeader.chroma_log2_weight_denom = cuc->SliceParam[slice_num].AVC.Log2WeightDenomChroma;
    enc->m_SliceHeader.slice_qs_delta = cuc->SliceParam[slice_num].AVC.SliceQsDelta;
    enc->m_SliceHeader.direct_spatial_mv_pred_flag = cuc->SliceParam[slice_num].AVC.DirectPredType;
    enc->m_SliceHeader.redundant_pic_cnt = cuc->SliceParam[slice_num].AVC.RedundantPicCnt;
#if 0
    cuc->SliceParam[slice_num].AVC.NumRefIdxL0Minus1;
    cuc->SliceParam[slice_num].AVC.NumRefIdxL1Minus1;
    cuc->SliceParam[slice_num].AVC.DeblockAlphaC0OffsetDiv2;
    cuc->SliceParam[slice_num].AVC.DeblockBetaOffsetDiv2;
    cuc->SliceParam[slice_num].AVC.BsInsertionFlag;
    cuc->SliceParam[slice_num].AVC.DeblockDisableIdc;
    cuc->SliceParam[slice_num].AVC.SliceId;
#endif
    curr_slice->m_InitialOffset = enc->m_InitialOffsets[enc->m_pCurrentFrame->m_bottom_field_flag[enc->m_field_index]];
    curr_slice->m_is_cur_mb_field = enc->m_pCurrentFrame->m_PictureStructureForDec < FRM_STRUCTURE;
    curr_slice->m_is_cur_mb_bottom_field = enc->m_pCurrentFrame->m_bottom_field_flag[enc->m_field_index] == 1;
    curr_slice->m_use_transform_for_intra_decision = enc->m_info.use_transform_for_intra_decision;
    curr_slice->m_Intra_MB_Counter = 0;
    curr_slice->m_MB_Counter = 0;
    curr_slice->m_iLastXmittedQP = enc->m_PicParamSet.pic_init_qp + curr_slice->m_slice_qp_delta;
    curr_slice->m_uSkipRun = 0;
    cur_mb->chroma_format_idc = enc->m_PicParamSet.chroma_format_idc;

    // loop over all MBs in the picture
    mfxU32 uFirstMB, uNumMBs;
    uNumMBs = cuc->SliceParam[slice_num].AVC.NumMb;
    uFirstMB = curr_slice->m_first_mb_in_slice;//m_field_index*uNumMBs;

    //curr_slice->m_Intra_MB_Counter = 0; // duplicates
    //curr_slice->m_MB_Counter = 0;
    //curr_slice->m_iLastXmittedQP = enc->m_PicParamSet.pic_init_qp + curr_slice->m_slice_qp_delta;

    curr_slice->num_ref_idx_active_override_flag =
        ((curr_slice->num_ref_idx_l0_active != enc->m_PicParamSet.num_ref_idx_l0_active)
          || (curr_slice->num_ref_idx_l1_active != enc->m_PicParamSet.num_ref_idx_l1_active));

    curr_slice->m_uSkipRun = 0;

    mfxExtAvcMvData* mvs = (mfxExtAvcMvData*)GetExtBuffer(cuc->ExtBuffer, cuc->NumExtBuffer, MFX_CUC_AVC_MV);
    mfxI16Pair* pMV = mvs->Mv;

    for (Ipp32u uMB = uFirstMB; uMB < uFirstMB+uNumMBs; uMB++){
        enc->m_pCurrentFrame->m_mbinfo.mbs[uMB].slice_id = slice_num;
        cur_mb->lambda = lambda_sq[curr_slice->m_iLastXmittedQP];
        cur_mb->uMB = uMB;
        cur_mb->mbPtr = enc->m_pCurrentFrame->m_pYPlane + enc->m_pMBOffsets[uMB].uLumaOffset[enc->m_is_cur_pic_afrm][curr_slice->m_is_cur_mb_field];
        cur_mb->mbPitchPixels =  enc->m_pCurrentFrame->m_pitchPixels<<curr_slice->m_is_cur_mb_field;
        cur_mb->uMBx = (uMB % enc->m_WidthInMBs);
        cur_mb->uMBy = (uMB / enc->m_WidthInMBs)- MBYAdjust;
        H264CoreEncoder_LoadPredictedMBInfo_8u16s(enc,curr_slice);
        H264CoreEncoder_UpdateCurrentMBInfo_8u16s(enc,curr_slice);
        cur_mb->LocalMacroblockInfo->QP = curr_slice->m_iLastXmittedQP;
        cur_mb->lumaQP = getLumaQP(cur_mb->LocalMacroblockInfo->QP, enc->m_PicParamSet.bit_depth_luma);
        cur_mb->lumaQP51 = getLumaQP51(cur_mb->LocalMacroblockInfo->QP, enc->m_PicParamSet.bit_depth_luma);
        cur_mb->chromaQP = getChromaQP(cur_mb->LocalMacroblockInfo->QP, enc->m_PicParamSet.chroma_qp_index_offset, enc->m_SeqParamSet.bit_depth_chroma);
        pSetMB8x8TSFlag(cur_mb->GlobalMacroblockInfo, 0);
        curr_slice->m_MB_Counter ++;

        H264CoreEncoder_MB_Decision_8u16s(enc,curr_slice);
        H264CoreEncoder_CEncAndRecMB_8u16s(enc,curr_slice);

        mfxMbCode* mb = &cuc->MbParam->Mb[uMB];
        mfxU8& MbType = mb->AVC.MbType;
        mb->AVC.CodedPattern4x4Y = 0xffff;//cur_mb.LocalMacroblockInfo->cbp_luma;
        mb->AVC.CodedPattern4x4U = 0xffff;//cur_mb.LocalMacroblockInfo->cbp_chroma & 0xffff;
        mb->AVC.CodedPattern4x4V = 0xffff;//(cur_mb.LocalMacroblockInfo->cbp_chroma>>16) & 0xffff;
        mb->AVC.Skip8x8Flag = 0;
        mb->AVC.TransformFlag = pGetMB8x8TSPackFlag(cur_mb->GlobalMacroblockInfo ) ? 1 : 0;
        mb->AVC.ChromaIntraPredMode = cur_mb->LocalMacroblockInfo->intra_chroma_mode;

        mb->AVC.MvDataFlag = 1;
        if( IS_INTRA_MBTYPE(cur_mb->GlobalMacroblockInfo->mbtype) ){
        switch (cur_mb->GlobalMacroblockInfo->mbtype) {
            case MBTYPE_INTRA :
                if( pGetMB8x8TSPackFlag(cur_mb->GlobalMacroblockInfo) ){
                    MbType = MFX_MBTYPE_INTRA_8X8;
                    mb->AVC.LumaIntraModes[0] = (cur_mb->intra_types[0] & 0xF)      |
                                                (cur_mb->intra_types[4] & 0xF) << 4 |
                                                (cur_mb->intra_types[8] & 0xF) << 8 |
                                                (cur_mb->intra_types[12] & 0xF) << 12;
                    mb->AVC.LumaIntraModes[1] =
                    mb->AVC.LumaIntraModes[2] =
                    mb->AVC.LumaIntraModes[3] = 0;
                    mb->AVC.ChromaIntraPredMode = cur_mb->LocalMacroblockInfo->intra_chroma_mode;
                }else{
                    MbType = MFX_MBTYPE_INTRA_4X4;
                    for( int i = 0; i<4; i++ ){
                        mb->AVC.LumaIntraModes[i] = (cur_mb->intra_types[4 * i + 0] & 0xF)      |
                                                    (cur_mb->intra_types[4 * i + 1] & 0xF) << 4 |
                                                    (cur_mb->intra_types[4 * i + 2] & 0xF) << 8 |
                                                    (cur_mb->intra_types[4 * i + 3] & 0xF) << 12;
                    }
                }
                break;
            case MBTYPE_INTRA_16x16 :
                if (level >= 2)
                    MbType = 1 + cur_mb->LocalMacroblockInfo->intra_16x16_mode + 4 * cur_mb->MacroblockCoeffsInfo->chromaNC + 12 * cur_mb->MacroblockCoeffsInfo->lumaAC;
                else
                    MbType = MFX_MBTYPE_INTRA_16X16_000;
                mb->AVC.LumaIntraModes[0] = cur_mb->LocalMacroblockInfo->intra_16x16_mode & 0xf;
                mb->AVC.LumaIntraModes[1] =
                mb->AVC.LumaIntraModes[2] =
                mb->AVC.LumaIntraModes[3] = 0;
                mb->AVC.ChromaIntraPredMode = cur_mb->LocalMacroblockInfo->intra_chroma_mode;
                break;
            case MBTYPE_PCM :
                MbType = MFX_MBTYPE_INTRA_PCM;
                break;
            }
        }else{
            MbType = mfx_mbtype[cur_mb->GlobalMacroblockInfo->mbtype];
            mfxI32 restoreSkip = 0;

            if( curr_slice->m_slice_type == PREDSLICE ){
                mb->AVC.MvUnpackedFlag = MFX_MVPACK_UNPACKED_16X16_P;
            }else{
                mb->AVC.MvUnpackedFlag = MFX_MVPACK_UNPACKED_16X16_B;
            }

            if(cur_mb->GlobalMacroblockInfo->mbtype ==  MBTYPE_SKIPPED ){
                //Reset CBP for skips
                mb->AVC.Skip8x8Flag = 0xf;
                mb->AVC.CodedPattern4x4Y =
                mb->AVC.CodedPattern4x4U =
                mb->AVC.CodedPattern4x4V = 0;
                if( curr_slice->m_slice_type == BPREDSLICE ){
                    cur_mb->GlobalMacroblockInfo->mbtype =  MBTYPE_DIRECT;
                    restoreSkip = 1;
                }
            }

            //Subblocks
            if( cur_mb->GlobalMacroblockInfo->mbtype ==  MBTYPE_DIRECT ){
                mb->AVC.Skip8x8Flag = 0xf;
                //set sbtype to pack vectors
                for( mfxI32 k=0; k<4; k++ ) cur_mb->GlobalMacroblockInfo->sbtype[k] = SBTYPE_BIDIR_4x4;
            }
            if( cur_mb->GlobalMacroblockInfo->mbtype == MBTYPE_B_8x8 ||
                cur_mb->GlobalMacroblockInfo->mbtype == MBTYPE_INTER_8x8 ||
                cur_mb->GlobalMacroblockInfo->mbtype == MBTYPE_INTER_8x8_REF0 ||
                cur_mb->GlobalMacroblockInfo->mbtype == MBTYPE_DIRECT
                ){
                mb->AVC.MvUnpackedFlag = MFX_MVPACK_UNPACKED_8X8_P;
                if(cur_mb->GlobalMacroblockInfo->mbtype == MBTYPE_B_8x8 ||
                    cur_mb->GlobalMacroblockInfo->mbtype == MBTYPE_DIRECT ) mb->AVC.MvUnpackedFlag = MFX_MVPACK_UNPACKED_8X8_B;
                mb->AVC.SubMbPredMode = mb->AVC.SubMbShape = 0;
                for( mfxI32 i = 0; i < 4; i++ ){
                    MFX_SBTYPE stype = mfx_sbtype[cur_mb->GlobalMacroblockInfo->sbtype[i]];
                    mb->AVC.SubMbPredMode |= stype.pred<<(i<<1);
                    mb->AVC.SubMbShape |= stype.shape<<(i<<1);
                }
            }
            mb->AVC.MvDataOffset = (mfxI8*)pMV - (mfxI8*)mvs->Mv;
            pMV = PackMVs(cur_mb->GlobalMacroblockInfo, pMV, cur_mb->MVs[0]->MotionVectors, cur_mb->MVs[1]->MotionVectors );
            //Reset refs
            for(mfxI32 i=0; i<2; i++ ){
                mb->AVC.RefPicSelect[i][0] = 0;
                mb->AVC.RefPicSelect[i][1] = 0;
                mb->AVC.RefPicSelect[i][2] = 0;
                mb->AVC.RefPicSelect[i][3] = 0;
            }
            PackRefIdxs(cur_mb->GlobalMacroblockInfo, mb, cur_mb->RefIdxs[0]->RefIdxs, cur_mb->RefIdxs[1]->RefIdxs);
            //Restore skip
            if( cur_mb->GlobalMacroblockInfo->mbtype ==  MBTYPE_DIRECT ){
                for( mfxI32 k = 0; k<4; k++ ) cur_mb->GlobalMacroblockInfo->sbtype[k] = 0;
            }
            if( restoreSkip ){
                cur_mb->GlobalMacroblockInfo->mbtype =  MBTYPE_SKIPPED;
            }

        }

        mb->AVC.IntraMbFlag = (cur_mb->GlobalMacroblockInfo->mbtype < MBTYPE_INTER) ? 1 : 0;
        mb->AVC.FieldMbFlag = 0;

        if (level > 0) {
            cur_mb->LocalMacroblockInfo->cbp_bits = 0;
            cur_mb->LocalMacroblockInfo->cbp_bits_chroma = 0;
            H264CoreEncoder_CEncAndRecMB_8u16s(enc,curr_slice);
        }
    }
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoEncH264::ENC_Frame(mfxFrameCUC *cuc, Ipp32s level)
{
    UMC::Status stsUMC;
    mfxStatus   stsMFX;

    EnumPicCodType picType = ConvertPicType_H264enc(cuc->FrameParam->AVC.FrameType);
    MFX_CHECK_COND(picType != (EnumPicCodType)-1);
    enc->m_pCurrentFrame->m_PicCodType = picType;
    EnumPicClass picClass = ConvertPicClass_H264enc(cuc->FrameParam->AVC.FrameType);
    MFX_CHECK_COND(picClass != (EnumPicClass)-1);

//    if (enc->m_Analyse & ANALYSE_FRAME_TYPE) H264CoreEncoder_FrameTypeDetect_8u16s(enc);

    if( enc->m_Analyse & ANALYSE_RECODE_FRAME /*&& m_pReconstructFrame == NULL //could be size change*/){ //make init reconstruct buffer
        enc->m_pReconstructFrame->m_bottom_field_flag[0] = enc->m_pCurrentFrame->m_bottom_field_flag[0];
        enc->m_pReconstructFrame->m_bottom_field_flag[1] = enc->m_pCurrentFrame->m_bottom_field_flag[1];
    }
    enc->m_is_cur_pic_afrm = (Ipp32s)(enc->m_pCurrentFrame->m_PictureStructureForDec==AFRM_STRUCTURE);
    for (enc->m_field_index = 0; enc->m_field_index <= (Ipp8u)(enc->m_pCurrentFrame->m_PictureStructureForDec < FRM_STRUCTURE); enc->m_field_index ++) {
        enc->m_NeedToCheckMBSliceEdges = enc->m_info.num_slices > 1 || enc->m_field_index >0;
        Ipp32s slice;

        if (picClass == IDR_PIC) {
            if (enc->m_field_index) {
                picClass = REFERENCE_PIC;
            } else {
                H264CoreEncoder_SetSequenceParameters_8u16s(enc);
                if( cuc->FrameParam->AVC.PicOrderCntType == 2 ){
                    enc->m_SeqParamSet.pic_order_cnt_type = 2;
                    enc->m_SeqParamSet.log2_max_pic_order_cnt_lsb = 0;
                }else{
                    enc->m_SeqParamSet.pic_order_cnt_type = 0;
                    enc->m_SeqParamSet.log2_max_pic_order_cnt_lsb = cuc->FrameParam->AVC.Log2MaxPicOrdCntMinus4 + 4;
                }
                enc->m_SeqParamSet.log2_max_frame_num = cuc->FrameParam->AVC.Log2MaxFrameCntMinus4 + 4;
                enc->m_SeqParamSet.direct_8x8_inference_flag = cuc->FrameParam->AVC.Direct8x8InferenceFlag;
                H264CoreEncoder_SetPictureParameters_8u16s(enc);
            }
        }
        enc->m_PicType = picType;
        enc->m_PicParamSet.chroma_format_idc = enc->m_SeqParamSet.chroma_format_idc;
        enc->m_PicParamSet.bit_depth_luma = enc->m_SeqParamSet.bit_depth_luma;
        //if (!(enc->m_Analyse & ANALYSE_RECODE_FRAME))
        //    enc->m_pReconstructFrame = enc->m_pCurrentFrame;
//        H264CoreEncoder_SetSliceHeaderCommon_8u16s(enc,enc->m_pCurrentFrame);
        enc->m_SliceHeader.frame_num = enc->m_pCurrentFrame->m_FrameNum % (1 << enc->m_SeqParamSet->log2_max_frame_num);
        // Assumes there is only one picture parameter set to choose from
        enc->m_SliceHeader.pic_parameter_set_id = enc->m_PicParamSet.pic_parameter_set_id;

        enc->m_SliceHeader.field_pic_flag = (Ipp8u)(enc->m_pCurrentFrame->m_PictureStructureForDec<FRM_STRUCTURE);
        enc->m_SliceHeader.bottom_field_flag = (Ipp8u)(enc->m_pCurrentFrame->m_bottom_field_flag[enc->m_field_index]);
        enc->m_SliceHeader.MbaffFrameFlag = (enc->m_SeqParamSet.mb_adaptive_frame_field_flag)&&(!enc->m_SliceHeader.field_pic_flag);
        enc->m_SliceHeader.delta_pic_order_cnt_bottom = enc->m_SeqParamSet.frame_mbs_only_flag == 0;


        if (enc->m_SeqParamSet.pic_order_cnt_type == 0) {
            enc->m_SliceHeader.pic_order_cnt_lsb = H264EncoderFrame_PicOrderCnt_8u16s(
                enc->m_pCurrentFrame, enc->m_field_index,
                    0) & ~(0xffffffff << enc->m_SeqParamSet.log2_max_pic_order_cnt_lsb);
        }
        enc->m_SliceHeader.adaptive_ref_pic_marking_mode_flag = 0;

        stsUMC = H264CoreEncoder_Start_Picture_8u16s(enc, &picClass, picType);
        if (stsUMC != UMC_OK)
            return ConvertStatus_H264enc(stsUMC);
//        UpdateRefPicListCommon();
#if defined _OPENMP
        vm_thread_priority mainTreadPriority = vm_get_current_thread_priority();
#pragma omp parallel for IPP_OMP_NUM_THREADS() private(slice)
#endif // _OPENMP
        for (slice = 0; slice < cuc->NumSlice; slice ++) {
#if defined _OPENMP
            vm_set_current_thread_priority(mainTreadPriority);
#endif // _OPENMP
             stsMFX = ENC_Slice(cuc, slice, enc->m_Slices + slice, level);
             MFX_CHECK_STS(stsMFX);
        }
        if (picClass != DISPOSABLE_PIC) {
//            End_Picture();
//            UpdateRefPicMarking();
        }
    }
    /*
    if( enc->m_Analyse & ANALYSE_RECODE_FRAME ){
        H264EncoderFrame_exchangeFrameYUVPointers_8u16s(enc->m_pReconstructFrame, enc->m_pCurrentFrame);
        ippsFree(enc->m_pReconstructFrame);
        enc->m_pReconstructFrame = NULL;
    }
    */
//    m_uFrames_Num ++;
    return MFX_ERR_NONE;
}

mfxStatus MFXVideoEncH264::Query(mfxVideoParam *in, mfxVideoParam *out)
{
    MFX_CHECK_NULL_PTR1(out)
    CHECK_VERSION(in->Version);
    CHECK_VERSION(out->Version);

    //First check for zero input
    if( in == NULL ){ //Set ones for filed that can be configured
        memset( out, 0, sizeof( mfxVideoParam ));
        out->mfx.CodecId = 1;
        out->mfx.CodecLevel = 1;
        out->mfx.CodecProfile = 1;
        out->mfx.FrameInfo.AspectRatioW = 1;
        out->mfx.FrameInfo.AspectRatioH = 1;
        out->mfx.FrameInfo.ChromaFormat = 1;
        out->mfx.FrameInfo.FrameRateExtN = 1;
        out->mfx.FrameInfo.FrameRateExtD = 1;
        out->mfx.FrameInfo.CropH = 1;
        out->mfx.FrameInfo.CropW = 1;
        out->mfx.FrameInfo.CropX = 1;
        out->mfx.FrameInfo.CropY = 1;
        out->mfx.FrameInfo.FourCC = 1;
        out->mfx.FrameInfo.Height = 1;
        //out->mfx.FrameInfo.NumFrameData = 1;
        out->mfx.FrameInfo.PicStruct = 1;
        out->mfx.FrameInfo.Width = 1;
        out->mfx.GopPicSize = 1;
        out->mfx.GopRefDist = 1;
        out->mfx.GopOptFlag = 1;
//        out->mfx.CAVLC = 1;
        out->mfx.NumSlice = 1;
        out->mfx.NumThread = 1;
        out->mfx.RateControlMethod = 1;
        out->mfx.TargetKbps = 1;
        out->mfx.TargetUsage = 1;
    }else{ //Check options for correctness
        memset( out, 0, sizeof( mfxVideoParam ));
        out->mfx.CodecId = MFX_CODEC_AVC;
        out->mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;

        //Check for valid height and width
        if( (in->mfx.FrameInfo.Width & 15) || (in->mfx.FrameInfo.Width > 4096) ) out->mfx.FrameInfo.Width = 0;
        else out->mfx.FrameInfo.Width = in->mfx.FrameInfo.Width;
        if( (in->mfx.FrameInfo.Height & 15) || (in->mfx.FrameInfo.Height > 4096) ) out->mfx.FrameInfo.Height = 0;
        else out->mfx.FrameInfo.Height = in->mfx.FrameInfo.Height;

        switch( in->mfx.CodecLevel ){
            case MFX_LEVEL_AVC_1:
            case MFX_LEVEL_AVC_1b:
            case MFX_LEVEL_AVC_11:
            case MFX_LEVEL_AVC_12:
            case MFX_LEVEL_AVC_13:
            case MFX_LEVEL_AVC_2:
            case MFX_LEVEL_AVC_21:
            case MFX_LEVEL_AVC_22:
            case MFX_LEVEL_AVC_3:
            case MFX_LEVEL_AVC_31:
            case MFX_LEVEL_AVC_32:
            case MFX_LEVEL_AVC_4:
            case MFX_LEVEL_AVC_41:
            case MFX_LEVEL_AVC_42:
            case MFX_LEVEL_AVC_5:
            case MFX_LEVEL_AVC_51:
                out->mfx.CodecLevel = in->mfx.CodecLevel;
                break;
            default:
                out->mfx.CodecLevel = MFX_LEVEL_AVC_5;
                break;
        }
        out->mfx.CodecProfile = MFX_PROFILE_AVC_MAIN;
        out->mfx.FrameInfo.AspectRatioH = 1;
        out->mfx.FrameInfo.AspectRatioW = 1;
        out->mfx.FrameInfo.FrameRateExtN = in->mfx.FrameInfo.FrameRateExtN;
        out->mfx.FrameInfo.FrameRateExtD = in->mfx.FrameInfo.FrameRateExtD;

        out->mfx.FrameInfo.ChromaFormat = 1;

        if (in->mfx.FrameInfo.CropH + in->mfx.FrameInfo.CropY > in->mfx.FrameInfo.Height)
        {
            out->mfx.FrameInfo.CropH = 0;
            out->mfx.FrameInfo.CropY = 0;
        }
        else
        {
            out->mfx.FrameInfo.CropH = in->mfx.FrameInfo.CropH;
            out->mfx.FrameInfo.CropY = in->mfx.FrameInfo.CropY;
        }
        if (in->mfx.FrameInfo.CropW + in->mfx.FrameInfo.CropX > in->mfx.FrameInfo.Width)
        {
            out->mfx.FrameInfo.CropW = 0;
            out->mfx.FrameInfo.CropX = 0;
        }
        else
        {
            out->mfx.FrameInfo.CropW = in->mfx.FrameInfo.CropW;
            out->mfx.FrameInfo.CropX = in->mfx.FrameInfo.CropX;
        }

        //if( in->mfx.FrameInfo.NumFrameData != 1 ) in->mfx.FrameInfo.NumFrameData = 1;

        out->mfx.FrameInfo.PicStruct = 1;

        if( in->mfx.GopPicSize <= 0 ) out->mfx.GopPicSize = 1;

        if( in->mfx.GopRefDist  < 0 ) out->mfx.GopRefDist = 0;
        if( in->mfx.GopRefDist  > 3 ) out->mfx.GopRefDist = 3;
        else out->mfx.GopRefDist = in->mfx.GopRefDist;

//        out->mfx.CAVLC = 1;

        if( in->mfx.NumSlice <= 0 ) out->mfx.NumSlice = 1;
        else out->mfx.NumSlice = 1;

        out->mfx.NumThread = 1;

        out->mfx.RateControlMethod = 1;
        if( in->mfx.TargetKbps < 10 ) out->mfx.TargetKbps = 10;

        out->mfx.TargetUsage = 0;
    }

    return MFX_ERR_NONE;
}


mfxStatus MFXVideoEncH264::QueryIOSurf(mfxVideoParam *par, mfxFrameAllocRequest *request)
{
    MFX_CHECK_NULL_PTR1(par)
        MFX_CHECK_NULL_PTR1(request)

        request->NumFrameMin = request->NumFrameSuggested = par->mfx.GopRefDist + 2;

    return MFX_ERR_NONE;
}

#endif //MFX_ENABLE_H264_VIDEO_ENCODER
