//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2004-2017 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"

#if defined (UMC_ENABLE_VC1_VIDEO_DECODER)

#include "umc_vc1_video_decoder_hw.h"
#include "umc_video_data.h"
#include "umc_media_data_ex.h"
#include "umc_vc1_dec_debug.h"
#include "umc_vc1_dec_seq.h"
#include "vm_sys_info.h"
#include "umc_vc1_dec_task_store.h"

#include "umc_memory_allocator.h"
#include "umc_vc1_common.h"
#include "umc_vc1_common_defs.h"
#include "umc_vc1_dec_exception.h"

#include "umc_va_base.h"
#include "umc_vc1_dec_frame_descr_va.h"

using namespace UMC;
using namespace UMC::VC1Common;
using namespace UMC::VC1Exceptions;

VC1VideoDecoderHW::VC1VideoDecoderHW():
                                   m_stCodes_VA(NULL)
{
}

VC1VideoDecoderHW::~VC1VideoDecoderHW()
{
    Close();
}


Status VC1VideoDecoderHW::Init(BaseCodecParams *pInit)
{
    VideoDecoderParams *init = DynamicCast<VideoDecoderParams, BaseCodecParams>(pInit);
    if (!init)
        return UMC_ERR_INIT;

    if (init->pVideoAccelerator)
    {
        if ((init->pVideoAccelerator->m_Profile & VA_CODEC) == VA_VC1)
            m_va = init->pVideoAccelerator;
        else
            return UMC_ERR_UNSUPPORTED;
    }

    Status umcRes = UMC_OK;
    umcRes = VC1VideoDecoder::Init(pInit);
    if (umcRes != UMC_OK)
        return umcRes;

    try
    // memory allocation and Init all env for frames/tasks store - VC1TaskStore object
    {
        m_pStore = (VC1TaskStore*)m_pHeap->s_alloc<VC1TaskStore>();
        m_pStore = new(m_pStore) VC1TaskStore(m_pMemoryAllocator);

        if (!m_pStore->Init(m_iThreadDecoderNum,
                            m_iMaxFramesInProcessing,
                            this) )
            return UMC_ERR_ALLOC;

        m_pStore->CreateDSQueue(m_pContext,m_va);
    }
    catch(...)
    {
        // only allocation errors here
        Close();
        return UMC_ERR_ALLOC;
    }

    return umcRes;
}

Status VC1VideoDecoderHW::Reset(void)
{
    Status umcRes = VC1VideoDecoder::Reset();
    if (umcRes != UMC_OK)
        return umcRes;

    if (m_pStore)
    {
        if (!m_pStore->Reset())
            return UMC_ERR_NOT_INITIALIZED;

        m_pStore->CreateDSQueue(&m_pInitContext, m_va);
    }

    return UMC_OK;
}

bool VC1VideoDecoderHW::InitVAEnvironment()
{
    if (!m_pContext->m_frmBuff.m_pFrames)
    {
        m_pHeap->s_new_one(&m_pContext->m_frmBuff.m_pFrames, m_SurfaceNum);
        if (!m_pContext->m_frmBuff.m_pFrames)
            return false;
    }

    SetVideoHardwareAccelerator(m_va);
    return true;
}

Ipp32u VC1VideoDecoderHW::CalculateHeapSize()
{
    Ipp32u Size = 0;

    Size += align_value<Ipp32u>(sizeof(VC1TaskStore));
    if (!m_va)
        Size += align_value<Ipp32u>(sizeof(Frame)*(2*m_iMaxFramesInProcessing + 2*VC1NUMREFFRAMES));
    else
        Size += align_value<Ipp32u>(sizeof(Frame)*(m_SurfaceNum));

    Size += align_value<Ipp32u>(sizeof(MediaDataEx));
    return Size;
}

