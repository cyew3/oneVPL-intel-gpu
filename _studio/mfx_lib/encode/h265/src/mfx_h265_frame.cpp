//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2012-2013 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#include "mfx_h265_defs.h"

#define ALIGN_VALUE 16
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

    Ipp32s plane_size = (width + padding * 2) * (height + padding * 2);

    pitch_luma = width + padding * 2;
    pitch_chroma = pitch_luma >> 1;


    Ipp32s len = numCtbs_parts * sizeof(H265CUData) + ALIGN_VALUE;
    len += plane_size * 3 / 2 + ALIGN_VALUE*3;

    mem = H265_Malloc(len);
    MFX_CHECK_NULL_PTR1(len);

    cu_data = align_pointer<H265CUData *> (mem, ALIGN_VALUE);
    y = align_pointer<Ipp8u *> (cu_data + numCtbs_parts, ALIGN_VALUE);
    u = align_pointer<Ipp8u *> (y + plane_size, ALIGN_VALUE);
    v = align_pointer<Ipp8u *> (u + plane_size / 4, ALIGN_VALUE);
    uv = u;

    y += (pitch_luma + 1) * padding;
    u += (pitch_chroma + 1) * padding >> 1;
    v += (pitch_chroma + 1) * padding >> 1;
    uv += (pitch_luma * padding >> 1) + padding;

    return MFX_ERR_NONE;
}

mfxStatus H265Frame::CopyFrame(mfxFrameSurface1 *surface)
{
    Ipp32s i;
    PixType *y0 = y;
    PixType *uv0 = uv;
    mfxU8 *ysrc = surface->Data.Y;
    mfxU8 *uvsrc = surface->Data.UV;
    Ipp32s input_width =  surface->Info.CropW ? surface->Info.CropW : surface->Info.Width;
    Ipp32s input_height = surface->Info.CropH ? surface->Info.CropH : surface->Info.Height;
    if (input_width > width || input_height > height) VM_ASSERT(0);

    memset(y0 - (pitch_luma + 1) * padding, 128, (pitch_luma + 1) * padding);

    for (i = 0; i < input_height; i++) {
        memcpy(y0, ysrc, input_width);
        memset(y0 + input_width, 128, padding * 2 + width - input_width);
        y0 += pitch_luma;
        ysrc += surface->Data.Pitch;
    }
    for (i = input_height; i < height; i++) {
        memset(y0, 128, padding * 2 + width);
        y0 += pitch_luma;
    }

    memset(y0, 128, (pitch_luma - 1) * padding);

    memset(uv0 - ((pitch_luma * padding >> 1) + padding), 128,
        (pitch_luma * padding >> 1) + padding);

    for (i = 0; i < input_height >> 1; i++) {
        memcpy(uv0, uvsrc, input_width);
        memset(uv0 + input_width, 128, padding * 2 + width - input_width);
        uv0 += pitch_luma;
        uvsrc += surface->Data.Pitch;
    }
    for (i = input_height >> 1; i < height >> 1; i++) {
        memset(uv0, 128, padding * 2 + width);
        uv0 += pitch_luma;
    }

    memset(uv0, 128, (pitch_luma * padding >> 1) - padding);

    return MFX_ERR_NONE;
}
/*
static void doPaddingPlane(PixType *piTxt, Ipp32s Stride, Ipp32s Width, Ipp32s Height, Ipp32s MarginX, Ipp32s MarginY)
{
    Ipp32s x, y;
    PixType *pi;

    pi = piTxt;
    for (y = 0; y < Height; y++)
    {
        for (x = 0; x < MarginX; x++ )
        {
            pi[-MarginX + x] = pi[0];
            pi[Width + x] = pi[Width - 1];
        }
        pi += Stride;
    }

    pi -= (Stride + MarginX);
    for (y = 0; y < MarginY; y++)
    {
        ::memcpy(pi + (y + 1) * Stride, pi, sizeof(PixType) * (Width + (MarginX << 1)));
    }

    pi -= ((Height - 1) * Stride);
    for (y = 0; y < MarginY; y++)
    {
        ::memcpy(pi - (y + 1) * Stride, pi, sizeof(PixType) * (Width + (MarginX << 1)));
    }
}
*/
void H265Frame::doPadding()
{
    ippiExpandPlane_H264_8u_C1R(y, width, height, pitch_luma, padding, IPPVC_FRAME);
    ippiExpandPlane_H264_8u_C1R(u, width>>1, height>>1, pitch_chroma, padding>>1, IPPVC_FRAME);
    ippiExpandPlane_H264_8u_C1R(v, width>>1, height>>1, pitch_chroma, padding>>1, IPPVC_FRAME);
/*    doPaddingPlane(y, pitch_luma, width, height, padding, padding);
    doPaddingPlane(u, pitch_chroma, width>>1, height>>1, padding>>1, padding>>1);
    doPaddingPlane(v, pitch_chroma, width>>1, height>>1, padding>>1, padding>>1);*/
}

