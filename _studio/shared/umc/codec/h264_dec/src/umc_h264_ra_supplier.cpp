/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2003-2013 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#include "umc_h264_ra_supplier.h"

#ifndef UMC_RESTRICTED_CODE_RA

#include "umc_h264_frame_list.h"
#include "umc_video_data_shared.h"

#include "umc_h264_dec_debug.h"

namespace UMC
{

struct FrameInformationToStore
{
    Ipp32s  m_PictureStructureForRef;
    Ipp32s  m_PicOrderCnt[2];    // Display order picture count mod MAX_PIC_ORDER_CNT.
    Ipp32s  m_bottom_field_flag[2];
    Ipp32s  m_PicNum[2];
    Ipp32s  m_LongTermPicNum[2];
    Ipp32s  m_FrameNum;
    Ipp32s  m_FrameNumWrap;
    Ipp32s  m_LongTermFrameIdx;
    Ipp32s  m_TopSliceCount;
    Ipp32s  m_iNumberOfSlices;

    typedef struct
    {
        bool is_top;
        bool is_sr;
        MemID memid;
    } ReferenceEntry;

    ReferenceEntry  m_References[150][2][32]; // [] - slices [] -

    void ToFrame(H264DecoderFrame * pFrame)
    {
        #define STORE_TO_FRAME(x) pFrame->x = x

        STORE_TO_FRAME(m_PictureStructureForRef);
        STORE_TO_FRAME(m_PicOrderCnt[0]);
        STORE_TO_FRAME(m_PicOrderCnt[1]);
        STORE_TO_FRAME(m_bottom_field_flag[0]);
        STORE_TO_FRAME(m_bottom_field_flag[1]);
        STORE_TO_FRAME(m_PicNum[0]);
        STORE_TO_FRAME(m_PicNum[1]);
        STORE_TO_FRAME(m_LongTermPicNum[0]);
        STORE_TO_FRAME(m_LongTermPicNum[1]);
        STORE_TO_FRAME(m_FrameNum);
        STORE_TO_FRAME(m_FrameNumWrap);
        STORE_TO_FRAME(m_LongTermPicNum[0]);
        STORE_TO_FRAME(m_LongTermFrameIdx);
        STORE_TO_FRAME(m_TopSliceCount);
        STORE_TO_FRAME(m_iNumberOfSlices);

        #undef STORE_TO_FRAME
    }

    void WriteReferences(H264DecoderFrame * pFrame)
    {
        for (Ipp32s slice = 0; slice < pFrame->m_iNumberOfSlices + 1; slice++)
        {
            for (Ipp32s list = 0; list < 2; list++)
            {
                if (!pFrame->GetRefPicList(slice, list))
                    continue;

                H264DecoderFrame ** pRefPicList = pFrame->GetRefPicList(slice, list)->m_RefPicList;
                ReferenceFlags*     pFields = pFrame->GetRefPicList(slice, list)->m_Flags;
                for (Ipp32s i = 0; i < 32; i++)
                {
                    if (pRefPicList[i])
                    {
                        m_References[slice][list][i].memid = pRefPicList[i]->m_index;
                        m_References[slice][list][i].is_top = pFields[i].field == 0;
                        m_References[slice][list][i].is_sr = pFields[i].isShortReference;
                    }
                    else
                    {
                        m_References[slice][list][i].memid = MID_INVALID;
                    }
                }
            }
        }
    }

