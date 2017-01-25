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

#include "umc_vc1_dec_seq.h"

#include "umc_vc1_dec_task_store.h"
#include "umc_vc1_dec_debug.h"
#include "umc_vc1_common.h"
#include "umc_vc1_dec_frame_descr.h"
#include "umc_vc1_dec_exception.h"
#include "mfx_trace.h"

#ifdef ALLOW_SW_VC1_FALLBACK
#include "umc_vc1_dec_job.h"
#endif // #ifdef ALLOW_SW_VC1_FALLBACK

using namespace UMC;
using namespace UMC::VC1Exceptions;


bool VC1FrameDescriptor::Init(Ipp32u         DescriporID,
                              VC1Context*    pContext,
                              VC1TaskStore*  pStore,
                              Ipp16s*        pResidBuf)
{
    VC1SequenceLayerHeader* seqLayerHeader = &pContext->m_seqLayerHeader;
    Ipp32u HeightMB = pContext->m_seqLayerHeader.MaxHeightMB;
    Ipp32u WidthMB = pContext->m_seqLayerHeader.MaxWidthMB;

    if(seqLayerHeader->INTERLACE)
        HeightMB = HeightMB + (HeightMB & 1); //in case of field with odd height

    if (!m_pContext)
    {
        Ipp8u* ptr = NULL;
        ptr += align_value<Ipp32u>(sizeof(VC1Context));
        ptr += align_value<Ipp32u>(sizeof(VC1PictureLayerHeader)*VC1_MAX_SLICE_NUM);
        ptr += align_value<Ipp32u>((HeightMB*seqLayerHeader->MaxWidthMB*VC1_MAX_BITPANE_CHUNCKS));
#ifdef ALLOW_SW_VC1_FALLBACK
        ptr += align_value<Ipp32u>(sizeof(Ipp16s)*HeightMB*WidthMB * 2 * 2);
        ptr += align_value<Ipp32u>(sizeof(Ipp8u)*HeightMB*WidthMB);
        ptr += align_value<Ipp32u>(sizeof(VC1MB)*(HeightMB*WidthMB));
        ptr += align_value<Ipp32u>(sizeof(VC1DCMBParam)*HeightMB*WidthMB);
#endif

        // Need to replace with MFX allocator
        if (m_pMemoryAllocator->Alloc(&m_iMemContextID,
                                      (size_t)ptr,
                                      UMC_ALLOC_PERSISTENT,
                                      16) != UMC_OK)
                                      return false;

        m_pContext = (VC1Context*)(m_pMemoryAllocator->Lock(m_iMemContextID));
        memset(m_pContext,0,size_t(ptr));
        m_pContext->bp_round_count = -1;
        ptr = (Ipp8u*)m_pContext;

        ptr += align_value<Ipp32u>(sizeof(VC1Context));
        m_pContext->m_picLayerHeader = (VC1PictureLayerHeader*)ptr;
        m_pContext->m_InitPicLayer = m_pContext->m_picLayerHeader;

        ptr += align_value<Ipp32u>((sizeof(VC1PictureLayerHeader)*VC1_MAX_SLICE_NUM));
        m_pContext->m_pBitplane.m_databits = ptr;

#ifdef ALLOW_SW_VC1_FALLBACK
        ptr += align_value<Ipp32u>((HeightMB*seqLayerHeader->MaxWidthMB*VC1_MAX_BITPANE_CHUNCKS));
        m_pContext->m_MBs = (VC1MB*)ptr;
        ptr += align_value<Ipp32u>(sizeof(VC1MB)*(HeightMB*WidthMB));
        m_pContext->DCACParams = (VC1DCMBParam*)ptr;
        ptr += align_value<Ipp32u>(sizeof(VC1DCMBParam)*HeightMB*WidthMB);
        m_pContext->savedMV = (Ipp16s*)(ptr);
        ptr += align_value<Ipp32u>(sizeof(Ipp16s)*HeightMB*WidthMB * 2 * 2);
        m_pContext->savedMVSamePolarity  = ptr;
#endif
    }
    Ipp32u buffSize =  2*(HeightMB*VC1_PIXEL_IN_LUMA)*(WidthMB*VC1_PIXEL_IN_LUMA);

    //buf size should be divisible by 4
    if(buffSize & 0x00000003)
        buffSize = (buffSize&0xFFFFFFFC) + 4;

    // Need to replace with MFX allocator
    if (m_pMemoryAllocator->Alloc(&m_iInernBufferID,
                                  buffSize,
                                  UMC_ALLOC_PERSISTENT,
                                  16) != UMC_OK)
                                  return false;

    m_pContext->m_pBufferStart = (Ipp8u*)m_pMemoryAllocator->Lock(m_iInernBufferID);
    memset(m_pContext->m_pBufferStart, 0, buffSize);

    // memory for diffs for each FrameDescriptor
    if (!m_pDiffMem)
    {
        if (!pResidBuf)
        {
            if(m_pMemoryAllocator->Alloc(&m_iDiffMemID,
                                         sizeof(Ipp16s)*WidthMB*HeightMB*8*8*6,
                                         UMC_ALLOC_PERSISTENT, 16) != UMC_OK )
            {
                return false;
            }
            m_pDiffMem = (Ipp16s*)m_pMemoryAllocator->Lock(m_iDiffMemID);
        }
        else
            m_pDiffMem = pResidBuf;
    }

    // Pointers to common pContext
    m_pStore = pStore;
    m_pContext->m_vlcTbl = pContext->m_vlcTbl;
    m_pContext->pRefDist = &pContext->RefDist;
    m_pContext->m_frmBuff.m_pFrames = pContext->m_frmBuff.m_pFrames;
    m_pContext->m_frmBuff.m_iDisplayIndex =  0;
    m_pContext->m_frmBuff.m_iCurrIndex    =  0;
    m_pContext->m_frmBuff.m_iPrevIndex    =  0;
    m_pContext->m_frmBuff.m_iNextIndex    =  1;
    m_pContext->m_frmBuff.m_iRangeMapIndex   =  pContext->m_frmBuff.m_iRangeMapIndex;
    m_pContext->m_frmBuff.m_iToFreeIndex = -1;

    m_pContext->m_seqLayerHeader = pContext->m_seqLayerHeader;
#ifdef ALLOW_SW_VC1_FALLBACK
    m_pContext->savedMV_Curr = pContext->savedMV_Curr;
    m_pContext->savedMVSamePolarity_Curr = pContext->savedMVSamePolarity_Curr;
#endif
    m_iSelfID = DescriporID;
    return true;
}

