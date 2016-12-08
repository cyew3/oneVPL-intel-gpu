//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2016 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#include "umc_h264_widevine_supplier.h"

#include "umc_va_dxva2_protected.h"
#include "umc_va_linux_protected.h"
#include "umc_h264_notify.h"

#if defined (MFX_VA) && !defined (MFX_PROTECTED_FEATURE_DISABLE)

namespace UMC
{

void POCDecoderWidevine::DecodePictureOrderCount(const H264Slice *slice, Ipp32s frame_num)
{
    const H264SliceHeader *sliceHeader = slice->GetSliceHeader();
    const H264SeqParamSet* sps = slice->GetSeqParam();

    if (sps->pic_order_cnt_type == 0)
    {
        m_PicOrderCnt = sliceHeader->pic_order_cnt_lsb;
        m_TopFieldPOC = sliceHeader->pic_order_cnt_lsb;
        m_BottomFieldPOC = sliceHeader->pic_order_cnt_lsb + sliceHeader->delta_pic_order_cnt_bottom;

        //Ipp32s CurrPicOrderCntMsb;
        //Ipp32s MaxPicOrderCntLsb = sps->MaxPicOrderCntLsb;

        //if ((sliceHeader->pic_order_cnt_lsb < m_PicOrderCntLsb) &&
        //     ((m_PicOrderCntLsb - sliceHeader->pic_order_cnt_lsb) >= (MaxPicOrderCntLsb >> 1)))
        //    CurrPicOrderCntMsb = m_PicOrderCntMsb + MaxPicOrderCntLsb;
        //else if ((sliceHeader->pic_order_cnt_lsb > m_PicOrderCntLsb) &&
        //        ((sliceHeader->pic_order_cnt_lsb - m_PicOrderCntLsb) > (MaxPicOrderCntLsb >> 1)))
        //    CurrPicOrderCntMsb = m_PicOrderCntMsb - MaxPicOrderCntLsb;
        //else
        //    CurrPicOrderCntMsb = m_PicOrderCntMsb;

        //if (sliceHeader->nal_ref_idc)
        //{
        //    // reference picture
        //    m_PicOrderCntMsb = CurrPicOrderCntMsb & (~(MaxPicOrderCntLsb - 1));
        //    m_PicOrderCntLsb = sliceHeader->pic_order_cnt_lsb;
        //}
        //m_PicOrderCnt = CurrPicOrderCntMsb + sliceHeader->pic_order_cnt_lsb;
        //if (sliceHeader->field_pic_flag == 0)
        //{
        //     m_TopFieldPOC = CurrPicOrderCntMsb + sliceHeader->pic_order_cnt_lsb;
        //     m_BottomFieldPOC = m_TopFieldPOC + sliceHeader->delta_pic_order_cnt_bottom;
        //}
    }
    else if (sps->pic_order_cnt_type == 1)
    {
        m_PicOrderCnt = sliceHeader->pic_order_cnt_lsb;
        m_TopFieldPOC = sliceHeader->pic_order_cnt_lsb;
        m_BottomFieldPOC = sliceHeader->pic_order_cnt_lsb + sliceHeader->delta_pic_order_cnt_bottom;

        if ((sliceHeader->delta_pic_order_cnt[0] == 0))
        {
            Ipp32u i;
            Ipp32u uAbsFrameNum;    // frame # relative to last IDR pic
            Ipp32u uPicOrderCycleCnt = 0;
            Ipp32u uFrameNuminPicOrderCntCycle = 0;
            Ipp32s ExpectedPicOrderCnt = 0;
            Ipp32s ExpectedDeltaPerPicOrderCntCycle;
            Ipp32u uNumRefFramesinPicOrderCntCycle = sps->num_ref_frames_in_pic_order_cnt_cycle;
            Ipp32s uMaxFrameNum = (1<<sps->log2_max_frame_num);

            if (frame_num < m_FrameNum)
                m_FrameNumOffset += uMaxFrameNum;

            if (uNumRefFramesinPicOrderCntCycle != 0)
                uAbsFrameNum = m_FrameNumOffset + frame_num;
            else
                uAbsFrameNum = 0;

            if ((sliceHeader->nal_ref_idc == false)  && (uAbsFrameNum > 0))
                uAbsFrameNum--;

            if (uAbsFrameNum)
            {
                uPicOrderCycleCnt = (uAbsFrameNum - 1) /
                        uNumRefFramesinPicOrderCntCycle;
                uFrameNuminPicOrderCntCycle = (uAbsFrameNum - 1) %
                        uNumRefFramesinPicOrderCntCycle;
            }

            ExpectedDeltaPerPicOrderCntCycle = 0;
            for (i=0; i < uNumRefFramesinPicOrderCntCycle; i++)
            {
                ExpectedDeltaPerPicOrderCntCycle +=
                    sps->poffset_for_ref_frame[i];
            }

            if (uAbsFrameNum)
            {
                ExpectedPicOrderCnt = uPicOrderCycleCnt * ExpectedDeltaPerPicOrderCntCycle;
                for (i=0; i<=uFrameNuminPicOrderCntCycle; i++)
                {
                    ExpectedPicOrderCnt +=
                        sps->poffset_for_ref_frame[i];
                }
            }
            else
                ExpectedPicOrderCnt = 0;

            if (sliceHeader->nal_ref_idc == false)
                ExpectedPicOrderCnt += sps->offset_for_non_ref_pic;
            m_PicOrderCnt = ExpectedPicOrderCnt + sliceHeader->delta_pic_order_cnt[0];
            if (sliceHeader->field_pic_flag==0)
            {
                m_TopFieldPOC = ExpectedPicOrderCnt + sliceHeader->delta_pic_order_cnt[ 0 ];
                m_BottomFieldPOC = m_TopFieldPOC +
                    sps->offset_for_top_to_bottom_field + sliceHeader->delta_pic_order_cnt[ 1 ];
            }
            else if (! sliceHeader->bottom_field_flag)
                m_PicOrderCnt = ExpectedPicOrderCnt + sliceHeader->delta_pic_order_cnt[ 0 ];
            else
                m_PicOrderCnt  = ExpectedPicOrderCnt + sps->offset_for_top_to_bottom_field + sliceHeader->delta_pic_order_cnt[ 0 ];
        }
    }
    else if (sps->pic_order_cnt_type == 2)
    {
        Ipp32s iMaxFrameNum = (1<<sps->log2_max_frame_num);
        Ipp32u uAbsFrameNum;    // frame # relative to last IDR pic

        if (frame_num < m_FrameNum)
            m_FrameNumOffset += iMaxFrameNum;
        uAbsFrameNum = frame_num + m_FrameNumOffset;
        m_PicOrderCnt = uAbsFrameNum*2;
        if (sliceHeader->nal_ref_idc == false)
            m_PicOrderCnt--;
            m_TopFieldPOC = m_PicOrderCnt;
            m_BottomFieldPOC = m_PicOrderCnt;
    }

    if (sliceHeader->nal_ref_idc)
    {
        m_PrevFrameRefNum = frame_num;
    }

    m_FrameNum = frame_num;
}

WidevineTaskSupplier::WidevineTaskSupplier():
    VATaskSupplier(),
    m_pWidevineDecrypter(0)
{
}

WidevineTaskSupplier::~WidevineTaskSupplier()
{
    if (m_pWidevineDecrypter)
        delete m_pWidevineDecrypter;
}

Status WidevineTaskSupplier::Init(VideoDecoderParams *pInit)
{
    Status umsRes = VATaskSupplier::Init(pInit);
    if (umsRes != UMC_OK)
        return umsRes;

#if defined(MFX_VA)
    if (m_initializationParams.pVideoAccelerator->GetProtectedVA() &&
        (IS_PROTECTION_WIDEVINE(m_initializationParams.pVideoAccelerator->GetProtectedVA()->GetProtected())))
    {
        m_pWidevineDecrypter = new WidevineDecrypter;
        m_pWidevineDecrypter->Init();
        m_pWidevineDecrypter->SetVideoHardwareAccelerator(((UMC::VideoDecoderParams*)pInit)->pVideoAccelerator);

        for (Ipp32s i = 0; i < MVC_Extension::GetViewCount(); i++)
        {
            ViewItem &view = GetViewByNumber(i);
            for (Ipp32u j = 0; j < MAX_NUM_LAYERS; j++)
            {
                view.pPOCDec[j].reset(new POCDecoderWidevine());
            }
        }
    }
#endif

#if defined(ANDROID)
    m_DPBSizeEx += 2; // Fix for Netflix freeze issue
#endif

    return UMC_OK;
}

void WidevineTaskSupplier::Reset()
{
    VATaskSupplier::Reset();

#if defined(MFX_VA)
    if (m_initializationParams.pVideoAccelerator->GetProtectedVA() &&
        (IS_PROTECTION_WIDEVINE(m_initializationParams.pVideoAccelerator->GetProtectedVA()->GetProtected())))
    {
        m_pWidevineDecrypter->Reset();
    }
#endif
}

static
bool IsNeedSPSInvalidate(const H264SeqParamSet *old_sps, const H264SeqParamSet *new_sps)
{
    if (!old_sps || !new_sps)
        return false;

    //if (new_sps->no_output_of_prior_pics_flag)
      //  return true;

    if (old_sps->frame_width_in_mbs != new_sps->frame_width_in_mbs)
        return true;

    if (old_sps->frame_height_in_mbs != new_sps->frame_height_in_mbs)
        return true;

    if (old_sps->vui.max_dec_frame_buffering < new_sps->vui.max_dec_frame_buffering)
        return true;

    if (old_sps->chroma_format_idc != new_sps->chroma_format_idc)
        return true;

    if (old_sps->profile_idc != new_sps->profile_idc)
        return true;

    if (old_sps->bit_depth_luma != new_sps->bit_depth_luma)
        return true;

    if (old_sps->bit_depth_chroma != new_sps->bit_depth_chroma)
        return true;

    return false;
}

Status WidevineTaskSupplier::ParseWidevineSPSPPS(DecryptParametersWrapper* pDecryptParams)
{
    ViewItem *view = GetViewCount() ? &GetViewByNumber(BASE_VIEW) : 0;
    Status umcRes = UMC_OK;

    H264HeadersBitstream bitStream;

    try
    {
        //H264MemoryPiece mem;
        //mem.SetData(nalUnit);

        //H264MemoryPiece * pMem = m_Heap.Allocate(nalUnit->GetDataSize() + DEFAULT_NU_TAIL_SIZE);

        //memset(pMem->GetPointer() + nalUnit->GetDataSize(), DEFAULT_NU_TAIL_VALUE, DEFAULT_NU_TAIL_SIZE);

        //SwapperBase * swapper = m_pNALSplitter->GetSwapper();
        //swapper->SwapMemory(pMem, &mem);

        //bitStream.Reset((Ipp8u*)pMem->GetPointer(), (Ipp32u)pMem->GetDataSize());

        //NAL_Unit_Type nal_unit_type;
        //Ipp32u nal_ref_idc;

        //bitStream.GetNALUnitType(nal_unit_type, nal_ref_idc);

        //switch(nal_unit_type)
        //{
        //// sequence parameter set
        //case NAL_UT_SPS:
            {
                H264SeqParamSet sps;
                sps.seq_parameter_set_id = MAX_NUM_SEQ_PARAM_SETS;
                umcRes = pDecryptParams->GetSequenceParamSet(&sps);
                if (umcRes != UMC_OK)
                {
                    H264SeqParamSet * old_sps = m_Headers.m_SeqParams.GetHeader(sps.seq_parameter_set_id);
                    if (old_sps)
                        old_sps->errorFlags = 1;
                    return UMC_ERR_INVALID_STREAM;
                }

                Ipp8u newDPBsize = (Ipp8u)CalculateDPBSize(sps.level_idc,
                                            sps.frame_width_in_mbs * 16,
                                            sps.frame_height_in_mbs * 16,
                                            sps.num_ref_frames);

                bool isNeedClean = sps.vui.max_dec_frame_buffering == 0;
                sps.vui.max_dec_frame_buffering = sps.vui.max_dec_frame_buffering ? sps.vui.max_dec_frame_buffering : newDPBsize;
                if (sps.vui.max_dec_frame_buffering > newDPBsize)
                    sps.vui.max_dec_frame_buffering = newDPBsize;

                if (sps.num_ref_frames > newDPBsize)
                    sps.num_ref_frames = newDPBsize;

                const H264SeqParamSet * old_sps = m_Headers.m_SeqParams.GetCurrentHeader();
                bool newResolution = false;
                if (IsNeedSPSInvalidate(old_sps, &sps))
                {
                    newResolution = true;
                }

                if (isNeedClean)
                    sps.vui.max_dec_frame_buffering = 0;

                //DEBUG_PRINT((VM_STRING("debug headers SPS - %d, num_ref_frames - %d \n"), sps.seq_parameter_set_id, sps.num_ref_frames));

                H264SeqParamSet * temp = m_Headers.m_SeqParams.GetHeader(sps.seq_parameter_set_id);
                m_Headers.m_SeqParams.AddHeader(&sps);

                // Validate the incoming bitstream's image dimensions.
                temp = m_Headers.m_SeqParams.GetHeader(sps.seq_parameter_set_id);
                if (!temp)
                {
                    return UMC_ERR_NULL_PTR;
                }
                if (view)
                {
                    view->SetDPBSize(&sps, m_level_idc);
                    temp->vui.max_dec_frame_buffering = (Ipp8u)view->maxDecFrameBuffering;
                }

                //m_pNALSplitter->SetSuggestedSize(CalculateSuggestedSize(&sps));

                if (!temp->vui.timing_info_present_flag || m_use_external_framerate)
                {
                    temp->vui.num_units_in_tick = 90000;
                    temp->vui.time_scale = (Ipp32u)(2*90000 / m_local_delta_frame_time);
                }

                m_local_delta_frame_time = 1 / ((0.5 * temp->vui.time_scale) / temp->vui.num_units_in_tick);

                ErrorStatus::isSPSError = 0;

                if (newResolution)
                    return UMC_NTF_NEW_RESOLUTION;
            }
        //    break;

        //case NAL_UT_SPS_EX:
        //    {
        //        H264SeqParamSetExtension sps_ex;
        //        umcRes = bitStream.GetSequenceParamSetExtension(&sps_ex);

        //        if (umcRes != UMC_OK)
        //            return UMC_ERR_INVALID_STREAM;

        //        m_Headers.m_SeqExParams.AddHeader(&sps_ex);
        //    }
        //    break;

        //    // picture parameter set
        //case NAL_UT_PPS:
            {
                H264PicParamSet pps;
                // set illegal id
                pps.pic_parameter_set_id = MAX_NUM_PIC_PARAM_SETS;

                // Get id
                umcRes = pDecryptParams->GetPictureParamSetPart1(&pps);
                if (UMC_OK != umcRes)
                {
                    H264PicParamSet * old_pps = m_Headers.m_PicParams.GetHeader(pps.pic_parameter_set_id);
                    if (old_pps)
                        old_pps->errorFlags = 1;
                    return UMC_ERR_INVALID_STREAM;
                }

                H264SeqParamSet *refSps = m_Headers.m_SeqParams.GetHeader(pps.seq_parameter_set_id);
                Ipp32u prevActivePPS = m_Headers.m_PicParams.GetCurrentID();

                if (!refSps || refSps->seq_parameter_set_id >= MAX_NUM_SEQ_PARAM_SETS)
                {
                    refSps = m_Headers.m_SeqParamsMvcExt.GetHeader(pps.seq_parameter_set_id);
                    if (!refSps || refSps->seq_parameter_set_id >= MAX_NUM_SEQ_PARAM_SETS)
                    {
                        refSps = m_Headers.m_SeqParamsSvcExt.GetHeader(pps.seq_parameter_set_id);
                        if (!refSps || refSps->seq_parameter_set_id >= MAX_NUM_SEQ_PARAM_SETS)
                            return UMC_ERR_INVALID_STREAM;
                    }
                }

                // Get rest of pic param set
                umcRes = pDecryptParams->GetPictureParamSetPart2(&pps);
                if (UMC_OK != umcRes)
                {
                    H264PicParamSet * old_pps = m_Headers.m_PicParams.GetHeader(pps.pic_parameter_set_id);
                    if (old_pps)
                        old_pps->errorFlags = 1;
                    return UMC_ERR_INVALID_STREAM;
                }

                //DEBUG_PRINT((VM_STRING("debug headers PPS - %d - SPS - %d\n"), pps.pic_parameter_set_id, pps.seq_parameter_set_id));
                m_Headers.m_PicParams.AddHeader(&pps);

                //m_Headers.m_SeqParams.SetCurrentID(pps.seq_parameter_set_id);
                // in case of MVC restore previous active PPS
                if ((H264VideoDecoderParams::H264_PROFILE_MULTIVIEW_HIGH == refSps->profile_idc) ||
                    (H264VideoDecoderParams::H264_PROFILE_STEREO_HIGH == refSps->profile_idc))
                {
                    m_Headers.m_PicParams.SetCurrentID(prevActivePPS);
                }

                ErrorStatus::isPPSError = 0;
            }
        //    break;

        //// subset sequence parameter set
        //case NAL_UT_SUBSET_SPS:
        //    {
        //        if (!IsExtension())
        //            break;

        //        H264SeqParamSet sps;
        //        umcRes = bitStream.GetSequenceParamSet(&sps);
        //        if (UMC_OK != umcRes)
        //        {
        //            return UMC_ERR_INVALID_STREAM;
        //        }

        //        m_pNALSplitter->SetSuggestedSize(CalculateSuggestedSize(&sps));

        //        DEBUG_PRINT((VM_STRING("debug headers SUBSET SPS - %d, profile_idc - %d, level_idc - %d, num_ref_frames - %d \n"), sps.seq_parameter_set_id, sps.profile_idc, sps.level_idc, sps.num_ref_frames));

        //        if ((sps.profile_idc == H264VideoDecoderParams::H264_PROFILE_SCALABLE_BASELINE) ||
        //            (sps.profile_idc == H264VideoDecoderParams::H264_PROFILE_SCALABLE_HIGH))
        //        {
        //            H264SeqParamSetSVCExtension spsSvcExt;
        //            H264SeqParamSet * sps_temp = &spsSvcExt;

        //            *sps_temp = sps;

        //            umcRes = bitStream.GetSequenceParamSetSvcExt(&spsSvcExt);
        //            if (UMC_OK != umcRes)
        //            {
        //                return UMC_ERR_INVALID_STREAM;
        //            }

        //            const H264SeqParamSetSVCExtension * old_sps = m_Headers.m_SeqParamsSvcExt.GetCurrentHeader();
        //            bool newResolution = false;
        //            if (old_sps && old_sps->profile_idc != spsSvcExt.profile_idc)
        //            {
        //                newResolution = true;
        //            }

        //            m_Headers.m_SeqParamsSvcExt.AddHeader(&spsSvcExt);

        //            SVC_Extension::ChooseLevelIdc(&spsSvcExt, sps.level_idc, sps.level_idc);

        //            if (view)
        //            {
        //                view->SetDPBSize(&sps, m_level_idc);
        //            }

        //            if (newResolution)
        //                return UMC_NTF_NEW_RESOLUTION;
        //        }

        //        // decode additional parameters
        //        if ((H264VideoDecoderParams::H264_PROFILE_MULTIVIEW_HIGH == sps.profile_idc) ||
        //            (H264VideoDecoderParams::H264_PROFILE_STEREO_HIGH == sps.profile_idc))
        //        {
        //            H264SeqParamSetMVCExtension spsMvcExt;
        //            H264SeqParamSet * sps_temp = &spsMvcExt;

        //            *sps_temp = sps;

        //            // skip one bit
        //            bitStream.Get1Bit();
        //            // decode  MVC extension
        //            umcRes = bitStream.GetSequenceParamSetMvcExt(&spsMvcExt);
        //            if (UMC_OK != umcRes)
        //            {
        //                return UMC_ERR_INVALID_STREAM;
        //            }

        //            const H264SeqParamSetMVCExtension * old_sps = m_Headers.m_SeqParamsMvcExt.GetCurrentHeader();
        //            bool newResolution = false;
        //            if (old_sps && old_sps->profile_idc != spsMvcExt.profile_idc)
        //            {
        //                newResolution = true;
        //            }

        //            DEBUG_PRINT((VM_STRING("debug headers SUBSET SPS MVC ext - %d \n"), sps.seq_parameter_set_id));
        //            m_Headers.m_SeqParamsMvcExt.AddHeader(&spsMvcExt);

        //            MVC_Extension::ChooseLevelIdc(&spsMvcExt, sps.level_idc, sps.level_idc);

        //            if (view)
        //            {
        //                view->SetDPBSize(&sps, m_level_idc);
        //            }

        //            if (newResolution)
        //                return UMC_NTF_NEW_RESOLUTION;
        //        }
        //    }
        //    break;

        //// decode a prefix nal unit
        //case NAL_UT_PREFIX:
        //    {
        //        umcRes = bitStream.GetNalUnitPrefix(&m_Headers.m_nalExtension, nal_ref_idc);
        //        if (UMC_OK != umcRes)
        //        {
        //            m_Headers.m_nalExtension.extension_present = 0;
        //            return UMC_ERR_INVALID_STREAM;
        //        }
        //    }
        //    break;

        //default:
        //    break;
        //}
    }
    catch(const h264_exception & ex)
    {
        return ex.GetStatus();
    }
    catch(...)
    {
        return UMC_ERR_INVALID_STREAM;
    }

    {
        H264SeqParamSet * currSPS = m_Headers.m_SeqParams.GetCurrentHeader();

        if (currSPS)
        {
            if (currSPS->chroma_format_idc > 2)
                throw h264_exception(UMC_ERR_UNSUPPORTED);

            switch (currSPS->profile_idc)
            {
            case H264VideoDecoderParams::H264_PROFILE_UNKNOWN:
            case H264VideoDecoderParams::H264_PROFILE_BASELINE:
            case H264VideoDecoderParams::H264_PROFILE_MAIN:
            case H264VideoDecoderParams::H264_PROFILE_EXTENDED:
            case H264VideoDecoderParams::H264_PROFILE_HIGH:
            case H264VideoDecoderParams::H264_PROFILE_HIGH10:
            case H264VideoDecoderParams::H264_PROFILE_HIGH422:

            case H264VideoDecoderParams::H264_PROFILE_MULTIVIEW_HIGH:
            case H264VideoDecoderParams::H264_PROFILE_STEREO_HIGH:

            case H264VideoDecoderParams::H264_PROFILE_SCALABLE_BASELINE:
            case H264VideoDecoderParams::H264_PROFILE_SCALABLE_HIGH:
                break;
            default:
                throw h264_exception(UMC_ERR_UNSUPPORTED);
            }

            DPBOutput::OnNewSps(currSPS);
        }
    }
//    {
//        // save sps/pps
//        Ipp32u nal_unit_type = nalUnit->GetExData()->values[0];
//        switch(nal_unit_type)
//        {
//            case NAL_UT_SPS:
//            case NAL_UT_PPS:
//                {
//                    static Ipp8u start_code_prefix[] = {0, 0, 0, 1};
//
//                    size_t size = nalUnit->GetDataSize();
//                    bool isSPS = (nal_unit_type == NAL_UT_SPS);
//                    RawHeader * hdr = isSPS ? GetSPS() : GetPPS();
//                    Ipp32s id = isSPS ? m_Headers.m_SeqParams.GetCurrentID() : m_Headers.m_PicParams.GetCurrentID();
//                    hdr->Resize(id, size + sizeof(start_code_prefix));
//                    memcpy(hdr->GetPointer(), start_code_prefix,  sizeof(start_code_prefix));
//                    memcpy(hdr->GetPointer() + sizeof(start_code_prefix), (Ipp8u*)nalUnit->GetDataPointer(), size);
//#ifdef __APPLE__
//                    hdr->SetRBSPSize(size);
//#endif
//                 }
//            break;
//        }
//    }

    //MediaDataEx::_MediaDataEx* pMediaDataEx = nalUnit->GetExData();
    if (/*(NAL_Unit_Type)pMediaDataEx->values[0] == NAL_UT_SPS && */m_firstVideoParams.mfx.FrameInfo.Width)
    {
        H264SeqParamSet * currSPS = m_Headers.m_SeqParams.GetCurrentHeader();

        if (currSPS)
        {
            if (m_firstVideoParams.mfx.FrameInfo.Width < (currSPS->frame_width_in_mbs * 16) ||
                m_firstVideoParams.mfx.FrameInfo.Height < (currSPS->frame_height_in_mbs * 16) ||
                (currSPS->level_idc && m_firstVideoParams.mfx.CodecLevel && m_firstVideoParams.mfx.CodecLevel < currSPS->level_idc))
            {
                return UMC_NTF_NEW_RESOLUTION;
            }
        }

        //return UMC_WRN_REPOSITION_INPROGRESS;
    }

    //Ipp32u nal_unit_type = nalUnit->GetExData()->values[0];
    if (/*nal_unit_type == NAL_UT_SPS && */m_firstVideoParams.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE &&
        isMVCProfile(m_firstVideoParams.mfx.CodecProfile) && m_va && (m_va->m_HWPlatform >= MFX_HW_HSW))
    {
        H264SeqParamSet * currSPS = m_Headers.m_SeqParams.GetCurrentHeader();
        if (currSPS && !currSPS->frame_mbs_only_flag)
        {
            return UMC_NTF_NEW_RESOLUTION;
        }
    }

    return UMC_OK;
}

H264Slice *WidevineTaskSupplier::ParseWidevineSliceHeader(DecryptParametersWrapper* pDecryptParams)
{
    if ((0 > m_Headers.m_SeqParams.GetCurrentID()) ||
        (0 > m_Headers.m_PicParams.GetCurrentID()))
    {
        return 0;
    }

    if (m_Headers.m_nalExtension.extension_present)
    {
        if (SVC_Extension::IsShouldSkipSlice(&m_Headers.m_nalExtension))
        {
            m_Headers.m_nalExtension.extension_present = 0;
            return 0;
        }
    }

    H264Slice * pSlice = m_ObjHeap.AllocateObject<H264Slice>();
    if (!pSlice)
    {
        return 0;
    }
    pSlice->SetHeap(&m_ObjHeap);
    pSlice->IncrementReference();

    notifier0<H264Slice> memory_leak_preventing_slice(pSlice, &H264Slice::DecrementReference);

    //H264MemoryPiece memCopy;
    //memCopy.SetData(nalUnit);

    //pSlice->m_pSource.Allocate(nalUnit->GetDataSize() + DEFAULT_NU_SLICE_TAIL_SIZE);

    //notifier0<H264MemoryPiece> memory_leak_preventing(&pSlice->m_pSource, &H264MemoryPiece::Release);

    //SwapperBase * swapper = m_pNALSplitter->GetSwapper();
    //swapper->SwapMemory(pMem, pMemCopy);

    Ipp32s pps_pid = pDecryptParams->PicParams.pic_parameter_set_id;//pSlice->RetrievePicParamSetNumber(pMem->GetPointer(), pMem->GetSize());
    if (pps_pid == -1)
    {
        ErrorStatus::isPPSError = 1;
        return 0;
    }

    H264SEIPayLoad * spl = m_Headers.m_SEIParams.GetHeader(SEI_RECOVERY_POINT_TYPE);
//May be uncomment?
    //if (m_WaitForIDR)
    //{
    //    if (pSlice->GetSliceHeader()->slice_type != INTRASLICE && !spl)
    //    {
    //        return 0;
    //    }
    //}

    pSlice->m_pPicParamSet = m_Headers.m_PicParams.GetHeader(pps_pid);
    if (!pSlice->m_pPicParamSet)
    {
        return 0;
    }

    Ipp32s seq_parameter_set_id = pSlice->m_pPicParamSet->seq_parameter_set_id;

    //if (NAL_UT_CODED_SLICE_EXTENSION == pSlice->GetSliceHeader()->nal_unit_type)
    //{
    //    if (pSlice->GetSliceHeader()->nal_ext.svc_extension_flag == 0)
    //    {
    //        pSlice->m_pSeqParamSetSvcEx = 0;
    //        pSlice->m_pSeqParamSet = pSlice->m_pSeqParamSetMvcEx = m_Headers.m_SeqParamsMvcExt.GetHeader(seq_parameter_set_id);

    //        if (NULL == pSlice->m_pSeqParamSet)
    //        {
    //            return 0;
    //        }

    //        m_Headers.m_SeqParamsMvcExt.SetCurrentID(pSlice->m_pSeqParamSetMvcEx->seq_parameter_set_id);
    //        m_Headers.m_PicParams.SetCurrentID(pSlice->m_pPicParamSet->pic_parameter_set_id);
    //    }
    //    else
    //    {
    //        pSlice->m_pSeqParamSetMvcEx = 0;
    //        pSlice->m_pSeqParamSet = pSlice->m_pSeqParamSetSvcEx = m_Headers.m_SeqParamsSvcExt.GetHeader(seq_parameter_set_id);

    //        if (NULL == pSlice->m_pSeqParamSet)
    //        {
    //            return 0;
    //        }

    //        m_Headers.m_SeqParamsSvcExt.SetCurrentID(pSlice->m_pSeqParamSetSvcEx->seq_parameter_set_id);
    //        m_Headers.m_PicParams.SetCurrentID(pSlice->m_pPicParamSet->pic_parameter_set_id);
    //    }
    //}
    //else
    {
        pSlice->m_pSeqParamSetSvcEx = m_Headers.m_SeqParamsSvcExt.GetCurrentHeader();
        pSlice->m_pSeqParamSetMvcEx = m_Headers.m_SeqParamsMvcExt.GetCurrentHeader();
        pSlice->m_pSeqParamSet = m_Headers.m_SeqParams.GetHeader(seq_parameter_set_id);

        if (!pSlice->m_pSeqParamSet)
        {
            ErrorStatus::isSPSError = 0;
            return 0;
        }

        m_Headers.m_SeqParams.SetCurrentID(pSlice->m_pPicParamSet->seq_parameter_set_id);
        m_Headers.m_PicParams.SetCurrentID(pSlice->m_pPicParamSet->pic_parameter_set_id);
    }

    if (pSlice->m_pSeqParamSet->errorFlags)
        ErrorStatus::isSPSError = 1;

    if (pSlice->m_pPicParamSet->errorFlags)
        ErrorStatus::isPPSError = 1;

    Status sts = InitializePictureParamSet(m_Headers.m_PicParams.GetHeader(pps_pid), pSlice->m_pSeqParamSet, NAL_UT_CODED_SLICE_EXTENSION == pSlice->GetSliceHeader()->nal_unit_type);
    if (sts != UMC_OK)
    {
        return 0;
    }

    pSlice->m_pSeqParamSetEx = m_Headers.m_SeqExParams.GetHeader(seq_parameter_set_id);
    pSlice->m_pCurrentFrame = 0;

    //memory_leak_preventing.ClearNotification();
    pSlice->m_dTime = pDecryptParams->GetTime();

    //if (!pSlice->Reset(&m_Headers.m_nalExtension))
    //bool H264Slice::Reset(H264NalExtension *pNalExt)
    {
        Ipp32s iMBInFrame;
        Ipp32s iFieldIndex;

        //m_BitStream.Reset((Ipp8u *) pSource, (Ipp32u) nSourceSize);

        // decode slice header
        {
            Status umcRes = UMC_OK;
            // Locals for additional slice data to be read into, the data
            // was read and saved from the first slice header of the picture,
            // is not supposed to change within the picture, so can be
            // discarded when read again here.
            Ipp32s iSQUANT;

            try
            {
                memset(&pSlice->m_SliceHeader, 0, sizeof(pSlice->m_SliceHeader));

                //umcRes = m_BitStream.GetNALUnitType(m_SliceHeader.nal_unit_type,
                //                                    m_SliceHeader.nal_ref_idc);
                //if (UMC_OK != umcRes)
                //    return 0;

                //if (pNalExt && (NAL_UT_CODED_SLICE_EXTENSION != m_SliceHeader.nal_unit_type))
                //{
                //    if (pNalExt->extension_present)
                //        m_SliceHeader.nal_ext = *pNalExt;
                //    pNalExt->extension_present = 0;
                //}

                //// adjust slice type for auxilary slice
                //if (NAL_UT_AUXILIARY == m_SliceHeader.nal_unit_type)
                //{
                //    if (!m_pCurrentFrame || !GetSeqParamEx())
                //        return 0;

                //    m_SliceHeader.nal_unit_type = m_pCurrentFrame->m_bIDRFlag ?
                //                                  NAL_UT_IDR_SLICE :
                //                                  NAL_UT_SLICE;
                //    m_SliceHeader.is_auxiliary = true;
                //}

                // decode first part of slice header
                umcRes = pDecryptParams->GetSliceHeaderPart1(&pSlice->m_SliceHeader);
                if (UMC_OK != umcRes)
                    return 0;


                // decode second part of slice header
                umcRes = pDecryptParams->GetSliceHeaderPart2(&pSlice->m_SliceHeader,
                                                         pSlice->m_pPicParamSet,
                                                         pSlice->m_pSeqParamSet);
                if (UMC_OK != umcRes)
                    return 0;

                // decode second part of slice header
                umcRes = pDecryptParams->GetSliceHeaderPart3(&pSlice->m_SliceHeader,
                                                         pSlice->m_PredWeight[0],
                                                         pSlice->m_PredWeight[1],
                                                         &pSlice->ReorderInfoL0,
                                                         &pSlice->ReorderInfoL1,
                                                         &pSlice->m_AdaptiveMarkingInfo,
                                                         &pSlice->m_BaseAdaptiveMarkingInfo,
                                                         pSlice->m_pPicParamSet,
                                                         pSlice->m_pSeqParamSet,
                                                         pSlice->m_pSeqParamSetSvcEx);
                if (UMC_OK != umcRes)
                    return 0;

                // decode 4 part of slice header
                umcRes = pDecryptParams->GetSliceHeaderPart4(&pSlice->m_SliceHeader, pSlice->m_pSeqParamSetSvcEx);
                if (UMC_OK != umcRes)
                    return 0;

                pSlice->m_iMBWidth = pSlice->m_pSeqParamSet->frame_width_in_mbs;
                pSlice->m_iMBHeight = pSlice->m_pSeqParamSet->frame_height_in_mbs;

                pSlice->m_WidevineStatusReportNumber = pDecryptParams->usStatusReportFeedbackNumber;

                // redundant slice, discard
                if (pSlice->m_SliceHeader.redundant_pic_cnt)
                    return 0;

                // Set next MB.
                if (pSlice->m_SliceHeader.first_mb_in_slice >= (Ipp32s) (pSlice->m_iMBWidth * pSlice->m_iMBHeight))
                {
                    return 0;
                }

                Ipp32s bit_depth_luma = pSlice->GetSeqParam()->bit_depth_luma;

                iSQUANT = pSlice->m_pPicParamSet->pic_init_qp +
                          pSlice->m_SliceHeader.slice_qp_delta;
                if (iSQUANT < QP_MIN - 6*(bit_depth_luma - 8)
                    || iSQUANT > QP_MAX)
                {
                    return 0;
                }

                //m_SliceHeader.hw_wa_redundant_elimination_bits[2] = (Ipp32u)m_BitStream.BitsDecoded();

                if (pSlice->m_pPicParamSet->entropy_coding_mode)
                    pSlice->m_BitStream.AlignPointerRight();
            }
            catch(const h264_exception & )
            {
                return 0;
            }
            catch(...)
            {
                return 0;
            }
        }

        pSlice->m_iMBWidth  = pSlice->GetSeqParam()->frame_width_in_mbs;
        pSlice->m_iMBHeight = pSlice->GetSeqParam()->frame_height_in_mbs;

        iMBInFrame = (pSlice->m_iMBWidth * pSlice->m_iMBHeight) / ((pSlice->m_SliceHeader.field_pic_flag) ? (2) : (1));
        iFieldIndex = (pSlice->m_SliceHeader.field_pic_flag && pSlice->m_SliceHeader.bottom_field_flag) ? (1) : (0);

        // set slice variables
        pSlice->m_iFirstMB = pSlice->m_SliceHeader.first_mb_in_slice;
        pSlice->m_iMaxMB = iMBInFrame;

        pSlice->m_iFirstMBFld = pSlice->m_SliceHeader.first_mb_in_slice + iMBInFrame * iFieldIndex;

        pSlice->m_iAvailableMB = iMBInFrame;

        if (pSlice->m_iFirstMB >= pSlice->m_iAvailableMB)
            return 0;

        // reset all internal variables
        /*pSlice->m_iCurMBToDec = pSlice->m_iFirstMB;
        pSlice->m_iCurMBToRec = pSlice->m_iFirstMB;
        pSlice->m_iCurMBToDeb = pSlice->m_iFirstMB;

        pSlice->m_bInProcess = false;
        pSlice->m_bDecVacant = 1;
        pSlice->m_bRecVacant = 1;
        pSlice->m_bDebVacant = 1;
        pSlice->m_bFirstDebThreadedCall = true;
        pSlice->m_bError = false;

        pSlice->m_MVsDistortion = 0;

        // reset through-decoding variables
        pSlice->m_nMBSkipCount = 0;
        pSlice->m_nQuantPrev = pSlice->m_pPicParamSet->pic_init_qp +
                               pSlice->m_SliceHeader.slice_qp_delta;
        pSlice->m_prev_dquant = 0;
        pSlice->m_field_index = iFieldIndex;

        if (pSlice->IsSliceGroups())
            pSlice->m_bNeedToCheckMBSliceEdges = true;
        else
            pSlice->m_bNeedToCheckMBSliceEdges = (0 == pSlice->m_SliceHeader.first_mb_in_slice) ? (false) : (true);*/

        // set conditional flags
        pSlice->m_bDecoded = false;
        pSlice->m_bPrevDeblocked = false;
        pSlice->m_bDeblocked = (pSlice->m_SliceHeader.disable_deblocking_filter_idc == DEBLOCK_FILTER_OFF);

        /*if (pSlice->m_bDeblocked)
        {
            pSlice->m_bDebVacant = 0;
            pSlice->m_iCurMBToDeb = pSlice->m_iMaxMB;
        }*/

        // frame is not associated yet
        pSlice->m_pCurrentFrame = NULL;

        pSlice->m_pSeqParamSet->IncrementReference();
        pSlice->m_pPicParamSet->IncrementReference();
        if (pSlice->m_pSeqParamSetEx)
            pSlice->m_pSeqParamSetEx->IncrementReference();
        if (pSlice->m_pSeqParamSetMvcEx)
            pSlice->m_pSeqParamSetMvcEx->IncrementReference();
        if (pSlice->m_pSeqParamSetSvcEx)
            pSlice->m_pSeqParamSetSvcEx->IncrementReference();
    }

    if (IsShouldSkipSlice(pSlice))
        return 0;

    if (!IsExtension())
    {
        memset(&pSlice->GetSliceHeader()->nal_ext, 0, sizeof(H264NalExtension));
    }

    if (spl)
    {
        if (m_WaitForIDR)
        {
            AllocateAndInitializeView(pSlice);
            ViewItem &view = GetView(pSlice->GetSliceHeader()->nal_ext.mvc.view_id);
            Ipp32u recoveryFrameNum = (spl->SEI_messages.recovery_point.recovery_frame_cnt + pSlice->GetSliceHeader()->frame_num) % (1 << pSlice->m_pSeqParamSet->log2_max_frame_num);
            view.GetDPBList(0)->SetRecoveryFrameCnt(recoveryFrameNum);
        }

        if ((pSlice->GetSliceHeader()->slice_type != INTRASLICE))
        {
            H264SEIPayLoad * splToRemove = m_Headers.m_SEIParams.GetHeader(SEI_RECOVERY_POINT_TYPE);
            m_Headers.m_SEIParams.RemoveHeader(splToRemove);
        }
    }

    m_WaitForIDR = false;
    memory_leak_preventing_slice.ClearNotification();

    //return pSlice;

    //nalUnit->SetDataSize(dataSize);

    if (!pSlice)
        return 0;

    //if (nalUnit->GetFlags() & MediaData::FLAG_VIDEO_DATA_NOT_FULL_FRAME)
    //{
    //    slice->m_pSource.Allocate(nalUnit->GetDataSize() + DEFAULT_NU_TAIL_SIZE);
    //    MFX_INTERNAL_CPY(slice->m_pSource.GetPointer(), nalUnit->GetDataPointer(), (Ipp32u)nalUnit->GetDataSize());
    //    memset(slice->m_pSource.GetPointer() + nalUnit->GetDataSize(), DEFAULT_NU_TAIL_VALUE, DEFAULT_NU_TAIL_SIZE);
    //    slice->m_pSource.SetDataSize(nalUnit->GetDataSize());
    //    slice->m_pSource.SetTime(nalUnit->GetTime());
    //}
    //else
    //{
    //    slice->m_pSource.SetData(nalUnit);
    //}

    //Ipp32u* pbs;
    //Ipp32u bitOffset;

    //slice->GetBitStream()->GetState(&pbs, &bitOffset);

    //size_t bytes = slice->GetBitStream()->BytesDecodedRoundOff();

    //slice->GetBitStream()->Reset(slice->m_pSource.GetPointer(), bitOffset,
    //    (Ipp32u)slice->m_pSource.GetDataSize());
    //slice->GetBitStream()->SetState((Ipp32u*)(slice->m_pSource.GetPointer() + bytes), bitOffset);

    return pSlice;
}

Status WidevineTaskSupplier::ParseWidevineSEI(DecryptParametersWrapper* pDecryptParams)
{
    //H264Bitstream bitStream;

    try
    {
        //H264MemoryPiece mem;
        //mem.SetData(nalUnit);

        //H264MemoryPiece swappedMem;
        //swappedMem.Allocate(nalUnit->GetDataSize() + DEFAULT_NU_TAIL_SIZE);

        //SwapperBase * swapper = m_pNALSplitter->GetSwapper();
        //swapper->SwapMemory(&swappedMem, &mem, DEFAULT_NU_HEADER_TAIL_VALUE);

        //bitStream.Reset((Ipp8u*)pMem->GetPointer(), (Ipp32u)pMem->GetDataSize());

        //NAL_Unit_Type nal_unit_type;
        //Ipp32u nal_ref_idc;

        //bitStream.GetNALUnitType(nal_unit_type, nal_ref_idc);

        {
            H264SEIPayLoad    m_SEIPayLoads;

            pDecryptParams->ParseSEIBufferingPeriod(m_Headers, &m_SEIPayLoads);
            if (m_SEIPayLoads.isValid)
            {
                H264SEIPayLoad* payload = m_Headers.m_SEIParams.AddHeader(&m_SEIPayLoads);
                m_accessUnit.m_payloads.AddPayload(payload);
            }
        }

        {
            H264SEIPayLoad    m_SEIPayLoads;

            pDecryptParams->ParseSEIPicTiming(m_Headers, &m_SEIPayLoads);
            if (m_SEIPayLoads.isValid)
            {
                H264SEIPayLoad* payload = m_Headers.m_SEIParams.AddHeader(&m_SEIPayLoads);
                m_accessUnit.m_payloads.AddPayload(payload);
            }
        }

        {
            H264SEIPayLoad    m_SEIPayLoads;

            pDecryptParams->ParseSEIRecoveryPoint(&m_SEIPayLoads);
            if (m_SEIPayLoads.isValid)
            {
                H264SEIPayLoad* payload = m_Headers.m_SEIParams.AddHeader(&m_SEIPayLoads);
                m_accessUnit.m_payloads.AddPayload(payload);
            }
        }

    } catch(...)
    {
        // nothing to do just catch it
    }

    return UMC_OK;
}

Status WidevineTaskSupplier::DecryptWidevineHeaders(MediaData *pSource, DecryptParametersWrapper* pDecryptParams)
{
    Status sts = m_pWidevineDecrypter->DecryptFrame(pSource, pDecryptParams);
    if (sts != UMC_OK)
    {
        return sts;
    }

    return UMC_OK;
}

Status WidevineTaskSupplier::AddSource(DecryptParametersWrapper* pDecryptParams)
{
    Status umcRes = UMC_OK;

    CompleteDecodedFrames(0);

    umcRes = AddOneFrame(pDecryptParams); // construct frame

    if (UMC_ERR_NOT_ENOUGH_BUFFER == umcRes)
    {
        ViewItem &view = GetView(m_currentView);
        H264DBPList *pDPB = view.GetDPBList(0);

        // in the following case(s) we can lay on the base view only,
        // because the buffers are managed synchronously.

        // frame is being processed. Wait for asynchronous end of operation.
        if (pDPB->IsDisposableExist())
        {
            return UMC_WRN_INFO_NOT_READY;
        }

        // frame is finished, but still referenced.
        // Wait for asynchronous complete.
        if (pDPB->IsAlmostDisposableExist())
        {
            return UMC_WRN_INFO_NOT_READY;
        }

        // some more hard reasons of frame lacking.
        if (!m_pTaskBroker->IsEnoughForStartDecoding(true))
        {
            if (CompleteDecodedFrames(0) == UMC_OK)
                return UMC_WRN_INFO_NOT_READY;

            if (GetFrameToDisplayInternal(true))
                return UMC_ERR_NOT_ENOUGH_BUFFER;

            PreventDPBFullness();
            return UMC_WRN_INFO_NOT_READY;
        }

        return UMC_WRN_INFO_NOT_READY;
    }

    return umcRes;
}

Status WidevineTaskSupplier::AddOneFrame(DecryptParametersWrapper* pDecryptParams)
{
#if defined(MFX_VA)
    if (!m_initializationParams.pVideoAccelerator->GetProtectedVA() ||
        !IS_PROTECTION_WIDEVINE(m_initializationParams.pVideoAccelerator->GetProtectedVA()->GetProtected()))
    {
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    }
#endif
    Status umsRes = UMC_OK;

    if (m_pLastSlice)
        return AddSlice(m_pLastSlice, true);

    umsRes = ParseWidevineSPSPPS(pDecryptParams);
    if (umsRes != UMC_OK)
        return umsRes;

    umsRes = ParseWidevineSEI(pDecryptParams);
    if (umsRes != UMC_OK)
        return umsRes;

    H264Slice * pSlice = ParseWidevineSliceHeader(pDecryptParams);
    if (pSlice)
    {
        umsRes = AddSlice(pSlice, true);
        if (umsRes == UMC_ERR_NOT_ENOUGH_BUFFER || umsRes == UMC_OK)
            return umsRes;
    }

    return AddSlice(0, true);
}

Status WidevineTaskSupplier::AddOneFrame(MediaData * pSource)
{
#if defined(MFX_VA)
    if (!m_initializationParams.pVideoAccelerator->GetProtectedVA() ||
        !IS_PROTECTION_WIDEVINE(m_initializationParams.pVideoAccelerator->GetProtectedVA()->GetProtected()))
    {
        return TaskSupplier::AddOneFrame(pSource);
    }
#endif
    Status umsRes = UMC_OK;

    if (m_pLastSlice)
    {
        Status sts = AddSlice(m_pLastSlice, !pSource);
        if (sts == UMC_ERR_NOT_ENOUGH_BUFFER || sts == UMC_OK)
            return sts;
    }

    DecryptParametersWrapper decryptParams;
    void* bsDataPointer = pSource ? pSource->GetDataPointer() : 0;
    Ipp32s size = pSource ? (Ipp32s)pSource->GetDataSize() : 0;

    {
        Status sts = DecryptWidevineHeaders(pSource, &decryptParams);
        if (sts != UMC_OK)
            return sts;
    }

    bool decrypted = pSource ? (bsDataPointer != pSource->GetDataPointer()) : false;

    if (!decrypted && pSource)
    {
        Ipp32u flags = pSource->GetFlags();

        if (!(flags & MediaData::FLAG_VIDEO_DATA_NOT_FULL_FRAME))
        {
            VM_ASSERT(!m_pLastSlice);
            return AddSlice(0, true);
        }

        return UMC_ERR_SYNC;
    }

    if (!decrypted)
    {
        if (!pSource)
            return AddSlice(0, true);

        return UMC_ERR_NOT_ENOUGH_DATA;
    }

    {

        umsRes = ParseWidevineSPSPPS(&decryptParams);
        if (umsRes != UMC_OK)
        {
            if (umsRes == UMC_NTF_NEW_RESOLUTION)
                pSource->MoveDataPointer(-size);

            return umsRes;
        }

        umsRes = ParseWidevineSEI(&decryptParams);
        if (umsRes != UMC_OK)
        {
            return umsRes;
        }

        H264Slice * pSlice = ParseWidevineSliceHeader(&decryptParams);
        if (pSlice)
        {
            umsRes = AddSlice(pSlice, !pSource);
            if (umsRes == UMC_ERR_NOT_ENOUGH_BUFFER || umsRes == UMC_OK)
            {
                return umsRes;
            }
        }
    }

    if (!pSource)
    {
        return AddSlice(0, true);
    }
    else
    {
        Ipp32u flags = pSource->GetFlags();

        if (!(flags & MediaData::FLAG_VIDEO_DATA_NOT_FULL_FRAME))
        {
            return AddSlice(0, true);
        }
    }

    return UMC_ERR_NOT_ENOUGH_DATA;
}

Status WidevineTaskSupplier::CompleteFrame(H264DecoderFrame * pFrame, Ipp32s field)
{
    Status sts = VATaskSupplier::CompleteFrame(pFrame, field);

#if defined(MFX_VA)
    if (m_initializationParams.pVideoAccelerator->GetProtectedVA() &&
        (IS_PROTECTION_WIDEVINE(m_initializationParams.pVideoAccelerator->GetProtectedVA()->GetProtected())))
    {
        m_pWidevineDecrypter->ReleaseForNewBitstream();
    }
#endif

    return sts;
}


} // namespace UMC

#endif // #if defined (MFX_VA) && !defined (MFX_PROTECTED_FEATURE_DISABLE)

#endif // UMC_ENABLE_H264_VIDEO_DECODER
