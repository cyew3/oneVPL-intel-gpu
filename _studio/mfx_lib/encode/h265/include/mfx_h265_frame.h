//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2012 - 2014 Intel Corporation. All Rights Reserved.
//

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#ifndef __MFX_H265_FRAME_H__
#define __MFX_H265_FRAME_H__

#include "mfx_h265_defs.h"
#include "mfx_h265_ctb.h"

namespace H265Enc {

class H265FrameList;

class H265Frame {
public:
    void *mem;
    H265CUData *cu_data;
    Ipp8u *y;
    Ipp8u *uv;
    Ipp32s width;
    Ipp32s height;
    Ipp32s padding;
    Ipp32s pitch_luma;
    Ipp32s pitch_chroma;
    mfxU64 TimeStamp;

    H265Frame  *m_pPreviousFrame;
    H265Frame  *m_pFutureFrame;
    Ipp32u   m_PicCodType;
    Ipp32s   m_RefPicListResetCount;
    Ipp32s   m_PicOrderCnt;
    Ipp32s   m_EncOrderNum;
    Ipp32u   m_PicOrderCounterAccumulated;
    Ipp32s   m_RPSIndex;
    Ipp32s   m_PGOPIndex;
    Ipp8u    m_isShortTermRef;
    Ipp8u    m_isLongTermRef;
    Ipp8u    m_wasEncoded;
    Ipp8u    m_BpyramidNumFrame;
    Ipp8u    m_BpyramidRefLayers;
    Ipp8u    m_bIsIDRPic;
    Ipp8u    m_isBRef;
    Ipp8u    m_isMarked;

    RefPicList m_refPicList[2];
    Ipp32s m_mapRefIdxL1ToL0[MAX_NUM_REF_IDX];
    Ipp32s m_allRefFramesAreFromThePast;

    H265Frame() {
        m_pPreviousFrame = m_pFutureFrame = NULL;
        mem = NULL;
        y = uv = NULL;
        cu_data = NULL;
        m_PicCodType = 0;
        m_RefPicListResetCount = 0;
        m_PicOrderCnt = 0;
        m_EncOrderNum = 0;
        m_PicOrderCounterAccumulated = 0;
        m_isShortTermRef = 0;
        m_isLongTermRef = 0;
        m_wasEncoded = 0;
        m_BpyramidNumFrame = 0;
        m_bIsIDRPic = 0;
        m_isBRef = 0;
        m_isMarked = 0;
    }

    H265Frame *previous() { return m_pPreviousFrame; }
    H265Frame *future()   { return m_pFutureFrame; }

    mfxStatus swapData(H265Frame *frame) {
/*        if (width != frame->width || height != frame->height ||
            padding != frame->padding || pitch_luma != frame->pitch_luma ||
            pitch_chroma != frame->pitch_chroma)
            return MFX_ERR_UNKNOWN;*/

        void *_mem = mem;
        H265CUData *_cu_data = cu_data;
        Ipp8u *_y = y;
        Ipp8u *_uv = uv;

        mem = frame->mem;
        cu_data = frame->cu_data;
        y = frame->y;
        uv = frame->uv;

        frame->mem = _mem;
        frame->cu_data = _cu_data;
        frame->y = _y;
        frame->uv = _uv;

        return MFX_ERR_NONE;
    }

    void setPrevious(H265Frame *pPrev)
    {
        m_pPreviousFrame = pPrev;
    }
    void setFuture(H265Frame *pFut)
    {
        m_pFutureFrame = pFut;
    }

    Ipp8u wasEncoded()    { return m_wasEncoded; }
    void setWasEncoded() { m_wasEncoded = true; }
    void unsetWasEncoded() { m_wasEncoded = false; }

    bool isDisposable()
    {
        return (!m_isShortTermRef && !m_isLongTermRef && m_wasEncoded);
    }

    Ipp8u isShortTermRef()
    {
        return m_isShortTermRef;
    }

    void SetisShortTermRef()
    {
        m_isShortTermRef = true;
    }

    Ipp32s PicOrderCnt()
    {
        return m_PicOrderCnt;
    }

