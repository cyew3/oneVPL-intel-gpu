/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2003-2016 Intel Corporation. All Rights Reserved.
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

Status VATaskSupplier::Init(VideoDecoderParams *pInit)
{
    SetVideoHardwareAccelerator(pInit->pVideoAccelerator);
    m_pMemoryAllocator = pInit->lpMemoryAllocator;

    pInit->numThreads = 1;

    Status umsRes = TaskSupplier::Init(pInit);
    if (umsRes != UMC_OK)
        return umsRes;

    m_iThreadNum = 1;

    DXVASupport<VATaskSupplier>::Init();
    if (m_va)
    {
        static_cast<TaskBrokerSingleThreadDXVA*>(m_pTaskBroker)->DXVAStatusReportingMode(m_va->IsUseStatusReport());
    }

    H264VideoDecoderParams *initH264 = DynamicCast<H264VideoDecoderParams> (pInit);
    m_DPBSizeEx = m_iThreadNum + (initH264 ? initH264->m_bufferedFrames : 0);

    m_sei_messages = new SEI_Storer();
    m_sei_messages->Init();

    return UMC_OK;
}

void VATaskSupplier::CreateTaskBroker()
{
    m_pTaskBroker = new TaskBrokerSingleThreadDXVA(this);

    for (Ipp32u i = 0; i < m_iThreadNum; i += 1)
    {
        m_pSegmentDecoder[i] = new H264_DXVA_SegmentDecoder(this);
    }
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

    MFXTaskSupplier::Reset();

    DXVASupport<VATaskSupplier>::Reset();
}

void VATaskSupplier::AfterErrorRestore()
{
    MFXTaskSupplier::AfterErrorRestore();
}

Status VATaskSupplier::DecodeHeaders(MediaDataEx *nalUnit)
{
    Status sts = MFXTaskSupplier::DecodeHeaders(nalUnit);

    if (sts != UMC_OK && sts != UMC_WRN_REPOSITION_INPROGRESS)
        return sts;

    Ipp32u nal_unit_type = nalUnit->GetExData()->values[0];
    if (nal_unit_type == NAL_UT_SPS && m_firstVideoParams.mfx.FrameInfo.PicStruct == MFX_PICSTRUCT_PROGRESSIVE &&
        isMVCProfile(m_firstVideoParams.mfx.CodecProfile) && m_va)
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
    if (view.GetDPBList(0)->countAllFrames() >= view.maxDecFrameBuffering + m_DPBSizeEx)
        pFrame = view.GetDPBList(0)->GetLastDisposable();

    VM_ASSERT(!pFrame || pFrame->GetRefCounter() == 0);

    // Did we find one?
    if (NULL == pFrame)
    {
        if (view.GetDPBList(0)->countAllFrames() >= view.maxDecFrameBuffering + m_DPBSizeEx)
        {
            DEBUG_PRINT((VM_STRING("Fail to find disposable frame\n")));
            return 0;
        }

        // Didn't find one. Let's try to insert a new one
        pFrame = new H264DecoderFrameExtension(m_pMemoryAllocator, &m_ObjHeap);
        if (NULL == pFrame)
            return 0;

        view.GetDPBList(0)->append(pFrame);
    }

    DEBUG_PRINT((VM_STRING("Cleanup frame POC - %d, %d, field - %d, uid - %d, frame_num - %d, viewId - %d\n"),
        pFrame->m_PicOrderCnt[0], pFrame->m_PicOrderCnt[1],
        pFrame->GetNumberByParity(pSlice->GetSliceHeader()->bottom_field_flag),
        pFrame->m_UID, pFrame->m_FrameNum, pFrame->m_viewId)
    );

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

    DEBUG_PRINT((VM_STRING("Complete frame POC - (%d,%d) type - %d, picStruct - %d, field - %d, count - %d, m_uid - %d, IDR - %d, viewId - %d\n"),
        pFrame->m_PicOrderCnt[0], pFrame->m_PicOrderCnt[1],
        pFrame->m_FrameType,
        pFrame->m_displayPictureStruct,
        field,
        pFrame->GetAU(field)->GetSliceCount(),
        pFrame->m_UID, pFrame->m_bIDRFlag, pFrame->m_viewId
    ));

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


            pFrame->SetisShortTermRef(false, 0);
            pFrame->SetisShortTermRef(false, 1);
            pFrame->SetisLongTermRef(false, 0);
            pFrame->SetisLongTermRef(false, 1);
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
