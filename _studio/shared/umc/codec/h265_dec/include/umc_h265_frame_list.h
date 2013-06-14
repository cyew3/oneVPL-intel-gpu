/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2013 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef __UMC_H265_FRAME_LIST_H__
#define __UMC_H265_FRAME_LIST_H__

#include "umc_h265_frame.h"

namespace UMC_HEVC_DECODER
{

class H265DecoderFrameList
{
public:
    // Default constructor
    H265DecoderFrameList(void);
    // Destructor
    virtual
    ~H265DecoderFrameList(void);

    H265DecoderFrame   *head() { return m_pHead; }
    H265DecoderFrame   *tail() { return m_pTail; }

    const H265DecoderFrame   *head() const { return m_pHead; }
    const H265DecoderFrame   *tail() const { return m_pTail; }

    bool isEmpty() { return !m_pHead; }

    void append(H265DecoderFrame *pFrame);
    // Append the given frame to our tail

    Ipp32s GetFreeIndex()
    {
        for(Ipp32s i = 0; i < 128; i++)
        {
            H265DecoderFrame *pFrm;

            for (pFrm = head(); pFrm && pFrm->m_index != i; pFrm = pFrm->future())
            {}

            if(pFrm == NULL)
            {
                return i;
            }
        }

        VM_ASSERT(false);
        return -1;
    };

protected:

    // Release object
    void Release(void);

    H265DecoderFrame *m_pHead;                          // (H265DecoderFrame *) pointer to first frame in list
    H265DecoderFrame *m_pTail;                          // (H265DecoderFrame *) pointer to last frame in list
};

class H265DBPList : public H265DecoderFrameList
{
public:

    H265DBPList();

    H265DecoderFrame * GetOldestDisposable();

    H265DecoderFrame * GetLastDisposable();

    bool IsDisposableExist();

    bool IsAlmostDisposableExist();

    H265DecoderFrame *GetDisposable(void);
    // Search through the list for the disposable frame to decode into
    // Move disposable frame to tail

    void removeAllRef();
    // Mark all frames as not used as reference frames.

    void IncreaseRefPicListResetCount(H265DecoderFrame *excludeFrame);
    // Mark all frames as not used as reference frames.

    H265DecoderFrame *findShortRefPic(Ipp32s picPOC);

    H265DecoderFrame *findLongTermRefPic(H265DecoderFrame *excludeFrame, Ipp32s picPOC, Ipp32u bitsForPOC, bool isUseMask);

    Ipp32u countAllFrames();

    void countActiveRefs(Ipp32u &numShortTerm, Ipp32u &numLongTerm);
    // Return number of active Ipp16s and long term reference frames.

    H265DecoderFrame * findFirstDisplayable();

    H265DecoderFrame * findDisplayableByDPBDelay();

    H265DecoderFrame *findOldestDisplayable(Ipp32s dbpSize);
    // Search through the list for the oldest displayable frame.

    Ipp32u countNumDisplayable();
    // Return number of displayable frames.

    void MoveToTail(H265DecoderFrame * pFrame);

    H265DecoderFrame * FindClosest(H265DecoderFrame * pFrame);

    H265DecoderFrame * FindByIndex(Ipp32s index);

    Ipp32s GetDPBSize() const
    {
        return m_dpbSize;
    }

    void SetDPBSize(Ipp32s dpbSize)
    {
        m_dpbSize = dpbSize;
    }

    // Reset the buffer and reset every single frame of it
    void Reset(void);

    void DebugPrint();
    void printDPB();

protected:
    Ipp32s m_dpbSize;
};

} // end namespace UMC_HEVC_DECODER

#endif // __UMC_H264_FRAME_LIST_H__
#endif // UMC_ENABLE_H264_VIDEO_DECODER