bool VC1FrameDescriptor::SetNewSHParams(VC1Context* pContext)
{
    m_pContext->m_seqLayerHeader = pContext->m_seqLayerHeader;
    return true;
}
void VC1FrameDescriptor::Reset()
{
    m_iFrameCounter = 0;
    m_iRefFramesDst = 0;
    m_bIsReadyToLoad = true;
    m_iSelfID = 0;
    m_iRefFramesDst = 0;
    m_iBFramesDst = 0;
    m_bIsReferenceReady = false;
    m_bIsReadyToDisplay = false;
    m_bIsSkippedFrame = false;
    m_bIsReadyToProcess = false;
    m_bIsBusy = false;
}
void VC1FrameDescriptor::Release()
{
    if(m_pMemoryAllocator)
    {
        if (static_cast<int>(m_iDiffMemID) != -1)
        {
            m_pMemoryAllocator->Unlock(m_iDiffMemID);
            m_pMemoryAllocator->Free(m_iDiffMemID);
            m_iDiffMemID = (MemID)-1;
        }

        if (static_cast<int>(m_iInernBufferID) != -1)
        {
            m_pMemoryAllocator->Unlock(m_iInernBufferID);
            m_pMemoryAllocator->Free(m_iInernBufferID);
            m_iInernBufferID = (MemID)-1;
        }
        if (static_cast<int>(m_iMemContextID) != -1)
        {
            m_pMemoryAllocator->Unlock(m_iMemContextID);
            m_pMemoryAllocator->Free(m_iMemContextID);
            m_iMemContextID = (MemID)-1;
        }
    }

}

