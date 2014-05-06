/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2014 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"

#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#include "umc_h265_frame_info.h"

namespace UMC_HEVC_DECODER
{
// Initialize tiles and slices threading information
void H265DecoderFrameInfo::FillTileInfo()
{
    H265Slice * slice = GetAnySlice();
    const H265PicParamSet * pps = slice->GetPicParam();
    Ipp32u tilesCount = pps->num_tile_columns*pps->num_tile_rows;
    m_tilesThreadingInfo.resize(tilesCount);

    for (Ipp32u i = 0; i < tilesCount; i++)
    {
        TileThreadingInfo & info = m_tilesThreadingInfo[i];
        info.firstCUAddr = m_pFrame->getCD()->GetInverseCUOrderMap(pps->tilesInfo[i].firstCUAddr);
        info.processInfo.Initialize(m_tilesThreadingInfo[i].firstCUAddr, pps->tilesInfo[i].width);
        info.m_maxCUToProcess = m_pFrame->getCD()->GetInverseCUOrderMap(pps->tilesInfo[i].endCUAddr);
        info.m_context = 0;
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

bool H265DecoderFrameInfo::IsCompleted() const
{
    if (GetStatus() == H265DecoderFrameInfo::STATUS_COMPLETED)
        return true;

    if (!m_pFrame->getCD())
        return false;

    Ipp32s maxCUAddr = m_pFrame->getCD()->m_NumCUsInFrame;
    for (Ipp32s i = 0; i < LAST_PROCESS_ID; i++)
    {
        if (m_curCUToProcess[i] < maxCUAddr)
            return false;
    }

    return true;
}

void H265DecoderFrameInfo::Reset()
{
    Free();

    m_hasTiles = false;
    memset(m_curCUToProcess, 0, sizeof(m_curCUToProcess));
    memset(m_processInProgress, 0, sizeof(m_processInProgress));
    m_tilesThreadingInfo.clear();

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

void H265DecoderFrameInfo::SkipDeblocking()
{
    m_isNeedDeblocking = false;

    for (Ipp32s i = 0; i < m_SliceCount; i ++)
    {
        H265Slice *pSlice = m_pSliceQueue[i];
        pSlice->processInfo.m_curCUToProcess[DEB_PROCESS_ID] = pSlice->m_iMaxMB;
        pSlice->GetSliceHeader()->slice_deblocking_filter_disabled_flag = true;
    }

    m_curCUToProcess[DEB_PROCESS_ID] = m_pFrame->getCD()->m_NumCUsInFrame;
}

void H265DecoderFrameInfo::SkipSAO()
{
    m_isNeedSAO = false;

    for (Ipp32s i = 0; i < m_SliceCount; i ++)
    {
        H265Slice *pSlice = m_pSliceQueue[i];
        pSlice->processInfo.m_curCUToProcess[SAO_PROCESS_ID] = pSlice->m_iMaxMB;
    }

    m_curCUToProcess[SAO_PROCESS_ID] = m_pFrame->getCD()->m_NumCUsInFrame;
}

} // namespace UMC_HEVC_DECODER
#endif // UMC_ENABLE_H265_VIDEO_DECODER
