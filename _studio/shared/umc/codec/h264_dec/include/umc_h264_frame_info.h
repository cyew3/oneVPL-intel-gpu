//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2018 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#ifndef __UMC_H264_FRAME_INFO_H
#define __UMC_H264_FRAME_INFO_H

#include <vector>

namespace UMC
{

class H264DecoderFrame;

extern H264DecoderFrame g_GlobalFakeFrame;

// Struct containing list 0 and list 1 reference picture lists for one slice.
// Length is plus 1 to provide for null termination.
class H264DecoderRefPicList
{
public:
    H264DecoderFrame **m_RefPicList;
    ReferenceFlags    *m_Flags;

    H264DecoderRefPicList()
    {
        memset(this, 0, sizeof(H264DecoderRefPicList));

        m_RefPicList = &(m_refPicList1[1]);
        m_Flags = &(m_flags1[1]);
        m_flags1[0].field = 0;
        m_flags1[0].isShortReference = 1;

        m_refPicList1[0] = &g_GlobalFakeFrame;
    }

    H264DecoderRefPicList(const H264DecoderRefPicList& copy)
    {
        m_RefPicList = &(m_refPicList1[1]);
        m_Flags = &(m_flags1[1]);

        MFX_INTERNAL_CPY(&m_refPicList1, &copy.m_refPicList1, sizeof(m_refPicList1));
        MFX_INTERNAL_CPY(&m_flags1, &copy.m_flags1, sizeof(m_flags1));
    }

    H264DecoderRefPicList& operator=(const H264DecoderRefPicList & copy)
    {
        MFX_INTERNAL_CPY(&m_refPicList1, &copy.m_refPicList1, sizeof(m_refPicList1));
        MFX_INTERNAL_CPY(&m_flags1, &copy.m_flags1, sizeof(m_flags1));
        return *this;
    }

private:
    H264DecoderFrame *m_refPicList1[MAX_NUM_REF_FRAMES + 3];
    ReferenceFlags    m_flags1[MAX_NUM_REF_FRAMES + 3];
};

class H264DecoderLayer
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

    H264DecoderLayer()
    {
        m_IsBottomField = false;
        m_IsSliceGroups = 0;
    }

    virtual ~H264DecoderLayer()
    {
    }

protected:

    std::vector<H264Slice*> m_pSliceQueue;

    bool m_IsSliceGroups;
    bool m_IsBottomField;

    struct H264DecoderRefPicListPair
    {
    public:
        H264DecoderRefPicList m_refPicList[2];
    };

    std::vector<H264DecoderRefPicListPair> m_refPicList;
};

class H264DecoderFrameInfo : public H264DecoderLayer
{
public:

    H264DecoderFrameInfo(H264DecoderFrame * pFrame,  H264_Heap_Objects * pObjHeap)
        : m_pFrame(pFrame)
        , m_prepared(0)
        , m_SliceCount(0)
        , m_pObjHeap(pObjHeap)
        , decRefPicMarking()
    {
        Reset();
    }

    virtual ~H264DecoderFrameInfo()
    {
    }

    bool IsSliceGroups() const
    {
        return m_IsSliceGroups;
    }

    bool IsField() const
    {
        return m_IsField;
    }

    void AddSlice(H264Slice * pSlice)
    {
        if (!m_SliceCount)
        {
            FillDecRefPicMarking(pSlice);  // on first slice
        }

        m_refPicList.resize(pSlice->GetSliceNum() + 1);

        m_pSliceQueue.push_back(pSlice);
        m_SliceCount++;

        const H264SliceHeader &sliceHeader = *(pSlice->GetSliceHeader());
        m_IsReferenceAU = m_IsReferenceAU || (sliceHeader.nal_ref_idc != 0);
        m_IsBottomField = sliceHeader.bottom_field_flag != 0;

        if (sliceHeader.nal_ext.extension_present && (0 == sliceHeader.nal_ext.svc_extension_flag) &&
            (sliceHeader.nal_ext.mvc.inter_view_flag))
        {
            m_IsReferenceAU = true;
        }

        m_IsIntraAU = m_IsIntraAU && (sliceHeader.slice_type == INTRASLICE);
        m_IsField   = !!sliceHeader.field_pic_flag;
        m_IsIDR     = sliceHeader.IdrPicFlag != 0;
        m_isBExist  = m_isBExist || (sliceHeader.slice_type == BPREDSLICE);
        m_isPExist  = m_isPExist || (sliceHeader.slice_type == PREDSLICE);

        m_IsNeedDeblocking = m_IsNeedDeblocking ||
            (sliceHeader.disable_deblocking_filter_idc != DEBLOCK_FILTER_OFF);
        m_IsSliceGroups = pSlice->IsSliceGroups();
    }

    const H264SEIPayLoadBase::SEIMessages::DecRefPicMarkingRepetition *GetDecRefPicMarking() const
    {
        return &decRefPicMarking;
    }

    void FillDecRefPicMarking(H264Slice * pSlice)
    {
        H264SliceHeader *hdr = pSlice->GetSliceHeader();
        decRefPicMarking.original_idr_flag          = (Ipp8u)hdr->IdrPicFlag;
        decRefPicMarking.original_field_pic_flag    = hdr->field_pic_flag;
        decRefPicMarking.original_bottom_field_flag = hdr->bottom_field_flag;
        decRefPicMarking.original_frame_num         = (Ipp8u)hdr->frame_num;
        decRefPicMarking.long_term_reference_flag   = hdr->long_term_reference_flag;
        MFX_INTERNAL_CPY(&decRefPicMarking.adaptiveMarkingInfo, pSlice->GetAdaptiveMarkingInfo(), sizeof(AdaptiveMarkingInfo));
    }

