//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2012 - 2014 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include "mfx_h265_frame.h"
#include "mfx_h265_enc.h"
#include "vm_file.h"
#include "ippi.h"
#include "ippvc.h"

namespace H265Enc {

#define ALIGN_VALUE 32
// template to align a pointer
template<class T> inline
T align_pointer(void *pv, size_t lAlignValue = UMC::DEFAULT_ALIGN_VALUE)
{
    // some compilers complain to conversion to/from
    // pointer types from/to integral types.
    return (T) ((((Ipp8u *) pv - (Ipp8u *) 0) + (lAlignValue - 1)) &
                ~(lAlignValue - 1));
}


mfxStatus H265Frame::Create(H265VideoParam *par)
{
    Ipp32s numCtbs = par->PicWidthInCtbs * par->PicHeightInCtbs;
    Ipp32s numCtbs_parts = numCtbs << par->Log2NumPartInCU;
//    Ipp32s numCtbs_padd = (par->PicWidthInCtbs + 2) * (par->PicHeightInCtbs + 2);

    padding = par->MaxCUSize + 16;

    width = par->Width;
    height = par->Height;
    m_bitDepthLuma =  par->bitDepthLuma;
    m_bitDepthChroma = par->bitDepthChroma;
    m_bdLumaFlag = par->bitDepthLuma > 8;
    m_bdChromaFlag = par->bitDepthChroma > 8;

    Ipp32s plane_size_luma = (width + padding * 2) * (height + padding * 2);
    Ipp32s plane_size_chroma = plane_size_luma >> 1;

    pitch_luma_bytes = pitch_luma_pix = width + padding * 2;
    pitch_chroma_bytes = pitch_chroma_pix = pitch_luma_pix;

    pitch_luma = pitch_luma_pix;

    if (m_bdLumaFlag) {
        plane_size_luma <<= 1;
        pitch_luma_bytes <<= 1;
    }
    if (m_bdChromaFlag) {
        plane_size_chroma <<= 1;
        pitch_chroma_bytes <<= 1;
    }

    Ipp32s len = numCtbs_parts * sizeof(H265CUData) + ALIGN_VALUE;
    len += (plane_size_luma + plane_size_chroma) + ALIGN_VALUE*3;

    mem = H265_Malloc(len);
    MFX_CHECK_NULL_PTR1(len);

    cu_data = align_pointer<H265CUData *> (mem, ALIGN_VALUE);
    y = align_pointer<Ipp8u *> (cu_data + numCtbs_parts, ALIGN_VALUE);
    uv = align_pointer<Ipp8u *> (y + plane_size_luma, ALIGN_VALUE);;

    y += ((pitch_luma_pix + 1) * padding) << m_bdLumaFlag;
    uv += (((pitch_chroma_pix >> 1) + 1) * padding) << m_bdChromaFlag;

    return MFX_ERR_NONE;
}

inline static void memset_bd(Ipp8u *dst, Ipp32s val, Ipp32s size_pix, Ipp8u bdFlag) {
    if (bdFlag) {
        ippsSet_16s(val, (Ipp16s *)dst, size_pix);
    } else {
        ippsSet_8u((Ipp8u)val, dst, size_pix);
    }
}

mfxStatus H265Frame::CopyFrame(mfxFrameSurface1 *surface)
{
    IppiSize roi = { width, height };
    mfxU16 InputBitDepthLuma = 8;
    mfxU16 InputBitDepthChroma = 8;

    if (surface->Info.FourCC == MFX_FOURCC_P010) {
        InputBitDepthLuma = 10;
        InputBitDepthChroma = 10;
    }

    switch ((m_bdLumaFlag << 1) | (InputBitDepthLuma > 8)) {
    case 0:
        ippiCopy_8u_C1R(surface->Data.Y, surface->Data.Pitch, y, pitch_luma, roi);
        break;
    case 1:
//        ippiRShiftC_16u_C1IR(InputBitDepthLuma - 8, (Ipp16u*)surface->Data.Y, surface->Data.Pitch, roi);
        ippiConvert_16u8u_C1R((Ipp16u*)surface->Data.Y, surface->Data.Pitch, y, pitch_luma_bytes, roi);
        break;
    case 2:
        ippiConvert_8u16u_C1R(surface->Data.Y, surface->Data.Pitch, (Ipp16u*)y, pitch_luma_bytes, roi);
        ippiLShiftC_16u_C1IR(m_bitDepthLuma - 8, (Ipp16u*)y, pitch_luma_bytes, roi);
        break;
    case 3:
        ippiCopy_16u_C1R((Ipp16u*)surface->Data.Y, surface->Data.Pitch, (Ipp16u*)y, pitch_luma_bytes, roi);
        break;
    default:
        VM_ASSERT(0);
    }

    roi.height >>= 1;

    switch ((m_bdChromaFlag << 1) | (InputBitDepthChroma > 8)) {
    case 0:
        ippiCopy_8u_C1R(surface->Data.UV, surface->Data.Pitch, uv, pitch_luma, roi);
        break;
    case 1:
//        ippiRShiftC_16u_C1IR(InputBitDepthChroma - 8, (Ipp16u*)surface->Data.UV, surface->Data.Pitch, roi);
        ippiConvert_16u8u_C1R((Ipp16u*)surface->Data.UV, surface->Data.Pitch, uv, pitch_chroma_bytes, roi);
        break;
    case 2:
        ippiConvert_8u16u_C1R(surface->Data.UV, surface->Data.Pitch, (Ipp16u*)uv, pitch_chroma_bytes, roi);
        ippiLShiftC_16u_C1IR(m_bitDepthChroma - 8, (Ipp16u*)uv, pitch_chroma_bytes, roi);
        break;
    case 3:
        ippiCopy_16u_C1R((Ipp16u*)surface->Data.UV, surface->Data.Pitch, (Ipp16u*)uv, pitch_chroma_bytes, roi);
        break;
    default:
        VM_ASSERT(0);
    }

    return MFX_ERR_NONE;
}

static void ExpandPlane(Ipp8u elem_shift, Ipp8u *ptr, Ipp32s pitch_bytes, Ipp32s width, Ipp32s height, Ipp32s padding_w, Ipp32s padding_h) {
    Ipp8u *src = ptr;
    Ipp8u *dst = src - pitch_bytes;
    for (Ipp32s y = 0; y < padding_h; y++, dst -= pitch_bytes)
        ippsCopy_8u(src, dst, width<<elem_shift);

    src = ptr + (height - 1) * pitch_bytes;
    dst = src + pitch_bytes;
    for (Ipp32s y = 0; y < padding_h; y++, dst += pitch_bytes)
        ippsCopy_8u(src, dst, width<<elem_shift);

    Ipp8u * p0 = ptr - padding_h * pitch_bytes;
    if (elem_shift == 0) {
        for (Ipp32s y = 0; y < height + padding_h * 2; y++, p0 += pitch_bytes)
            ippsSet_8u(*p0, p0 - padding_w, padding_w);
    } else if (elem_shift == 1) {
        for (Ipp32s y = 0; y < height + padding_h * 2; y++, p0 += pitch_bytes)
            ippsSet_16s(*(Ipp16s*)p0, (Ipp16s*)p0 - padding_w, padding_w);
    } else if (elem_shift == 2) {
        for (Ipp32s y = 0; y < height + padding_h * 2; y++, p0 += pitch_bytes)
            ippsSet_32s(*(Ipp32s*)p0, (Ipp32s*)p0 - padding_w, padding_w);
    }

    p0 = ptr + ((width - 1) << elem_shift) - padding_h * pitch_bytes;
    if (elem_shift == 0) {
        for (Ipp32s y = 0; y < height + padding_h * 2; y++, p0 += pitch_bytes)
            ippsSet_8u(*p0, p0 + 1, padding_w);
    } else if (elem_shift == 1) {
        for (Ipp32s y = 0; y < height + padding_h * 2; y++, p0 += pitch_bytes)
            ippsSet_16s(*(Ipp16s*)p0, (Ipp16s*)p0 + 1, padding_w);
    } else if (elem_shift == 2) {
        for (Ipp32s y = 0; y < height + padding_h * 2; y++, p0 += pitch_bytes)
            ippsSet_32s(*(Ipp32s*)p0, (Ipp32s*)p0 + 1, padding_w);
    }
}

void H265Frame::doPadding()
{
    if (!m_bdLumaFlag) {
        ippiExpandPlane_H264_8u_C1R(y, width, height, pitch_luma_bytes, padding, IPPVC_FRAME);
    } else {
        ExpandPlane(1, y, pitch_luma_bytes, width, height, padding, padding);
    }

    if (!m_bdChromaFlag) {
        ExpandPlane(1, uv, pitch_chroma_bytes, width >> 1, height >> 1, padding >> 1, padding >> 1);
    } else {
        ExpandPlane(2, uv, pitch_chroma_bytes, width >> 1, height >> 1, padding >> 1, padding >> 1);
    }
}

void H265Frame::Dump(const vm_char* fname, H265VideoParam *par, H265FrameList *dpb, Ipp32s frame_num)
{
    vm_file *f;
    mfxU8* fbuf = NULL;
    Ipp32s W = par->Width - par->CropLeft - par->CropRight;
    Ipp32s H = par->Height - par->CropTop - par->CropBottom;
    Ipp32s bd_shift_luma = par->bitDepthLuma > 8;
    Ipp32s bd_shift_chroma = par->bitDepthChroma > 8;
    Ipp32s plane_size = (W*H << bd_shift_luma) + (W*H/2 << bd_shift_chroma);

    if (!fname || !fname[0])
        return;

    Ipp32s numlater = 0; // number of dumped frames with later POC
    if (m_PicCodType == MFX_FRAMETYPE_B) {
        H265Frame *pFrm;
        for (pFrm = dpb->head(); pFrm; pFrm = pFrm->m_pFutureFrame) {
            if (pFrm->m_wasEncoded && m_PicOrderCnt < pFrm->m_PicOrderCnt)
                numlater++;
        }
    }
    if ( numlater ) { // simple reorder for B: read last ref, replacing with B, write ref
        f = vm_file_fopen(fname,VM_STRING("r+b"));
        if (!f) return;
        fbuf = new mfxU8[numlater*plane_size];
        vm_file_fseek(f, -numlater*plane_size, VM_FILE_SEEK_END);
        vm_file_fread(fbuf, 1, numlater*plane_size, f);
        vm_file_fseek(f, -numlater*plane_size, VM_FILE_SEEK_END);
    } else {
        f = vm_file_fopen(fname, frame_num ? VM_STRING("a+b") : VM_STRING("wb"));
        if (!f) return;
    }

    if (f == NULL)
        return;

    int i;
    mfxU8 *p = y + ((par->CropLeft + par->CropTop * pitch_luma_pix) << bd_shift_luma);
    for (i = 0; i < H; i++) {
        vm_file_fwrite(p, 1<<bd_shift_luma, W, f);
        p += pitch_luma_bytes;
    }
    // writing nv12 to yuv420
    // maxlinesize = 4096
    if (W <= 2048*2) { // else invalid dump
        if (bd_shift_chroma) {
            mfxU16 uvbuf[2048];
            mfxU16 *p;
            for (int part = 0; part <= 1; part++) {
                p = (Ipp16u*)uv + part + par->CropLeft + (par->CropTop>>1) * pitch_chroma_pix;
                for (i = 0; i < H >> 1; i++) {
                    for (int j = 0; j < W>>1; j++)
                        uvbuf[j] = p[2*j];
                    vm_file_fwrite(uvbuf, 2, W>>1, f);
                    p += pitch_chroma_pix;
                }
            }
        } else {
            mfxU8 uvbuf[2048];
            for (int part = 0; part <= 1; part++) {
                p = uv + part + par->CropLeft + (par->CropTop>>1) * pitch_chroma_pix;
                for (i = 0; i < H >> 1; i++) {
                    for (int j = 0; j < W>>1; j++)
                        uvbuf[j] = p[2*j];
                    vm_file_fwrite(uvbuf, 1, W>>1, f);
                    p += pitch_chroma_pix;
                }
            }
        }
    }

    if (fbuf) {
        vm_file_fwrite(fbuf, 1, numlater*plane_size, f);
        delete[] fbuf;
    }
    vm_file_fclose(f);
}

void H265Frame::Destroy()
{
    if (mem)
        H265_Free(mem);
}


void H265FrameList::append(H265Frame *pFrame)
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
        m_pCurrent = m_pHead;
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

void H265FrameList::insertAtCurrent(H265Frame *pFrame)
{
    if (m_pCurrent)
    {
        H265Frame *pFutureFrame = m_pCurrent->future();

        pFrame->setFuture(pFutureFrame);

        if (pFutureFrame)
        {
            pFutureFrame->setPrevious(pFrame);
        }
        else
        {
            // Must be at the tail
            m_pTail = pFrame;
        }

        pFrame->setPrevious(m_pCurrent);
        m_pCurrent->setFuture(pFrame);
    }
    else
    {
        // Must be the first frame
        append(pFrame);
    }
}

H265FrameList::~H265FrameList()
{
    H265Frame *pCurr = m_pHead;

    while (pCurr)
    {
        H265Frame *pNext = pCurr->future();
        pCurr->Destroy();
        delete pCurr;
        pCurr = pNext;
    }
    m_pHead = m_pTail = 0;
}

H265Frame* H265FrameList::detachHead()
{
    H265Frame *pHead = m_pHead;
    if (pHead)
    {
        m_pHead = m_pHead->future();
        if (m_pHead)
            m_pHead->setPrevious(0);
        else
        {
            m_pTail = 0;
        }
    }
    m_pCurrent = m_pHead;
    return pHead;
}


void H265FrameList::RemoveFrame(H265Frame *pFrm)
{
    H265Frame *pPrev = pFrm->previous();
    H265Frame *pFut = pFrm->future();
    if (pFrm==m_pHead) //must be equal to pPrev==NULL
    {
        VM_ASSERT(pPrev==NULL);
        detachHead();
    }
    else
    {
        if (pFut==NULL)
        {
            m_pTail = pPrev;
            pPrev->setFuture(0);
        }
        else
        {
            pPrev->setFuture(pFut);
            pFut->setPrevious(pPrev);
        }
    }
    m_pCurrent = m_pHead;
}

H265Frame *H265FrameList::findMostDistantShortTermRefs(Ipp32s POC)
{
    H265Frame *pFrm = NULL;
    Ipp32s maxDeltaPOC = 0;//0x7fffffff;

    for (Ipp32s i = 0; i < 2; i++) {
        H265Frame *pCurr = m_pHead;
        while (pCurr)
        {
            if (pCurr->isShortTermRef() && (i == 0 ? pCurr->m_PGOPIndex : 1))
            {
                Ipp32s deltaPOC = pCurr->PicOrderCnt() - POC;

                if (deltaPOC < 0)
                    deltaPOC = -deltaPOC;

                if (deltaPOC > maxDeltaPOC)
                {
                    maxDeltaPOC = deltaPOC;
                    pFrm = pCurr;
                }
            }
            pCurr = pCurr->future();
        }
        if (pFrm) return pFrm;
    }

    return pFrm;
}

H265Frame *H265FrameList::findMostDistantLongTermRefs(Ipp32s POC)
{
    H265Frame *pCurr = m_pHead;
    H265Frame *pFrm = NULL;
    Ipp32s maxDeltaPOC = 0x7fffffff;

    while (pCurr)
    {
        if (pCurr->isLongTermRef())
        {
            Ipp32s deltaPOC = pCurr->PicOrderCnt() - POC;

            if (deltaPOC < 0)
                deltaPOC = -deltaPOC;

            if (deltaPOC < maxDeltaPOC)
            {
                maxDeltaPOC = deltaPOC;
                pFrm = pCurr;
            }
        }
        pCurr = pCurr->future();
    }

    return pFrm;
}

void H265FrameList::countActiveRefs(Ipp32u &NumShortTermL0,
                                           Ipp32u &NumLongTermL0,
                                           Ipp32u &NumShortTermL1,
                                           Ipp32u &NumLongTermL1,
                                           Ipp32s curPOC)
{
    H265Frame *pCurr = m_pHead;
    NumShortTermL0 = 0;
    NumLongTermL0 = 0;
    NumShortTermL1 = 0;
    NumLongTermL1 = 0;

    while (pCurr)
    {
        if (pCurr->isShortTermRef())
        {
            if (pCurr->PicOrderCnt() < curPOC)
            {
                NumShortTermL0++;
            }
            else
            {
                NumShortTermL1++;
            }
        }
        else if (pCurr->isLongTermRef())
        {
            if (pCurr->PicOrderCnt() < curPOC)
            {
                NumLongTermL0++;
            }
            else
            {
                NumLongTermL1++;
            }
        }
        pCurr = pCurr->future();
    }

}    // countActiveRefs

void H265FrameList::countL1Refs(Ipp32u &NumRefs, Ipp32s curPOC)
{
    H265Frame *pCurr = m_pHead;
    NumRefs = 0;

    while (pCurr)
    {
        if ((pCurr->isShortTermRef() || pCurr->isLongTermRef()) && pCurr->PicOrderCnt()>curPOC)
            NumRefs++;
        pCurr = pCurr->future();
    }

}    // countL1Refs

void H265FrameList::IncreaseRefPicListResetCount(H265Frame *ExcludeFrame)
{
    H265Frame *pCurr = m_pHead;

    while (pCurr)
    {
        if (pCurr!=ExcludeFrame)
        {
            pCurr->IncreaseRefPicListResetCount();
        }
        pCurr = pCurr->future();
    }

}    // IncreaseRefPicListResetCount

H265Frame *H265FrameList::findFrameByPOC(Ipp32s POC)
{
    H265Frame *pCurr = m_pHead;

    while (pCurr)
    {
        if (pCurr->PicOrderCnt() == POC)
        {
            return pCurr;
        }
        pCurr = pCurr->future();
    }
    return NULL;
}

H265Frame *H265FrameList::findNextDisposable(void)
{
    H265Frame *pTmp;

    if (!m_pCurrent)
    {
        // There are no frames in the list, return
        return NULL;
    }

    // Loop through starting with the next frame after the current one
    for (pTmp = m_pCurrent->future(); pTmp; pTmp = pTmp->future())
    {
        if (pTmp->isDisposable())
        {
            // We got one

            // Update the current
            m_pCurrent = pTmp;

            return pTmp;
        }
    }

    // We got all the way to the tail without finding a free frame
    // Start from the head and go until the current
    for (pTmp = m_pHead; pTmp && pTmp->previous() != m_pCurrent; pTmp = pTmp->future())
    {
        if (pTmp->isDisposable())
        {
            // We got one

            // Update the current
            m_pCurrent = pTmp;
            return pTmp;
        }
    }

    // We never found one
    return NULL;
}

H265Frame *H265FrameList::findOldestToEncode(H265FrameList *dpb,
                                             Ipp32u min_L1_refs,
                                             Ipp8u isBpyramid,
                                             Ipp8u ByramidNextFrameNum)
{
    H265Frame *pCurr = m_pHead;
    H265Frame *pOldest = NULL;
    Ipp32s  SmallestPicOrderCnt = 0x7fffffff;    // very large positive
    Ipp32s  LargestRefPicListResetCount = 0;
    Ipp8u exclude_cur=false;
    while (pCurr)
    {
        if (!pCurr->wasEncoded())
        {
            if (pCurr->m_PicCodType == MFX_FRAMETYPE_B)
            {
                Ipp32u active_L1_refs;
                dpb->countL1Refs(active_L1_refs,pCurr->PicOrderCnt());
                exclude_cur=active_L1_refs<min_L1_refs;

                if ((min_L1_refs > 0) && (isBpyramid) && (!exclude_cur))
                {
                    exclude_cur = (pCurr->m_BpyramidNumFrame != ByramidNextFrameNum);
                }
            }
            if (!exclude_cur)
            {
                // corresponding frame
                if (pCurr->RefPicListResetCount() > LargestRefPicListResetCount )
                {
                    pOldest = pCurr;
                    SmallestPicOrderCnt = pCurr->PicOrderCnt();
                    LargestRefPicListResetCount = pCurr->RefPicListResetCount();
                }
                else if    ((pCurr->PicOrderCnt() < SmallestPicOrderCnt ) && (pCurr->RefPicListResetCount() == LargestRefPicListResetCount ))
                {
                    pOldest = pCurr;
                    SmallestPicOrderCnt = pCurr->PicOrderCnt();
                }
            }
        }
        pCurr = pCurr->future();
        exclude_cur=false;
    }

    if (pOldest && pOldest->m_bIsIDRPic) { // correct POC for leading B w/o L0
        for (pCurr = m_pHead; pCurr; pCurr = pCurr->future()) {
            if (!pCurr->wasEncoded() && pCurr->m_PicCodType == MFX_FRAMETYPE_B &&
                pCurr->m_PicOrderCounterAccumulated < pOldest->m_PicOrderCounterAccumulated) {
                pCurr->m_PicOrderCnt += pCurr->m_PicOrderCounterAccumulated - pOldest->m_PicOrderCounterAccumulated;
                pCurr->m_PicOrderCounterAccumulated = pOldest->m_PicOrderCounterAccumulated;
                pCurr->m_RefPicListResetCount = pOldest->RefPicListResetCount();
            }
        }
    }
    // may be OK if NULL
    return pOldest;

}    // findOldestDisplayable

H265Frame *H265FrameList::findOldestToLookAheadProcess(void)
{
    H265Frame *pCurr = m_pHead;
    H265Frame *pOldest = NULL;
    Ipp32s  smallestPOC = 0x7fffffff;    // very large positive
    Ipp32s  LargestRefPicListResetCount = 0;
    Ipp8u exclude_cur=false;
    while (pCurr)
    {
        if (!pCurr->wasLAProcessed())
        {
            if( pCurr->PicOrderCnt() < smallestPOC)
            {
                pOldest = pCurr;
                smallestPOC = pCurr->PicOrderCnt();
            }
        }
        pCurr = pCurr->future();
    }

    // may be OK if NULL
    return pOldest;

}    // findOldestDisplayable

void H265FrameList::unMarkAll()
{
    H265Frame *pCurr = m_pHead;

    while (pCurr)
    {
        pCurr->m_isMarked = false;
        pCurr = pCurr->future();
    }
}

void H265FrameList::removeAllUnmarkedRef()
{
    H265Frame *pCurr = m_pHead;

    while (pCurr)
    {
        if ((pCurr->wasEncoded()) && (!pCurr->m_isMarked))
        {
            pCurr->unSetisLongTermRef();
            pCurr->unSetisShortTermRef();
        }
        pCurr = pCurr->future();
    }
}


H265Frame *H265FrameList::InsertFrame(mfxFrameSurface1 *surface,
                                      H265VideoParam *par
                                      /*,
                                      I32            num_slices,
                                      BlockSize      padded_size,
                                      I32            NumLCU*/)
{
    H265Frame *pFrm;
    pFrm = findNextDisposable();

    //if there are no unused frames allocate new frame
    if (!pFrm){
        pFrm = new H265Frame(); //Init/allocate from input data
        if (!pFrm) return NULL;
        if (pFrm->Create(par) != MFX_ERR_NONE) {
            delete pFrm;
            return NULL;
        }
        insertAtCurrent(pFrm);
    }
    else
    {
//        pFrm->SetSurfaceHandle(rFrame->GetSurfaceHandle());
    }

//    rFrame->GetTime( pFrm->pts_start, pFrm->pts_end );
    pFrm->TimeStamp = surface->Data.TimeStamp;

    //Make copy of input data
    pFrm->CopyFrame(surface);
    pFrm->unsetWasEncoded();
    pFrm->unsetWasLAProcessed();
    return pFrm;
}

} // namespace

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
