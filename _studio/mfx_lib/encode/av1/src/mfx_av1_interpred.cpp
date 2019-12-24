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
template void InterpolateAv1SingleRefLuma<uint16_t>(const uint16_t*, int32_t, uint16_t*, AV1MV, int32_t, int32_t, int32_t);

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

//#if ENABLE_10BIT
template void InterpolateAv1SingleRefLuma<uint16_t>(const uint16_t*, int32_t, uint16_t*, AV1MV, int32_t, int32_t, int32_t);
template void InterpolateAv1SingleRefChroma<uint16_t>(const uint16_t*, int32_t, uint16_t*, AV1MV, int32_t, int32_t, int32_t);
template void InterpolateAv1FirstRefLuma<uint16_t>(const uint16_t*, int32_t, int16_t*, AV1MV, int32_t, int32_t, int32_t);
template void InterpolateAv1FirstRefChroma<uint16_t>(const uint16_t*, int32_t, int16_t*, AV1MV, int32_t, int32_t, int32_t);
template void InterpolateAv1SecondRefLuma<uint16_t>(const uint16_t*, int32_t, int16_t*, uint16_t*, AV1MV, int32_t, int32_t, int32_t);
template void InterpolateAv1SecondRefChroma<uint16_t>(const uint16_t*,int32_t,int16_t*,uint16_t*,AV1MV,int32_t,int32_t,int32_t);
//#endif

#if ENABLE_SW_ME
template <typename PixType>
void AV1CU<PixType>::MeInterpolate(const AV1MEInfo* me_info, const AV1MV *MV, PixType *src,
    int32_t srcPitch, PixType *dst, int32_t dstPitch) const
{
    int32_t w = me_info->width;
    int32_t h = me_info->height;
    int32_t dx = (MV->mvx << 1) & 15;
    int32_t dy = (MV->mvy << 1) & 15;
    int32_t log2w = BSR(w >> 2);

    int32_t refOffset = m_ctbPelX + me_info->posx + (MV->mvx >> 3) + (m_ctbPelY + me_info->posy + (MV->mvy >> 3)) * srcPitch;
    src += refOffset;

    assert(!(dx == 0 && dy == 0));
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

template void AV1CU<uint8_t>::MeInterpolate(const AV1MEInfo *meInfo, const AV1MV *MV, uint8_t *in_pSrc, int32_t in_SrcPitch, uint8_t *buf, int32_t buf_pitch) const;
template void AV1CU<uint8_t>::MeInterpolateSave(const AV1MEInfo* me_info, const AV1MV *MV, uint8_t *src, int32_t srcPitch, uint8_t *dst, int32_t dstPitch) const;
#if ENABLE_10BIT
template void AV1CU<uint16_t>::MeInterpolate(const AV1MEInfo *meInfo, const AV1MV *MV, uint16_t *in_pSrc, int32_t in_SrcPitch, uint16_t *buf, int32_t buf_pitch) const;
template void AV1CU<uint16_t>::MeInterpolateSave(const AV1MEInfo* me_info, const AV1MV *MV, uint16_t *src, int32_t srcPitch, uint16_t *dst, int32_t dstPitch) const;
#endif
#endif
} // namespace

#endif // MFX_ENABLE_AV1_VIDEO_ENCODE