    Ipp32u GetSliceCount() const
    {
        return m_SliceCount;
    }

    H264Slice* GetAnySlice() const
    {
        H264Slice* slice = GetSlice(0);
        if (!slice)
            throw h264_exception(UMC_ERR_FAILED);
        return slice;
    }

    H264Slice* GetSlice(Ipp32s num) const
    {
        if (num < 0 || num >= m_SliceCount)
            return 0;
        return m_pSliceQueue[num];
    }

    void SwapSlices(Ipp32u pos1, Ipp32u pos2)
    {
        H264Slice* slice = m_pSliceQueue[pos2];
        m_pSliceQueue[pos2] = m_pSliceQueue[pos1];
        m_pSliceQueue[pos1] = slice;
    }

    H264Slice* GetSliceByNumber(Ipp32s num) const
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
        m_refPicList.clear();

        m_iDecMBReady = 0;
        m_iRecMBReady = 0;
        m_IsNeedDeblocking = false;

        m_IsReferenceAU = false;
        m_IsIntraAU     = true;
        m_IsField       = false;

        m_IsIDR         = false;
        m_isPExist      = false;
        m_isBExist      = false;

        m_IsBottomField = false;

        m_NextAU = 0;
        m_PrevAU = 0;
        m_RefAU = 0;
        m_IsSliceGroups = false;

        m_Status = STATUS_NONE;
        m_prepared = 0;
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
            H264Slice * pCurSlice = m_pSliceQueue[i];
            pCurSlice->Release();
            pCurSlice->DecrementReference();
        }

        m_SliceCount = 0;
        m_pSliceQueue.clear();
        m_prepared = 0;
    }

    void RemoveSlice(Ipp32s num)
    {
        H264Slice * pCurSlice = GetSlice(num);

        if (!pCurSlice) // nothing to do
            return;

        for (Ipp32s i = num; i < m_SliceCount - 1; i++)
        {
            m_pSliceQueue[i] = m_pSliceQueue[i + 1];
        }

        m_SliceCount--;
        m_pSliceQueue[m_SliceCount] = pCurSlice;
    }

    bool IsNeedDeblocking () const
    {
        return m_IsNeedDeblocking;
    }

    void SkipDeblocking()
    {
        m_IsNeedDeblocking = false;

        for (Ipp32s i = 0; i < m_SliceCount; i ++)
        {
            H264Slice *pSlice = m_pSliceQueue[i];

            pSlice->m_bDeblocked = true;
            pSlice->GetSliceHeader()->disable_deblocking_filter_idc = DEBLOCK_FILTER_OFF;
        }
    }

    bool IsCompleted() const
    {
        if (GetStatus() == H264DecoderFrameInfo::STATUS_COMPLETED)
            return true;

        for (Ipp32s i = 0; i < m_SliceCount; i ++)
        {
            const H264Slice *pSlice = m_pSliceQueue[i];

            if (!pSlice->m_bDecoded || !pSlice->m_bDeblocked)
                return false;
        }

        return true;
    }

    bool IsBottom() const
    {
        return m_IsBottomField;
    }

    bool IsIntraAU() const
    {
        return m_IsIntraAU;
    }

    bool IsReference() const
    {
        return m_IsReferenceAU;
    }

    bool IsInterviewReference() const
    {
        H264Slice * slice = GetSlice(0);
        if (!slice)
            return false;

        return ( /*slice->GetSliceHeader()->nal_ext.extension_present && */ (0 == slice->GetSliceHeader()->nal_ext.svc_extension_flag) &&
            slice->GetSliceHeader()->nal_ext.mvc.inter_view_flag);
    }

    void SetReference(bool isReference)
    {
        m_IsReferenceAU = isReference;
    }

    H264DecoderRefPicList* GetRefPicList(Ipp32u sliceNumber, Ipp32s list)
    {
        VM_ASSERT(list <= LIST_1 && list >= 0);

        if (sliceNumber >= m_refPicList.size())
        {
            return 0;
        }

        return &m_refPicList[sliceNumber].m_refPicList[list];
    };

    bool CheckReferenceFrameError();

    H264DecoderFrameInfo * GetNextAU() {return m_NextAU;}
    H264DecoderFrameInfo * GetPrevAU() {return m_PrevAU;}
    H264DecoderFrameInfo * GetRefAU() {return m_RefAU;}

    void SetNextAU(H264DecoderFrameInfo *au) {m_NextAU = au;}
    void SetPrevAU(H264DecoderFrameInfo *au) {m_PrevAU = au;}
    void SetRefAU(H264DecoderFrameInfo *au) {m_RefAU = au;}

    Ipp32s m_iDecMBReady;
    Ipp32s m_iRecMBReady;
    H264DecoderFrame * m_pFrame;
    Ipp32s m_prepared;

    bool m_isBExist;
    bool m_isPExist;
    bool m_IsIDR;

    void IncreaseRefPicList(size_t newSize)  // DEBUG !!! temporal solution
    {
        m_refPicList.resize(newSize);
    }

private:

    FillnessStatus m_Status;

    Ipp32s m_SliceCount;

    H264_Heap_Objects * m_pObjHeap;
    bool m_IsNeedDeblocking;

    bool m_IsReferenceAU;
    bool m_IsIntraAU;
    bool m_IsField;

    H264DecoderFrameInfo *m_NextAU;
    H264DecoderFrameInfo *m_RefAU;
    H264DecoderFrameInfo *m_PrevAU;

    H264SEIPayLoadBase::SEIMessages::DecRefPicMarkingRepetition decRefPicMarking;

    class FakeFrameInitializer
    {
    public:
        FakeFrameInitializer();
    };

    static FakeFrameInitializer g_FakeFrameInitializer;
};

} // namespace UMC

#endif // __UMC_H264_FRAME_INFO_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
