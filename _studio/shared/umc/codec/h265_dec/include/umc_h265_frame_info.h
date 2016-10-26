//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2016 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef __UMC_H265_FRAME_INFO_H
#define __UMC_H265_FRAME_INFO_H

#include <vector>
#include "umc_h265_frame.h"
#include "umc_h265_slice_decoding.h"

namespace UMC_HEVC_DECODER
{
class H265DecoderFrame;

#ifndef MFX_VA
struct TileThreadingInfo
{
    CUProcessInfo processInfo;
    DecodingContext * m_context;
};
#endif

// Collection of slices that constitute one decoder frame
class H265DecoderFrameInfo
{
public:
    enum FillnessStatus
    {
        STATUS_NONE,
        STATUS_NOT_FILLED,
        STATUS_FILLED,
        STATUS_COMPLETED,
        STATUS_STARTED
    };

    H265DecoderFrameInfo(H265DecoderFrame * pFrame, Heap_Objects * pObjHeap)
        : m_pFrame(pFrame)
        , m_prepared(0)
        , m_sps(0)
        , m_SliceCount(0)
        , m_pObjHeap(pObjHeap)
    {
        Reset();
    }

    virtual ~H265DecoderFrameInfo()
    {
    }

    H265Slice *GetAnySlice() const
    {
        H265Slice* slice = GetSlice(0);
        if (!slice)
            throw h265_exception(UMC::UMC_ERR_FAILED);
        return slice;
    }

    const H265SeqParamSet *GetSeqParam() const
    {
        return m_sps;
    }

    void AddSlice(H265Slice * pSlice)
    {
        m_pSliceQueue.push_back(pSlice);
        m_SliceCount++;

        const H265SliceHeader &sliceHeader = *(pSlice->GetSliceHeader());

        m_isIntraAU = m_isIntraAU && (sliceHeader.slice_type == I_SLICE);
        m_IsIDR     = sliceHeader.IdrPicFlag != 0;
        m_hasDependentSliceSegments = m_hasDependentSliceSegments || sliceHeader.dependent_slice_segment_flag;
        m_isNeedDeblocking = m_isNeedDeblocking || (!sliceHeader.slice_deblocking_filter_disabled_flag);
        m_isNeedSAO = m_isNeedSAO || (sliceHeader.slice_sao_luma_flag || sliceHeader.slice_sao_chroma_flag);
        m_hasTiles = pSlice->GetPicParam()->getNumTiles() > 1;

        m_WA_different_disable_deblocking = m_WA_different_disable_deblocking || (sliceHeader.slice_deblocking_filter_disabled_flag != m_pSliceQueue[0]->GetSliceHeader()->slice_deblocking_filter_disabled_flag);

        if (!m_sps)
        {
            m_sps = (H265SeqParamSet *)pSlice->GetSeqParam();
            m_sps->IncrementReference();
        }
    }

    Ipp32u GetSliceCount() const
    {
        return m_SliceCount;
    }

    H265Slice* GetSlice(Ipp32s num) const
    {
        if (num < 0 || num >= m_SliceCount)
            return 0;
        return m_pSliceQueue[num];
    }

    H265Slice* GetSliceByNumber(Ipp32s num) const
    {
        size_t count = m_pSliceQueue.size();
        for (Ipp32u i = 0; i < count; i++)
        {
            if (m_pSliceQueue[i]->GetSliceNum() == num)
                return m_pSliceQueue[i];
        }

        return 0;
    }

    Ipp32s GetPositionByNumber(Ipp32s num) const
    {
        size_t count = m_pSliceQueue.size();
        for (Ipp32u i = 0; i < count; i++)
        {
            if (m_pSliceQueue[i]->GetSliceNum() == num)
                return i;
        }

        return -1;
    }

    void Reset();

    void SetStatus(FillnessStatus status)
    {
        m_Status = status;
    }

    FillnessStatus GetStatus () const
    {
        return m_Status;
    }

    void Free();
    void RemoveSlice(Ipp32s num);

    bool IsNeedDeblocking() const
    {
        return m_isNeedDeblocking;
    }

    bool IsNeedSAO() const
    {
        return m_isNeedSAO;
    }

    bool IsCompleted() const;

    bool IsIntraAU() const
    {
        return m_isIntraAU;
    }

    bool IsReference() const
    {
        return m_pFrame->m_isUsedAsReference;
    }

    bool HasDependentSliceSegments() const
    {
        return m_hasDependentSliceSegments;
    }

    bool IsNeedWorkAroundForDeblocking() const
    {
        return m_WA_different_disable_deblocking;
    }

    // Initialize tiles and slices threading information
    void FillTileInfo();

    // reorder slices and set maxCUAddr
    void EliminateASO();

    H265DecoderFrameInfo * GetNextAU() const {return m_nextAU;}
    H265DecoderFrameInfo * GetPrevAU() const {return m_prevAU;}
    H265DecoderFrameInfo * GetRefAU() const {return m_refAU;}

    void SetNextAU(H265DecoderFrameInfo *au) {m_nextAU = au;}
    void SetPrevAU(H265DecoderFrameInfo *au) {m_prevAU = au;}
    void SetRefAU(H265DecoderFrameInfo *au) {m_refAU = au;}

#ifndef MFX_VA
    std::vector<TileThreadingInfo> m_tilesThreadingInfo;

    Ipp32s m_curCUToProcess[LAST_PROCESS_ID];
    Ipp32s m_processInProgress[LAST_PROCESS_ID];
#endif

    bool   m_hasTiles;

    H265DecoderFrame * m_pFrame;
    Ipp32s m_prepared;
    bool m_IsIDR;

private:

    FillnessStatus m_Status;

    H265SeqParamSet * m_sps;
    std::vector<H265Slice*> m_pSliceQueue;

    Ipp32s m_SliceCount;

    Heap_Objects * m_pObjHeap;
    bool m_isNeedDeblocking;
    bool m_isNeedSAO;

    bool m_isIntraAU;
    bool m_hasDependentSliceSegments;

    bool m_WA_different_disable_deblocking;

    H265DecoderFrameInfo *m_nextAU;
    H265DecoderFrameInfo *m_prevAU;
    H265DecoderFrameInfo *m_refAU;
};

} // namespace UMC_HEVC_DECODER

#endif // __UMC_H265_FRAME_INFO_H
#endif // UMC_ENABLE_H265_VIDEO_DECODER
