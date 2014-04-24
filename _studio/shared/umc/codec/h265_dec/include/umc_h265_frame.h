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
#include "umc_h265_frame_coding_data.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef __UMC_H265_FRAME_H__
#define __UMC_H265_FRAME_H__

#include <stdlib.h>
#include "umc_h265_yuv.h"
#include "umc_h265_notify.h"

namespace UMC_HEVC_DECODER
{
class H265DecoderFrameInfo;
class H265FrameCodingData;
class H265CodingUnit;

// Struct containing list 0 and list 1 reference picture lists for one slice.
// Length is plus 1 to provide for null termination.
class H265DecoderRefPicList
{
public:

    struct ReferenceInformation // flags use for reference frames of slice
    {
        H265DecoderFrame * refFrame;
        bool isLongReference;
    };

    ReferenceInformation *m_refPicList;

    H265DecoderRefPicList()
    {
        memset(m_refPicList1, 0, sizeof(m_refPicList1));

        m_refPicList = &(m_refPicList1[1]);

        m_refPicList1[0].refFrame = 0;
        m_refPicList1[0].isLongReference = 0;
    }

    void Reset()
    {
        memset(&m_refPicList1, 0, sizeof(m_refPicList1));
    }

    H265DecoderRefPicList (const H265DecoderRefPicList& copy)
    {
        m_refPicList = &(m_refPicList1[1]);
        MFX_INTERNAL_CPY(&m_refPicList1, &copy.m_refPicList1, sizeof(m_refPicList1));
    }

    H265DecoderRefPicList& operator=(const H265DecoderRefPicList & copy)
    {
        MFX_INTERNAL_CPY(&m_refPicList1, &copy.m_refPicList1, sizeof(m_refPicList1));
        return *this;
    }

private:
    ReferenceInformation m_refPicList1[MAX_NUM_REF_PICS + 3];
};

// HEVC decoder frame class
class H265DecoderFrame : public H265DecYUVBufferPadded, public RefCounter
{
public:

    Ipp32s  m_PicOrderCnt;    // Display order picture count mod MAX_PIC_ORDER_CNT.

    Ipp32s  m_frameOrder;
    Ipp32s  m_ErrorType;

    H265DecoderFrameInfo * m_pSlicesInfo;

    bool prepared;

    H265DecoderFrameInfo * GetAU() const
    {
        return m_pSlicesInfo;
    }

    H265DecoderFrame *m_pPreviousFrame;
    H265DecoderFrame *m_pFutureFrame;

    H265SEIPayLoad m_UserData;

    Ipp64f           m_dFrameTime;
    bool             m_isOriginalPTS;

    bool             m_IsFrameExist;

    Ipp32s           m_dpb_output_delay;
    DisplayPictureStruct_H265  m_DisplayPictureStruct_H265;

    // For type 1 calculation of m_PicOrderCnt. m_FrameNum is needed to
    // be used as previous frame num.

    bool post_procces_complete;

    Ipp32s m_index;
    Ipp32s m_UID;
    UMC::FrameType m_FrameType;

    UMC::MemID m_MemID;

    Ipp32s           m_RefPicListResetCount;
    Ipp32s           m_crop_left;
    Ipp32s           m_crop_right;
    Ipp32s           m_crop_top;
    Ipp32s           m_crop_bottom;
    Ipp32s           m_crop_flag;

    Ipp32s           m_aspect_width;
    Ipp32s           m_aspect_height;

    bool             m_isShortTermRef;
    bool             m_isLongTermRef;
    bool             m_isUsedAsReference;

    // Returns whether frame has all slices found
    bool IsFullFrame() const;
    // Set frame flag denoting that all slices for it were found
    void SetFullFrame(bool isFull);

    struct
    {
        Ipp8u  isFull    : 1;
        Ipp8u  isDecoded : 1;
        Ipp8u  isDecodingStarted : 1;
        Ipp8u  isDecodingCompleted : 1;
        Ipp8u  isSkipped : 1;
    } m_Flags;

