/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2003-2014 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#ifndef UMC_RESTRICTED_CODE_VA

#include "umc_h264_va_supplier.h"
#include "umc_h264_frame_list.h"
#include "umc_h264_nal_spl.h"
#include "umc_h264_bitstream.h"

#include "umc_h264_dec_defs_dec.h"
#include "umc_h264_segment_decoder_mt.h"

#include "umc_h264_task_broker.h"
#include "umc_structures.h"

#include "umc_h264_dec_debug.h"

namespace UMC
{

VATaskSupplier::VATaskSupplier()
    : m_bufferedFrameNumber(0)
{
}

Status VATaskSupplier::Init(BaseCodecParams *pInit)
{
    SetVideoHardwareAccelerator(static_cast<UMC::VideoDecoderParams*>(pInit)->pVideoAccelerator);
    m_pMemoryAllocator = pInit->lpMemoryAllocator;

    Status umsRes = TaskSupplier::Init(pInit);
    if (umsRes != UMC_OK)
        return umsRes;

    for (Ipp32u i = 0; i < m_iThreadNum; i += 1)
    {
        delete m_pSegmentDecoder[i];
        m_pSegmentDecoder[i] = 0;
    }

    m_iThreadNum = 1;
    delete m_pTaskBroker;
    m_pTaskBroker = new TaskBrokerSingleThreadDXVA(this);
    m_pTaskBroker->Init(m_iThreadNum, true);

    DXVASupport<VATaskSupplier>::Init();

    for (Ipp32u i = 0; i < m_iThreadNum; i += 1)
    {
        m_pSegmentDecoder[i] = new H264_DXVA_SegmentDecoder(this);
        if (0 == m_pSegmentDecoder[i])
            return UMC_ERR_ALLOC;
    }

    for (Ipp32u i = 0; i < m_iThreadNum; i += 1)
    {
        if (UMC_OK != m_pSegmentDecoder[i]->Init(i))
            return UMC_ERR_INIT;
    }

    if (m_va)
    {
        static_cast<TaskBrokerSingleThreadDXVA*>(m_pTaskBroker)->DXVAStatusReportingMode(m_va->IsUseStatusReport());
    }

    H264VideoDecoderParams *initH264 = DynamicCast<H264VideoDecoderParams> (pInit);
    m_DPBSizeEx = m_iThreadNum + (initH264 ? initH264->m_bufferedFrames : 0);

    m_sei_messages = new SEI_Storer();
    m_sei_messages->Init();

    m_pLastDecodedFrame = 0;

    return UMC_OK;
}

void VATaskSupplier::SetBufferedFramesNumber(Ipp32u buffered)
{
    m_DPBSizeEx += buffered;
    m_bufferedFrameNumber = buffered;

    if (buffered)
        DPBOutput::Reset(true);
}

H264DecoderFrame * VATaskSupplier::GetFrameToDisplayInternal(bool force)
{
    ViewItem &view = GetViewByNumber(BASE_VIEW);
    view.maxDecFrameBuffering += m_bufferedFrameNumber;

    H264DecoderFrame * frame = MFXTaskSupplier::GetFrameToDisplayInternal(force);

    view.maxDecFrameBuffering -= m_bufferedFrameNumber;
    return frame;
}

void VATaskSupplier::Reset()
{
    if (m_pTaskBroker)
        m_pTaskBroker->Reset();

    m_pLastDecodedFrame = 0;

    MFXTaskSupplier::Reset();

    DXVASupport<VATaskSupplier>::Reset();
}

void VATaskSupplier::AfterErrorRestore()
{
    m_pLastDecodedFrame = 0;
    MFXTaskSupplier::AfterErrorRestore();
}

bool VATaskSupplier::GetFrameToDisplay(MediaData *dst, bool force)
{
    // Perform output color conversion and video effects, if we didn't
    // already write our output to the application's buffer.
    VideoData *pVData = DynamicCast<VideoData> (dst);
    if (!pVData)
        return false;

    H264DecoderFrame * pPreviousFrame = m_pLastDisplayed;

    if (m_va)
    {
        if (pPreviousFrame && !pPreviousFrame->IsSkipped())
            pPreviousFrame->DecrementReference();
    }

    m_pLastDisplayed = 0;

    H264DecoderFrame * pFrame = 0;
    do
    {
        pFrame = GetFrameToDisplayInternal(force);
        if (!pFrame || !pFrame->IsDecoded())
        {
            return false;
        }

        PostProcessDisplayFrame(dst, pFrame);

        if (pFrame->IsSkipped())
        {
            pFrame->setWasOutputted();
            pFrame->setWasDisplayed();
        }
    } while (pFrame->IsSkipped());

    m_pLastDisplayed = pFrame;
    pVData->SetInvalid(pFrame->GetError());

    VideoData data;
    InitColorConverter(pFrame, &data, 0);

    if (m_va)
    {
        m_va->DisplayFrame(pFrame->m_index, static_cast<VideoData*>(dst));
        pFrame->IncrementReference();
    }
    else
    {
        if (m_pPostProcessing->GetFrame(&data, pVData) != UMC_OK)
        {
            pFrame->setWasOutputted();
            pFrame->setWasDisplayed();
            return false;
        }
    }

    pVData->SetDataSize(pVData->GetMappingSize());
    pFrame->setWasOutputted();
    pFrame->setWasDisplayed();

    return true;
}

Status VATaskSupplier::DecodeHeaders(MediaDataEx *nalUnit)
{
    Status sts = MFXTaskSupplier::DecodeHeaders(nalUnit);

    if (sts != UMC_OK && sts != UMC_WRN_REPOSITION_INPROGRESS)
        return sts;

    Ipp32u nal_unit_type = nalUnit->GetExData()->values[0];
    if (nal_unit_type == NAL_UT_SPS && m_firstVideoParams.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE &&
        isMVCProfile(m_firstVideoParams.mfx.CodecProfile) && m_va && (m_va->m_HWPlatform >= VA_HW_HSW))
    {
        H264SeqParamSet * currSPS = m_Headers.m_SeqParams.GetCurrentHeader();
        if (currSPS && !currSPS->frame_mbs_only_flag)
        {
            return UMC_NTF_NEW_RESOLUTION;
        }
    }

    return sts;
}

H264DecoderFrame *VATaskSupplier::GetFreeFrame(const H264Slice * pSlice)
{
    AutomaticUMCMutex guard(m_mGuard);
    ViewItem &view = GetView(pSlice->GetSliceHeader()->nal_ext.mvc.view_id);

    H264DecoderFrame *pFrame = 0;

    // Traverse list for next disposable frame
    if (view.GetDPBList(0)->countAllFrames() >= view.dpbSize + m_DPBSizeEx)
        pFrame = view.GetDPBList(0)->GetLastDisposable();

    VM_ASSERT(!pFrame || pFrame->GetRefCounter() == 0);

    // Did we find one?
    if (NULL == pFrame)
    {
        if (view.GetDPBList(0)->countAllFrames() >= view.dpbSize + m_DPBSizeEx)
        {
            return 0;
        }

        // Didn't find one. Let's try to insert a new one
        pFrame = new H264DecoderFrameExtension(m_pMemoryAllocator, &m_ObjHeap);
        if (NULL == pFrame)
            return 0;

        view.GetDPBList(0)->append(pFrame);
    }

    DecReferencePictureMarking::Remove(pFrame);
    pFrame->Reset();

    // Set current as not displayable (yet) and not outputted. Will be
    // updated to displayable after successful decode.
    pFrame->unsetWasOutputted();
    pFrame->unSetisDisplayable();
    pFrame->SetSkipped(false);
    pFrame->SetFrameExistFlag(true);
    pFrame->IncrementReference();

    if (GetAuxiliaryFrame(pFrame))
    {
        GetAuxiliaryFrame(pFrame)->Reset();
    }

    m_UIDFrameCounter++;
    pFrame->m_UID = m_UIDFrameCounter;

    m_pLastDecodedFrame = pFrame;

    if (m_va && m_va->m_HWPlatform == VA_HW_LAKE)
    {
        view.GetDPBList(0)->swapFrames(view.GetDPBList(0)->tail(), m_pLastDecodedFrame);
    }

    return pFrame;
}

inline bool isFreeFrame(H264DecoderFrame * pTmp)
{
    return (!pTmp->m_isShortTermRef[0] &&
        !pTmp->m_isShortTermRef[1] &&
        !pTmp->m_isLongTermRef[0] &&
        !pTmp->m_isLongTermRef[1] //&&
        //((pTmp->m_wasOutputted != 0) || (pTmp->m_Flags.isDisplayable == 0)) &&
        //!pTmp->m_BusyState
        );
}

Status VATaskSupplier::CompleteFrame(H264DecoderFrame * pFrame, Ipp32s field)
{
    if (!pFrame)
        return UMC_OK;

    H264DecoderFrameInfo * slicesInfo = pFrame->GetAU(field);

    if (slicesInfo->GetStatus() > H264DecoderFrameInfo::STATUS_NOT_FILLED)
        return UMC_OK;

    Status umsRes = InitializeLayers(pFrame, field);
    if (umsRes != UMC_OK)
        return umsRes;

    DEBUG_PRINT((VM_STRING("Complete frame POC - (%d,%d) type - %d, field - %d, count - %d, m_uid - %d, IDR - %d, viewId - %d\n"), pFrame->m_PicOrderCnt[0], pFrame->m_PicOrderCnt[1], pFrame->m_FrameType, field, pFrame->GetAU(field)->GetSliceCount(), pFrame->m_UID, pFrame->m_bIDRFlag, pFrame->m_viewId));

    // skipping algorithm
    {
        if (((slicesInfo->IsField() && field) || !slicesInfo->IsField()) &&
            IsShouldSkipFrame(pFrame, field))
        {
            if (slicesInfo->IsField())
            {
                pFrame->GetAU(0)->SetStatus(H264DecoderFrameInfo::STATUS_COMPLETED);
                pFrame->GetAU(1)->SetStatus(H264DecoderFrameInfo::STATUS_COMPLETED);
            }
            else
            {
                pFrame->GetAU(0)->SetStatus(H264DecoderFrameInfo::STATUS_COMPLETED);
            }

            for (Ipp32s i = 0; i <= pFrame->m_maxDId; i++)
            {
                if (!pFrame->m_pLayerFrames[i])
                    continue;

                pFrame->m_pLayerFrames[i]->SetisShortTermRef(false, 0);
                pFrame->m_pLayerFrames[i]->SetisShortTermRef(false, 1);
                pFrame->m_pLayerFrames[i]->SetisLongTermRef(false, 0);
                pFrame->m_pLayerFrames[i]->SetisLongTermRef(false, 1);
            }

            pFrame->SetSkipped(true);
            pFrame->OnDecodingCompleted();
            return UMC_OK;
        }
        else
        {
            if (IsShouldSkipDeblocking(pFrame, field))
            {
                pFrame->GetAU(field)->SkipDeblocking();
            }
        }
    }

    if (slicesInfo->GetAnySlice()->IsSliceGroups())
    {
        throw h264_exception(UMC_ERR_UNSUPPORTED);
    }

    DecodePicture(pFrame, field);

    if (!field)
        m_pLastDecodedFrame = pFrame;

    bool is_auxiliary_exist = slicesInfo->GetAnySlice()->GetSeqParamEx() &&
        (slicesInfo->GetAnySlice()->GetSeqParamEx()->aux_format_idc != 0);

    if (is_auxiliary_exist)
    {
        if (!pFrame->IsAuxiliaryFrame())
            DBPUpdate(pFrame, field);
    }
    else
        DBPUpdate(pFrame, field);

    pFrame->MoveToNotifiersChain(m_DefaultNotifyChain);

    slicesInfo->SetStatus(H264DecoderFrameInfo::STATUS_FILLED);

    if (m_va && m_va->m_HWPlatform == VA_HW_LAKE)
    {
        AutomaticUMCMutex guard(m_mGuard);
        ViewItem &view = GetView(pFrame->m_viewId);

        Ipp32u position = 0;
        for (H264DecoderFrame * pFrm = view.GetDPBList(0)->head(); pFrm; pFrm = pFrm->future(), position++)
        {
            if (pFrm == pFrame)
            {
                break;
            }
        }

        if (!field && (view.GetDPBList(0)->countAllFrames() == m_DPBSizeEx + view.dpbSize) &&
            position >= view.dpbSize)
        {
            VM_ASSERT(m_pLastDecodedFrame == pFrame);
            for (H264DecoderFrame * pFrm = view.GetDPBList(0)->head(); pFrm; pFrm = pFrm->future())
            {
                if (isFreeFrame(pFrm))
                {
                    view.GetDPBList(0)->swapFrames(pFrm, pFrame);
                    m_pLastDecodedFrame = 0;
                    break;
                }
            }

            VM_ASSERT(!m_pLastDecodedFrame || (!m_pLastDecodedFrame->isShortTermRef() && !m_pLastDecodedFrame->isLongTermRef()));
        }
    }

    return UMC_OK;
}

void VATaskSupplier::InitFrameCounter(H264DecoderFrame * pFrame, const H264Slice *pSlice)
{
    TaskSupplier::InitFrameCounter(pFrame, pSlice);
}

Status VATaskSupplier::AllocateFrameData(H264DecoderFrame * pFrame, IppiSize dimensions, Ipp32s bit_depth, ColorFormat color_format)
{
    VideoDataInfo info;
    info.Init(dimensions.width, dimensions.height, color_format, bit_depth);

    FrameMemID frmMID;
    Status sts = m_pFrameAllocator->Alloc(&frmMID, &info, 0);

    if (sts != UMC_OK)
    {
        throw h264_exception(UMC_ERR_ALLOC);
    }

    FrameData frmData;
    frmData.Init(&info, frmMID, m_pFrameAllocator);
    pFrame->allocate(&frmData, &info);

    pFrame->m_index = frmMID;

    return UMC_OK;
}

H264Slice * VATaskSupplier::DecodeSliceHeader(MediaDataEx *nalUnit)
{
    size_t dataSize = nalUnit->GetDataSize();
    nalUnit->SetDataSize(IPP_MIN(1024, dataSize));

    H264Slice * slice = TaskSupplier::DecodeSliceHeader(nalUnit);

    nalUnit->SetDataSize(dataSize);

    if (!slice)
        return 0;

    if (nalUnit->GetFlags() & MediaData::FLAG_VIDEO_DATA_NOT_FULL_FRAME)
    {
        slice->m_pSource.Allocate(nalUnit->GetDataSize() + DEFAULT_NU_TAIL_SIZE);
        MFX_INTERNAL_CPY(slice->m_pSource.GetPointer(), nalUnit->GetDataPointer(), (Ipp32u)nalUnit->GetDataSize());
        memset(slice->m_pSource.GetPointer() + nalUnit->GetDataSize(), DEFAULT_NU_TAIL_VALUE, DEFAULT_NU_TAIL_SIZE);
        slice->m_pSource.SetDataSize(nalUnit->GetDataSize());
        slice->m_pSource.SetTime(nalUnit->GetTime());
    }
    else
    {
        slice->m_pSource.SetData(nalUnit);
    }
    
    Ipp32u* pbs;
    Ipp32u bitOffset;

    slice->GetBitStream()->GetState(&pbs, &bitOffset);

    size_t bytes = slice->GetBitStream()->BytesDecodedRoundOff();

    slice->GetBitStream()->Reset(slice->m_pSource.GetPointer(), bitOffset,
        (Ipp32u)slice->m_pSource.GetDataSize());
    slice->GetBitStream()->SetState((Ipp32u*)(slice->m_pSource.GetPointer() + bytes), bitOffset);

    return slice;
}


} // namespace UMC

#endif // UMC_RESTRICTED_CODE_VA
#endif // UMC_ENABLE_H264_VIDEO_DECODER
