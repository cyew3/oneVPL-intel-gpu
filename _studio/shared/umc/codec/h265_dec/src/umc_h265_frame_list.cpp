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

#include "umc_h265_frame_list.h"
#include "umc_h265_dec_debug.h"
#include "umc_h265_task_supplier.h"

namespace UMC_HEVC_DECODER
{

H265DecoderFrameList::H265DecoderFrameList(void)
{
    m_pHead = NULL;
    m_pTail = NULL;
} // H265DecoderFrameList::H265DecoderFrameList(void)

H265DecoderFrameList::~H265DecoderFrameList(void)
{
    Release();

} // H265DecoderFrameList::~H265DecoderFrameList(void)

void H265DecoderFrameList::Release(void)
{
    // destroy frame list
    while (m_pHead)
    {
        H265DecoderFrame *pNext = m_pHead->future();
        delete m_pHead;
        m_pHead = pNext;
    }

    m_pHead = NULL;
    m_pTail = NULL;

} // void H265DecoderFrameList::Release(void)


//////////////////////////////////////////////////////////////////////////////
// append
//   Appends a new decoded frame buffer to the "end" of the linked list
//////////////////////////////////////////////////////////////////////////////
void H265DecoderFrameList::append(H265DecoderFrame *pFrame)
{
    // Error check
    if (!pFrame)
    {
        // Sent in a NULL frame
        return;
    }

    // Has a list been constructed - is their a head?
    if (!m_pHead)
    {
        // Must be the first frame appended
        // Set the head to the current
        m_pHead = pFrame;
        m_pHead->setPrevious(0);
    }

    if (m_pTail)
    {
        // Set the old tail as the previous for the current
        pFrame->setPrevious(m_pTail);

        // Set the old tail's future to the current
        m_pTail->setFuture(pFrame);
    }
    else
    {
        // Must be the first frame appended
        // Set the tail to the current
        m_pTail = pFrame;
    }

    // The current is now the new tail
    m_pTail = pFrame;
    m_pTail->setFuture(0);
    //
}

H265DBPList::H265DBPList()
    : m_dpbSize(0)
{
}

H265DecoderFrame * H265DBPList::GetOldestDisposable(void)
{
    H265DecoderFrame *pOldest = NULL;
    Ipp32s  SmallestPicOrderCnt = 0x7fffffff;    // very large positive
    Ipp32s  LargestRefPicListResetCount = 0;

    for (H265DecoderFrame * pTmp = m_pHead; pTmp; pTmp = pTmp->future())
    {
        if (pTmp->isDisposable())
        {
            if (pTmp->RefPicListResetCount() > LargestRefPicListResetCount)
            {
                pOldest = pTmp;
                SmallestPicOrderCnt = pTmp->PicOrderCnt();
                LargestRefPicListResetCount = pTmp->RefPicListResetCount();
            }
            else if ((pTmp->PicOrderCnt() < SmallestPicOrderCnt) &&
                     (pTmp->RefPicListResetCount() == LargestRefPicListResetCount))
            {
                pOldest = pTmp;
                SmallestPicOrderCnt = pTmp->PicOrderCnt();
            }
        }
    }

    return pOldest;
} // H265DecoderFrame *H265DBPList::GetDisposable(void)

H265DecoderFrame * H265DBPList::GetLastDisposable(void)
{
    H265DecoderFrame *pOldest = NULL;

    for (H265DecoderFrame * pTmp = m_pHead; pTmp; pTmp = pTmp->future())
    {
        if (pTmp->isDisposable())
        {
            pOldest = pTmp;
        }
    }
    return pOldest;
} // H265DecoderFrame *H265DBPList::GetDisposable(void)

void H265DBPList::MoveToTail(H265DecoderFrame * pFrame)
{
    if (m_pHead == m_pTail || pFrame == m_pTail)
    {
        return;
    }

    // move to tail
    if (pFrame == m_pHead)
    {
        m_pHead = pFrame->m_pFutureFrame;
        m_pHead->m_pPreviousFrame = 0;
    }
    else
    {
        pFrame->m_pPreviousFrame->m_pFutureFrame = pFrame->m_pFutureFrame;
        pFrame->m_pFutureFrame->m_pPreviousFrame = pFrame->m_pPreviousFrame;
    }

    m_pTail->m_pFutureFrame = pFrame;
    pFrame->m_pPreviousFrame = m_pTail;
    m_pTail = pFrame;
    pFrame->m_pFutureFrame = 0;
}

bool H265DBPList::IsDisposableExist()
{
    for (H265DecoderFrame * pTmp = m_pHead; pTmp; pTmp = pTmp->future())
    {
        if (pTmp->isDisposable())
        {
            return true;
        }
    }

    return false;
}

bool H265DBPList::IsAlmostDisposableExist()
{
    Ipp32s count = 0;
    for (H265DecoderFrame * pTmp = m_pHead; pTmp; pTmp = pTmp->future())
    {
        count++;
        if (isAlmostDisposable(pTmp))
        {
            return true;
        }
    }

    return count < m_dpbSize;
}

H265DecoderFrame * H265DBPList::GetDisposable(void)
{
    for (H265DecoderFrame * pTmp = m_pHead; pTmp; pTmp = pTmp->future())
    {
        if (pTmp->isDisposable())
        {
            return pTmp;
        }
    }

    // We never found one
    return NULL;
} // H265DecoderFrame *H265DBPList::GetDisposable(void)

H265DecoderFrame * H265DBPList::findDisplayableByDPBDelay(void)
{
    H265DecoderFrame *pCurr = m_pHead;
    H265DecoderFrame *pOldest = NULL;
    Ipp32s  SmallestPicOrderCnt = 0x7fffffff;    // very large positive
    Ipp32s  LargestRefPicListResetCount = 0;
    Ipp32s  uid = 0x7fffffff;

    Ipp32s count = 0;
    while (pCurr)
    {
        if ((pCurr->isDisplayable() || (!pCurr->IsDecoded() && pCurr->IsFullFrame())) && !pCurr->wasOutputted() && !pCurr->m_dpb_output_delay)
        {
            // corresponding frame
            if (pCurr->RefPicListResetCount() > LargestRefPicListResetCount)
            {
                pOldest = pCurr;
                SmallestPicOrderCnt = pCurr->PicOrderCnt();
                LargestRefPicListResetCount = pCurr->RefPicListResetCount();
            }
            else if ((pCurr->PicOrderCnt() <= SmallestPicOrderCnt) &&
                     (pCurr->RefPicListResetCount() == LargestRefPicListResetCount) && pCurr->m_UID < uid)
            {
                pOldest = pCurr;
                SmallestPicOrderCnt = pCurr->PicOrderCnt();
            }

            if (!pOldest->IsFrameExist() && pCurr->IsFrameExist())
            {
                if (pCurr->PicOrderCnt() == SmallestPicOrderCnt &&
                    pCurr->RefPicListResetCount() == LargestRefPicListResetCount)
                    pOldest = pCurr;
            }

            //if (count == dbpSize + 1)
              //  break;
            count++;
        }

        pCurr = pCurr->future();
    }

    // may be OK if NULL
    return pOldest;
}

///////////////////////////////////////////////////////////////////////////////
// findOldestDisplayable
// Search through the list for the oldest displayable frame. It must be
// not disposable, not outputted, and have smallest PicOrderCnt.
///////////////////////////////////////////////////////////////////////////////
H265DecoderFrame * H265DBPList::findOldestDisplayable(Ipp32s /*dbpSize*/ )
{
    H265DecoderFrame *pCurr = m_pHead;
    H265DecoderFrame *pOldest = NULL;
    Ipp32s  SmallestPicOrderCnt = 0x7fffffff;    // very large positive
    Ipp32s  LargestRefPicListResetCount = 0;
    Ipp32s  uid = 0x7fffffff;

    Ipp32s count = 0;
    while (pCurr)
    {
        if ((pCurr->isDisplayable() || (!pCurr->IsDecoded() && pCurr->IsFullFrame())) && !pCurr->wasOutputted())
        {
            // corresponding frame
            if (pCurr->RefPicListResetCount() > LargestRefPicListResetCount)
            {
                pOldest = pCurr;
                SmallestPicOrderCnt = pCurr->PicOrderCnt();
                LargestRefPicListResetCount = pCurr->RefPicListResetCount();
            }
            else if ((pCurr->PicOrderCnt() <= SmallestPicOrderCnt) &&
                     (pCurr->RefPicListResetCount() == LargestRefPicListResetCount) && pCurr->m_UID < uid)
            {
                pOldest = pCurr;
                SmallestPicOrderCnt = pCurr->PicOrderCnt();
            }

            if (!pOldest->IsFrameExist() && pCurr->IsFrameExist())
            {
                if (pCurr->PicOrderCnt() == SmallestPicOrderCnt &&
                    pCurr->RefPicListResetCount() == LargestRefPicListResetCount)
                    pOldest = pCurr;
            }

            //if (count == dbpSize + 1)
              //  break;
            count++;
        }

        pCurr = pCurr->future();
    }

    // may be OK if NULL
    return pOldest;

}    // findOldestDisplayable

H265DecoderFrame * H265DBPList::findFirstDisplayable()
{
    VM_ASSERT(0);
    H265DecoderFrame *pCurr = m_pHead;

    while (pCurr)
    {
        if ((pCurr->isDisplayable() || !pCurr->IsDecoded()) && !pCurr->wasOutputted())
        {
            return pCurr;
        }

        pCurr = pCurr->future();
    }

    return 0;
}    // findFirstDisplayable

Ipp32u H265DBPList::countAllFrames()
{
    H265DecoderFrame *pCurr = head();
    Ipp32u count = 0;

    while (pCurr)
    {
        count++;
        pCurr = pCurr->future();
    }

    return count;
}

///////////////////////////////////////////////////////////////////////////////
// countNumDisplayable
//  Return number of displayable frames.
///////////////////////////////////////////////////////////////////////////////
Ipp32u H265DBPList::countNumDisplayable()
{
    H265DecoderFrame *pCurr = head();
    Ipp32u NumDisplayable = 0;

    while (pCurr)
    {
        if ((pCurr->isDisplayable() || (!pCurr->IsDecoded() && pCurr->IsFullFrame())) && !pCurr->wasOutputted())
        {
            NumDisplayable++;
        }
        pCurr = pCurr->future();
    }

    return NumDisplayable;
}    // countNumDisplayable

///////////////////////////////////////////////////////////////////////////////
// countActiveRefs
//  Return number of active Ipp16s and long term reference frames.
///////////////////////////////////////////////////////////////////////////////
void H265DBPList::countActiveRefs(Ipp32u &NumShortTerm, Ipp32u &NumLongTerm)
{
    H265DecoderFrame *pCurr = m_pHead;
    NumShortTerm = 0;
    NumLongTerm = 0;

    while (pCurr)
    {
        if (pCurr->isShortTermRef())
            NumShortTerm++;
        else if (pCurr->isLongTermRef())
            NumLongTerm++;
        pCurr = pCurr->future();
    }

}    // countActiveRefs

///////////////////////////////////////////////////////////////////////////////
// removeAllRef
// Marks all frames as not used as reference frames.
///////////////////////////////////////////////////////////////////////////////
void H265DBPList::removeAllRef()
{
    H265DecoderFrame *pCurr = m_pHead;

    while (pCurr)
    {
        if (pCurr->isShortTermRef() || pCurr->isLongTermRef())
        {
            pCurr->SetisLongTermRef(false);
            pCurr->SetisShortTermRef(false);

            if (pCurr && !pCurr->IsFrameExist())
            {
                pCurr->setWasOutputted();
                pCurr->setWasDisplayed();
            }
        }

        pCurr = pCurr->future();
    }

}    // removeAllRef

void H265DBPList::IncreaseRefPicListResetCount(H265DecoderFrame *ExcludeFrame)
{
    H265DecoderFrame *pCurr = m_pHead;

    while (pCurr)
    {
        if (pCurr!=ExcludeFrame)
        {
            pCurr->IncreaseRefPicListResetCount();
        }
        pCurr = pCurr->future();
    }

}    // IncreaseRefPicListResetCount

void H265DBPList::printDPB()
{
    H265DecoderFrame *pCurr = m_pHead;

    DEBUG_PRINT((VM_STRING("DPB: (")));
    while (pCurr)
    {
        DEBUG_PRINT((VM_STRING("POC = %d %p (%d)"), pCurr->PicOrderCnt(), (RefCounter *)pCurr, pCurr->GetRefCounter()));
        pCurr = pCurr->future();
        if (pCurr)
            DEBUG_PRINT((VM_STRING(", ")));
    }
    DEBUG_PRINT((VM_STRING(")\n")));
}

H265DecoderFrame *H265DBPList::findShortRefPic(Ipp32s picPOC)
{
    H265DecoderFrame *pCurr = m_pHead;

    while (pCurr)
    {
        if (pCurr->isShortTermRef() && pCurr->PicOrderCnt() == picPOC)
            break;

        pCurr = pCurr->future();
    }

    return pCurr;
}

H265DecoderFrame *H265DBPList::findLongTermRefPic(H265DecoderFrame *excludeFrame, Ipp32s picPOC, Ipp32u bitsForPOC, bool isUseMask)
{
    H265DecoderFrame *pCurr = m_pHead;
    H265DecoderFrame *pStPic = pCurr;
    Ipp32u POCmask = (1 << bitsForPOC) - 1;

    if (!isUseMask)
        POCmask = 0xffffffff;

    while (pCurr)
    {
        if (pCurr && pCurr != excludeFrame && (pCurr->PicOrderCnt() & POCmask) == (picPOC & POCmask))
        {
            if (pCurr->isLongTermRef())
                return pCurr;
            else
                pStPic = pCurr;
            break;
        }

        pCurr = pCurr->future();
    }

    return pStPic;
} // findLongTermRefPic

H265DecoderFrame * H265DBPList::FindClosest(H265DecoderFrame * pFrame)
{
    Ipp32s originalPOC = pFrame->PicOrderCnt();
    Ipp32s originalResetCount = pFrame->RefPicListResetCount();

    H265DecoderFrame * pOldest = 0;

    Ipp32s  SmallestPicOrderCnt = 0;    // very large positive
    Ipp32s  SmallestRefPicListResetCount = 0x7fffffff;

    for (H265DecoderFrame * pTmp = m_pHead; pTmp; pTmp = pTmp->future())
    {
        if (pTmp->IsSkipped() || pTmp == pFrame || pTmp->GetRefCounter() >= 2)
            continue;

        if (pTmp->m_chroma_format != pFrame->m_chroma_format ||
            pTmp->lumaSize().width != pFrame->lumaSize().width ||
            pTmp->lumaSize().height != pFrame->lumaSize().height)
            continue;

        if (pTmp->RefPicListResetCount() < SmallestRefPicListResetCount)
        {
            pOldest = pTmp;
            SmallestPicOrderCnt = pTmp->PicOrderCnt();
            SmallestRefPicListResetCount = pTmp->RefPicListResetCount();
        }
        else if (pTmp->RefPicListResetCount() == SmallestRefPicListResetCount)
        {
            if (pTmp->RefPicListResetCount() == originalResetCount)
            {
                if (SmallestRefPicListResetCount != originalResetCount)
                {
                    SmallestPicOrderCnt = 0x7fff;
                }

                if (abs(pTmp->PicOrderCnt() - originalPOC) < SmallestPicOrderCnt)
                {
                    pOldest = pTmp;
                    SmallestPicOrderCnt = pTmp->PicOrderCnt();
                    SmallestRefPicListResetCount = pTmp->RefPicListResetCount();
                }
            }
            else
            {
                if (pTmp->PicOrderCnt() > SmallestPicOrderCnt)
                {
                    pOldest = pTmp;
                    SmallestPicOrderCnt = pTmp->PicOrderCnt();
                    SmallestRefPicListResetCount = pTmp->RefPicListResetCount();
                }
            }
        }
    }

    return pOldest;
}

H265DecoderFrame * H265DBPList::FindByIndex(Ipp32s index)
{
    for (H265DecoderFrame * pTmp = m_pHead; pTmp; pTmp = pTmp->future())
    {
        if (pTmp->m_index == index)
            return pTmp;
    }

    return 0;
}

void H265DBPList::Reset(void)
{
    H265DecoderFrame *pFrame ;

    for (pFrame = head(); pFrame; pFrame = pFrame->future())
    {
        pFrame->FreeResources();
    }

    for (pFrame = head(); pFrame; pFrame = pFrame->future())
    {
        pFrame->Reset();
    }
} // void H265DBPList::Reset(void)

void H265DBPList::DebugPrint()
{
#if 1
    Trace(VM_STRING("-==========================================\n"));
    for (H265DecoderFrame * pTmp = m_pHead; pTmp; pTmp = pTmp->future())
    {
        Trace(VM_STRING("\n\nPTR = %p UID - %d POC - %d  - resetcount - %d\n"), (RefCounter *)pTmp, pTmp->m_UID, pTmp->m_PicOrderCnt, pTmp->RefPicListResetCount());
        Trace(VM_STRING("Short - %d \n"), pTmp->isShortTermRef());
        Trace(VM_STRING("Long - %d \n"), pTmp->isLongTermRef());
        Trace(VM_STRING("Busy - %d \n"), pTmp->GetRefCounter());
        Trace(VM_STRING("Skipping_H265 - %d, FrameExist - %d \n"), pTmp->IsSkipped(), pTmp->IsFrameExist());
        Trace(VM_STRING("Disp - %d , wasOutput - %d wasDisplayed - %d\n"), pTmp->isDisplayable(), pTmp->wasOutputted(), pTmp->wasDisplayed());
        Trace(VM_STRING("frame ID - %d \n"), pTmp->GetFrameData()->GetFrameMID());

    }

    Trace(VM_STRING("-==========================================\n"));
    //fflush(stdout);
#endif
}


} // end namespace UMC_HEVC_DECODER
#endif // UMC_ENABLE_H265_VIDEO_DECODER