    void FromFrame(H264DecoderFrame * pFrame)
    {
        #define STORE_TO_FRAME(x) x = pFrame->x

        STORE_TO_FRAME(m_PictureStructureForRef);
        STORE_TO_FRAME(m_PicOrderCnt[0]);
        STORE_TO_FRAME(m_PicOrderCnt[1]);
        STORE_TO_FRAME(m_bottom_field_flag[0]);
        STORE_TO_FRAME(m_bottom_field_flag[1]);
        STORE_TO_FRAME(m_PicNum[0]);
        STORE_TO_FRAME(m_PicNum[1]);
        STORE_TO_FRAME(m_LongTermPicNum[0]);
        STORE_TO_FRAME(m_LongTermPicNum[1]);
        STORE_TO_FRAME(m_FrameNum);
        STORE_TO_FRAME(m_FrameNumWrap);
        STORE_TO_FRAME(m_LongTermPicNum[0]);
        STORE_TO_FRAME(m_LongTermFrameIdx);
        STORE_TO_FRAME(m_TopSliceCount);
        STORE_TO_FRAME(m_iNumberOfSlices);

        #undef STORE_TO_FRAME
    }
};

RATaskSupplier::RATaskSupplier(VideoData &lastDecodedFrame, BaseCodec *&postProcessing)
    : TaskSupplier(lastDecodedFrame, postProcessing)
    , g_reflist(0)
{
}

bool RATaskSupplier::GetFrameToDisplay(MediaData *dst, bool force)
{
    // Perform output color conversion and video effects, if we didn't
    // already write our output to the application's buffer.
    VideoData *pVData = DynamicCast<VideoData> (dst);
    if (!pVData)
        return false;

    m_pLastDisplayed = 0;

    H264DecoderFrame * pFrame = GetFrameToDisplayInternal(dst, force);
    if (!pFrame)
    {
        return false;
    }

    m_pLastDisplayed = pFrame;
    pVData->SetInvalid(pFrame->GetError());

    if (m_pLastDisplayed->IsSkipped())
    {
        m_LastDecodedFrame.SetDataSize(1);
        pFrame->setWasOutputted();
        return true;
    }

    InitColorConverter(pFrame, 0);
    m_LastDecodedFrame.SetTime(dst->GetTime());

    VideoDataShared* shared = DynamicCast<VideoDataShared, MediaData>(dst);
    if (shared)
    {
        *(VideoData *)shared = m_LastDecodedFrame;
        shared->SetMID(pFrame->m_MemID);
        //m_pMemoryAllocator->DontFree(pFrame->m_MemID);
        m_pMemoryAllocator->Unlock(pFrame->m_MemID);
    }

    if (m_PostProcessing->GetFrame(&m_LastDecodedFrame, pVData) != UMC_OK)
    {
        m_LastDecodedFrame.SetDataSize(0);
        pFrame->setWasOutputted();
        return false;
    }

    m_LastDecodedFrame.SetDataSize(m_LastDecodedFrame.GetMappingSize());
    pVData->SetDataSize(pVData->GetMappingSize());
    pFrame->setWasOutputted();

    return true;
}

void RATaskSupplier::InitFrameCounter(H264DecoderFrame * pFrame, const H264Slice *pSlice)
{
    TaskSupplier::InitFrameCounter(pFrame, pSlice);
    if (g_reflist)
    {
        m_TopFieldPOC = g_reflist->m_Pocs[0];
        m_BottomFieldPOC = g_reflist->m_Pocs[1];
        m_PicOrderCnt = g_reflist->m_Pocs[m_field_index];
    }

    //transfer previosly calculated PicOrdeCnts into current Frame
    if (pFrame->m_PictureStructureForRef < FRM_STRUCTURE)
    {
        pFrame->setPicOrderCnt(m_PicOrderCnt, m_field_index);
        if (!m_field_index) // temporally set same POC for second field
            pFrame->setPicOrderCnt(m_PicOrderCnt, 1);
    }
    else
    {
        pFrame->setPicOrderCnt(m_TopFieldPOC, 0);
        pFrame->setPicOrderCnt(m_BottomFieldPOC, 1);
    }

    DEBUG_PRINT((VM_STRING("Init frame POC - %d, %d, field - %d, uid - %d, frame_num - %d\n"), pFrame->m_PicOrderCnt[0], pFrame->m_PicOrderCnt[1], m_field_index, pFrame->m_UID, pFrame->m_FrameNum));
} // void RATaskSupplier::InitFrameCounter(H264DecoderFrame * pFrame, const H264Slice *pSlice)

Status RATaskSupplier::AllocateFrameData(H264DecoderFrame * pFrame, IppiSize dimensions, Ipp32s bit_depth, Ipp32s chroma_format_idc)
{
    Status umcRes = UMC_OK;

    if (pSlice->IsReference())
    {
        size_t yuv_size = pFrame->GetPreferrableSize(dimensions, bit_depth, chroma_format_idc);
        size_t data_size = pFrame->GetFrameDataSize(dimensions);
        size_t extended_size = sizeof(FrameInformationToStore);

        size_t size = yuv_size + data_size + extended_size;

        MemID mid;
        if (UMC_OK != m_pMemoryAllocator->Alloc(&mid,
                                                size,
                                                UMC_ALLOC_PERSISTENT))
        {
            // Reset all our state variables
            return UMC_ERR_ALLOC;
        }

        Ipp8u * ptr = (Ipp8u *) m_pMemoryAllocator->Lock(mid);

        // DEBUG: SetExternalPointers call was removed should use frameDataInfo and allocate method instead
        // pFrame->allocate
        pFrame->SetExternalPointers(dimensions, bit_depth, chroma_format_idc, ptr);

        ptr += yuv_size;

        ptr += extended_size;

        pFrame->SetExternalFrameData(ptr, size);

        //m_pMemoryAllocator->DontFree(mid);

        pFrame->m_MemID = mid;
    }
    else
    {
        size_t yuv_size = pFrame->GetPreferrableSize(dimensions, bit_depth, chroma_format_idc);
        size_t extended_size = sizeof(FrameInformationToStore);
        size_t size = yuv_size + extended_size;

        MemID mid;
        if (UMC_OK != m_pMemoryAllocator->Alloc(&mid,
                                                size,
                                                UMC_ALLOC_PERSISTENT))
        {
            // Reset all our state variables
            return UMC_ERR_ALLOC;
        }

        Ipp8u * ptr = (Ipp8u *) m_pMemoryAllocator->Lock(mid);
        pFrame->SetExternalPointers(dimensions, bit_depth, chroma_format_idc, ptr);
        ptr += yuv_size;

        umcRes = pFrame->allocateParsedFrameData();

        pFrame->m_MemID = mid;
    }

    return umcRes;
}

void RATaskSupplier::CompleteFrame(H264DecoderFrame * pFrame, Ipp32s field)
{
    H264DecoderFrameInfo * slicesInfo = pFrame->GetAU(field);

    DEBUG_PRINT((VM_STRING("Complete frame POC - (%d,%d) type - %d, field - %d, count - %d, m_uid - %d, IDR - %d\n"), pFrame->m_PicOrderCnt[0], pFrame->m_PicOrderCnt[1], pFrame->m_FrameType, m_field_index, pFrame->GetAU(m_field_index)->GetSliceCount(), pFrame->m_UID, pFrame->m_bIDRFlag[0]));
    if (slicesInfo->GetStatus() > H264DecoderFrameInfo::STATUS_NOT_FILLED)
        return;

    TaskSupplier::CompleteFrame(pFrame, field);

    {
        size_t yuv_size = pFrame->GetPreferrableSize(pFrame->lumaSize(), pFrame->m_bpp, pFrame->m_chroma_format);

        Ipp8u * ptr = (Ipp8u *) m_pMemoryAllocator->Lock(pFrame->m_MemID);
        ptr += yuv_size;

        size_t extended_size = sizeof(FrameInformationToStore);

        FrameInformationToStore frame_store;
        frame_store.FromFrame(pFrame);

        H264Slice * pSlice = pFrame->GetAU(field)->GetSlice(0);
        if (pSlice->IsReference())
            frame_store.WriteReferences(pFrame);

        MFX_INTERNAL_CPY(ptr, &frame_store, extended_size);
        m_pMemoryAllocator->Unlock(pFrame->m_MemID);
    }
}

void LoadFrame(H264DecoderFrame * pFrame, Ipp8u * ptr, VideoData &lastDecodedFrame)
{
    /*IppiSize dimensions = pFrame->lumaSize();
    Ipp32s bpp = pFrame->m_bpp;
    Ipp32s chroma_format = pFrame->m_chroma_format;*/

    IppiSize dimensions;
    dimensions.width = lastDecodedFrame.GetWidth();
    dimensions.height = lastDecodedFrame.GetHeight();

    Ipp32s bpp = IPP_MAX(lastDecodedFrame.GetPlaneBitDepth(0), lastDecodedFrame.GetPlaneBitDepth(1));

    Ipp32s chroma_format = 1;

    switch (lastDecodedFrame.GetColorFormat())
    {
    case YUV420:
        chroma_format = 1;
        break;
    case YUV422:
        chroma_format = 2;
        break;
    case YUV444:
        chroma_format = 3;
        break;
    default:
        chroma_format = 1;
        break;
    }

    size_t yuv_size = pFrame->GetPreferrableSize(dimensions, bpp, chroma_format);
    size_t data_size = pFrame->GetFrameDataSize(dimensions);

    size_t extended_size = sizeof(FrameInformationToStore); // (Ipp8u*)&(pFrame->m_pSlicesInfo) - (Ipp8u*)&(pFrame->m_PictureStructureForRef);
    FrameInformationToStore frame_store;
    MFX_INTERNAL_CPY(&frame_store, ptr + yuv_size, extended_size);
    frame_store.ToFrame(pFrame);

    pFrame->SetExternalPointers(dimensions, bpp, chroma_format, ptr);
    ptr += yuv_size;

    ptr += extended_size;

    pFrame->SetExternalFrameData(ptr, data_size);

    pFrame->SetisDisplayable();
    pFrame->SetSkipped(true);
    pFrame->SetFrameExistFlag(false);
    pFrame->setWasOutputted();
    pFrame->SetisShortTermRef(0);
    pFrame->SetisShortTermRef(1);
}

void RATaskSupplier::LoadDPB(ReferenceList* reflist)
{
    for (H264DecoderFrame * pTmp = m_pDecodedFramesList->head(); pTmp; pTmp = pTmp->future())
    {
        pTmp->m_index = -1;
    }

    for (Ipp32s i = 0; i < reflist->m_NumberOfRefs; i++)
    {
        H264DecoderFrame * pFrame = GetFreeFrame();
        MemID id = reflist->m_RefFrames[i];
        Ipp8u* frameData = (Ipp8u*)m_pMemoryAllocator->Lock(id);
        pFrame->m_MemID = id;
        pFrame->m_index = id;
        LoadFrame(pFrame, frameData, m_LastNonCropDecodedFrame);
    }

    for (H264DecoderFrame * pTmp = m_pDecodedFramesList->head(); pTmp; pTmp = pTmp->future())
    {
        size_t yuv_size = pTmp->GetPreferrableSize(pTmp->lumaSize(), pTmp->m_bpp, pTmp->m_chroma_format);

        Ipp8u* ptr = (Ipp8u*)m_pMemoryAllocator->Lock(pTmp->m_index);

        if (!ptr)
            continue;

        FrameInformationToStore *frame_store = (FrameInformationToStore *)(ptr + yuv_size);

        for (Ipp32s slice = 0; slice < pTmp->m_iNumberOfSlices + 1; slice++)
        {
            for (Ipp32s list = 0; list < 2; list++)
            {
                H264DecoderFrame ** pRefPicList = pTmp->GetRefPicList(slice, list)->m_RefPicList;
                ReferenceFlags*     pFields = pTmp->GetRefPicList(slice, list)->m_Flags;

                for (Ipp32s i = 0; i < 32; i++)
                {
                    MemID memid = frame_store->m_References[slice][list][i].memid;
                    if (memid != MID_INVALID)
                    {
                        pRefPicList[i] = m_pDecodedFramesList->FindByIndex(memid);
                        pFields[i].field = frame_store->m_References[slice][list][i].is_top ? 0 : 1;
                        pFields[i].isShortReference = frame_store->m_References[slice][list][i].is_sr ? 1 : 0;
                    }
                    else
                    {
                        pRefPicList[i] = 0;
                        pFields[i].field = 0;
                        pFields[i].isShortReference = false;
                    }
                }
            }
        }

        m_pMemoryAllocator->Unlock(pTmp->m_index);
    }
}

void CleanDPB(MemoryAllocator *pMemoryAllocator, ReferenceList* reflist)
{
    for (Ipp32s i = 0; i < reflist->m_NumberOfRefs; i++)
    {
        MemID id = reflist->m_RefFrames[i];
        pMemoryAllocator->Unlock(id);
    }
}

Status RATaskSupplier::GetRandomFrame(MediaData *pSource, MediaData *pDst, ReferenceList* reflist)
{
    Status umsRes = UMC_OK;

    Reset();

    VM_ASSERT(pSource->GetDataSize() > 3);
    AddOneFrame(pSource, 0); // decode headers only
    VM_ASSERT(pSource->GetDataSize() > 3);
    if (pSource->GetDataSize() <= 3)
        return UMC_ERR_INVALID_PARAMS;

    LoadDPB(reflist); // construct reference list

    m_WaitForIDR = false;

    g_reflist = reflist;

    AddOneFrame(pSource, pDst); // construct frame

    g_reflist = 0;

    RunDecoding(true); // decode frame

    // return frame to display
    umsRes = GetFrameToDisplay(pDst, true) ? UMC_OK : UMC_ERR_NOT_ENOUGH_DATA;

    if (umsRes != UMC_OK)
    {
        GetFrameToDisplay(pDst, true);
    }

    VM_ASSERT(umsRes == UMC_OK);

    CleanDPB(m_pMemoryAllocator, reflist);

    return umsRes;
}

} // namespace UMC

#endif // UMC_RESTRICTED_CODE_RA
#endif // UMC_ENABLE_H264_VIDEO_DECODER