Status VC1VideoDecoderHW::Close(void)
{
    Status umcRes = UMC_OK;

    m_AllocBuffer = 0;

    // reset all values 
    umcRes = Reset();

    if (m_pStore)
    {
        delete m_pStore;
        m_pStore = 0;
    }
    
    FreeAlloc(m_pContext);

    if(m_pMemoryAllocator)
    {
        if (static_cast<int>(m_iMemContextID) != -1)
        {
            m_pMemoryAllocator->Unlock(m_iMemContextID);
            m_pMemoryAllocator->Free(m_iMemContextID);
            m_iMemContextID = (MemID)-1;
        }

        if (static_cast<int>(m_iHeapID) != -1)
        {
            m_pMemoryAllocator->Unlock(m_iHeapID);
            m_pMemoryAllocator->Free(m_iHeapID);
            m_iHeapID = (MemID)-1;
        }

        if (static_cast<int>(m_iFrameBufferID) != -1)
        {
            m_pMemoryAllocator->Unlock(m_iFrameBufferID);
            m_pMemoryAllocator->Free(m_iFrameBufferID);
            m_iFrameBufferID = (MemID)-1;
        }
    }

    m_pContext = NULL;
    m_dataBuffer = NULL;
    m_stCodes = NULL;
    m_frameData = NULL;
    m_pHeap = NULL;

    memset(&m_pInitContext,0,sizeof(VC1Context));

    m_pMemoryAllocator = 0;

#if defined (_WIN32) && (_DEBUG)
#ifdef VC1_DEBUG_ON
    VM_Debug::GetInstance(VC1DebugRoutine).Release();
#endif
#endif

    if (m_stCodes_VA)
    {
        free(m_stCodes_VA);
        m_stCodes_VA = NULL;
    }
    m_pStore = NULL;
    m_pContext = NULL;

    m_iThreadDecoderNum = 0;
    m_decoderInitFlag = 0;
    m_decoderFlags = 0;
    return umcRes;
}

void   VC1VideoDecoderHW::SetVideoHardwareAccelerator            (VideoAccelerator* va)
{
    if (va)
        m_va = (VideoAccelerator*)va;
}

bool VC1VideoDecoderHW::InitAlloc(VC1Context* pContext, Ipp32u )
{
    if (!InitTables(pContext))
        return false;

    pContext->m_frmBuff.m_iDisplayIndex = -1;
    pContext->m_frmBuff.m_iCurrIndex = -1;
    pContext->m_frmBuff.m_iPrevIndex = -1;
    pContext->m_frmBuff.m_iNextIndex = -1;
    pContext->m_frmBuff.m_iBFrameIndex = -1;
    pContext->m_frmBuff.m_iRangeMapIndex = -1;
    pContext->m_frmBuff.m_iRangeMapIndexPrev = -1;

    m_bLastFrameNeedDisplay = true;

    //for slice, field start code
    if (m_stCodes_VA == NULL)
    {
        m_stCodes_VA = (MediaDataEx::_MediaDataEx *)malloc(START_CODE_NUMBER * 2 * sizeof(Ipp32s) + sizeof(MediaDataEx::_MediaDataEx));
        memset(m_stCodes_VA, 0, (START_CODE_NUMBER * 2 * sizeof(Ipp32s) + sizeof(MediaDataEx::_MediaDataEx)));
        m_stCodes_VA->count = 0;
        m_stCodes_VA->index = 0;
        m_stCodes_VA->bstrm_pos = 0;
        m_stCodes_VA->offsets = (Ipp32u*)((Ipp8u*)m_stCodes_VA +
            sizeof(MediaDataEx::_MediaDataEx));
        m_stCodes_VA->values = (Ipp32u*)((Ipp8u*)m_stCodes_VA->offsets +
            START_CODE_NUMBER * sizeof(Ipp32u));
    }

    return true;
}