    void setPicOrderCnt(Ipp32s PicOrderCnt)
    {
        m_PicOrderCnt = PicOrderCnt;
    }

    Ipp32s EncOrderNum()
    {
        return m_EncOrderNum;
    }

    void setEncOrderNum(Ipp32s EncOrderNum)
    {
        m_EncOrderNum = EncOrderNum;
    }

    Ipp8u isLongTermRef()
    {
        return m_isLongTermRef;
    }

    void SetisLongTermRef()
    {
        m_isLongTermRef = true;
    }

    void unSetisShortTermRef()
    {
        m_isShortTermRef = false;
    }

    void unSetisLongTermRef()
    {
        m_isLongTermRef = false;
    }

    void IncreaseRefPicListResetCount()
    {
        m_RefPicListResetCount++;
    }

    void InitRefPicListResetCount()
    {
        m_RefPicListResetCount = 0;
    }

    Ipp32s RefPicListResetCount()
    {
        return m_RefPicListResetCount;
    }

    mfxStatus Create(H265VideoParam *par);
    mfxStatus CopyFrame(mfxFrameSurface1 *surface);
    void doPadding();

    void Destroy();
    void Dump(const vm_char* filename, H265VideoParam *par, H265FrameList *dpb, Ipp32s frame_num);
};

class H265FrameList
{
private:
    // m_pHead points to the first element in the list, and m_pTail
    // points to the last.  m_pHead->previous() and m_pTail->future()
    // are both NULL.
    H265Frame *m_pHead;
    H265Frame *m_pTail;
    H265Frame *m_pCurrent;

public:

    H265FrameList() {
        m_pHead = m_pTail = m_pCurrent = NULL;
    }
    virtual ~H265FrameList();

    H265Frame *head() { return m_pHead; }
    H265Frame *tail() { return m_pTail; }

    bool isEmpty() { return !m_pHead; }

    H265Frame *detachHead();    // Detach the first frame and return a pointer to it,
    void RemoveFrame(H265Frame*);    // Removes selected frame from list
    // or return NULL if the list is empty.

    void append(H265Frame *pFrame);
    H265Frame* InsertFrame(mfxFrameSurface1 *surface,
        H265VideoParam *par);

    // Append the given frame to our tail

    void insertList(H265FrameList &src);
    // Move the given list to the beginning of our list.

    void destroy();

    H265Frame *findNextDisposable();
    // Search through the list for the next disposable frame to decode into

    void insertAtCurrent(H265Frame *pFrame);
    // Inserts a frame immediately after the position pointed to by m_pCurrent

    void resetCurrent(void) { m_pCurrent = m_pTail; }
    // Resets the position of the current to the tail. This allows
    // us to start "before" the head when we wrap around.

    void setCurrent(H265Frame *pFrame) { m_pCurrent = pFrame; }
    // Set the position of the current to pFrame

    void removeAllRef();
    // Mark all frames as not used as reference frames.

    void IncreaseRefPicListResetCount(H265Frame *ExcludeFrame);
    // Mark all frames as not used as reference frames.

    void countActiveRefs(Ipp32u &NumShortTermL0, Ipp32u &NumLongTermL0,
                         Ipp32u &NumShortTermL1, Ipp32u &NumLongTermL1, Ipp32s curPOC);
    void countL1Refs(Ipp32u &NumRefs, Ipp32s curPOC);
    // Return number of active short and long term reference frames.

    H265Frame *findOldestToEncode(H265FrameList* dpb,Ipp32u min_L1_refs,
                                         Ipp8u isByramid = 0, Ipp8u ByramidNextFrameNum = 0);
    // Search through the list for the oldest displayable frame.

    H265Frame *findFrameByPOC(Ipp32s POC);
    void unMarkAll();
    void removeAllUnmarkedRef();
    H265Frame *findMostDistantShortTermRefs(Ipp32s POC);
    H265Frame *findMostDistantLongTermRefs(Ipp32s POC);
};

} // namespace

#endif // __MFX_H265_FRAME_H__

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