void H265Frame::Dump(H265VideoParam *par, H265FrameList *dpb, Ipp32s frame_num)
{
#ifdef DUMP_RECON
//    char fname[128];
    char *fname = "rec.yuv";
//    mfxU8 buf[2048];
    FILE *f;
    mfxU8* fbuf = NULL;
    Ipp32s W = par->Width - par->CropRight;
    Ipp32s H = par->Height - par->CropBottom;
//    mfxU8* fbuf = 0;
//    sprintf(fname, "rec%dx%d_l%d_q%d.yuv", W, H, core_enc->m_svc_layer.svc_ext.dependency_id, core_enc->m_svc_layer.svc_ext.quality_id);

    Ipp32s numlater = 0; // number of dumped frames with later POC
    if (m_PicCodType == MFX_FRAMETYPE_B) {
        H265Frame *pFrm;
        for (pFrm = dpb->head(); pFrm; pFrm = pFrm->m_pFutureFrame) {
            if (pFrm->m_wasEncoded && m_PicOrderCnt < pFrm->m_PicOrderCnt)
                numlater++;
        }
    }
    if ( numlater ) { // simple reorder for B: read last ref, replacing with B, write ref
        fbuf = new mfxU8[numlater*W*H*3/2];
        f = fopen(fname,"r+b");
        fseek(f, -numlater*W*H*3/2, SEEK_END);
        fread(fbuf, 1, numlater*W*H*3/2, f);
        //fclose(f);
        //f = fopen(fname,"r+b");
        fseek(f, -numlater*W*H*3/2, SEEK_END);
    } else {
        f = fopen(fname, frame_num ? "a+b" : "wb");
    }

    if (f == NULL)
        return;

    int i;//,j;
    mfxU8 *p = y;
    for (i = 0; i < H; i++) {
        fwrite(p, 1, W, f);
        p += pitch_luma;
    }
    p = u;
    for (i = 0; i < H >> 1; i++) {
/*        for (j = 0; j < W >> 1; j++) {
            buf[j] = p[j];
        }*/
        fwrite(p, 1, W >> 1, f);
        p += pitch_chroma;
    }
    p = v;
    for (i = 0; i < H >> 1; i++) {
/*        for (j = 0; j < W >> 1; j++) {
            buf[j] = p[j];
        }*/
        fwrite(p, 1, W >> 1, f);
        p += pitch_chroma;
    }
    if (fbuf) {
        fwrite(fbuf, 1, numlater*W*H*3/2, f);
        delete[] fbuf;
    }
    fclose(f);
#endif
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
    H265Frame *pCurr = m_pHead;
    H265Frame *pFrm = NULL;
    Ipp32s maxDeltaPOC = 0x7fffffff;

    while (pCurr)
    {
        if (pCurr->isShortTermRef())
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
        if (pFrm->Create(par) != MFX_ERR_NONE) return NULL;
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
    return pFrm;
}

#endif // MFX_ENABLE_H265_VIDEO_ENCODE