#ifdef UMC_VA_DXVA
Status VC1VideoDecoderHW::GetStatusReport(DXVA_Status_VC1 *pStatusReport)
{
    UMC::Status sts = UMC_OK;
    {
        sts = m_va->ExecuteStatusReportBuffer((void*)pStatusReport, sizeof(DXVA_Status_VC1)*VC1_MAX_REPORTS);
        if (sts != UMC_OK)
            return UMC_ERR_FAILED;

        for (Ipp32u i = 0; i < VC1_MAX_REPORTS; i++)
        {
            // status report presents
            if (pStatusReport[i].StatusReportFeedbackNumber)
            {
                return sts;
            }
        }
        return UMC_WRN_INFO_NOT_READY;
    }
}
#endif

void VC1VideoDecoderHW::GetStartCodes_HW(MediaData* in, Ipp32u &sShift)
{
    Ipp8u* readPos = (Ipp8u*)in->GetBufferPointer();
    Ipp32u readBufSize = (Ipp32u)in->GetDataSize();
    Ipp8u* readBuf = (Ipp8u*)in->GetBufferPointer();

    Ipp32u frameSize = 0;
    MediaDataEx::_MediaDataEx *stCodes = m_stCodes_VA;

    stCodes->count = 0;

    sShift = 0;


    Ipp32u size = 0;
    Ipp8u* ptr = NULL;
    Ipp32u readDataSize = 0;
    Ipp32u a = 0x0000FF00 | (*readPos);
    Ipp32u b = 0xFFFFFFFF;
    Ipp32u FrameNum = 0;

    memset(stCodes->offsets, 0, START_CODE_NUMBER * sizeof(Ipp32s));
    memset(stCodes->values, 0, START_CODE_NUMBER * sizeof(Ipp32s));


    while (readPos < (readBuf + readBufSize))
    {
        if (stCodes->count > 512)
            return;
        //find sequence of 0x000001 or 0x000003
        while (!(b == 0x00000001 || b == 0x00000003)
            && (++readPos < (readBuf + readBufSize)))
        {
            a = (a << 8) | (Ipp32s)(*readPos);
            b = a & 0x00FFFFFF;
        }

        //check end of read buffer
        if (readPos < (readBuf + readBufSize - 1))
        {
            if (*readPos == 0x01)
            {
                if ((*(readPos + 1) == VC1_Slice) ||
                    (*(readPos + 1) == VC1_Field) ||
                    (*(readPos + 1) == VC1_FrameHeader) ||
                    (*(readPos + 1) == VC1_SliceLevelUserData) ||
                    (*(readPos + 1) == VC1_FieldLevelUserData) ||
                    (*(readPos + 1) == VC1_FrameLevelUserData)
                    )
                {
                    readPos += 2;
                    ptr = readPos - 5;
                    size = (Ipp32u)(ptr - readBuf - readDataSize + 1);

                    frameSize = frameSize + size;

                    stCodes->offsets[stCodes->count] = frameSize;
                    if (FrameNum)
                        sShift = 1;

                    stCodes->values[stCodes->count] = ((*(readPos - 1)) << 24) + ((*(readPos - 2)) << 16) + ((*(readPos - 3)) << 8) + (*(readPos - 4));

                    readDataSize = (Ipp32u)(readPos - readBuf - 4);
                    a = 0x00010b00 | (Ipp32s)(*readPos);
                    b = a & 0x00FFFFFF;
                    stCodes->count++;
                }
                else
                {
                    {
                        readPos += 2;
                        ptr = readPos - 5;

                        //trim zero bytes
                        if (stCodes->count)
                        {
                            while ((*ptr == 0) && (ptr > readBuf))
                                ptr--;
                        }

                        //slice or field size
                        size = (Ipp32u)(readPos - readBuf - readDataSize - 4);

                        frameSize = frameSize + size;

                        readDataSize = readDataSize + size;
                        FrameNum++;
                    }
                }
            }
            else //if(*readPos == 0x03)
            {
                readPos++;
                a = (a << 8) | (Ipp32s)(*readPos);
                b = a & 0x00FFFFFF;
            }
        }
        else
        {
            //end of stream
            size = (Ipp32u)(readPos - readBuf - readDataSize);
            readDataSize = readDataSize + size;
            return;
        }
    }
}

