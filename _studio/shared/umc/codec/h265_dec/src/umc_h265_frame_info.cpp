//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2017 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"

#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#include "umc_h265_frame_info.h"

namespace UMC_HEVC_DECODER
{

#ifndef MFX_VA
// Initialize tiles and slices threading information
void H265DecoderFrameInfo::FillTileInfo()
{
    H265Slice * slice = GetAnySlice();
    const H265PicParamSet * pps = slice->GetPicParam();
    Ipp32u tilesCount = pps->getNumTiles();
    m_tilesThreadingInfo.resize(tilesCount);

    for (Ipp32u i = 0; i < tilesCount; i++)
    {
        TileThreadingInfo & info = m_tilesThreadingInfo[i];
        info.processInfo.firstCU = m_pFrame->getCD()->GetInverseCUOrderMap(pps->tilesInfo[i].firstCUAddr);
        info.processInfo.Initialize(m_tilesThreadingInfo[i].processInfo.firstCU, pps->tilesInfo[i].width);
        info.processInfo.maxCU = m_pFrame->getCD()->GetInverseCUOrderMap(pps->tilesInfo[i].endCUAddr);
        info.m_context = 0;
    }

    if (!m_hasTiles)
    {
        for (Ipp32u i = 0; i < LAST_PROCESS_ID; i++)
        {
            m_curCUToProcess[i] = m_pSliceQueue[0]->GetFirstMB(); // it can be more then 0
        }
    }

    if (!IsNeedSAO())
    {
        m_curCUToProcess[SAO_PROCESS_ID] = m_pFrame->getCD()->m_NumCUsInFrame;
    }

    if (!IsNeedDeblocking())
    {
        m_curCUToProcess[DEB_PROCESS_ID] = m_pFrame->getCD()->m_NumCUsInFrame;
    }

    if (!m_hasTiles)
    {
        bool deblStopped = !IsNeedDeblocking();
        for (Ipp32s i = 0; i < m_SliceCount; i ++)
        {
            H265Slice *slice = m_pSliceQueue[i];

            slice->processInfo.maxCU = slice->m_iMaxMB;

            if (slice->GetSliceHeader()->slice_deblocking_filter_disabled_flag)
            {
                slice->processInfo.m_curCUToProcess[DEB_PROCESS_ID] = slice->m_iMaxMB;
                if (!deblStopped)
                    m_curCUToProcess[DEB_PROCESS_ID] = slice->m_iMaxMB;
            }else
                deblStopped = true;
        }
    }
}
#endif

bool H265DecoderFrameInfo::IsCompleted() const
{
    if (GetStatus() == H265DecoderFrameInfo::STATUS_COMPLETED)
        return true;

#ifndef MFX_VA
    if (!m_pFrame->getCD())
        return false;

    Ipp32s maxCUAddr = m_pFrame->getCD()->m_NumCUsInFrame;
    for (Ipp32s i = 0; i < LAST_PROCESS_ID; i++)
    {
        if (m_curCUToProcess[i] < maxCUAddr)
            return false;
    }

    return true;
#else
    return false;
#endif
}

void H265DecoderFrameInfo::Reset()
{
    Free();

    m_hasTiles = false;

#ifndef MFX_VA
    memset(m_curCUToProcess, 0, sizeof(m_curCUToProcess));
    memset(m_processInProgress, 0, sizeof(m_processInProgress));
    m_tilesThreadingInfo.clear();
#endif

    m_isNeedDeblocking = false;
    m_isNeedSAO = false;

    m_isIntraAU = true;
    m_hasDependentSliceSegments = false;
    m_WA_different_disable_deblocking = false;

    m_nextAU = 0;
    m_prevAU = 0;
    m_refAU = 0;

    m_Status = STATUS_NONE;
    m_prepared = 0;
    m_IsIDR = false;

    if (m_sps)
    {
        m_sps->DecrementReference();
        m_sps = 0;
    }
}

void H265DecoderFrameInfo::Free()
{
    size_t count = m_pSliceQueue.size();
    for (size_t i = 0; i < count; i ++)
    {
        H265Slice * pCurSlice = m_pSliceQueue[i];
        pCurSlice->Release();
        pCurSlice->DecrementReference();
    }

    m_SliceCount = 0;

    m_pSliceQueue.clear();
    m_prepared = 0;
}

void H265DecoderFrameInfo::RemoveSlice(Ipp32s num)
{
    H265Slice * pCurSlice = GetSlice(num);

    if (!pCurSlice) // nothing to do
        return;

    for (Ipp32s i = num; i < m_SliceCount - 1; i++)
    {
        m_pSliceQueue[i] = m_pSliceQueue[i + 1];
    }

    m_SliceCount--;
    m_pSliceQueue[m_SliceCount] = pCurSlice;
}

// Function works with a list of slices sorted by slice_segment_address
void H265DecoderFrameInfo::EliminateErrors()
{
    if (!GetSlice(0))
        return;

    // Remove dependent slices without a corresponding independent slice
    for (Ipp32u sliceId = 0; sliceId < GetSliceCount(); sliceId++)
    {
        H265Slice * slice = GetSlice(sliceId);

        if (slice->GetSliceHeader()->dependent_slice_segment_flag)
        {
            RemoveSlice(sliceId);
            sliceId = Ipp32u(-1);
            continue;
        }
        else
            break;
    }

    {
        // HEVC 7.4.7.1 General slice segment header semantics
        H265Slice *baseSlice = GetSlice(0); // after the for() loop above ,the first slice is treated as 'base' slice

        bool bIndepSliceMissing = false;
        for (Ipp32u sliceId = 1; sliceId < GetSliceCount(); sliceId++)
        {
            H265SliceHeader *sliceHeader = GetSlice(sliceId)->GetSliceHeader();

            if (!sliceHeader->dependent_slice_segment_flag)
                bIndepSliceMissing = false;

            bool bSpecViolation = sliceHeader->slice_temporal_mvp_enabled_flag !=
                  baseSlice->GetSliceHeader()->slice_temporal_mvp_enabled_flag;

            bool bRemoveDependent = bIndepSliceMissing && sliceHeader->dependent_slice_segment_flag;

            if (bSpecViolation || bRemoveDependent)
            {
                RemoveSlice(sliceId);
                sliceId--;
                if (false == bIndepSliceMissing)
                    bIndepSliceMissing = !sliceHeader->dependent_slice_segment_flag;
            }
        }
    }

    // Remove slices with duplicated slice_segment_address syntax
    for (Ipp32u sliceId = 0; sliceId < GetSliceCount(); sliceId++)
    {
        H265Slice * slice = GetSlice(sliceId);
        H265Slice * nextSlice = GetSlice(sliceId + 1);

        if (!nextSlice)
            break;

        if (slice->GetFirstMB() == slice->GetMaxMB())
        {
            Ipp32u sliceIdToRemove;

            // Heuristic logic:
            if (slice->GetSliceHeader()->dependent_slice_segment_flag && !nextSlice->GetSliceHeader()->dependent_slice_segment_flag)
            {
                // dependent slices are prone to errors
                sliceIdToRemove = sliceId;
            }
            else
            {
                // among two independent or dependent slices, prefer to keep the first slice
                sliceIdToRemove = sliceId + 1;
            }
            RemoveSlice(sliceIdToRemove);
            sliceId = Ipp32u(-1);
            continue;
        }
    }
}

void H265DecoderFrameInfo::EliminateASO()
{
    static Ipp32s MAX_MB_NUMBER = 0x7fffffff;

    if (!GetSlice(0))
        return;

    Ipp32u count = GetSliceCount();
    for (Ipp32u sliceId = 0; sliceId < count; sliceId++)
    {
        H265Slice * curSlice = GetSlice(sliceId);
        Ipp32s minFirst = MAX_MB_NUMBER;
        Ipp32u minSlice = 0;

        for (Ipp32u j = sliceId; j < count; j++)
        {
            H265Slice * slice = GetSlice(j);
            if (slice->GetFirstMB() < curSlice->GetFirstMB() && minFirst > slice->GetFirstMB())
            {
                minFirst = slice->GetFirstMB();
                minSlice = j;
            }
        }

        if (minFirst != MAX_MB_NUMBER)
        {
            H265Slice * temp = m_pSliceQueue[sliceId];
            m_pSliceQueue[sliceId] = m_pSliceQueue[minSlice];
            m_pSliceQueue[minSlice] = temp;
        }
    }

    for (Ipp32u sliceId = 0; sliceId < count; sliceId++)
    {
        H265Slice * slice = GetSlice(sliceId);
        H265Slice * nextSlice = GetSlice(sliceId + 1);

        if (!nextSlice)
            break;

        slice->SetMaxMB(nextSlice->GetFirstMB());
    }
}

} // namespace UMC_HEVC_DECODER
#endif // UMC_ENABLE_H265_VIDEO_DECODER
