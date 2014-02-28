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

#ifndef __UMC_H265_FRAME_INFO_H
#define __UMC_H265_FRAME_INFO_H

#include <vector>
#include "umc_h265_frame.h"
#include "umc_h265_slice_decoding.h"

namespace UMC_HEVC_DECODER
{
class H265DecoderFrame;

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
        , m_SliceCount(0)
        , m_pObjHeap(pObjHeap)
        , m_sps(0)
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
        m_refPicList.resize(pSlice->GetSliceNum() + 1);

        m_pSliceQueue.push_back(pSlice);
        m_SliceCount++;

        const H265SliceHeader &sliceHeader = *(pSlice->GetSliceHeader());

        m_isIntraAU = m_isIntraAU && (sliceHeader.slice_type == I_SLICE);
        m_hasDependentSliceSegments = m_hasDependentSliceSegments || sliceHeader.dependent_slice_segment_flag;
        m_isNeedDeblocking = m_isNeedDeblocking || (!sliceHeader.slice_deblocking_filter_disabled_flag);
        m_isNeedSAO = m_isNeedSAO || !pSlice->m_bSAOed;
        m_hasTiles = pSlice->GetPicParam()->getNumTiles() > 1;

        m_WA_diffrent_disable_deblocking = m_WA_diffrent_disable_deblocking || (sliceHeader.slice_deblocking_filter_disabled_flag != m_pSliceQueue[0]->GetSliceHeader()->slice_deblocking_filter_disabled_flag);

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

    void Reset()
    {
        Free();

        m_hasTiles = false;
        m_decAddrReady = 0;
        m_recAddrReady = 0;
        m_curMBToSAO = 0;
        m_saoInProcess = 0;

        m_isNeedDeblocking = false;
        m_isNeedSAO = false;

        m_isIntraAU = true;
        m_hasDependentSliceSegments = false;
        m_WA_diffrent_disable_deblocking = false;

        m_NextAU = 0;
        m_PrevAU = 0;

        m_Status = STATUS_NONE;
        m_prepared = 0;

        if (m_sps)
        {
            m_sps->DecrementReference();
            m_sps = 0;
        }
    }

    void SetStatus(FillnessStatus status)
    {
        m_Status = status;
    }

    FillnessStatus GetStatus () const
    {
        return m_Status;
    }

    void Free()
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

    void RemoveSlice(Ipp32s num)
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

    bool IsNeedDeblocking() const
    {
        return m_isNeedDeblocking;
    }

    bool IsNeedSAO() const
    {
        return m_isNeedSAO;
    }

    void SkipDeblocking()
    {
        m_isNeedDeblocking = false;

        for (Ipp32s i = 0; i < m_SliceCount; i ++)
        {
            H265Slice *pSlice = m_pSliceQueue[i];

            pSlice->m_bDeblocked = true;
            pSlice->m_processVacant[DEB_PROCESS_ID] = 0;
            pSlice->m_curMBToProcess[DEB_PROCESS_ID] = pSlice->m_iMaxMB;
            pSlice->GetSliceHeader()->slice_deblocking_filter_disabled_flag = true;
        }
    }

    void SkipSAO()
    {
        m_isNeedSAO = false;

        for (Ipp32s i = 0; i < m_SliceCount; i ++)
        {
            H265Slice *pSlice = m_pSliceQueue[i];
            pSlice->m_bSAOed = true;
            pSlice->m_processVacant[SAO_PROCESS_ID] = 0;
            pSlice->m_curMBToProcess[SAO_PROCESS_ID] = pSlice->m_iMaxMB;
        }
    }

    bool IsCompleted() const
    {
        if (GetStatus() == H265DecoderFrameInfo::STATUS_COMPLETED)
            return true;

        for (Ipp32s i = 0; i < m_SliceCount; i ++)
        {
            const H265Slice *pSlice = m_pSliceQueue[i];

            if (!pSlice->m_bDecoded || !pSlice->m_bDeblocked || !pSlice->m_bSAOed)
                return false;
        }

        return true;
    }

    bool IsIntraAU() const
    {
        return m_isIntraAU;
    }

    bool HasDependentSliceSegments() const
    {
        return m_hasDependentSliceSegments;
    }

    bool IsNeedWorkAroundForDeblocking() const
    {
        return m_WA_diffrent_disable_deblocking;
    }

    H265_FORCEINLINE H265DecoderRefPicList* GetRefPicList(Ipp32u sliceNumber, Ipp32s list)
    {
        VM_ASSERT(list <= REF_PIC_LIST_1 && list >= 0);

        if (sliceNumber >= m_refPicList.size())
        {
            return 0;
        }

        return &m_refPicList[sliceNumber].m_refPicList[list];
    };

    bool CheckReferenceFrameError()
    {
        Ipp32u checkedErrorMask = UMC::ERROR_FRAME_MINOR | UMC::ERROR_FRAME_MAJOR | UMC::ERROR_FRAME_REFERENCE_FRAME;
        for (size_t i = 0; i < m_refPicList.size(); i ++)
        {
            H265DecoderRefPicList* list = &m_refPicList[i].m_refPicList[REF_PIC_LIST_0];
            for (size_t k = 0; list->m_refPicList[k].refFrame; k++)
            {
                if (list->m_refPicList[k].refFrame->GetError() & checkedErrorMask)
                    return true;
            }

            list = &m_refPicList[i].m_refPicList[REF_PIC_LIST_1];
            for (size_t k = 0; list->m_refPicList[k].refFrame; k++)
            {
                if (list->m_refPicList[k].refFrame->GetError() & checkedErrorMask)
                    return true;
            }
        }

        return false;
    }

    H265DecoderFrameInfo * GetNextAU() {return m_NextAU;}
    H265DecoderFrameInfo * GetPrevAU() {return m_PrevAU;}

    void SetNextAU(H265DecoderFrameInfo *au) {m_NextAU = au;}
    void SetPrevAU(H265DecoderFrameInfo *au) {m_PrevAU = au;}

    Ipp32s m_decAddrReady;
    Ipp32s m_recAddrReady;
    Ipp32s m_curMBToSAO;
    Ipp32s m_saoInProcess;
    bool   m_hasTiles;

    H265DecoderFrame * m_pFrame;
    Ipp32s m_prepared;

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

    bool m_WA_diffrent_disable_deblocking;

    H265DecoderFrameInfo *m_NextAU;
    H265DecoderFrameInfo *m_PrevAU;

    struct H264DecoderRefPicListPair
    {
    public:
        H265DecoderRefPicList m_refPicList[2];
    };

    // ML: OPT: TODO: std::vector<> results with relatively slow access code
    std::vector<H264DecoderRefPicListPair> m_refPicList;

    class FakeFrameInitializer
    {
    public:
        FakeFrameInitializer();
    };

    static FakeFrameInitializer g_FakeFrameInitializer;
};

#if defined(_WIN32) || defined(_WIN64)
H265_FORCEINLINE H265DecoderRefPicList* H265DecoderFrame::GetRefPicList(Ipp32s sliceNumber, Ipp32s list) const
{
    return GetAU()->GetRefPicList(sliceNumber, list);
}   // RefPicList. Returns pointer to start of specified ref pic list.
#endif

} // namespace UMC_HEVC_DECODER

#endif // __UMC_H264_FRAME_INFO_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
