/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2003-2016 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#include <algorithm>
#include "umc_h264_frame.h"
#include "umc_h264_task_supplier.h"
#include "umc_h264_dec_debug.h"

namespace UMC
{

H264DecoderFrame g_GlobalFakeFrame(0, 0);

H264DecoderFrameInfo::FakeFrameInitializer H264DecoderFrameInfo::g_FakeFrameInitializer;

H264DecoderFrameInfo::FakeFrameInitializer::FakeFrameInitializer()
{
    g_GlobalFakeFrame.m_ID[0] = (size_t)-1;
    g_GlobalFakeFrame.m_ID[1] = (size_t)-1;
};

H264DecoderFrame::H264DecoderFrame(MemoryAllocator *pMemoryAllocator, H264_Heap_Objects * pObjHeap)
    : H264DecYUVBufferPadded()
    , m_TopSliceCount(0)
    , m_frameOrder(0)
    , m_ErrorType(0)
    , m_pSlicesInfo(0)
    , m_pSlicesInfoBottom(0)
    , m_pPreviousFrame(0)
    , m_pFutureFrame(0)
    , m_dFrameTime(-1.0)
    , m_isOriginalPTS(false)
    , m_IsFrameExist(0)
    , m_dpb_output_delay(INVALID_DPB_OUTPUT_DELAY)
    , m_PictureStructureForDec(0)
    , m_displayPictureStruct(DPS_UNKNOWN)
    , totalMBs(0)
    , post_procces_complete(false)
    , m_iNumberOfSlices(0)
    , m_auIndex(-1)
    , m_index(-1)
    , m_UID(-1)
    , m_FrameType(NONE_PICTURE)
    , m_MemID(0)
    , m_crop_left(0)
    , m_crop_right(0)
    , m_crop_top(0)
    , m_crop_bottom(0)
    , m_crop_flag(0)
    , m_aspect_width(0)
    , m_aspect_height(0)
    , m_pObjHeap(pObjHeap)
{
    m_isShortTermRef[0] = m_isShortTermRef[1] = false;
    m_isLongTermRef[0] = m_isLongTermRef[1] = false;
    m_isInterViewRef[0] = m_isInterViewRef[1] = false;
    m_LongTermPicNum[0] = m_LongTermPicNum[1] = -1;
    m_FrameNumWrap = m_FrameNum  = -1;
    m_LongTermFrameIdx = -1;
    m_viewId = 0;
    m_RefPicListResetCount[0] = m_RefPicListResetCount[1] = 0;
    m_PicNum[0] = m_PicNum[1] = -1;
    m_LongTermFrameIdx = -1;
    m_FrameNumWrap = m_FrameNum  = -1;
    m_LongTermPicNum[0] = m_PicNum[1] = -1;
    m_PicOrderCnt[0] = m_PicOrderCnt[1] = 0;
    m_bIDRFlag = false;

    // set memory managment tools
    m_pMemoryAllocator = pMemoryAllocator;

    ResetRefCounter();

    m_pSlicesInfo = new H264DecoderFrameInfo(this, m_pObjHeap);
    m_pSlicesInfoBottom = new H264DecoderFrameInfo(this, m_pObjHeap);

    m_ID[0] = (Ipp32s) ((Ipp8u *) this - (Ipp8u *) 0);
    m_ID[1] = m_ID[0] + 1;

    m_PictureStructureForRef = FRM_STRUCTURE;

    m_Flags.isFull = 0;
    m_Flags.isDecoded = 0;
    m_Flags.isDecodingStarted = 0;
    m_Flags.isDecodingCompleted = 0;
    m_isDisplayable = 0;
    m_Flags.isSkipped = 0;
    m_Flags.isActive = 0;
    m_wasOutputted = 0;
    m_wasDisplayed = 0;
    prepared[0] = false;
    prepared[1] = false;

    m_iResourceNumber = -1;
}

H264DecoderFrame::~H264DecoderFrame()
{
    if (m_pSlicesInfo)
    {
        delete m_pSlicesInfo;
        delete m_pSlicesInfoBottom;
        m_pSlicesInfo = 0;
        m_pSlicesInfoBottom = 0;
    }

    // Just to be safe.
    m_pPreviousFrame = 0;
    m_pFutureFrame = 0;
    Reset();
    deallocate();
}

void H264DecoderFrame::AddReference(RefCounter * reference)
{
    if (!reference || reference == this)
        return;

    if (std::find(m_references.begin(), m_references.end(), reference) != m_references.end())
        return;

    reference->IncrementReference();
    m_references.push_back(reference);
}

void H264DecoderFrame::FreeReferenceFrames()
{
    ReferenceList::iterator iter = m_references.begin();
    ReferenceList::iterator end_iter = m_references.end();

    for (; iter != end_iter; ++iter)
    {
        RefCounter * reference = *iter;
        reference->DecrementReference();

    }

    m_references.clear();
}

void H264DecoderFrame::Reset()
{
    m_TopSliceCount = 0;

    if (m_pSlicesInfo)
    {
        m_pSlicesInfo->Reset();
        m_pSlicesInfoBottom->Reset();
    }

    ResetRefCounter();

    m_isShortTermRef[0] = m_isShortTermRef[1] = false;
    m_isLongTermRef[0] = m_isLongTermRef[1] = false;
    m_isInterViewRef[0] = m_isInterViewRef[1] = false;

    post_procces_complete = false;
    m_bIDRFlag = false;

    m_RefPicListResetCount[0] = m_RefPicListResetCount[1] = 0;
    m_PicNum[0] = m_PicNum[1] = -1;
    m_LongTermPicNum[0] = m_LongTermPicNum[1] = -1;
    m_PictureStructureForRef = FRM_STRUCTURE;
    m_PicOrderCnt[0] = m_PicOrderCnt[1] = 0;

    m_viewId = 0;
    m_LongTermFrameIdx = -1;
    m_FrameNumWrap = m_FrameNum  = -1;
    m_Flags.isFull = 0;
    m_Flags.isDecoded = 0;
    m_Flags.isDecodingStarted = 0;
    m_Flags.isDecodingCompleted = 0;
    m_isDisplayable = 0;
    m_Flags.isSkipped = 0;
    m_Flags.isActive = 0;
    m_wasOutputted = 0;
    m_wasDisplayed = 0;
    m_dpb_output_delay = INVALID_DPB_OUTPUT_DELAY;

    m_bottom_field_flag[0] = m_bottom_field_flag[1] = -1;

    m_dFrameTime = -1;
    m_isOriginalPTS = false;

    m_IsFrameExist = false;
    m_iNumberOfSlices = 0;

    m_UserData.Reset();

    m_ErrorType = 0;
    m_UID = -1;
    m_index = -1;
    m_auIndex = -1;
    m_iResourceNumber = -1;

    m_MemID = MID_INVALID;

    prepared[0] = false;
    prepared[1] = false;

    m_displayPictureStruct = DPS_UNKNOWN;

    FreeReferenceFrames();

    deallocate();
}

bool H264DecoderFrame::IsFullFrame() const
{
    return (m_Flags.isFull == 1);
}

void H264DecoderFrame::SetFullFrame(bool isFull)
{
    m_Flags.isFull = (Ipp8u) (isFull ? 1 : 0);
}

bool H264DecoderFrame::IsDecoded() const
{
    return m_Flags.isDecoded == 1;
}

void H264DecoderFrame::FreeResources()
{
    FreeReferenceFrames();

    if (m_pSlicesInfo && IsDecoded())
    {
        m_pSlicesInfo->Free();
        m_pSlicesInfoBottom->Free();
    }
}

void H264DecoderFrame::CompleteDecoding()
{
    m_Flags.isDecodingCompleted = 1;
    UpdateErrorWithRefFrameStatus();
}

void H264DecoderFrame::UpdateErrorWithRefFrameStatus()
{
    if (m_pSlicesInfo->CheckReferenceFrameError() || m_pSlicesInfoBottom->CheckReferenceFrameError())
    {
        SetErrorFlagged(ERROR_FRAME_REFERENCE_FRAME);
    }
}

void H264DecoderFrame::OnDecodingCompleted()
{
    UpdateErrorWithRefFrameStatus();

    SetisDisplayable();

    m_Flags.isDecoded = 1;
    FreeResources();
    DecrementReference();
}

void H264DecoderFrame::SetisShortTermRef(bool isRef, Ipp32s WhichField)
{
    if (isRef)
    {
        if (!isShortTermRef() && !isLongTermRef())
            IncrementReference();

        if (m_PictureStructureForRef >= FRM_STRUCTURE)
            m_isShortTermRef[0] = m_isShortTermRef[1] = true;
        else
            m_isShortTermRef[WhichField] = true;
    }
    else
    {
        bool wasRef = isShortTermRef() != 0;

        if (m_PictureStructureForRef >= FRM_STRUCTURE)
        {
            m_isShortTermRef[0] = m_isShortTermRef[1] = false;
        }
        else
            m_isShortTermRef[WhichField] = false;

        if (wasRef && !isShortTermRef() && !isLongTermRef())
        {
            if (!IsFrameExist())
            {
                setWasOutputted();
                setWasDisplayed();
            }
            DecrementReference();
        }
    }
}

void H264DecoderFrame::SetisLongTermRef(bool isRef, Ipp32s WhichField)
{
    if (isRef)
    {
        if (!isShortTermRef() && !isLongTermRef())
            IncrementReference();

        if (m_PictureStructureForRef >= FRM_STRUCTURE)
            m_isLongTermRef[0] = m_isLongTermRef[1] = true;
        else
            m_isLongTermRef[WhichField] = true;
    }
    else
    {
        bool wasRef = isLongTermRef() != 0;

        if (m_PictureStructureForRef >= FRM_STRUCTURE)
        {
            m_isLongTermRef[0] = m_isLongTermRef[1] = false;
        }
        else
            m_isLongTermRef[WhichField] = false;

        if (wasRef && !isShortTermRef() && !isLongTermRef())
        {
            if (!IsFrameExist())
            {
                setWasOutputted();
                setWasDisplayed();
            }

            DecrementReference();
        }
    }
}

void H264DecoderFrame::setWasOutputted()
{
    m_wasOutputted = 1;
}

void H264DecoderFrame::Free()
{
    DEBUG_PRINT((VM_STRING("Free reference POC - %d, ref. count - %d, uid - %d, state D.%d O.%d\n"), m_PicOrderCnt[0], m_refCounter, m_UID, m_wasDisplayed, m_wasOutputted));

    if (wasDisplayed() && wasOutputted())
        Reset();
}

H264DecoderRefPicList* H264DecoderFrame::GetRefPicList(Ipp32s sliceNumber, Ipp32s list)
{
    H264DecoderRefPicList *pList;
    pList = GetAU(sliceNumber > m_TopSliceCount ? 1 : 0)->GetRefPicList(sliceNumber, list);
    return pList;
}   // RefPicList. Returns pointer to start of specified ref pic list.

//////////////////////////////////////////////////////////////////////////////
// updateFrameNumWrap
//  Updates m_FrameNumWrap and m_PicNum if the frame is a short-term
//  reference and a frame number wrap has occurred.
//////////////////////////////////////////////////////////////////////////////
void H264DecoderFrame::UpdateFrameNumWrap(Ipp32s  CurrFrameNum, Ipp32s  MaxFrameNum, Ipp32s CurrPicStruct)
{
    if (isShortTermRef())
    {
        m_FrameNumWrap = m_FrameNum;
        if (m_FrameNum > CurrFrameNum)
            m_FrameNumWrap -= MaxFrameNum;
        if (CurrPicStruct >= FRM_STRUCTURE)
        {
            setPicNum(m_FrameNumWrap, 0);
            setPicNum(m_FrameNumWrap, 1);
            m_PictureStructureForRef = FRM_STRUCTURE;
        }
        else
        {
            m_PictureStructureForRef = FLD_STRUCTURE;
            if (m_bottom_field_flag[0])
            {
                //1st - bottom, 2nd - top
                if (isShortTermRef(0)) setPicNum((2*m_FrameNumWrap) + (CurrPicStruct == BOTTOM_FLD_STRUCTURE), 0);
                if (isShortTermRef(1)) setPicNum((2*m_FrameNumWrap) + (CurrPicStruct == TOP_FLD_STRUCTURE), 1);
            }
            else
            {
                //1st - top , 2nd - bottom
                if (isShortTermRef(0)) setPicNum((2*m_FrameNumWrap) + (CurrPicStruct == TOP_FLD_STRUCTURE), 0);
                if (isShortTermRef(1)) setPicNum((2*m_FrameNumWrap) + (CurrPicStruct == BOTTOM_FLD_STRUCTURE), 1);
            }
        }
    }

}    // updateFrameNumWrap

//////////////////////////////////////////////////////////////////////////////
// updateLongTermPicNum
//  Updates m_LongTermPicNum for if long term reference, based upon
//  m_LongTermFrameIdx.
//////////////////////////////////////////////////////////////////////////////
void H264DecoderFrame::UpdateLongTermPicNum(Ipp32s CurrPicStruct)
{
    if (isLongTermRef())
    {
        if (CurrPicStruct >= FRM_STRUCTURE)
        {
            m_LongTermPicNum[0] = m_LongTermFrameIdx;
            m_LongTermPicNum[1] = m_LongTermFrameIdx;
            m_PictureStructureForRef = FRM_STRUCTURE;
        }
        else
        {
            m_PictureStructureForRef = FLD_STRUCTURE;
            if (m_bottom_field_flag[0])
            {
                //1st - bottom, 2nd - top
                m_LongTermPicNum[0] = 2*m_LongTermFrameIdx + (CurrPicStruct == BOTTOM_FLD_STRUCTURE);
                m_LongTermPicNum[1] = 2*m_LongTermFrameIdx + (CurrPicStruct == TOP_FLD_STRUCTURE);
            }
            else
            {
                //1st - top , 2nd - bottom
                m_LongTermPicNum[0] = 2*m_LongTermFrameIdx + (CurrPicStruct == TOP_FLD_STRUCTURE);
                m_LongTermPicNum[1] = 2*m_LongTermFrameIdx + (CurrPicStruct == BOTTOM_FLD_STRUCTURE);
            }
        }
    }
}    // updateLongTermPicNum

} // end namespace UMC
#endif // UMC_ENABLE_H264_VIDEO_DECODER
