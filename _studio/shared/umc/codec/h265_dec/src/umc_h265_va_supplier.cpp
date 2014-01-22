/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2013-2014 Intel Corporation. All Rights Reserved.
//
//
*/

#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef UMC_RESTRICTED_CODE_VA

#include "umc_h265_bitstream.h"
#include "umc_h265_va_supplier.h"
#include "umc_h265_frame_list.h"
#include "umc_automatic_mutex.h"
#include "umc_h265_nal_spl.h"

#include "umc_h265_dec_defs_dec.h"
#include "vm_sys_info.h"
#include "umc_h265_segment_decoder_mt.h"

#include "umc_h265_task_broker.h"
#include "umc_video_processing.h"
#include "umc_structures.h"

#include "umc_h265_dec_debug.h"

namespace UMC_HEVC_DECODER
{

VATaskSupplier::VATaskSupplier()
    : m_bufferedFrameNumber(0)
{
}

UMC::Status VATaskSupplier::Init(UMC::BaseCodecParams *pInit)
{
    SetVideoHardwareAccelerator(((UMC::VideoDecoderParams*)pInit)->pVideoAccelerator);
    m_pMemoryAllocator = pInit->lpMemoryAllocator;

    UMC::Status umsRes = TaskSupplier_H265::Init(pInit);
    if (umsRes != UMC::UMC_OK)
        return umsRes;

    Ipp32u i;
    for (i = 0; i < m_iThreadNum; i += 1)
    {
        delete m_pSegmentDecoder[i];
        m_pSegmentDecoder[i] = 0;
    }

    m_iThreadNum = 1;
    delete m_pTaskBroker;
    m_pTaskBroker = new TaskBrokerSingleThreadDXVA(this);
    m_pTaskBroker->Init(m_iThreadNum, true);

    DXVASupport<VATaskSupplier>::Init();

    for (i = 0; i < m_iThreadNum; i += 1)
    {
        m_pSegmentDecoder[i] = new H265_DXVA_SegmentDecoder(this);
        if (0 == m_pSegmentDecoder[i])
            return UMC::UMC_ERR_ALLOC;
    }

    for (i = 0; i < m_iThreadNum; i += 1)
    {
        if (UMC::UMC_OK != m_pSegmentDecoder[i]->Init(i))
            return UMC::UMC_ERR_INIT;
    }

    if (m_va && !m_va->IsSimulate())
    {
        ((TaskBrokerSingleThreadDXVA*)m_pTaskBroker)->DXVAStatusReportingMode(false);//m_va->m_HWPlatform != UMC::VA_HW_LAKE && m_va->m_HWPlatform != UMC::VA_HW_HSW);
        m_DPBSizeEx = 1;
    }

    m_sei_messages = new SEI_Storer_H265();
    m_sei_messages->Init();

    return UMC::UMC_OK;
}

void VATaskSupplier::SetBufferedFramesNumber(Ipp32u buffered)
{
    m_DPBSizeEx = 1 + buffered;
    m_bufferedFrameNumber = buffered;

    if (buffered)
        m_isUseDelayDPBValues = false;
}

H265DecoderFrame * VATaskSupplier::GetFrameToDisplayInternal(bool force)
{
    //ViewItem_H265 &view = *GetView();
    //view.maxDecFrameBuffering += m_bufferedFrameNumber;

    H265DecoderFrame * frame = MFXTaskSupplier_H265::GetFrameToDisplayInternal(force);

    //view.maxDecFrameBuffering -= m_bufferedFrameNumber;

    return frame;
}

void VATaskSupplier::Reset()
{
    if (m_pTaskBroker)
        m_pTaskBroker->Reset();

    MFXTaskSupplier_H265::Reset();
}

inline bool isFreeFrame(H265DecoderFrame * pTmp)
{
    return (!pTmp->m_isShortTermRef &&
        !pTmp->m_isLongTermRef
        //((pTmp->m_wasOutputted != 0) || (pTmp->m_Flags.isDisplayable == 0)) &&
        //!pTmp->m_BusyState
        );
}

void VATaskSupplier::CompleteFrame(H265DecoderFrame * pFrame)
{
    if (!pFrame)
        return;

    H265DecoderFrameInfo * slicesInfo = pFrame->GetAU();

    if (slicesInfo->GetStatus() > H265DecoderFrameInfo::STATUS_NOT_FILLED)
        return;

    DEBUG_PRINT((VM_STRING("Complete frame POC - (%d) type - %d, count - %d, m_uid - %d, IDR - %d\n"), pFrame->m_PicOrderCnt, pFrame->m_FrameType, slicesInfo->GetSliceCount(), pFrame->m_UID, slicesInfo->GetAnySlice()->GetSliceHeader()->IdrPicFlag));

    // skipping algorithm
    {
        if (IsShouldSkipFrame(pFrame) || IsSkipForCRAorBLA(slicesInfo->GetAnySlice()))
        {
            slicesInfo->SetStatus(H265DecoderFrameInfo::STATUS_COMPLETED);

            pFrame->SetisShortTermRef(false);
            pFrame->SetisLongTermRef(false);
            pFrame->SetSkipped(true);
            pFrame->OnDecodingCompleted();
            return;
        }
        else
        {
            if (IsShouldSkipDeblocking(pFrame))
            {
                pFrame->GetAU()->SkipDeblocking();
                pFrame->GetAU()->SkipSAO();
            }
        }
    }

        Ipp32s count = slicesInfo->GetSliceCount();

        H265Slice * pFirstSlice = 0;
        for (Ipp32s j = 0; j < count; j ++)
        {
            H265Slice * pSlice = slicesInfo->GetSlice(j);
            if (!pFirstSlice || pSlice->m_iFirstMB < pFirstSlice->m_iFirstMB)
            {
                pFirstSlice = pSlice;
            }
        }

        if (pFirstSlice->m_iFirstMB)
        {
            m_pSegmentDecoder[0]->RestoreErrorRect(0, pFirstSlice->m_iFirstMB, pFirstSlice);
        }

        for (Ipp32s i = 0; i < count; i ++)
        {
            H265Slice * pCurSlice = slicesInfo->GetSlice(i);

#define MAX_MB_NUMBER 0x7fffffff

            Ipp32s minFirst = MAX_MB_NUMBER;
            for (Ipp32s j = 0; j < count; j ++)
            {
                H265Slice * pSlice = slicesInfo->GetSlice(j);
                if (pSlice->m_iFirstMB > pCurSlice->m_iFirstMB && minFirst > pSlice->m_iFirstMB)
                {
                    minFirst = pSlice->m_iFirstMB;
                }
            }

            if (minFirst != MAX_MB_NUMBER)
            {
                pCurSlice->m_iMaxMB = minFirst;
            }
        }

        StartDecodingFrame(pFrame);
        EndDecodingFrame();

    slicesInfo->SetStatus(H265DecoderFrameInfo::STATUS_FILLED);
}

void VATaskSupplier::InitFrameCounter(H265DecoderFrame * pFrame, const H265Slice *pSlice)
{
    TaskSupplier_H265::InitFrameCounter(pFrame, pSlice);
}

UMC::Status VATaskSupplier::AllocateFrameData(H265DecoderFrame * pFrame, IppiSize dimensions, Ipp32s bit_depth, const H265SeqParamSet* , const H265PicParamSet *)
{
    UMC::ColorFormat chroma_format_idc = pFrame->GetColorFormat();
    UMC::VideoDataInfo info;
    info.Init(dimensions.width, dimensions.height, chroma_format_idc, bit_depth);    

    UMC::FrameMemID frmMID;
    UMC::Status sts = m_pFrameAllocator->Alloc(&frmMID, &info, 0);

    if (sts != UMC::UMC_OK)
    {
        throw h265_exception(UMC::UMC_ERR_ALLOC);
    }

    UMC::FrameData frmData;
    frmData.Init(&info, frmMID, m_pFrameAllocator);
    pFrame->allocate(&frmData, &info);

    pFrame->m_index = frmMID;

    return UMC::UMC_OK;
}

H265Slice * VATaskSupplier::DecodeSliceHeader(UMC::MediaDataEx *nalUnit)
{
    size_t dataSize = nalUnit->GetDataSize();
    nalUnit->SetDataSize(IPP_MIN(1024, dataSize));

    H265Slice * slice = TaskSupplier_H265::DecodeSliceHeader(nalUnit);

    nalUnit->SetDataSize(dataSize);

    if (!slice)
        return 0;

    MemoryPiece * pMemCopy = m_Heap.Allocate(nalUnit->GetDataSize() + DEFAULT_NU_TAIL_SIZE);
    notifier1<Heap, MemoryPiece*> memory_leak_preventing1(&m_Heap, &Heap::Free, pMemCopy);

    MFX_INTERNAL_CPY(pMemCopy->GetPointer(), nalUnit->GetDataPointer(), nalUnit->GetDataSize());
    memset(pMemCopy->GetPointer() + nalUnit->GetDataSize(), DEFAULT_NU_TAIL_VALUE, DEFAULT_NU_TAIL_SIZE);
    pMemCopy->SetDataSize(nalUnit->GetDataSize());
    pMemCopy->SetTime(nalUnit->GetTime());

    Ipp32u* pbs;
    Ipp32u bitOffset;

    slice->GetBitStream()->GetState(&pbs, &bitOffset);

    size_t bytes = slice->GetBitStream()->BytesDecodedRoundOff();

    m_Heap.Free(slice->m_pSource);

    slice->m_pSource = pMemCopy;
    slice->GetBitStream()->Reset(slice->m_pSource->GetPointer(), bitOffset,
        (Ipp32u)slice->m_pSource->GetDataSize());
    slice->GetBitStream()->SetState((Ipp32u*)(slice->m_pSource->GetPointer() + bytes), bitOffset);

    memory_leak_preventing1.ClearNotification();

    return slice;
}


} // namespace UMC_HEVC_DECODER

#endif // UMC_RESTRICTED_CODE_VA
#endif // UMC_ENABLE_H265_VIDEO_DECODER