Status VC1FrameDescriptor::SetPictureIndices(Ipp32u PTYPE, bool& skip)
{
    Status vc1Sts = VC1_OK;
    FrameMemID CheckIdx = 0;

    switch(PTYPE)
    {
    case VC1_I_FRAME:
    case VC1_P_FRAME:
        if (m_pStore->IsNeedSkipFrame(PTYPE))
        {
            skip = true;
            return UMC_ERR_NOT_ENOUGH_DATA;
        }

        m_pContext->m_frmBuff.m_iPrevIndex = m_pStore->GetPrevIndex();
        m_pContext->m_frmBuff.m_iNextIndex = m_pStore->GetNextIndex();

        if (m_pContext->m_frmBuff.m_iPrevIndex == -1)
        {
            CheckIdx = m_pStore->LockSurface(&m_pContext->m_frmBuff.m_iPrevIndex);
            m_pContext->m_frmBuff.m_iCurrIndex = m_pContext->m_frmBuff.m_iPrevIndex;
        }
        else if (m_pContext->m_frmBuff.m_iNextIndex == -1)
        {
            CheckIdx = m_pStore->LockSurface(&m_pContext->m_frmBuff.m_iNextIndex);
            m_pContext->m_frmBuff.m_iCurrIndex = m_pContext->m_frmBuff.m_iNextIndex;
        }
        else
        {
            m_pContext->m_frmBuff.m_iToFreeIndex = m_pContext->m_frmBuff.m_iPrevIndex;
            CheckIdx = m_pStore->LockSurface(&m_pContext->m_frmBuff.m_iPrevIndex);

            m_pContext->m_frmBuff.m_iCurrIndex = m_pContext->m_frmBuff.m_iPrevIndex;
            m_pContext->m_frmBuff.m_iPrevIndex = m_pContext->m_frmBuff.m_iNextIndex;
            m_pContext->m_frmBuff.m_iNextIndex = m_pContext->m_frmBuff.m_iCurrIndex;
        }

        // for Range mapping get prev index
        m_pContext->m_frmBuff.m_iDisplayIndex = m_pStore->GetNextIndex();
        if (-1 == m_pContext->m_frmBuff.m_iDisplayIndex)
            m_pContext->m_frmBuff.m_iDisplayIndex = m_pStore->GetPrevIndex();

        m_pStore->SetNextIndex(m_pContext->m_frmBuff.m_iNextIndex);
        m_pStore->SetCurrIndex(m_pContext->m_frmBuff.m_iCurrIndex);
        m_pStore->SetPrevIndex(m_pContext->m_frmBuff.m_iPrevIndex);

        m_pContext->m_frmBuff.m_pFrames[m_pContext->m_frmBuff.m_iCurrIndex].corrupted= 0;
        break;
    case VC1_B_FRAME:
        if (m_pStore->IsNeedSkipFrame(PTYPE))
        {
            skip = true;
            return UMC_ERR_NOT_ENOUGH_DATA;
        }
        m_pContext->m_frmBuff.m_iPrevIndex = m_pStore->GetPrevIndex();
        m_pContext->m_frmBuff.m_iNextIndex = m_pStore->GetNextIndex();
        m_pContext->m_frmBuff.m_iBFrameIndex = m_pStore->GetBFrameIndex();
        CheckIdx = m_pStore->LockSurface(&m_pContext->m_frmBuff.m_iBFrameIndex);
        m_pContext->m_frmBuff.m_iCurrIndex = m_pContext->m_frmBuff.m_iBFrameIndex;

        m_pStore->SetCurrIndex(m_pContext->m_frmBuff.m_iCurrIndex);
        m_pStore->SetBFrameIndex(m_pContext->m_frmBuff.m_iBFrameIndex);
        // possible situation in case of SKIP frames
        if (m_pContext->m_frmBuff.m_iNextIndex == -1)
        {
            m_pContext->m_frmBuff.m_iNextIndex = m_pContext->m_frmBuff.m_iPrevIndex;
            m_pContext->m_frmBuff.m_iDisplayIndex = m_pContext->m_frmBuff.m_iCurrIndex;
            m_pContext->m_frmBuff.m_pFrames[m_pContext->m_frmBuff.m_iCurrIndex].corrupted = ERROR_FRAME_MAJOR;
        }
        else
        {
            m_pContext->m_frmBuff.m_iDisplayIndex = m_pContext->m_frmBuff.m_iCurrIndex;
            m_pContext->m_frmBuff.m_pFrames[m_pContext->m_frmBuff.m_iCurrIndex].corrupted= 0;
        }

        m_pContext->m_frmBuff.m_iDisplayIndex = m_pContext->m_frmBuff.m_iCurrIndex;
        break;
    case VC1_BI_FRAME:
        if (m_pStore->IsNeedSkipFrame(PTYPE))
        {
            skip = true;
            return UMC_ERR_NOT_ENOUGH_DATA;
        }
        m_pContext->m_frmBuff.m_iPrevIndex = m_pStore->GetPrevIndex();
        m_pContext->m_frmBuff.m_iNextIndex = m_pStore->GetNextIndex();
        m_pContext->m_frmBuff.m_iBFrameIndex = m_pStore->GetBFrameIndex();
        CheckIdx = m_pStore->LockSurface(&m_pContext->m_frmBuff.m_iBFrameIndex);
        m_pContext->m_frmBuff.m_iCurrIndex = m_pContext->m_frmBuff.m_iBFrameIndex;
        m_pStore->SetCurrIndex(m_pContext->m_frmBuff.m_iCurrIndex);
        m_pStore->SetBFrameIndex(m_pContext->m_frmBuff.m_iBFrameIndex);
        m_pContext->m_frmBuff.m_iDisplayIndex = m_pContext->m_frmBuff.m_iCurrIndex;
        m_pContext->m_frmBuff.m_pFrames[m_pContext->m_frmBuff.m_iCurrIndex].corrupted= 0;
        break;
    case VC1_SKIPPED_FRAME:
        m_pContext->m_frmBuff.m_iCurrIndex = m_pContext->m_frmBuff.m_iNextIndex =  m_pContext->m_frmBuff.m_iDisplayIndex = m_pStore->GetNextIndex();
        if (-1 == m_pContext->m_frmBuff.m_iCurrIndex)
            m_pContext->m_frmBuff.m_iCurrIndex = m_pStore->GetPrevIndex();
        if (-1 == m_pContext->m_frmBuff.m_iDisplayIndex)
            m_pContext->m_frmBuff.m_iDisplayIndex = m_pStore->GetPrevIndex();
        if (m_pContext->m_seqLayerHeader.RANGE_MAPY_FLAG ||
            m_pContext->m_seqLayerHeader.RANGE_MAPUV_FLAG ||
            m_pContext->m_seqLayerHeader.RANGERED)
            m_pContext->m_frmBuff.m_iRangeMapIndex = m_pStore->GetRangeMapIndex();

        m_pContext->m_frmBuff.m_iPrevIndex = m_pStore->GetPrevIndex();
        CheckIdx = m_pStore->LockSurface(&m_pContext->m_frmBuff.m_iToSkipCoping, true);
        m_pContext->m_frmBuff.m_pFrames[m_pContext->m_frmBuff.m_iCurrIndex].corrupted= 0;

        break;
    default:
        break;
    }
        
    if (-1 == CheckIdx)
        return VC1_FAIL;

    if ((VC1_P_FRAME == PTYPE) || (VC1_SKIPPED_FRAME == PTYPE))
    {
        if (m_pContext->m_frmBuff.m_iPrevIndex == -1)
            return UMC_ERR_NOT_ENOUGH_DATA;
    } 
    else if (VC1_B_FRAME == PTYPE)
    {
        if ((m_pContext->m_frmBuff.m_iPrevIndex == -1)||
            (m_pContext->m_frmBuff.m_iNextIndex == -1))
            return UMC_ERR_NOT_ENOUGH_DATA;
    }

    m_pContext->m_frmBuff.m_pFrames[m_pContext->m_frmBuff.m_iCurrIndex].FCM = m_pContext->m_picLayerHeader->FCM;

#ifdef ALLOW_SW_VC1_FALLBACK
    m_pContext->LumaTable[0] =  m_pContext->m_frmBuff.m_pFrames[m_pContext->m_frmBuff.m_iCurrIndex].LumaTable[0];
    m_pContext->LumaTable[1] =  m_pContext->m_frmBuff.m_pFrames[m_pContext->m_frmBuff.m_iCurrIndex].LumaTable[1];
    m_pContext->LumaTable[2] =  m_pContext->m_frmBuff.m_pFrames[m_pContext->m_frmBuff.m_iCurrIndex].LumaTable[2];
    m_pContext->LumaTable[3] =  m_pContext->m_frmBuff.m_pFrames[m_pContext->m_frmBuff.m_iCurrIndex].LumaTable[3];

    m_pContext->ChromaTable[0] = m_pContext->m_frmBuff.m_pFrames[m_pContext->m_frmBuff.m_iCurrIndex].ChromaTable[0];
    m_pContext->ChromaTable[1] = m_pContext->m_frmBuff.m_pFrames[m_pContext->m_frmBuff.m_iCurrIndex].ChromaTable[1];
    m_pContext->ChromaTable[2] = m_pContext->m_frmBuff.m_pFrames[m_pContext->m_frmBuff.m_iCurrIndex].ChromaTable[2];
    m_pContext->ChromaTable[3] = m_pContext->m_frmBuff.m_pFrames[m_pContext->m_frmBuff.m_iCurrIndex].ChromaTable[3];
    *m_pContext->m_frmBuff.m_pFrames[m_pContext->m_frmBuff.m_iCurrIndex].pRANGE_MAPY =
        m_pContext->m_picLayerHeader->RANGEREDFRM - 1;
#endif

    m_pContext->m_frmBuff.m_pFrames[m_pContext->m_frmBuff.m_iCurrIndex].ICFieldMask = 0;

    if (m_pContext->m_frmBuff.m_iPrevIndex > -1)
        m_pContext->PrevFCM = m_pContext->m_frmBuff.m_pFrames[m_pContext->m_frmBuff.m_iPrevIndex].FCM;
    else
        m_pContext->PrevFCM = m_pContext->m_picLayerHeader->FCM;


    if (m_pContext->m_frmBuff.m_iNextIndex > -1)
        m_pContext->NextFCM = m_pContext->m_frmBuff.m_pFrames[m_pContext->m_frmBuff.m_iNextIndex].FCM;
    else
        m_pContext->NextFCM = m_pContext->m_picLayerHeader->FCM;

    return vc1Sts;
}

