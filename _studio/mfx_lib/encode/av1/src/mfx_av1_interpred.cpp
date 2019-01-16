// Copyright (c) 2014-2019 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "mfx_common.h"

#if defined (MFX_ENABLE_AV1_VIDEO_ENCODE)

#include "mfx_av1_defs.h"
#include "mfx_av1_tables.h"
#include "mfx_av1_ctb.h"
#include "mfx_av1_enc.h"
#include "mfx_av1_dispatcher_wrappers.h"

namespace AV1Enc {
    enum EnumAddAverageType { AVERAGE_NO = 0, AVERAGE_FROM_PIC, AVERAGE_FROM_BUF };

template <typename PixType>
int32_t AV1CU<PixType>::ClipMV(AV1MV *rcMv) const
{
    int32_t change = 0;
    if (rcMv->mvx < HorMin) {
        change = 1;
        rcMv->mvx = (int16_t)HorMin;
    } else if (rcMv->mvx > HorMax) {
        change = 1;
        rcMv->mvx = (int16_t)HorMax;
    }
    if (rcMv->mvy < VerMin) {
        change = 1;
        rcMv->mvy = (int16_t)VerMin;
    } else if (rcMv->mvy > VerMax) {
        change = 1;
        rcMv->mvy = (int16_t)VerMax;
    }

    return change;
}


template <typename PixType, int32_t PLANE_TYPE>
PixType *GetRefPointer(FrameData *refFrame, int32_t blockX, int32_t blockY, const AV1MV &mv, int32_t chromaShiftH)
{
    return (PLANE_TYPE == PLANE_TYPE_Y)
        ? (PixType*)refFrame->y  + blockX + (mv.mvx >> 3) + (blockY + (mv.mvy >> 3)) * refFrame->pitch_luma_pix
        : (PixType*)refFrame->uv + blockX + (mv.mvx >> 4 << 1) + ((blockY >> chromaShiftH) + (mv.mvy >> (3+chromaShiftH))) * refFrame->pitch_chroma_pix;
}

template <typename PixType>
template <int32_t PLANE_TYPE>
void AV1CU<PixType>::PredInterUni(int32_t puX, int32_t puY, int32_t puW, int32_t puH, int32_t listIdx,
                                   const int8_t refIdx[2], const AV1MV mvs[2], PixType *dst, int32_t dstPitch,
                                   int32_t isBiPred, int32_t averageMode)
{
    const int32_t isLuma = (PLANE_TYPE == PLANE_TYPE_Y);
    const RefPicList *refPicList = m_currFrame->m_refPicList;
    FrameData *ref = m_currFrame->m_dpbVP9[ m_currFrame->refFrameIdx[refIdx[listIdx]] ]->m_recon;

    int32_t listIdx2 = !listIdx;

    int32_t bitDepth, tap, dx, dy;
    int32_t dx2, dy2;
    if (PLANE_TYPE == PLANE_TYPE_Y) {
        bitDepth = m_par->bitDepthLuma;
        tap = 8;
        dx = (mvs[listIdx].mvx << 1) & 15;
        dy = (mvs[listIdx].mvy << 1) & 15;
        if (isBiPred) {
            dx2 = (mvs[listIdx2].mvx << 1) & 15;
            dy2 = (mvs[listIdx2].mvy << 1) & 15;
        }
    } else {
        puW >>= m_par->chromaShiftW;
        puH >>= m_par->chromaShiftH;
        bitDepth = m_par->bitDepthChroma;
        tap = 4;
        int32_t shiftHor = 3 + m_par->chromaShiftW;
        int32_t shiftVer = 3 + m_par->chromaShiftH;
        int32_t maskHor = (1 << shiftHor) - 1;
        int32_t maskVer = (1 << shiftVer) - 1;

        dx = mvs[listIdx].mvx & maskHor;
        dy = mvs[listIdx].mvy & maskVer;
        dx <<= m_par->chromaShiftWInv;
        dy <<= m_par->chromaShiftHInv;

        if (isBiPred) {
            dx2 = mvs[listIdx2].mvx & maskHor;
            dy2 = mvs[listIdx2].mvy & maskVer;
            dx2 <<= m_par->chromaShiftWInv;
            dy2 <<= m_par->chromaShiftHInv;
        }
    }

    int32_t srcPitch2 = 0;
    PixType *src2 = NULL;
    if (averageMode == AVERAGE_FROM_PIC) {
        int8_t refIdx2 = refIdx[listIdx2];
        //FrameData *ref2 = refPicList[listIdx2].m_refFrames[refIdx2]->m_recon;
        FrameData *ref2 = m_currFrame->m_dpbVP9[m_currFrame->refFrameIdx[refIdx2]]->m_recon;

        srcPitch2 = isLuma ? ref2->pitch_luma_pix : ref2->pitch_chroma_pix;
        src2 = GetRefPointer<PixType, PLANE_TYPE>(ref2, (int32_t)m_ctbPelX + puX, (int32_t)m_ctbPelY + puY, mvs[listIdx2], m_par->chromaShiftH);
    }

    int32_t srcPitch = isLuma ? ref->pitch_luma_pix : ref->pitch_chroma_pix;
    PixType *src = GetRefPointer<PixType, PLANE_TYPE>(ref, (int32_t)m_ctbPelX + puX, (int32_t)m_ctbPelY + puY, mvs[listIdx], m_par->chromaShiftH);

    //int32_t dstBufPitch = MAX_CU_SIZE << (!isLuma ? m_par->chromaShiftWInv : 0);
    int32_t dstBufPitch = UMC::align_value<int32_t>(puW << (PLANE_TYPE == PLANE_TYPE_UV), 16);
    int16_t *dstBuf = m_scratchPad.interp.predInterUni.interpBuf;
    int16_t *preAvgTmpBuf = m_scratchPad.interp.interpWithAvg.tmpBuf;

    int32_t dstPicPitch = dstPitch;
    PixType *dstPic = dst;

    int32_t shift = 6;
    int32_t offset = 32;

    if (isBiPred) {
        shift = bitDepth - 8;
        offset = 0;
    }

    const int32_t log2PuW = BSR(puW);
    if (PLANE_TYPE == PLANE_TYPE_Y) {
        AV1PP::interp_vp9(src, srcPitch, dstPic, dstPicPitch, dx, dy, log2PuW, puH, 0, DEF_FILTER);
        if (isBiPred)
            AV1PP::interp_vp9(src2, srcPitch2, dstPic, dstPicPitch, dx2, dy2, log2PuW, puH, 1, DEF_FILTER);
    } else {
        AV1PP::interp_nv12_vp9(src, srcPitch, dstPic, dstPicPitch, dx, dy, log2PuW, puH, 0, DEF_FILTER);
        if (isBiPred)
            AV1PP::interp_nv12_vp9(src2, srcPitch2, dstPic, dstPicPitch, dx2, dy2, log2PuW, puH, 1, DEF_FILTER);
    }
}
template void AV1CU<uint8_t>::PredInterUni<PLANE_TYPE_Y>(int32_t,int32_t,int32_t,int32_t,int32_t,const int8_t*,const AV1MV*,uint8_t*,int32_t,int32_t,int32_t);
template void AV1CU<uint8_t>::PredInterUni<PLANE_TYPE_UV>(int32_t,int32_t,int32_t,int32_t,int32_t,const int8_t*,const AV1MV*,uint8_t*,int32_t,int32_t,int32_t);
template void AV1CU<uint16_t>::PredInterUni<PLANE_TYPE_Y>(int32_t,int32_t,int32_t,int32_t,int32_t,const int8_t*,const AV1MV*,uint16_t*,int32_t,int32_t,int32_t);
template void AV1CU<uint16_t>::PredInterUni<PLANE_TYPE_UV>(int32_t,int32_t,int32_t,int32_t,int32_t,const int8_t*,const AV1MV*,uint16_t*,int32_t,int32_t,int32_t);


template <typename PixType>
void AV1CU<PixType>::MeInterpolate(const AV1MEInfo* me_info, const AV1MV *MV, PixType *src,
                                    int32_t srcPitch, PixType *dst, int32_t dstPitch) const
{
    int32_t w = me_info->width;
    int32_t h = me_info->height;
    int32_t dx = (MV->mvx << 1) & 15;
    int32_t dy = (MV->mvy << 1) & 15;
    int32_t log2w = BSR(w>>2);

    int32_t refOffset = m_ctbPelX + me_info->posx + (MV->mvx >> 3) + (m_ctbPelY + me_info->posy + (MV->mvy >> 3)) * srcPitch;
    src += refOffset;

    assert (!(dx == 0 && dy == 0));
    if (dy == 0)
        AV1PP::interp_av1(src, srcPitch, dst, dstPitch, dx, dy, h, log2w, 0, DEF_FILTER_DUAL);
    else if (dx == 0)
        AV1PP::interp_av1(src, srcPitch, dst, dstPitch, dx, dy, h, log2w, 0, DEF_FILTER_DUAL);
    else
        AV1PP::interp_av1(src, srcPitch, dst, dstPitch, dx, dy, h, log2w, 0, DEF_FILTER_DUAL);
}

template <typename PixType>
void AV1CU<PixType>::MeInterpolateSave(const AV1MEInfo* me_info, const AV1MV *MV, PixType *src,
    int32_t srcPitch, PixType *dst, int32_t dstPitch) const
{
    int32_t w = me_info->width;
    int32_t h = me_info->height;
    int32_t dx = (MV->mvx << 1) & 15;
    int32_t dy = (MV->mvy << 1) & 15;
    //int32_t log2w = BSR(w >> 2);

    int32_t refOffset = m_ctbPelX + me_info->posx + (MV->mvx >> 3) + (m_ctbPelY + me_info->posy + (MV->mvy >> 3)) * srcPitch;
    src += refOffset;

    assert(!(dx == 0 && dy == 0));
    AV1PP::interp_flex(src, srcPitch, dst, dstPitch, dx, dy, w, h, 0, DEF_FILTER, true);
}

template<typename PixType>
void InterpolateVp9Luma(const PixType *refColoc, int32_t refPitch, PixType *dst, AV1MV mv, int32_t h, int32_t log2w, int32_t avg, int32_t interp)
{
    const int32_t intx = mv.mvx >> 3;
    const int32_t inty = mv.mvy >> 3;
    const int32_t dx = (mv.mvx << 1) & 15;
    const int32_t dy = (mv.mvy << 1) & 15;
    refColoc += intx + inty * refPitch;
    AV1PP::interp_vp9(refColoc, refPitch, dst, dx, dy, h, log2w, avg, interp);
}
template void InterpolateVp9Luma<uint8_t> (const uint8_t*, int32_t,uint8_t*, AV1MV,int32_t,int32_t,int32_t,int32_t);
template void InterpolateVp9Luma<uint16_t>(const uint16_t*,int32_t,uint16_t*,AV1MV,int32_t,int32_t,int32_t,int32_t);

template<typename PixType>
void InterpolateVp9Chroma(const PixType *refColoc, int32_t refPitch, PixType *dst, AV1MV mv, int32_t h, int32_t log2w, int32_t avg, int32_t interp)
{
    const int32_t intx = (mv.mvx >> 4) << 1;
    const int32_t inty = mv.mvy >> 4;
    const int32_t dx = mv.mvx & 15;
    const int32_t dy = mv.mvy & 15;
    refColoc += intx + inty * refPitch;
    AV1PP::interp_nv12_vp9(refColoc, refPitch, dst, dx, dy, h, log2w, avg, interp);
}
template void InterpolateVp9Chroma<uint8_t> (const uint8_t*, int32_t,uint8_t*, AV1MV,int32_t,int32_t,int32_t,int32_t);
template void InterpolateVp9Chroma<uint16_t>(const uint16_t*,int32_t,uint16_t*,AV1MV,int32_t,int32_t,int32_t,int32_t);

template<typename PixType>
void InterpolateAv1SingleRefLuma(const PixType *refColoc, int32_t refPitch, PixType *dst, AV1MV mv, int32_t h, int32_t log2w, int32_t interp)
{
    const int32_t intx = mv.mvx >> 3;
    const int32_t inty = mv.mvy >> 3;
    const int32_t dx = (mv.mvx << 1) & 15;
    const int32_t dy = (mv.mvy << 1) & 15;
    refColoc += intx + inty * refPitch;
    AV1PP::interp_av1(refColoc, refPitch, dst, dx, dy, h, log2w, interp);
}
template void InterpolateAv1SingleRefLuma<uint8_t> (const uint8_t*, int32_t,uint8_t*, AV1MV,int32_t,int32_t,int32_t);
template void InterpolateAv1SingleRefLuma<uint16_t>(const uint16_t*,int32_t,uint16_t*,AV1MV,int32_t,int32_t,int32_t);

template<typename PixType>
void InterpolateAv1SingleRefChroma(const PixType *refColoc, int32_t refPitch, PixType *dst, AV1MV mv, int32_t h, int32_t log2w, int32_t interp)
{
    const int32_t intx = (mv.mvx >> 4) << 1;
    const int32_t inty = mv.mvy >> 4;
    const int32_t dx = mv.mvx & 15;
    const int32_t dy = mv.mvy & 15;
    refColoc += intx + inty * refPitch;
    AV1PP::interp_nv12_av1(refColoc, refPitch, dst, dx, dy, h, log2w, interp);
}
template void InterpolateAv1SingleRefChroma<uint8_t> (const uint8_t*, int32_t,uint8_t*, AV1MV,int32_t,int32_t,int32_t);
template void InterpolateAv1SingleRefChroma<uint16_t>(const uint16_t*,int32_t,uint16_t*,AV1MV,int32_t,int32_t,int32_t);

template<typename PixType>
void InterpolateAv1FirstRefLuma(const PixType *refColoc, int32_t refPitch, int16_t *dst, AV1MV mv, int32_t h, int32_t log2w, int32_t interp)
{
    const int32_t intx = mv.mvx >> 3;
    const int32_t inty = mv.mvy >> 3;
    const int32_t dx = (mv.mvx << 1) & 15;
    const int32_t dy = (mv.mvy << 1) & 15;
    refColoc += intx + inty * refPitch;
    AV1PP::interp_av1(refColoc, refPitch, dst, dx, dy, h, log2w, interp);
}
template void InterpolateAv1FirstRefLuma<uint8_t> (const uint8_t*, int32_t,int16_t*,AV1MV,int32_t,int32_t,int32_t);
template void InterpolateAv1FirstRefLuma<uint16_t>(const uint16_t*,int32_t,int16_t*,AV1MV,int32_t,int32_t,int32_t);

template<typename PixType>
void InterpolateAv1FirstRefChroma(const PixType *refColoc, int32_t refPitch, int16_t *dst, AV1MV mv, int32_t h, int32_t log2w, int32_t interp)
{
    const int32_t intx = (mv.mvx >> 4) << 1;
    const int32_t inty = mv.mvy >> 4;
    const int32_t dx = mv.mvx & 15;
    const int32_t dy = mv.mvy & 15;
    refColoc += intx + inty * refPitch;
    AV1PP::interp_nv12_av1(refColoc, refPitch, dst, dx, dy, h, log2w, interp);
}
template void InterpolateAv1FirstRefChroma<uint8_t> (const uint8_t*, int32_t,int16_t*,AV1MV,int32_t,int32_t,int32_t);
template void InterpolateAv1FirstRefChroma<uint16_t>(const uint16_t*,int32_t,int16_t*,AV1MV,int32_t,int32_t,int32_t);

template<typename PixType>
void InterpolateAv1SecondRefLuma(const PixType *refColoc, int32_t refPitch, int16_t *ref0, PixType *dst, AV1MV mv, int32_t h, int32_t log2w, int32_t interp)
{
    const int32_t intx = mv.mvx >> 3;
    const int32_t inty = mv.mvy >> 3;
    const int32_t dx = (mv.mvx << 1) & 15;
    const int32_t dy = (mv.mvy << 1) & 15;
    refColoc += intx + inty * refPitch;
    AV1PP::interp_av1(refColoc, refPitch, ref0, dst, dx, dy, h, log2w, interp);
}
template void InterpolateAv1SecondRefLuma<uint8_t> (const uint8_t*, int32_t,int16_t*,uint8_t*, AV1MV,int32_t,int32_t,int32_t);
template void InterpolateAv1SecondRefLuma<uint16_t>(const uint16_t*,int32_t,int16_t*,uint16_t*,AV1MV,int32_t,int32_t,int32_t);

template<typename PixType>
void InterpolateAv1SecondRefChroma(const PixType *refColoc, int32_t refPitch, int16_t *ref0, PixType *dst, AV1MV mv, int32_t h, int32_t log2w, int32_t interp)
{
    const int32_t intx = (mv.mvx >> 4) << 1;
    const int32_t inty = mv.mvy >> 4;
    const int32_t dx = mv.mvx & 15;
    const int32_t dy = mv.mvy & 15;
    refColoc += intx + inty * refPitch;
    AV1PP::interp_nv12_av1(refColoc, refPitch, ref0, dst, dx, dy, h, log2w, interp);
}
template void InterpolateAv1SecondRefChroma<uint8_t> (const uint8_t*, int32_t,int16_t*,uint8_t*, AV1MV,int32_t,int32_t,int32_t);
template void InterpolateAv1SecondRefChroma<uint16_t>(const uint16_t*,int32_t,int16_t*,uint16_t*,AV1MV,int32_t,int32_t,int32_t);

template class AV1CU<uint8_t>;
template class AV1CU<uint16_t>;

} // namespace

#endif // MFX_ENABLE_AV1_VIDEO_ENCODE