Status VC1VideoDecoderHW::FillAndExecute(MediaData* in)
{
    Ipp32u stShift = 0;
    Ipp32s SCoffset = 0;

    if ((VC1_PROFILE_ADVANCED != m_pContext->m_seqLayerHeader.PROFILE))
        // special header (with frame size) in case of .rcv format
        SCoffset = -VC1FHSIZE;

    VC1FrameDescriptor* pPackDescriptorChild = m_pStore->GetLastDS();
    pPackDescriptorChild->m_bIsFieldAbsent = false;


    if ((pPackDescriptorChild->m_pContext->m_picLayerHeader->PTYPE != VC1_SKIPPED_FRAME) &&
        (VC1_PROFILE_ADVANCED == m_pContext->m_seqLayerHeader.PROFILE))
    {
        GetStartCodes_HW(in, stShift);
        if (stShift) // we begin since start code frame in case of external MS splitter
        {
            SCoffset -= *m_stCodes_VA->offsets;
            pPackDescriptorChild->m_pContext->m_FrameSize -= *m_stCodes_VA->offsets;

        }
    }
    try
    {
        pPackDescriptorChild->PrepareVLDVABuffers(m_pContext->m_Offsets,
            m_pContext->m_values,
            (Ipp8u*)in->GetDataPointer() - SCoffset,
            m_stCodes_VA + stShift);

    }
    catch (vc1_exception ex)
    {
        exception_type e_type = ex.get_exception_type();
        if (mem_allocation_er == e_type)
            return UMC_ERR_NOT_ENOUGH_BUFFER;
    }
    if (pPackDescriptorChild->m_pContext->m_picLayerHeader->PTYPE != VC1_SKIPPED_FRAME)
    {
        m_va->EndFrame();
    }
    in->MoveDataPointer(pPackDescriptorChild->m_pContext->m_FrameSize);

    if (pPackDescriptorChild->m_pContext->m_picLayerHeader->FCM == VC1_FieldInterlace && m_stCodes_VA->count < 2)
        pPackDescriptorChild->m_bIsFieldAbsent = true;


    if ((VC1_PROFILE_ADVANCED != m_pContext->m_seqLayerHeader.PROFILE))
    {
        m_pContext->m_seqLayerHeader.RNDCTRL = pPackDescriptorChild->m_pContext->m_seqLayerHeader.RNDCTRL;
    }

    return UMC_OK;
}

Status VC1VideoDecoderHW::VC1DecodeFrame(MediaData* in, VideoData* out_data)
{
    if (m_va->m_Profile == VC1_VLD)
    {
#if defined (UMC_VA_DXVA)
        if (m_va->IsIntelCustomGUID())
        {
#if !defined(MFX_PROTECTED_FEATURE_DISABLE)
            if (m_va->GetProtectedVA())
                return VC1DecodeFrame_VLD<VC1FrameDescriptorVA_Protected<VC1PackerDXVA_Protected> >(in, out_data);
            else
#endif
                return VC1DecodeFrame_VLD<VC1FrameDescriptorVA_EagleLake<VC1PackerDXVA_EagleLake> >(in, out_data);
        }
        else
            return VC1DecodeFrame_VLD<VC1FrameDescriptorVA<VC1PackerDXVA> >(in, out_data);
#endif
#if defined (UMC_VA_LINUX)
        return VC1DecodeFrame_VLD<VC1FrameDescriptorVA_Linux<VC1PackerLVA> >(in, out_data);
#endif
    }
    return UMC_ERR_FAILED;
}