#ifdef ALLOW_SW_VC1_FALLBACK
void VC1FrameDescriptor::processFrame(Ipp32u*  pOffsets,
    Ipp32u*  pValues)
{
    MFX_AUTO_LTRACE(MFX_TRACE_LEVEL_HOTSPOTS, "VC1FrameDescriptor::processFrame");
    SliceParams slparams;
    memset(&slparams, 0, sizeof(SliceParams));
    VC1Task task(0);

    VC1TaskStoreSW *pStore = (VC1TaskStoreSW *)m_pStore;

    Ipp32u temp_value = 0;
    Ipp32u* bitstream;
    Ipp32s bitoffset = 31;

    Ipp16u heightMB = m_pContext->m_seqLayerHeader.heightMB;
    Ipp32u IsField = 0;

    bool isSecondField = false;
    slparams.MBStartRow = 0;
    slparams.is_continue = 1;
    slparams.MBEndRow = heightMB;

    if (m_pContext->m_picLayerHeader->PTYPE == VC1_SKIPPED_FRAME)
    {
        m_bIsSkippedFrame = true;
        m_bIsReadyToProcess = false;
        SZTables(m_pContext);
        return;
    }
    else
    {
        m_bIsSkippedFrame = false;
    }


    if (m_pContext->m_seqLayerHeader.PROFILE == VC1_PROFILE_ADVANCED)
        DecodePicHeader(m_pContext);
    else
        Decode_PictureLayer(m_pContext);


    slparams.m_pstart = m_pContext->m_bitstream.pBitstream;
    slparams.m_bitOffset = m_pContext->m_bitstream.bitOffset;
    slparams.m_picLayerHeader = m_pContext->m_picLayerHeader;
    slparams.m_vlcTbl = m_pContext->m_vlcTbl;

    if (m_pContext->m_seqLayerHeader.PROFILE != VC1_PROFILE_ADVANCED)
    {
        slparams.MBRowsToDecode = slparams.MBEndRow - slparams.MBStartRow;
        task.m_pSlice = &slparams;
        task.setSliceParams(m_pContext);
        task.m_isFieldReady = true;
        pStore->AddSampleTask(&task, m_iSelfID);
        task.m_pSlice = NULL;
        pStore->DistributeTasks(m_iSelfID);
        m_pContext->m_frmBuff.m_pFrames[m_pContext->m_frmBuff.m_iCurrIndex].isIC = m_pContext->m_bIntensityCompensation;

        SZTables(m_pContext);
        if (VC1_P_FRAME == m_pContext->m_picLayerHeader->PTYPE)
            CreateComplexICTablesForFrame(m_pContext);
        return;
    }

    //skip user data
    while (*(pValues + 1) == 0x1B010000 || *(pValues + 1) == 0x1D010000 || *(pValues + 1) == 0x1C010000)
    {
        pOffsets++;
        pValues++;
    }


    if (*(pValues + 1) == 0x0B010000)
    {
        bitstream = reinterpret_cast<Ipp32u*>(m_pContext->m_pBufferStart + *(pOffsets + 1));
        VC1BitstreamParser::GetNBits(bitstream, bitoffset, 32, temp_value);
        bitoffset = 31;
        VC1BitstreamParser::GetNBits(bitstream, bitoffset, 9, slparams.MBEndRow);     //SLICE_ADDR

        m_pContext->m_picLayerHeader->is_slice = 1;
    }
    else if (*(pValues + 1) == 0x0C010000)
    {
        slparams.m_picLayerHeader->CurrField = 0;
        slparams.m_picLayerHeader->PTYPE = m_pContext->m_picLayerHeader->PTypeField1;
        slparams.m_picLayerHeader->BottomField = (Ipp8u)(1 - m_pContext->m_picLayerHeader->TFF);
        slparams.MBEndRow = (heightMB + 1) / 2;
        IsField = 1;
    }


    slparams.MBRowsToDecode = slparams.MBEndRow - slparams.MBStartRow;
    task.m_pSlice = &slparams;
#ifdef SLICE_INFO
    Ipp32s slice_counter = 0;
    printf("Slice number %d\n", slice_counter);
    printf("Number MB rows to decode  =%d\n", slparams.MBRowsToDecode);
    ++slice_counter;
#endif
    task.setSliceParams(m_pContext);

    task.m_isFieldReady = true;
    pStore->AddSampleTask(&task, m_iSelfID);

    pOffsets++;
    pValues++;

    while (*pOffsets)
    {
        task.m_isFirstInSecondSlice = false;
        if (*(pValues) == 0x0C010000)
        {
            isSecondField = true;
            IsField = 1;
            task.m_isFirstInSecondSlice = true;
            m_pContext->m_bitstream.pBitstream = reinterpret_cast<Ipp32u*>(m_pContext->m_pBufferStart + *pOffsets);
            m_pContext->m_bitstream.pBitstream += 1; // skip start code
            m_pContext->m_bitstream.bitOffset = 31;
            ++m_pContext->m_picLayerHeader;
            *m_pContext->m_picLayerHeader = *m_pContext->m_InitPicLayer;

            m_pContext->m_picLayerHeader->BottomField = (Ipp8u)m_pContext->m_InitPicLayer->TFF;
            m_pContext->m_picLayerHeader->PTYPE = m_pContext->m_InitPicLayer->PTypeField2;
            m_pContext->m_picLayerHeader->CurrField = 1;
            m_pContext->m_frmBuff.m_pFrames[m_pContext->m_frmBuff.m_iCurrIndex].RANGE_MAPY = m_pContext->m_seqLayerHeader.RANGE_MAPY;
            m_pContext->m_frmBuff.m_pFrames[m_pContext->m_frmBuff.m_iCurrIndex].RANGE_MAPUV = m_pContext->m_seqLayerHeader.RANGE_MAPUV;

            m_pContext->m_picLayerHeader->is_slice = 0;
            DecodePicHeader(m_pContext);

            slparams.MBStartRow = (heightMB + 1) / 2;

            //skip user data
            while (*(pValues + 1) == 0x1B010000 || *(pValues + 1) == 0x1D010000 || *(pValues + 1) == 0x1C010000)
            {
                pOffsets++;
                pValues++;
            }

            if (*(pOffsets + 1) && *(pValues + 1) == 0x0B010000)
            {
                bitstream = reinterpret_cast<Ipp32u*>(m_pContext->m_pBufferStart + *(pOffsets + 1));
                VC1BitstreamParser::GetNBits(bitstream, bitoffset, 32, temp_value);
                bitoffset = 31;
                VC1BitstreamParser::GetNBits(bitstream, bitoffset, 9, slparams.MBEndRow);
            }
            else
            {
                if (!IsField)
                    slparams.MBEndRow = heightMB;
                else
                {
                    slparams.MBEndRow = heightMB + (heightMB & 1);
                }
            }

            slparams.m_picLayerHeader = m_pContext->m_picLayerHeader;
            slparams.m_vlcTbl = m_pContext->m_vlcTbl;
            slparams.m_pstart = m_pContext->m_bitstream.pBitstream;
            slparams.m_bitOffset = m_pContext->m_bitstream.bitOffset;
            slparams.MBRowsToDecode = slparams.MBEndRow - slparams.MBStartRow;
            task.m_pSlice = &slparams;
            task.setSliceParams(m_pContext);
            if (isSecondField)
                task.m_isFieldReady = false;
            else
                task.m_isFieldReady = true;
            pStore->AddSampleTask(&task, m_iSelfID);
            slparams.MBStartRow = slparams.MBEndRow;
            ++pOffsets;
            ++pValues;
        }
        else if (*(pValues) == 0x0B010000)
        {
            m_pContext->m_bitstream.pBitstream = reinterpret_cast<Ipp32u*>(m_pContext->m_pBufferStart + *pOffsets);
            VC1BitstreamParser::GetNBits(m_pContext->m_bitstream.pBitstream, m_pContext->m_bitstream.bitOffset, 32, temp_value);
            m_pContext->m_bitstream.bitOffset = 31;

            VC1BitstreamParser::GetNBits(m_pContext->m_bitstream.pBitstream, m_pContext->m_bitstream.bitOffset, 9, slparams.MBStartRow);     //SLICE_ADDR
            VC1BitstreamParser::GetNBits(m_pContext->m_bitstream.pBitstream, m_pContext->m_bitstream.bitOffset, 1, temp_value);            //PIC_HEADER_FLAG


            if (temp_value == 1)                //PIC_HEADER_FLAG
            {
                ++m_pContext->m_picLayerHeader;
                if (isSecondField)
                    m_pContext->m_picLayerHeader->CurrField = 1;
                else
                    m_pContext->m_picLayerHeader->CurrField = 0;
                DecodePictureHeader_Adv(m_pContext);
                DecodePicHeader(m_pContext);
            }
            m_pContext->m_picLayerHeader->is_slice = 1;

            //skip user data
            while (*(pValues + 1) == 0x1B010000 || *(pValues + 1) == 0x1D010000 || *(pValues + 1) == 0x1C010000)
            {
                pOffsets++;
                pValues++;
            }

            if (*(pOffsets + 1) && (*(pValues + 1) == 0x0B010000 || *(pValues + 1) == 0x1B010000))
            {
                bitstream = reinterpret_cast<Ipp32u*>(m_pContext->m_pBufferStart + *(pOffsets + 1));
                VC1BitstreamParser::GetNBits(bitstream, bitoffset, 32, temp_value);
                bitoffset = 31;
                VC1BitstreamParser::GetNBits(bitstream, bitoffset, 9, slparams.MBEndRow);

            }
            else if (*(pValues + 1) == 0x0C010000)
                slparams.MBEndRow = (heightMB + 1) / 2;
            else
                if (!IsField)
                    slparams.MBEndRow = heightMB;
                else
                {
                    slparams.MBEndRow = heightMB + (heightMB & 1);
                }

            slparams.m_picLayerHeader = m_pContext->m_picLayerHeader;
            slparams.m_vlcTbl = m_pContext->m_vlcTbl;
            slparams.m_pstart = m_pContext->m_bitstream.pBitstream;
            slparams.m_bitOffset = m_pContext->m_bitstream.bitOffset;
            slparams.MBRowsToDecode = slparams.MBEndRow - slparams.MBStartRow;
            task.m_pSlice = &slparams;
            task.setSliceParams(m_pContext);
            if (isSecondField)
                task.m_isFieldReady = false;
            else
                task.m_isFieldReady = true;
            pStore->AddSampleTask(&task, m_iSelfID);
#ifdef SLICE_INFO
            printf("Slice number %d\n", slice_counter);
            printf("Number MB rows to decode  =%d\n", slparams.MBRowsToDecode);
            ++slice_counter;
#endif
            slparams.MBStartRow = slparams.MBEndRow;

            ++pOffsets;
            ++pValues;
        }
        else
        {
            pOffsets++;
            pValues++;
        }
    }

    // incorrect field bs. Absence of second field
    if (m_pContext->m_picLayerHeader->FCM == VC1_FieldInterlace && !isSecondField)
    {
        task.m_pSlice = NULL;
        throw VC1Exceptions::vc1_exception(VC1Exceptions::invalid_stream);
    }
    SZTables(m_pContext);
    if ((VC1_B_FRAME != m_pContext->m_picLayerHeader->PTYPE) &&
        (VC1_BI_FRAME != m_pContext->m_picLayerHeader->PTYPE))

    {
        if ((m_pContext->m_frmBuff.m_pFrames[m_pContext->m_frmBuff.m_iCurrIndex].ICFieldMask) ||
            ((m_pContext->m_frmBuff.m_iPrevIndex > -1) &&
            (m_pContext->m_frmBuff.m_pFrames[m_pContext->m_frmBuff.m_iPrevIndex].ICFieldMask)))
        {
            if (m_pContext->m_picLayerHeader->FCM == VC1_FieldInterlace)
                CreateComplexICTablesForFields(m_pContext);
            else
                CreateComplexICTablesForFrame(m_pContext);
        }
    }


    // Intensity compensation for frame
    m_pContext->m_frmBuff.m_pFrames[m_pContext->m_frmBuff.m_iCurrIndex].isIC = m_pContext->m_bIntensityCompensation;
    task.m_pSlice = NULL;
    pStore->DistributeTasks(m_iSelfID);
}

