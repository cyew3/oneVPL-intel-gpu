//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2016 Intel Corporation. All Rights Reserved.
//

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

    // Searches DPB for a reusable frame with biggest POC
    H265DecoderFrame * GetOldestDisposable();

    // Returns whether DPB contains frames which may be reused
    bool IsDisposableExist();

    // Returns whether DPB contains frames which may be reused after asynchronous decoding finishes
    bool IsAlmostDisposableExist();

    // Returns first reusable frame in DPB
    H265DecoderFrame *GetDisposable(void);

    // Marks all frames as not used as reference frames.
    void removeAllRef();

    // Increase ref pic list reset count except for one frame
    void IncreaseRefPicListResetCount(H265DecoderFrame *excludeFrame);

    // Searches DPB for a short term reference frame with specified POC
    H265DecoderFrame *findShortRefPic(Ipp32s picPOC);

    // Searches DPB for a long term reference frame with specified POC
    H265DecoderFrame *findLongTermRefPic(const H265DecoderFrame *excludeFrame, Ipp32s picPOC, Ipp32u bitsForPOC, bool isUseMask);

    // Returns the number of frames in DPB
    Ipp32u countAllFrames();

    // Return number of active short and long term reference frames.
    void countActiveRefs(Ipp32u &numShortTerm, Ipp32u &numLongTerm);

    // Search through the list for the oldest displayable frame.
    H265DecoderFrame *findOldestDisplayable(Ipp32s dbpSize);

    void calculateInfoForDisplay(Ipp32u &countDisplayable, Ipp32u &countDPBFullness, Ipp32s &maxUID);

    // Try to find a frame closest to specified for error recovery
    H265DecoderFrame * FindClosest(H265DecoderFrame * pFrame);

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

    // Debug print
    void DebugPrint();
    // Debug print
    void printDPB();

protected:
    Ipp32s m_dpbSize;
};

} // end namespace UMC_HEVC_DECODER

#endif // __UMC_H265_FRAME_LIST_H__
#endif // UMC_ENABLE_H265_VIDEO_DECODER