FrameMemID  VC1VideoDecoderHW::ProcessQueuesForNextFrame(bool& isSkip, mfxU16& Corrupted)
{
    FrameMemID currIndx = -1;
    UMC::VC1FrameDescriptor *pCurrDescriptor = 0;
    UMC::VC1FrameDescriptor *pTempDescriptor = 0;

    m_RMIndexToFree = -1;
    m_CurrIndexToFree = -1;

    pCurrDescriptor = m_pStore->GetFirstDS();

    // free first descriptor
    m_pStore->SetFirstBusyDescriptorAsReady();

    if (!m_pStore->GetPerformedDS(&pTempDescriptor))
        m_pStore->GetReadySkippedDS(&pTempDescriptor);

    if (pCurrDescriptor)
    {
        SetCorrupted(pCurrDescriptor, Corrupted);
        if (pCurrDescriptor->m_pContext->m_picLayerHeader->PTYPE == VC1_SKIPPED_FRAME)
        {
            isSkip = true;
            if (!pCurrDescriptor->isDescriptorValid())
            {
                return currIndx;
            }
            else
            {
                currIndx = m_pStore->GetIdx(pCurrDescriptor->m_pContext->m_frmBuff.m_iCurrIndex);
            }

            // Range Map
            if ((pCurrDescriptor->m_pContext->m_seqLayerHeader.RANGE_MAPY_FLAG) ||
                (pCurrDescriptor->m_pContext->m_seqLayerHeader.RANGE_MAPUV_FLAG))
            {
                currIndx = m_pStore->GetIdx(pCurrDescriptor->m_pContext->m_frmBuff.m_iRangeMapIndex);
            }
            m_pStore->UnLockSurface(pCurrDescriptor->m_pContext->m_frmBuff.m_iToSkipCoping);
            return currIndx;
        }
        else
        {
            currIndx = m_pStore->GetIdx(pCurrDescriptor->m_pContext->m_frmBuff.m_iCurrIndex);

            // We should unlock after LockRect in PrepareOutPut function
            if ((pCurrDescriptor->m_pContext->m_seqLayerHeader.RANGE_MAPY_FLAG) ||
                (pCurrDescriptor->m_pContext->m_seqLayerHeader.RANGE_MAPUV_FLAG) ||
                (pCurrDescriptor->m_pContext->m_seqLayerHeader.RANGERED))
            {
                if (!VC1_IS_REFERENCE(pCurrDescriptor->m_pContext->m_picLayerHeader->PTYPE))
                {
                    currIndx = m_pStore->GetIdx(pCurrDescriptor->m_pContext->m_frmBuff.m_iRangeMapIndex);
                    m_RMIndexToFree = pCurrDescriptor->m_pContext->m_frmBuff.m_iRangeMapIndex;
                }
                else
                {
                    currIndx = m_pStore->GetIdx(pCurrDescriptor->m_pContext->m_frmBuff.m_iRangeMapIndex);
                    m_RMIndexToFree = pCurrDescriptor->m_pContext->m_frmBuff.m_iRangeMapIndexPrev;
                }
            }
            // Asynchrony Unlock
            if (!VC1_IS_REFERENCE(pCurrDescriptor->m_pContext->m_picLayerHeader->PTYPE))
                m_CurrIndexToFree = pCurrDescriptor->m_pContext->m_frmBuff.m_iDisplayIndex;
            else
            {
                if (pCurrDescriptor->m_pContext->m_frmBuff.m_iToFreeIndex > -1)
                    m_CurrIndexToFree = pCurrDescriptor->m_pContext->m_frmBuff.m_iToFreeIndex;

            }
        }
    }
    return currIndx;
}

Status  VC1VideoDecoderHW::SetRMSurface()
{
    Status sts;
    sts = VC1VideoDecoder::SetRMSurface();
    UMC_CHECK_STATUS(sts);
    FillAndExecute(m_pCurrentIn);
    return UMC_OK;
}

UMC::FrameMemID VC1VideoDecoderHW::GetSkippedIndex(bool isIn)
{
    return VC1VideoDecoder::GetSkippedIndex(m_pStore->GetLastDS(), isIn);
}

#endif //UMC_ENABLE_VC1_VIDEO_DECODER