    Ipp8u  m_isDisplayable;
    Ipp8u  m_wasDisplayed;
    Ipp8u  m_wasOutputted;
    Ipp32s m_maxUIDWhenWasDisplayed;

    typedef std::list<RefCounter *>  ReferenceList;
    ReferenceList m_references;

    // Add target frame to the list of reference frames
    void AddReferenceFrame(H265DecoderFrame * frm);
    // Clear all references to other frames
    void FreeReferenceFrames();

    // Reinitialize frame structure before reusing frame
    void Reset();
    // Clean up data after decoding is done
    void FreeResources();

public:
    // FIXME: make coding data a member, not pointer
    H265FrameCodingData *m_CodingData;

    H265FrameCodingData* getCD() const {return m_CodingData;}

    // Returns a CTB by its raster address
    H265CodingUnit* getCU(Ipp32u CUaddr) const;

    // Returns number of CTBs in frame
    Ipp32u getNumCUsInFrame() const;
    // Returns number of minimal partitions in CTB
    Ipp32u getNumPartInCUSize() const;
    // Returns number of CTBs in frame width
    Ipp32u getFrameWidthInCU() const;
    // Returns number of CTBs in frame height
    Ipp32u getFrameHeightInCU() const;

    Ipp32s*  m_cuOffsetY;
    Ipp32s*  m_cuOffsetC;
    Ipp32s*  m_buOffsetY;
    Ipp32s*  m_buOffsetC;

    // Delete unneeded references and set flags after decoding is done
    void OnDecodingCompleted();

    // Free resources if possible
    virtual void Free();

    void SetSkipped(bool isSkipped)
    {
        m_Flags.isSkipped = (Ipp8u) (isSkipped ? 1 : 0);
    }

    bool IsSkipped() const
    {
        return m_Flags.isSkipped != 0;
    }

    // Returns whether frame has been decoded
    bool IsDecoded() const;

    bool IsFrameExist() const
    {
        return m_IsFrameExist;
    }

    void SetFrameExistFlag(bool isFrameExist)
    {
        m_IsFrameExist = isFrameExist;
    }

    H265DecoderFrame(UMC::MemoryAllocator *pMemoryAllocator, Heap_Objects * pObjHeap);

    virtual ~H265DecoderFrame();

    // The following methods provide access to the HEVC Decoder's doubly
    // linked list of H265DecoderFrames.  Note that m_pPreviousFrame can
    // be non-NULL even for an I frame.
    H265DecoderFrame *future() const  { return m_pFutureFrame; }

    void setPrevious(H265DecoderFrame *pPrev)
    {
        m_pPreviousFrame = pPrev;
    }

    void setFuture(H265DecoderFrame *pFut)
    {
        m_pFutureFrame = pFut;
    }

    bool        isDisplayable()    { return m_isDisplayable != 0; }

    void        SetisDisplayable()
    {
        m_isDisplayable = 1;
    }

    bool IsDecodingStarted() const { return m_Flags.isDecodingStarted != 0;}
    void StartDecoding() { m_Flags.isDecodingStarted = 1;}

    bool IsDecodingCompleted() const { return m_Flags.isDecodingCompleted != 0;}
    // Flag frame as completely decoded
    void CompleteDecoding();

    // Check reference frames for error status and flag this frame if error is found
    void UpdateErrorWithRefFrameStatus();

    void        unSetisDisplayable() { m_isDisplayable = 0; }

    bool        wasDisplayed()    { return m_wasDisplayed != 0; }
    void        setWasDisplayed() { m_wasDisplayed = 1; }
    void        unsetWasDisplayed() { m_wasDisplayed = 0; }

    bool        wasOutputted()    { return m_wasOutputted != 0; }
    // Flag frame after it was output
    void        setWasOutputted();
    void        unsetWasOutputted() { m_wasOutputted = 0; }

    bool        isDisposable()    { return (!m_isShortTermRef &&
                                            !m_isLongTermRef &&
                                            ((m_wasOutputted != 0 && m_wasDisplayed != 0) || (m_isDisplayable == 0)) &&
                                            !GetRefCounter()); }