Status VC1FrameDescriptor::preProcData(VC1Context*            pContext,
    Ipp32u                 bufferSize,
    Ipp64u                 frameCount,
    bool& skip)
{
    Status vc1Sts = UMC_OK;

    Ipp32u Ptype;
    Ipp8u* pbufferStart = pContext->m_pBufferStart;
    m_iFrameCounter = frameCount;
    m_pContext->m_FrameSize = bufferSize;

    MFX_INTERNAL_CPY(m_pContext->m_pBufferStart, pbufferStart, (bufferSize & 0xFFFFFFF8) + 8); // (bufferSize & 0xFFFFFFF8) + 8 - skip frames
    m_pContext->m_bitstream.pBitstream = (Ipp32u*)m_pContext->m_pBufferStart;

    m_pContext->m_bitstream.pBitstream += 1;

    m_pContext->m_bitstream.bitOffset = 31;

    m_pContext->m_picLayerHeader = m_pContext->m_InitPicLayer;
    m_pContext->m_seqLayerHeader = pContext->m_seqLayerHeader;

    m_bIsSpecialBSkipFrame = false;
    if (m_pContext->m_seqLayerHeader.PROFILE == VC1_PROFILE_ADVANCED)
    {
        GetNextPicHeader_Adv(m_pContext);
        Ptype = m_pContext->m_picLayerHeader->PTYPE | m_pContext->m_picLayerHeader->PTypeField1;
        vc1Sts = SetPictureIndices(Ptype, skip);
        if (vc1Sts != UMC_OK)
            return vc1Sts;
    }
    else
    {
        m_pContext->m_bitstream.pBitstream = (Ipp32u*)m_pContext->m_pBufferStart + 2;
        GetNextPicHeader(m_pContext, false);
        vc1Sts = SetPictureIndices(m_pContext->m_picLayerHeader->PTYPE, skip);
        if (vc1Sts != UMC_OK)
            return vc1Sts;
    }
    m_pContext->m_bIntensityCompensation = 0;
    m_pContext->m_frmBuff.m_pFrames[m_pContext->m_frmBuff.m_iCurrIndex].m_bIsExpanded = 0;
    return vc1Sts;
}
#endif // #ifdef ALLOW_SW_VC1_FALLBACK

#endif