    bool isShortTermRef() const
    {
        return m_isShortTermRef;
    }

    // Mark frame as short term reference frame
    void SetisShortTermRef(bool isRef);

    Ipp32s PicOrderCnt() const
    {
        return m_PicOrderCnt;
    }
    void setPicOrderCnt(Ipp32s PicOrderCnt)
    {
        m_PicOrderCnt = PicOrderCnt;
    }

    bool isLongTermRef() const
    {
        return m_isLongTermRef;
    }

    // Mark frame as long term reference frame
    void SetisLongTermRef(bool isRef);

    void IncreaseRefPicListResetCount()
    {
        m_RefPicListResetCount++;
    }

    void InitRefPicListResetCount()
    {
        m_RefPicListResetCount = 0;
    }

    Ipp32s RefPicListResetCount() const
    {
        return m_RefPicListResetCount;
    }

    // GetRefPicList
    // Returns pointer to start of specified ref pic list.
    H265DecoderRefPicList* GetRefPicList(Ipp32s sliceNumber, Ipp32s list) const;

    // Copy plane data from target frame
    void CopyPlanes(H265DecoderFrame *pRefFrame);
    // Fill frame planes with default values
    void DefaultFill(Ipp32s field, bool isChromaOnly, Ipp8u defaultValue = 128);

    Ipp32s GetError() const
    {
        return m_ErrorType;
    }

    void SetError(Ipp32s errorType)
    {
        m_ErrorType = errorType;
    }

    void SetErrorFlagged(Ipp32s errorType)
    {
        m_ErrorType |= errorType;
    }


    // Allocate and initialize frame array of CTBs and SAO parameters
    void allocateCodingData(const H265SeqParamSet* pSeqParamSet, const H265PicParamSet *pPicParamSet);
    // Free array of CTBs
    void deallocateCodingData();

    //  Access starting position of original picture for specific coding unit (CU)
    H265PlanePtrYCommon GetLumaAddr(Ipp32s CUAddr) const;
    //  Access starting position of original picture for specific coding unit (CU)
    H265PlanePtrUVCommon GetCbAddr(Ipp32s CUAddr) const;
    //  Access starting position of original picture for specific coding unit (CU)
    H265PlanePtrUVCommon GetCrAddr(Ipp32s CUAddr) const;
    //  Access starting position of original picture for specific coding unit (CU)
    H265PlanePtrUVCommon GetCbCrAddr(Ipp32s CUAddr) const;
    //  Access starting position of original picture for specific coding unit (CU) and partition unit (PU)
    H265PlanePtrYCommon GetLumaAddr(Ipp32s CUAddr, Ipp32u AbsZorderIdx) const;
    //  Access starting position of original picture for specific coding unit (CU) and partition unit (PU)
    H265PlanePtrUVCommon GetCbAddr(Ipp32s CUAddr, Ipp32u AbsZorderIdx) const;
    //  Access starting position of original picture for specific coding unit (CU) and partition unit (PU)
    H265PlanePtrUVCommon GetCrAddr(Ipp32s CUAddr, Ipp32u AbsZorderIdx) const;
    //  Access starting position of original picture for specific coding unit (CU) and partition unit (PU)
    H265PlanePtrUVCommon GetCbCrAddr(Ipp32s CUAddr, Ipp32u AbsZorderIdx) const;
protected:
    // Declare memory management tools
    UMC::MemoryAllocator *m_pMemoryAllocator;   // FIXME: should be removed because it duplicated in base class

    Heap_Objects * m_pObjHeap;
};

// Returns if frame is not needed by decoder
inline bool isAlmostDisposable(H265DecoderFrame * pTmp)
{
    return (!pTmp->m_isShortTermRef &&
        !pTmp->m_isLongTermRef &&
        ((pTmp->m_wasOutputted != 0) || (pTmp->m_isDisplayable == 0)) &&
        !pTmp->GetRefCounter());
}

} // end namespace UMC_HEVC_DECODER

#endif // __UMC_H265_FRAME_H__
#endif // UMC_ENABLE_H265_VIDEO_DECODER
