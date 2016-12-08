//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2004-2016 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined(UMC_ENABLE_H264_VIDEO_ENCODER)

#include <string.h>
#include <stddef.h>
#include <math.h>
#include "ippdefs.h"
#include "ippi.h"
#include "vm_time.h"
#include "umc_h264_video_encoder.h"
#include "umc_h264_core_enc.h"
#include "umc_h264_tables.h"
#include "umc_h264_to_ipp.h"
#include "umc_h264_bme.h"
#include "umc_h264_defs.h"
#include "umc_h264_wrappers.h"
#include "umc_h264_resample.h"

#define PIXTYPE Ipp8u
#define COEFFSTYPE Ipp16s
#define H264ENC_MAKE_NAME(NAME) NAME##_8u16s

#define H264BsBaseType H264ENC_MAKE_NAME(H264BsBase)
#define H264BsRealType H264ENC_MAKE_NAME(H264BsReal)
#define H264BsFakeType H264ENC_MAKE_NAME(H264BsFake)
#define H264CoreEncoderType H264ENC_MAKE_NAME(H264CoreEncoder)
#define H264SliceType H264ENC_MAKE_NAME(H264Slice)
#define H264CurrentMacroblockDescriptorType H264ENC_MAKE_NAME(H264CurrentMacroblockDescriptor)
#define H264EncoderFrameType H264ENC_MAKE_NAME(H264EncoderFrame)

#if defined (__ICL)
//non-pointer conversion from "unsigned __int64" to "Ipp32s={signed int}" may lose significant bits
#pragma warning(disable:2259)
#endif

namespace UMC_H264_ENCODER
{

static int umc_h264_getAddr(Ipp32s m_isMBAFF, Ipp32s frame_width_in_mbs, Ipp32s i, Ipp32s j) {
    if (m_isMBAFF) {
        return (j/2 * (frame_width_in_mbs) + i) * 2 + (j & 1);
    } else {
        return j * (frame_width_in_mbs) + i;
    }
}

///////////////////////////////
// SVC interlayer prediction //
///////////////////////////////

static const Ipp32s NxNPartIdx[6][4] =
{
    { 0, 0, 0, 0 }, // MODE_SKIP (P Slice)
    { 0, 0, 0, 0 }, // MODE_16x16     or    BLK_8x8
    { 0, 0, 2, 2 }, // MODE_16x8      or    BLK_8x4
    { 0, 1, 0, 1 }, // MODE_8x16      or    BLK_4x8
    { 0, 1, 2, 3 }, // MODE_8x8       or    BLK_4x4
    { 0, 1, 2, 3 }  // MODE_8x8ref0
};

static int BicubiFilter[16][4] = {
    { 0, 32,  0,  0},
    {-1, 32,  2, -1},
    {-2, 31,  4, -1},
    {-3, 30,  6, -1},
    {-3, 28,  8, -1},
    {-4, 26, 11, -1},
    {-4, 24, 14, -2},
    {-3, 22, 16, -3},
    {-3, 19, 19, -3},
    {-3, 16, 22, -3},
    {-2, 14, 24, -4},
    {-1, 11, 26, -4},
    {-1,  8, 28, -3},
    {-1,  6, 30, -3},
    {-1,  4, 31, -2},
    {-1,  2, 32, -1}
};

#define INTRA -1
#define SBTYPE_UNDEF -1
#define MBTYPE_UNDEF -1

static Ipp32s shiftTable[] =
{
    0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4
};

static inline Ipp32s countShift(Ipp32s src)
{
    Ipp32s shift;

    if (src == 0)
        return 31;

    shift = 0;
    src--;

    if (src >= 32768)
    {
        src >>= 16;
        shift = 16;
    }

    if (src >= 256)
    {
        src >>= 8;
        shift += 8;
    }

    if (src >= 16)
    {
        src >>= 4;
        shift += 4;
    }

    return (31 - (shift + shiftTable[src]));
}
#define BICFILTER(xPhase, v0, v1, v2, v3) \
    (BicubiFilter[xPhase][0]*(v0) + BicubiFilter[xPhase][1]*(v1) + BicubiFilter[xPhase][2]*(v2) + BicubiFilter[xPhase][3]*(v3))

#define PlaneY Ipp8u
#define PlaneUV Ipp8u
#define Coeffs Ipp16s
#define PlanePtrY PlaneY*
#define PlanePtrUV PlaneUV*

static void umc_h264_MB_mv_resampling(//H264SegmentDecoderMultiThreaded *sd,
                                      //H264DecoderLayerDescriptor *curLayerD,
                                      H264LayerResizeParameter *pResizeParameter,
                                      H264CoreEncoder_8u16s *curEnc,
                                      H264CoreEncoder_8u16s *refEnc,
                                      Ipp32s curMB_X,
                                      Ipp32s curMB_Y,
                                      Ipp32s curMBAddr,
                                      Ipp32s slice_type,
                                      //H264DecoderLayerDescriptor *refLayerMb,
                                      Ipp32s RestrictedSpatialResolutionChangeFlag,
                                      Ipp32s CroppingChangeFlag);

static void CleanResidualsOverIntra (H264CoreEncoder_8u16s *curEnc);

static void bicubicLumaD(Ipp32s srcWidth,
                         Ipp32s srcHeight,
                         Ipp32s srcStride,
                         PlaneY *src,
                         Ipp32s srcOffL,
                         Ipp32s srcOffT,
                         Ipp32s srcOffR,
                         Ipp32s srcOffB,
                         Ipp32s dstWidth,
                         Ipp32s dstHeight,
                         Ipp32s dstStride,
                         PlaneY *dst,
                         Ipp32s scaleX,
                         Ipp32s shiftX,
                         Ipp32s scaleY,
                         Ipp32s shiftY)
{
    Ipp32s ix, iy, pos, phY, phX;
    Ipp32s i, j;

    srcWidth -= srcOffR;
    srcHeight -= srcOffB;

    for (j = 0; j < dstHeight; j++)
    {
        PlaneY*  outPtr = (PlaneY*)((Ipp8u*)dst + j * dstStride);
        iy = (j*scaleY + (1<<(shiftY-3))) >> (shiftY-4); // coord*16 wo offset
        pos = (iy>>4) + srcOffT;
        PlaneY *p0, *p1, *p2, *p3;
        p1 = (PlaneY*)((Ipp8u*)src + pos * srcStride);
        p0 = (pos > 0) ? p1 - srcStride : p1;
        p2 = (pos+1 < srcHeight) ? (PlaneY*)((Ipp8u*)p1 + srcStride) : p1;
        p3 = (pos+2 < srcHeight) ? (PlaneY*)((Ipp8u*)p2 + srcStride) : p2;
        phY = pos & 15;

        Ipp32s acc[8] = {0};
        Ipp32s ia;
        ix = 0;
        pos = (ix>>4) + srcOffL;

        acc[5] = acc[1] = BICFILTER(phY, *p0, *p1, *p2, *p3);
        acc[4] = acc[0] = (srcOffL > 0) ? BICFILTER(phY, p0[-1], p1[-1], p2[-1], p3[-1]) : acc[1];
        acc[6] = acc[2] = (srcOffL+1 < srcWidth) ? BICFILTER(phY, p0[1], p1[1], p2[1], p3[1]) : acc[1];
        p0+=2; p1+=2; p2+=2; p3+=2;
        ia = 3;
        for (i = 0; i < dstWidth; i++, ia = (ia+1)&3)
        {
            ix = (i*scaleX + (1<<(shiftX-3))) >> (shiftX-4); // coord*16 wo offset
            pos = (ix>>4) + srcOffL;
            phX = pos & 15;
            acc[ia+4] = acc[ia] = (pos+2 < srcWidth) ? BICFILTER(phY, p0[pos], p1[pos], p2[pos], p3[pos]) : acc[ia+3];
            outPtr[i] = (Ipp8u)((BICFILTER(phX, acc[ia+1], acc[ia+2], acc[ia+3], acc[ia+0]) + 511) >> 10);
        }
    }
}

Status DownscaleSurface(H264EncoderFrameType* to, H264EncoderFrameType* from, Ipp32u offsets[4])
{
    Ipp32u shiftX, shiftY;
    Ipp32u scaleX, scaleY;
    Ipp32u scaledW, scaledH;
    Ipp32u offl = offsets[0], offt = offsets[1], offr = offsets[2], offb = offsets[3]; // keep array order
    Ipp32u tmp;
    for(shiftX = 0, tmp = to->m_lumaSize.width; tmp<0x80000000; shiftX++, tmp <<= 1);
    for(shiftY = 0, tmp = to->m_lumaSize.height; tmp<0x80000000; shiftY++, tmp <<= 1);
    scaledW = from->m_lumaSize.width - offl - offr;
    scaledH = from->m_lumaSize.height - offt - offb;
    scaleX = ((to->m_lumaSize.width  << shiftX)+(scaledW>>1)) / scaledW;
    scaleY = ((to->m_lumaSize.height << shiftY)+(scaledH>>1)) / scaledH;

    bicubicLumaD(from->m_lumaSize.width, from->m_lumaSize.height, from->m_pitchBytes, from->m_pYPlane,
        offsets[0], offsets[1], offsets[2], offsets[3],
        to->m_lumaSize.width, to->m_lumaSize.height, to->m_pitchBytes, to->m_pYPlane,
        scaleX, shiftX, scaleY, shiftY);


    return UMC_ERR_NOT_IMPLEMENTED; // while does nothing
}


//// UPSAMPLING ////

static void inline saturateAndBorder(Ipp32s width,
                                     Ipp32s height,
                                     Ipp32s srcStride,
                                     Ipp32s *src,
                                     Ipp32s dstLeftOffset,
                                     Ipp32s dstRightOffset,
                                     Ipp32s dstTopOffset,
                                     Ipp32s dstBottomOffset,
                                     Ipp32s dstStride,
                                     Ipp8u  *dst,
                                     Ipp32s verticalInterpolationFlag,
                                     Ipp32s yBorder)
{
    Ipp32s i, j;

    for (j = 0; j < dstTopOffset; j++)
    {
        Ipp8u*  outPtr = dst + j * dstStride;

        for (i = 0; i < width + dstLeftOffset + dstRightOffset; i++)
        {
            outPtr[i] = 128;
        }
    }

    for (j = dstTopOffset; j < height + dstTopOffset; j++)
    {
        Ipp8u*  outPtr = dst + j * dstStride;

        for (i = 0; i < dstLeftOffset; i++)
        {
            outPtr[i] = 128;
        }

        if (verticalInterpolationFlag) {
            Ipp32s* intPtr = (Ipp32s*)((Ipp8u*)src + (yBorder + ((j - dstTopOffset) >> 1)) * srcStride);
            for (i = dstLeftOffset; i < width + dstLeftOffset; i++) {

                if( ( (j - dstTopOffset) % 2 ) == 0/*iBotField */)
                {
                    outPtr[i] = (Ipp8u)intPtr[i - dstLeftOffset]; // already saturated
                }
                else
                {
                    int k = i - dstLeftOffset;
                    Ipp32s acc;

                    acc   = 16;
                    acc  += 19 * ( intPtr[ k                  ] + intPtr[ k +     1 * srcStride/4 ] );
                    acc  -=  3 * ( intPtr[ k - 1 * srcStride/4 ] + intPtr[ k + 2 * srcStride/4 ] );
                    acc >>=  5;

                    if (acc > 255) acc = 255;
                    else if (acc < 0) acc = 0;

                    outPtr[i] = (Ipp8u)acc;
                }
            }
        } else {
            Ipp32s* intPtr = (Ipp32s*)((Ipp8u*)src + (j - dstTopOffset) * srcStride);
            for (i = dstLeftOffset; i < width + dstLeftOffset; i++)
            {
                Ipp32s tmp = intPtr[i - dstLeftOffset];

                if      (tmp < 0)   outPtr[i] = 0;
                else if (tmp > 255) outPtr[i] = 255;
                else                outPtr[i] = (Ipp8u)tmp;
            }
        }

        for (i = 0; i < dstRightOffset; i++)
        {
            outPtr[width + dstLeftOffset + i] = 128;
        }
    }

    for (j = height + dstTopOffset; j < height + dstTopOffset + dstBottomOffset; j++)
    {
        Ipp8u*  outPtr = dst + j * dstStride;

        for (i = 0; i < width + dstLeftOffset + dstRightOffset; i++)
        {
            outPtr[i] = 128;
        }
    }
}

static void bicubicLuma(Ipp32s srcWidth,
                        Ipp32s srcHeight,
                        Ipp32s srcStride,
                        PlaneY *src,
                        Ipp32s dstXstart,
                        Ipp32s dstXend,
                        Ipp32s dstYstart,
                        Ipp32s dstYend,
                        Ipp32s dstStride,
                        Ipp32s *dst,
                        Ipp32s scaleX,
                        Ipp32s addX,
                        Ipp32s shiftX,
                        Ipp32s scaleY,
                        Ipp32s addY,
                        Ipp32s shiftY,
                        Ipp32s deltaY,
                        Ipp32s *tmpBuf,
                        Ipp32s  yBorder)
{
    Ipp32s srcYstart, srcYend;
    Ipp32s i, j, k;

    i = ((dstYstart * scaleY + addY) >> (shiftY - 4)) - deltaY;
    srcYstart = (i >> 4) - 1;
    if (srcYstart < 0) srcYstart = 0;


    i = (((dstYend - 1) * scaleY + addY) >> (shiftY - 4)) - deltaY;
    srcYend = (i >> 4) + 3;
    if (srcYend >= srcHeight) srcYend = srcHeight;

    for (j = srcYstart; j < srcYend; j++)
    {
        PlaneY*  intPtr = (PlaneY*)((Ipp8u*)src + j * srcStride);
        Ipp32s*  outPtr = (Ipp32s*)((Ipp8u*)dst + j * dstStride);

        for (i = dstXstart; i < dstXend; i++)
        {
            Ipp32s acc = 0;
            Ipp32s xRef16 = ((i * scaleX + addX) >> (shiftX - 4)) - 8;
            Ipp32s xRef = xRef16 >> 4;
            Ipp32s xPhase = xRef16 & 0xf;

            acc = 0;
            for (k = 0; k < 4; k++)
            {
                Ipp32s ind = xRef - 1 + k;
                if (ind < 0) ind = 0;
                else if (ind > (srcWidth - 1)) ind = srcWidth - 1;

                acc += BicubiFilter[xPhase][k]*intPtr[ind];
            }
            outPtr[i] = acc;
        }
    }

    for (i = dstXstart; i < dstXend; i++)
    {
        Ipp8u*  intPtr = (Ipp8u *)(dst + i);

        for (j = dstYstart - yBorder; j < dstYend + yBorder; j++)
        {
            Ipp32s acc = 0;
            Ipp32s yRef16 = ((j * scaleY + addY) >> (shiftY - 4)) - deltaY;
            Ipp32s yRef = yRef16 >> 4;
            Ipp32s yPhase = yRef16 & 0xf;

            acc = 0;
            for (k = 0; k < 4; k++)
            {
                Ipp32s ind = yRef - 1 + k;
                if (ind < 0) ind = 0;
                else if (ind > (srcHeight - 1)) ind = srcHeight - 1;

                acc += BicubiFilter[yPhase][k]*
                    (*((Ipp32s*)((Ipp8u*)intPtr + ind * dstStride)));
            }
            tmpBuf[j + yBorder] = (acc + 512) >> 10;
            if (tmpBuf[j + yBorder] > 255) tmpBuf[j + yBorder] = 255;
            else if (tmpBuf[j + yBorder] < 0) tmpBuf[j + yBorder] = 0;
        }

        intPtr += (dstYstart) * dstStride;

        for (j = dstYstart - yBorder; j < dstYend + yBorder; j++)
        {
            *(Ipp32s*)intPtr = tmpBuf[j + yBorder]; // PK
            intPtr += dstStride;
        }
    }
}

static void bilinearChromaConstrained(Ipp32s  srcWidth,
                                      Ipp32s  srcHeight,
                                      Ipp32s  srcStride,
                                      PlaneUV *src,
                                      Ipp32s  dstXstart,
                                      Ipp32s  dstXend,
                                      Ipp32s  dstYstart,
                                      Ipp32s  dstYend,
                                      Ipp32s  dstStride,
                                      Ipp32s  *dst,
                                      Ipp32s  scaleX,
                                      Ipp32s  addX,
                                      Ipp32s  shiftX,
                                      Ipp32s  deltaX,
                                      Ipp32s  scaleY,
                                      Ipp32s  addY,
                                      Ipp32s  shiftY,
                                      Ipp32s  deltaY,
                                      Ipp32s  *tmpBuf)
{
    Ipp32s srcYstart, srcYend;
    Ipp32s i, j;
    const Ipp32s mulNV12 = 2; // NV12 only

    i = ((dstYstart * scaleY + addY) >> (shiftY - 4)) - deltaY;
    srcYstart = (i >> 4);
    if (srcYstart < 0) srcYstart = 0;


    i = (((dstYend - 1) * scaleY + addY) >> (shiftY - 4)) - deltaY;
    srcYend = (i >> 4) + 2;
    if (srcYend >= srcHeight) srcYend = srcHeight;

    for (j = srcYstart; j < srcYend; j++)
    {
        PlaneUV* intPtr = (PlaneUV*)((Ipp8u*)src + j * srcStride);
        Ipp32s*  outPtr = (Ipp32s*)((Ipp8u*)dst +  j * dstStride);

        for (i = dstXstart; i < dstXend; i++)
        {
            Ipp32s xRef16 = ((i * scaleX + addX) >> (shiftX - 4)) - deltaX;
            Ipp32s xRef = xRef16 >> 4;
            Ipp32s xPhase = xRef16 & 0xf;
            Ipp32s ind0, ind1;

            ind0 = xRef;
            if (ind0 < 0) ind0 = 0;
            else if (ind0 > (srcWidth - 1)) ind0 = srcWidth - 1;

            ind1 = xRef + 1;
            if (ind1 < 0) ind1 = 0;
            else if (ind1 > (srcWidth - 1)) ind1 = srcWidth - 1;

            outPtr[i*mulNV12] = (16 - xPhase) * intPtr[ind0*mulNV12] + xPhase * intPtr[ind1*mulNV12];
        }
    }

    for (i = dstXstart; i < dstXend; i++)
    {
        Ipp32s*  intPtr = dst + i*mulNV12;

        for (j = dstYstart; j < dstYend; j++)
        {
            Ipp32s yRef16 = ((j * scaleY + addY) >> (shiftY - 4)) - deltaY;
            Ipp32s yRef = yRef16 >> 4;
            Ipp32s yPhase = yRef16 & 0xf;
            Ipp32s ind0, ind1;

            ind0 = yRef;
            if (ind0 < 0) ind0 = 0;
            else if (ind0 > (srcHeight - 1)) ind0 = srcHeight - 1;

            ind1 = yRef + 1;
            if (ind1 < 0) ind1 = 0;
            else if (ind1 > (srcHeight - 1)) ind1 = srcHeight - 1;

            tmpBuf[j] = (((16 - yPhase) * intPtr[ind0*dstStride/4] +
                yPhase  * intPtr[ind1*dstStride/4] + 128) >> 8);
        }

        intPtr += dstYstart * (dstStride / 4);

        for (j = dstYstart; j < dstYend; j++)
        {
            *(Ipp32s*)intPtr = tmpBuf[j];
            intPtr += (dstStride / 4);
        }
    }
}

static inline void border(Ipp32s width,
                          Ipp32s height,
                          Ipp32s stride,
                          Ipp32s leftOffset,
                          Ipp32s rightOffset,
                          Ipp32s topOffset,
                          Ipp32s bottomOffset,
                          Ipp8u *dst,
                          Ipp32s nv12_flag)
{
    Ipp32s i, j;
    if (nv12_flag) nv12_flag = 1;

    for (j = 0; j < topOffset; j++)
    {
        Ipp8u*  outPtr = dst + j * stride;

        for (i = 0; i < width + leftOffset + rightOffset; i++)
        {
            outPtr[i << nv12_flag] = 128;
        }
    }

    for (j = topOffset; j < height + topOffset; j++)
    {
        Ipp8u*  outPtr = dst + j * stride;

        for (i = 0; i < leftOffset; i++)
        {
            outPtr[i << nv12_flag] = 128;
        }

        for (i = 0; i < rightOffset; i++)
        {
            outPtr[(width + leftOffset + i) << nv12_flag] = 128;
        }
    }

    for (j = height + topOffset; j < height + topOffset + bottomOffset; j++)
    {
        Ipp8u*  outPtr = dst + j * stride;

        for (i = 0; i < width + leftOffset + rightOffset; i++)
        {
            outPtr[i << nv12_flag] = 128;
        }
    }
}

static inline void border(Ipp32s width,
                          Ipp32s height,
                          Ipp32s stride,
                          Ipp32s leftOffset,
                          Ipp32s rightOffset,
                          Ipp32s topOffset,
                          Ipp32s bottomOffset,
                          Ipp32s *dst)
{
    Ipp32s i, j;

    for (j = 0; j < topOffset; j++)
    {
        Ipp32s*  outPtr = (Ipp32s*)((Ipp8u*)dst + j * stride);

        for (i = 0; i < width + leftOffset + rightOffset; i++)
        {
            outPtr[i] = 128;
        }
    }

    for (j = topOffset; j < height + topOffset; j++)
    {
        Ipp32s*  outPtr = (Ipp32s*)((Ipp8u*)dst + j * stride);

        for (i = 0; i < leftOffset; i++)
        {
            outPtr[i] = 128;
        }

        for (i = 0; i < rightOffset; i++)
        {
            outPtr[width + leftOffset + i] = 128;
        }
    }

    for (j = height + topOffset; j < height + topOffset + bottomOffset; j++)
    {
        Ipp32s*  outPtr = (Ipp32s*)((Ipp8u*)dst + j * stride);

        for (i = 0; i < width + leftOffset + rightOffset; i++)
        {
            outPtr[i] = 128;
        }
    }
}

void umc_h264_upsampling(H264LayerResizeParameter *pResizeParameter,
                         H264CoreEncoder_8u16s *curEnc,
                         PlaneY  *pOutYPlane,
                         PlaneUV *pOutUPlane,
                         PlaneY  *pRefYPlane,
                         PlaneUV *pRefUPlane,
                         Ipp32s   outLumaStride,
                         Ipp32s   outChromaStride,
                         Ipp32s   refLumaStride,
                         Ipp32s   refChromaStride)
{
    ippiUpsampleIntraLuma_SVC_8u_C1R(pRefYPlane, refLumaStride,
        pOutYPlane + (pResizeParameter->mbRegionFull.x << 4) +
            (pResizeParameter->mbRegionFull.y << 4) * outLumaStride,
        outLumaStride, curEnc->svc_UpsampleIntraLumaSpec,
        pResizeParameter->mbRegionFull, curEnc->svc_UpsampleBuf);

    ippiUpsampleIntraChroma_SVC_8u_C2R(pRefUPlane, refChromaStride,
        pOutUPlane + (pResizeParameter->mbRegionFull.x << 4) +
            (pResizeParameter->mbRegionFull.y << 3) * outChromaStride,
        outChromaStride, curEnc->svc_UpsampleIntraChromaSpec,
        pResizeParameter->mbRegionFull, curEnc->svc_UpsampleBuf);
}

void diagonalConstruction(PlaneY  *srcdstY,
                          Ipp32s  strideY,
                          PlaneUV *srcdstU,
                          Ipp32s  strideU,
                          PlaneUV *srcdstV,
                          Ipp32s  strideV,
                          Ipp32s  quarter,
                          Ipp32s isCorner,
                          Ipp32s half)
{
    Ipp32s hor[9], vert[9];
    PlaneUV *srcdst;
    PlaneY *ptrYH, *ptrYV;
    Ipp32s stride, strideX;
    Ipp32s x;
    Ipp32s i, j, ii;
    const Ipp32s mulNV12 = 2; // NV12 only

    strideX = 1;

    if (quarter >= 2)
    {
        srcdstY = (PlaneY*)((Ipp8u*)srcdstY + 7 * strideY);
        srcdstU = (PlaneUV*)((Ipp8u*)srcdstU + 3 * strideU);
        srcdstV = (PlaneUV*)((Ipp8u*)srcdstV + 3 * strideV);
        strideY = -strideY;
        strideU = -strideU;
        strideV = -strideV;
    }
    if ((quarter & 1) == 1)
    {
        srcdstY += 7;
        srcdstU += 3*mulNV12;
        srcdstV += 3*mulNV12;
        strideX = -1;
    }

    /* Luma */

    ptrYH = (PlaneY*)((Ipp8u*)srcdstY - strideY);
    ptrYV = srcdstY - strideX;

    for (i = 1; i < 9; i++)
    {
        hor[i] = (Ipp32s)(*ptrYH);
        ptrYH += strideX;
    }

    for (i = 1; i < ((8 >> half) + 1); i++)
    {
        vert[i] = (Ipp32s)(*ptrYV);
        ptrYV = (PlaneY*)((Ipp8u*)ptrYV + strideY);
    }

    if (isCorner)
    {
        ptrYH = ((PlaneY*)((Ipp8u*)srcdstY - strideY)) - strideX;
        hor[0] = vert[0] =  (Ipp32s)ptrYH[0];
    }
    else
    {
        hor[0] = vert[0] = (hor[1] + vert[1] + 1) >> 1;
    }

    x = (hor[1] + vert[0] * 2 + vert[1] + 2) >> 2;

    for (j = 0; j < (8 >> half); j++)
    {
        PlaneY *ptrY = srcdstY;

        for (i = 0; i < j; i++)
        {
            *ptrY = (PlaneY)((vert[j-i-1] + 2 * vert[j-i] + vert[j-i+1] + 2) >> 2);
            ptrY += strideX;
        }

        *ptrY = (PlaneY)x;
        ptrY += strideX;

        for (i = j + 1; i < 8; i++)
        {
            *ptrY = (PlaneY)((hor[i-j-1] + 2 * hor[i-j] + hor[i-j+1] + 2) >> 2);
            ptrY += strideX;
        }

        srcdstY = (PlaneY*)((Ipp8u*)srcdstY + strideY);
    }

    /* Chroma */
    strideX *= mulNV12;

    srcdst = srcdstU;
    stride = strideU;

    for (ii = 0; ii < 2; ii++)
    {
        PlaneUV *ptrH, *ptrV;

        ptrH = (PlaneUV*)((Ipp8u*)srcdst - stride);
        ptrV = srcdst - strideX;

        for (i = 1; i < 5; i++)
        {
            hor[i] = (Ipp32s)(*ptrH);
            ptrH += strideX;
        }

        for (i = 1; i < (4 >> half) + 1; i++)
        {
            vert[i] = (Ipp32s)(*ptrV);
            ptrV = (PlaneUV*)((Ipp8u*)ptrV + stride);
        }

        if (isCorner)
        {
            ptrH = ((PlaneUV*)((Ipp8u*)srcdst - stride)) - strideX;
            hor[0] = vert[0] =  (Ipp32s)ptrH[0];
        }
        else
        {
            hor[0] = vert[0] = (hor[1] + vert[1] + 1) >> 1;
        }

        x = (hor[1] + vert[0] * 2 + vert[1] + 2) >> 2;

        for (j = 0; j < (4 >> half); j++)
        {
            PlaneUV *ptr = srcdst;

            for (i = 0; i < j; i++)
            {
                *ptr = (PlaneUV)((vert[j-i-1] + 2 * vert[j-i] + vert[j-i+1] + 2) >> 2);
                ptr += strideX;
            }

            *ptr = (PlaneUV)x;
            ptr += strideX;

            for (i = j + 1; i < 4; i++)
            {
                *ptr = (PlaneUV)((hor[i-j-1] + 2 * hor[i-j] + hor[i-j+1] + 2) >> 2);
                ptr += strideX;
            }

            srcdst = (PlaneUV*)((Ipp8u*)srcdst + stride);
        }

        srcdst = srcdstV;
        stride = strideV;
    }
}

void fillPlanes(PlaneY  *srcdstY,
                Ipp32s  strideY,
                PlaneUV *srcdstU,
                Ipp32s  strideU,
                PlaneUV *srcdstV,
                Ipp32s  strideV,
                Ipp32s  y,
                Ipp32s  u,
                Ipp32s  v,
                Ipp32s half)
{
    Ipp32s i, j;
    const Ipp32s mulNV12 = 2; // NV12 only!!

    /* Luma */

    for (j = 0; j < (8 >> half); j++)
    {
        for (i = 0; i < 8; i++)
        {
            srcdstY[i] = (PlaneY)y;
        }

        srcdstY = (PlaneY*)((Ipp8u*)srcdstY + strideY);
    }

    /* Chroma */

    for (j = 0; j < (4 >> half); j++)
    {
        for (i = 0; i < 4; i++)
        {
            srcdstU[i*mulNV12] = (PlaneUV)u;
        }

        srcdstU = (PlaneUV*)((Ipp8u*)srcdstU + strideU);
    }

    for (j = 0; j < (4 >> half); j++)
    {
        for (i = 0; i < 4; i++)
        {
            srcdstV[i*mulNV12] = (PlaneUV)v;
        }

        srcdstV = (PlaneUV*)((Ipp8u*)srcdstV + strideV);
    }
}

void copyPlanesFromAboveBelow(PlaneY  *srcdstY,
                              Ipp32s  strideY,
                              PlaneUV *srcdstU,
                              Ipp32s  strideU,
                              PlaneUV *srcdstV,
                              Ipp32s  strideV,
                              Ipp32s above,
                              Ipp32s half)
{
    PlaneY  *ptrY;
    PlaneUV *ptrU;
    PlaneUV *ptrV;
    Ipp32s i, j;
    const Ipp32s mulNV12 = 2; // NV12 only

    if (above)
    {
        ptrY = (PlaneY*)((Ipp8u*)srcdstY - strideY);
        ptrU = (PlaneUV*)((Ipp8u*)srcdstU - strideU);
        ptrV = (PlaneUV*)((Ipp8u*)srcdstV - strideV);
    }
    else
    {
        ptrY = (PlaneY*)((Ipp8u*)srcdstY + 8 * strideY);
        ptrU = (PlaneUV*)((Ipp8u*)srcdstU + 4 * strideU);
        ptrV = (PlaneUV*)((Ipp8u*)srcdstV + 4 * strideV);
        if (half) {
            srcdstY = (PlaneY*)((Ipp8u*)srcdstY + 4 * strideY);
            srcdstU = (PlaneUV*)((Ipp8u*)srcdstU + 2 * strideU);
            srcdstV = (PlaneUV*)((Ipp8u*)srcdstV + 2 * strideV);
        }
    }

    /* Luma */

    for (j = 0; j < (8 >> half); j++)
    {
        for (i = 0; i < 8; i++)
        {
            srcdstY[i] = ptrY[i];
        }

        srcdstY = (PlaneY*)((Ipp8u*)srcdstY + strideY);
    }

    /* Chroma */

    for (j = 0; j < (4 >> half); j++)
    {
        for (i = 0; i < 4; i++)
        {
            srcdstU[i*mulNV12] = ptrU[i*mulNV12];
        }

        srcdstU = (PlaneUV*)((Ipp8u*)srcdstU + strideU);
    }

    for (j = 0; j < (4 >> half); j++)
    {
        for (i = 0; i < 4; i++)
        {
            srcdstV[i*mulNV12] = ptrV[i*mulNV12];
        }

        srcdstV = (PlaneUV*)((Ipp8u*)srcdstV + strideV);
    }
}

void copyPlanesFromLeftRight(PlaneY  *srcdstY,
                             Ipp32s  strideY,
                             PlaneUV *srcdstU,
                             Ipp32s  strideU,
                             PlaneUV *srcdstV,
                             Ipp32s  strideV,
                             Ipp32s  left)
{
    PlaneY  *ptrY;
    PlaneUV *ptrU;
    PlaneUV *ptrV;
    Ipp32s i, j;
    const Ipp32s mulNV12 = 2; // NV12 only

    if (left)
    {
        ptrY = srcdstY - 1;
        ptrU = srcdstU - 1*mulNV12;
        ptrV = srcdstV - 1*mulNV12;
    }
    else
    {
        ptrY = srcdstY + 8;
        ptrU = srcdstU + 4*mulNV12;
        ptrV = srcdstV + 4*mulNV12;
    }

    /* Luma */

    for (j = 0; j < 8; j++)
    {
        for (i = 0; i < 8; i++)
        {
            srcdstY[i] = ptrY[0];
        }

        srcdstY = (PlaneY*)((Ipp8u*)srcdstY + strideY);
        ptrY = (PlaneY*)((Ipp8u*)ptrY + strideY);
    }

    /* Chroma */

    for (j = 0; j < 4; j++)
    {
        for (i = 0; i < 4; i++)
        {
            srcdstU[i*mulNV12] = ptrU[0];
        }

        srcdstU = (PlaneUV*)((Ipp8u*)srcdstU + strideU);
        ptrU = (PlaneUV*)((Ipp8u*)ptrU + strideU);
    }

    for (j = 0; j < 4; j++)
    {
        for (i = 0; i < 4; i++)
        {
            srcdstV[i*mulNV12] = ptrV[0];
        }

        srcdstV = (PlaneUV*)((Ipp8u*)srcdstV + strideV);
        ptrV = (PlaneUV*)((Ipp8u*)ptrV + strideV);
    }
}

void fillMB(PlaneY *tmpPtrY,
                   Ipp32s strideY,
                   PlaneUV *tmpPtrU,
                   Ipp32s strideU,
                   PlaneUV *tmpPtrV,
                   Ipp32s strideV,
                   Ipp32s mask)
{
    Ipp32s isAboveIntra      = 0 != (mask & 0x01);
    Ipp32s isBelowIntra      = 0 != (mask & 0x10);
    Ipp32s isLeftIntra       = 0 != (mask & 0x40);
    Ipp32s isRightIntra      = 0 != (mask & 0x04);
    Ipp32s isAboveLeftIntra  = 0 != (mask & 0x80);
    Ipp32s isAboveRightIntra = 0 != (mask & 0x02);
    Ipp32s isBelowLeftIntra  = 0 != (mask & 0x20);
    Ipp32s isBelowRightIntra = 0 != (mask & 0x08);
    const Ipp32s mulNV12 = 2; // NV12 only

    /* 0 quarter */

    if (isLeftIntra)
    {
        if (isAboveIntra)
        {
            diagonalConstruction(tmpPtrY, strideY, tmpPtrU, strideU,
                tmpPtrV, strideV, 0, isAboveLeftIntra, 0);
        }
        else
        {
            copyPlanesFromLeftRight(tmpPtrY, strideY, tmpPtrU, strideU,
                tmpPtrV, strideV, 1);

        }
    }
    else if (isAboveIntra)
    {
        copyPlanesFromAboveBelow(tmpPtrY, strideY, tmpPtrU, strideU,
            tmpPtrV, strideV, 1, 0);
    }
    else if (isAboveLeftIntra)
    {
        fillPlanes(tmpPtrY, strideY, tmpPtrU, strideU, tmpPtrV, strideV,
            (Ipp32s)(*(((PlaneY*)((Ipp8u*)tmpPtrY - strideY)) - 1)),
            (Ipp32s)(*(((PlaneUV*)((Ipp8u*)tmpPtrU - strideU)) - 1*mulNV12)),
            (Ipp32s)(*(((PlaneUV*)((Ipp8u*)tmpPtrV - strideV)) - 1*mulNV12)), 0);
    }
    else
    {
        fillPlanes(tmpPtrY, strideY, tmpPtrU, strideU, tmpPtrV, strideV,
            0, 0, 0, 0);
    }

    /* 1 quarter */

    if (isRightIntra)
    {
        if (isAboveIntra)
        {
            diagonalConstruction(tmpPtrY + 8, strideY, tmpPtrU + 4*mulNV12, strideU,
                tmpPtrV + 4*mulNV12, strideV, 1, isAboveRightIntra, 0);
        }
        else
        {
            copyPlanesFromLeftRight(tmpPtrY + 8, strideY, tmpPtrU + 4*mulNV12, strideU,
                tmpPtrV + 4*mulNV12, strideV, 0);

        }
    }
    else if (isAboveIntra)
    {
        copyPlanesFromAboveBelow(tmpPtrY + 8, strideY, tmpPtrU + 4*mulNV12, strideU,
            tmpPtrV + 4*mulNV12, strideV, 1, 0);
    }
    else if (isAboveRightIntra)
    {
        fillPlanes(tmpPtrY + 8, strideY, tmpPtrU + 4*mulNV12, strideU, tmpPtrV + 4*mulNV12, strideV,
            (Ipp32s)(*(((PlaneY*)((Ipp8u*)tmpPtrY - strideY)) + 16)),
            (Ipp32s)(*(((PlaneUV*)((Ipp8u*)tmpPtrU - strideU)) + 8*mulNV12)),
            (Ipp32s)(*(((PlaneUV*)((Ipp8u*)tmpPtrV - strideV)) + 8*mulNV12)), 0);
    }
    else
    {
        fillPlanes(tmpPtrY + 8, strideY, tmpPtrU + 4*mulNV12, strideU, tmpPtrV + 4*mulNV12, strideV,
            0, 0, 0, 0);
    }

    tmpPtrY = (PlaneY*)((Ipp8u*)tmpPtrY + 8 * strideY);
    tmpPtrU = (PlaneY*)((Ipp8u*)tmpPtrU + 4 * strideU);
    tmpPtrV = (PlaneY*)((Ipp8u*)tmpPtrV + 4 * strideV);

    /* 2 quarter */

    if (isLeftIntra)
    {
        if (isBelowIntra)
        {
            diagonalConstruction(tmpPtrY, strideY, tmpPtrU, strideU,
                tmpPtrV, strideV, 2, isBelowLeftIntra, 0);
        }
        else
        {
            copyPlanesFromLeftRight(tmpPtrY, strideY, tmpPtrU, strideU,
                tmpPtrV, strideV, 1);

        }
    }
    else if (isBelowIntra)
    {
        copyPlanesFromAboveBelow(tmpPtrY, strideY, tmpPtrU, strideU,
            tmpPtrV, strideV, 0, 0);
    }
    else if (isBelowLeftIntra)
    {
        fillPlanes(tmpPtrY, strideY, tmpPtrU, strideU, tmpPtrV, strideV,
            (Ipp32s)(*(((PlaneY*)((Ipp8u*)tmpPtrY + 8 *strideY)) - 1)),
            (Ipp32s)(*(((PlaneUV*)((Ipp8u*)tmpPtrU + 4 * strideU)) - 1*mulNV12)),
            (Ipp32s)(*(((PlaneUV*)((Ipp8u*)tmpPtrV + 4 * strideV)) - 1*mulNV12)), 0);
    }
    else
    {
        fillPlanes(tmpPtrY, strideY, tmpPtrU, strideU, tmpPtrV, strideV,
            0, 0, 0, 0);
    }

    /* 3 quarter */

    if (isRightIntra)
    {
        if (isBelowIntra)
        {
            diagonalConstruction(tmpPtrY + 8, strideY, tmpPtrU + 4*mulNV12, strideU,
                tmpPtrV + 4*mulNV12, strideV, 3, isBelowRightIntra, 0);
        }
        else
        {
            copyPlanesFromLeftRight(tmpPtrY + 8, strideY, tmpPtrU + 4*mulNV12, strideU,
                tmpPtrV + 4*mulNV12, strideV, 0);

        }
    }
    else if (isBelowIntra)
    {
        copyPlanesFromAboveBelow(tmpPtrY + 8, strideY, tmpPtrU + 4*mulNV12, strideU,
            tmpPtrV + 4*mulNV12, strideV, 0, 0);
    }
    else if (isBelowRightIntra)
    {
        fillPlanes(tmpPtrY + 8, strideY, tmpPtrU + 4*mulNV12, strideU, tmpPtrV + 4*mulNV12, strideV,
            (Ipp32s)(*(((PlaneY*)((Ipp8u*)tmpPtrY + 8 * strideY)) + 16)),
            (Ipp32s)(*(((PlaneUV*)((Ipp8u*)tmpPtrU + 4 * strideU)) + 8*mulNV12)),
            (Ipp32s)(*(((PlaneUV*)((Ipp8u*)tmpPtrV + 4 * strideV)) + 8*mulNV12)), 0);
    }
    else
    {
        fillPlanes(tmpPtrY + 8, strideY, tmpPtrU + 4*mulNV12, strideU, tmpPtrV + 4*mulNV12, strideV,
            0, 0, 0, 0);
    }
}

#define IS_REF_INTRA(refMbAddr) ((mbs[refMbAddr].mbtype) <= MBTYPE_PCM)
// IS_REF_INTRA_INSLICE with sliceId is used in constrained upsampling, not supported
#define IS_REF_INTRA_INSLICE(refMbAddr, sliceId) IS_REF_INTRA(refMbAddr)

// note: MBAFF support deleted
// only NV12

void interpolateInterMBs(H264EncoderFrameType *srcframe_mbs,
                         PlaneY *dstY,
                         PlaneUV *dstU,
                         PlaneUV *dstV,
                         Ipp32s  pitch,
                         Ipp32s  widthMB,
                         Ipp32s  heightMB)
{
    // first copy reconstructed (src) frame data to dst
    // then interpolate in dst in place

    Ipp32s refMbAddr;
    H264MacroblockGlobalInfo *mbs = srcframe_mbs->m_mbinfo.mbs;
    const Ipp32s mulNV12 = 2;
    PlaneY  *srcdstY = dstY;
    PlaneUV *srcdstU = dstU;
    PlaneUV *srcdstV = dstV;
    Ipp32s  strideY  = pitch;
    Ipp32s  strideU  = pitch;
    Ipp32s  strideV  = pitch;

    refMbAddr = 0;

    for (Ipp32s mb_y = 0; mb_y < heightMB; mb_y++)
    {
        PlaneY *tmpPtrY = (PlaneY*)((Ipp8u*)srcdstY + 16 * mb_y * strideY);
        PlaneUV *tmpPtrU = (PlaneY*)((Ipp8u*)srcdstU + 8 * mb_y * strideU);
        PlaneUV *tmpPtrV = (PlaneY*)((Ipp8u*)srcdstV + 8 * mb_y * strideV);

        for (Ipp32s mb_x = 0; mb_x < widthMB; mb_x++)
        {
            Ipp32s isAboveIntra = 0;
            Ipp32s isBelowIntra = 0;
            Ipp32s isLeftIntra = 0;
            Ipp32s isRightIntra = 0;
            Ipp32s isAboveLeftIntra = 0;
            Ipp32s isAboveRightIntra = 0;
            Ipp32s isBelowLeftIntra = 0;
            Ipp32s isBelowRightIntra = 0;
            Ipp32s isIntra = 0;

            Ipp32s mask = 0;
            Ipp32s edge = ((mb_y == 0) || (mb_y == (heightMB - 1)) || (mb_x == 0) || (mb_x == (widthMB - 1)));

            if (IS_REF_INTRA_INSLICE(refMbAddr, curSliceNum))
                isIntra = 1;

            if (mb_y > 0)
            {
                if (IS_REF_INTRA_INSLICE(refMbAddr-widthMB, curSliceNum))
                {
                    isAboveIntra = 1;
                }

                if ((mb_x > 0) && IS_REF_INTRA_INSLICE(refMbAddr-widthMB-1, curSliceNum))
                {
                    isAboveLeftIntra = 1;
                }

                if ((mb_x < widthMB - 1) && IS_REF_INTRA_INSLICE(refMbAddr-widthMB+1, curSliceNum))
                {
                    isAboveRightIntra = 1;
                }
            }

            if (mb_y < heightMB - 1)
            {
                if (IS_REF_INTRA_INSLICE(refMbAddr+widthMB, curSliceNum))
                {
                    isBelowIntra = 1;
                }

                if ((mb_x > 0) && IS_REF_INTRA_INSLICE(refMbAddr+widthMB-1, curSliceNum))
                {
                    isBelowLeftIntra = 1;
                }

                if ((mb_x < widthMB - 1) && IS_REF_INTRA_INSLICE(refMbAddr+widthMB+1, curSliceNum))
                {
                    isBelowRightIntra = 1;
                }
            }

            if ((mb_x > 0) && IS_REF_INTRA_INSLICE(refMbAddr-1, curSliceNum))
            {
                isLeftIntra = 1;
            }

            if ((mb_x < widthMB - 1) && IS_REF_INTRA_INSLICE(refMbAddr+1, curSliceNum))
            {
                isRightIntra = 1;
            }

            mask |= ( isAboveIntra      ? 0x01 : 0);
            mask |= ( isBelowIntra      ? 0x10 : 0);
            mask |= ( isLeftIntra       ? 0x40 : 0);
            mask |= ( isRightIntra      ? 0x04 : 0);
            mask |= ( isAboveLeftIntra  ? 0x80 : 0);
            mask |= ( isAboveRightIntra ? 0x02 : 0);
            mask |= ( isBelowLeftIntra  ? 0x20 : 0);
            mask |= ( isBelowRightIntra ? 0x08 : 0);

            if (!isIntra) {
                fillMB(tmpPtrY, strideY, tmpPtrU, strideU, tmpPtrV, strideV, mask);
            }

            if (edge)
            {
                Ipp32s edgeMask;

                if (mb_y == 0)
                {
                    edgeMask = 0;

                    if (isLeftIntra)  edgeMask |= 0x20; /* isBelowLeftIntra */
                    if (isIntra)      edgeMask |= 0x10; /* isBelowIntra */
                    if (isRightIntra) edgeMask |= 0x08; /* isBelowRightIntra */

                    fillMB(tmpPtrY-16*strideY, strideY,
                        tmpPtrU- 8*strideU, strideU,
                        tmpPtrV- 8*strideV, strideV,
                        edgeMask);

                    if (mb_x == 0)
                    {
                        edgeMask = isIntra ? 0x08 : 0; /* isBelowRightIntra */;
                        fillMB(tmpPtrY-16*strideY-16, strideY,
                            tmpPtrU- 8*strideU-8*mulNV12,  strideU,
                            tmpPtrV- 8*strideV-8*mulNV12,  strideV,
                            edgeMask);
                    }

                    if (mb_x == widthMB - 1)
                    {
                        edgeMask = isIntra ? 0x20 : 0; /* isBelowLeftIntra */;
                        fillMB(tmpPtrY-16*strideY+16, strideY,
                            tmpPtrU- 8*strideU+8*mulNV12,  strideU,
                            tmpPtrV- 8*strideV+8*mulNV12,  strideV,
                            edgeMask);
                    }

                }

                if (mb_y == heightMB - 1)
                {
                    edgeMask = 0;

                    if (isLeftIntra)  edgeMask |= 0x80; /* isAboveLeftIntra */
                    if (isIntra)      edgeMask |= 0x01; /* isAboveIntra */
                    if (isRightIntra) edgeMask |= 0x02; /* isAboveRightIntra */

                    fillMB(tmpPtrY+16*strideY, strideY,
                        tmpPtrU+ 8*strideU, strideU,
                        tmpPtrV+ 8*strideV, strideV,
                        edgeMask);

                    if (mb_x == 0)
                    {
                        edgeMask = isIntra ? 0x02 : 0; /* isAboveRightIntra */;
                        fillMB(tmpPtrY+16*strideY-16, strideY,
                            tmpPtrU+ 8*strideU-8*mulNV12,  strideU,
                            tmpPtrV+ 8*strideV-8*mulNV12,  strideV,
                            edgeMask);
                    }

                    if (mb_x == widthMB - 1)
                    {
                        edgeMask = isIntra ? 0x80 : 0; /* isAboveLeftIntra */;
                        fillMB(tmpPtrY+16*strideY+16, strideY,
                            tmpPtrU+ 8*strideU+8*mulNV12,  strideU,
                            tmpPtrV+ 8*strideV+8*mulNV12,  strideV,
                            edgeMask);
                    }
                }

                if (mb_x == 0)
                {
                    edgeMask = 0;

                    if (isAboveIntra) edgeMask |= 0x02; /* isAboveRightIntra */
                    if (isIntra)      edgeMask |= 0x04; /* isRightIntra */
                    if (isBelowIntra) edgeMask |= 0x08; /* isBelowRightIntra */

                    fillMB(tmpPtrY-16, strideY,
                        tmpPtrU -8*mulNV12, strideU,
                        tmpPtrV -8*mulNV12, strideV,
                        edgeMask);
                }

                if (mb_x == widthMB - 1)
                {
                    edgeMask = 0;

                    if (isAboveIntra) edgeMask |= 0x80; /* isAboveLeftIntra */
                    if (isIntra)      edgeMask |= 0x40; /* isLeftIntra */
                    if (isBelowIntra) edgeMask |= 0x20; /* isBelowLeftIntra */

                    fillMB(tmpPtrY+16, strideY,
                        tmpPtrU +8*mulNV12, strideU,
                        tmpPtrV +8*mulNV12, strideV,
                        edgeMask);
                }
            }

            tmpPtrY += 16;
            tmpPtrU += 16;
            tmpPtrV += 16;
            refMbAddr++;
        }
    }
}

static void upsampleResidualPrepareIdc(Ipp32s  srcWidth,
                             Ipp32s  srcHeight,
                             Ipp8u  *tmpBuf0,
                             H264CoreEncoder_8u16s *refEnc)
{
    Ipp32s mbWidth, mbHeight;
    Ipp32s srcWidthInMbs, srcHeightInMbs, srcWidthIdc;
    H264EncoderFrameType *frame = refEnc->m_pCurrentFrame;
    H264MacroblockGlobalInfo *mbs = frame->m_mbinfo.mbs;

    mbWidth = 4;
    mbHeight = 4;
    srcWidthInMbs = srcWidth >> 4;
    srcHeightInMbs = srcHeight >> 4;
    srcWidthIdc = srcWidth >> 2;

    Ipp32s i, j, ii, jj;

    for (j = 0; j < srcHeightInMbs; j++)
    {
        for (i = 0; i < srcWidthInMbs; i++)
        {
            Ipp32s  iMbAddr = j * srcWidthInMbs + i;
            Ipp32s    flag8x8      = GetMB8x8TSPackFlag(mbs[iMbAddr]);

            Ipp8u*  bufPtr = tmpBuf0 + j * mbHeight * srcWidthIdc + i * mbWidth;

            if (flag8x8)
            {
                for (jj = 0; jj < mbHeight; jj++, bufPtr += srcWidthIdc)
                {
                    for( ii = 0; ii < mbWidth; ii++ )
                    {
                        bufPtr[ii] = ((jj >> 1) << 1) + (ii >> 1);
                    }
                }
            }
            else
            {
                for (jj = 0; jj < mbHeight; jj++, bufPtr += srcWidthIdc)
                {
                    for (ii = 0; ii < mbWidth; ii++)
                    {
                        bufPtr[ii] = ((jj & 1) << 1) + (ii & 1);
                    }
                }
            }
        }
    }
}

// TODO: store residuals

void upsamplingResidual(H264LayerResizeParameter* pResizeParameter,
                        H264CoreEncoder_8u16s *curEnc,
                        H264CoreEncoder_8u16s *refEnc,
                        Ipp8u *tmpBuf,
                        Ipp32s refLumaW, Ipp32s refLumaH)
{
    Ipp32s refW, refH, fullW;
    refW = refLumaW;
    refH = refLumaH;
    fullW = curEnc->m_SeqParamSet->frame_width_in_mbs << 4;

    upsampleResidualPrepareIdc(refW, refH, tmpBuf, refEnc);

    ippiUpsampleResidualsLuma_SVC_16s_C1R(refEnc->m_pYResidual, refW*2,
        curEnc->m_pYInputResidual + (pResizeParameter->mbRegionFull.x << 4) +
            (pResizeParameter->mbRegionFull.y << 4) * fullW,
        fullW*2, curEnc->svc_UpsampleResidLumaSpec,
        pResizeParameter->mbRegionFull, tmpBuf, curEnc->svc_UpsampleBuf);

    refW >>= 1;
    fullW >>= 1;

    ippiUpsampleResidualsChroma_SVC_16s_C1R(refEnc->m_pUResidual, refW*2,
        curEnc->m_pUInputResidual + (pResizeParameter->mbRegionFull.x << 3) +
            (pResizeParameter->mbRegionFull.y << 3) * fullW,
        fullW*2, curEnc->svc_UpsampleResidChromaSpec,
        pResizeParameter->mbRegionFull, curEnc->svc_UpsampleBuf);
    
    ippiUpsampleResidualsChroma_SVC_16s_C1R(refEnc->m_pVResidual, refW*2,
        curEnc->m_pVInputResidual + (pResizeParameter->mbRegionFull.x << 3) +
            (pResizeParameter->mbRegionFull.y << 3) * fullW,
        fullW*2, curEnc->svc_UpsampleResidChromaSpec,
        pResizeParameter->mbRegionFull, curEnc->svc_UpsampleBuf);

    CleanResidualsOverIntra(curEnc);
}

// copy upsampled residuals to another buffer
// then set to zero all samples which are Intra predicted in BaseMode
static void CleanResidualsOverIntra (H264CoreEncoder_8u16s *curEnc)
{
    Ipp32s i, j;
    Ipp32s curMB_X;
    Ipp32s curMB_Y;
    Ipp32s mbWidth = curEnc->m_SeqParamSet->frame_width_in_mbs;
    Ipp32s mbHeight = curEnc->m_SeqParamSet->frame_height_in_mbs;
    Ipp32s resStride = curEnc->m_info.info.clip_info.width;

    MFX_INTERNAL_CPY(curEnc->m_pYInputBaseResidual, curEnc->m_pYInputResidual, mbHeight*mbWidth*256*sizeof(COEFFSTYPE));
    MFX_INTERNAL_CPY(curEnc->m_pUInputBaseResidual, curEnc->m_pUInputResidual, mbHeight*mbWidth*64*sizeof(COEFFSTYPE));
    MFX_INTERNAL_CPY(curEnc->m_pVInputBaseResidual, curEnc->m_pVInputResidual, mbHeight*mbWidth*64*sizeof(COEFFSTYPE));

    for (curMB_Y = 0; curMB_Y < mbHeight; curMB_Y++) {
        for (curMB_X = 0; curMB_X < mbWidth; curMB_X++) {
            Ipp32s curMBAddr = mbWidth * curMB_Y + curMB_X;
            H264GlobalMacroblocksDescriptor *gmbinfo = &(curEnc->m_pReconstructFrame->m_mbinfo);
            H264MacroblockGlobalInfo *gmbs = &(gmbinfo->mbs[curMBAddr]);
            for (i=0; i<2; i++) {
                for (j=0; j<2; j++) {
                    if(!gmbs->i_pred[2*j + i]) continue;

                    Ipp32s offx = curEnc->svc_ResizeMap[0][curMB_X].border;
                    Ipp32s offy = curEnc->svc_ResizeMap[1][curMB_Y].border;
                    IppiSize roiSize = {i?16-offx:offx, j?16-offy:offy};
                    if(!i) offx = 0;
                    if(!j) offy = 0;

                    ippiSet_16s_C1R(0, curEnc->m_pYInputBaseResidual +
                        (curMB_Y*16 + offy) * resStride + curMB_X*16 + offx,
                        resStride<<1, roiSize);

                    offx = (curEnc->svc_ResizeMap[0][curMB_X].border+1)>>1;
                    offy = (curEnc->svc_ResizeMap[1][curMB_Y].border+1)>>1;
                    roiSize.width = i?8-offx:offx;
                    roiSize.height = j?8-offy:offy;
                    if(!i) offx = 0;
                    if(!j) offy = 0;
                    ippiSet_16s_C1R(0, curEnc->m_pUInputBaseResidual +
                        (curMB_Y*8 + offy) * (resStride>>1) + curMB_X*8 + offx,
                        resStride, roiSize);
                    ippiSet_16s_C1R(0, curEnc->m_pVInputBaseResidual +
                        (curMB_Y*8 + offy) * (resStride>>1) + curMB_X*8 + offx,
                        resStride, roiSize);
                }
            }
        }
    }

}

void umc_h264_mv_resampling(//H264SegmentDecoderMultiThreaded *sd,
                            H264LayerResizeParameter *pResizeParameter,
                            H264CoreEncoder_8u16s *curEnc,
                            H264CoreEncoder_8u16s *refEnc,
                            //H264Slice *curSlice,
                            //H264DecoderLayerDescriptor *refLayer,
                            //H264DecoderLayerDescriptor *curLayer,
                            H264GlobalMacroblocksDescriptor *gmbinfo,
                            H264LocalMacroblockDescriptor *mbinfo)
{
    //const H264SeqParamSet *curSps = curSlice->GetSeqParam();
    //H264SliceHeader *curSliceHeader = curSlice->GetSliceHeader();
    //H264DecoderLayerResizeParameter *pResizeParameter = curLayer->m_pResizeParameter;

    Ipp32s leftOffset, rightOffset, topOffset, bottomOffset;
    Ipp32s refW, refH, scaledW, scaledH;
    Ipp32s scaleX, scaleY;
    Ipp32s shiftX, shiftY;
    Ipp32s phaseX, phaseY, refPhaseX, refPhaseY;
    Ipp32s CroppingChangeFlag;
    Ipp32s SpatialResolutionChangeFlag, RestrictedSpatialResolutionChangeFlag;
    Ipp32s i, j;
    Ipp32s slice_type = (curEnc->m_pCurrentFrame->m_PicCodType == PREDPIC) ? PREDSLICE : (curEnc->m_pCurrentFrame->m_PicCodType == INTRAPIC) ? INTRASLICE : BPREDSLICE;
    phaseX = pResizeParameter->phaseX;
    phaseY = pResizeParameter->phaseY;
    refPhaseX = pResizeParameter->refPhaseX;
    refPhaseY = pResizeParameter->refPhaseY;
    leftOffset = pResizeParameter->leftOffset;
    topOffset = pResizeParameter->topOffset;
    rightOffset = pResizeParameter->rightOffset;
    bottomOffset = pResizeParameter->bottomOffset;

    if (curEnc->m_SeqParamSet->extended_spatial_scalability == 2)
        CroppingChangeFlag = 1;
    else
        CroppingChangeFlag = 0;

    scaledW = pResizeParameter->scaled_ref_layer_width;
    scaledH = pResizeParameter->scaled_ref_layer_height;

    refW = pResizeParameter->ref_layer_width;
    refH = pResizeParameter->ref_layer_height;

    shiftX = pResizeParameter->shiftX;
    shiftY = pResizeParameter->shiftY;

    scaleX = pResizeParameter->scaleX;
    scaleY = pResizeParameter->scaleY;

    //Ipp32s scaleMVX = ((scaledW<<16)+(refW>>1))/refW;
    //Ipp32s scaleMVY = ((scaledH<<16)+(refH>>1))/refH;

    SpatialResolutionChangeFlag = 1;
    RestrictedSpatialResolutionChangeFlag = 0;

    if ((CroppingChangeFlag == 0) &&
        (refW == scaledW) &&
        (refH == scaledH) &&
        ((leftOffset & 0xF) == 0) &&
        ((topOffset % (16 * (1 + curEnc->m_SeqParamSet->frame_mbs_only_flag))) == 0) &&
        //(curSliceHeader->MbaffFrameFlag == refLayer->is_MbAff) &&
        (refPhaseX == phaseX))
    {
        SpatialResolutionChangeFlag = 0;
        RestrictedSpatialResolutionChangeFlag = 1;
    }

    if ((CroppingChangeFlag == 0) &&
        ((refW == scaledW) || (2 * refW == scaledW)) &&
        ((refH == scaledH) || (2 * refH == scaledH)) &&
        ((leftOffset & 0xF) == 0) &&
        ((topOffset % (16 * (1 + curEnc->m_SeqParamSet->frame_mbs_only_flag))) == 0) //&&
        //(curSliceHeader->MbaffFrameFlag == refLayer->is_MbAff)
        )
    {
        RestrictedSpatialResolutionChangeFlag = 1;
    }

    curEnc->m_spatial_resolution_change = SpatialResolutionChangeFlag ? true : false;
    curEnc->m_restricted_spatial_resolution_change = RestrictedSpatialResolutionChangeFlag ? true : false;
    //curEnc->m_next_spatial_resolution_change = 1; // fixme

    /* all offsets in macroblocks */

    leftOffset = (leftOffset + 15) >> 4;
    rightOffset = (rightOffset + 15) >> 4;
    topOffset = (topOffset + 15) >> 4;
    bottomOffset = (bottomOffset + 15) >> 4;

    for (j = 0; j < (Ipp32s)curEnc->m_SeqParamSet->frame_height_in_mbs; j++)
    {
        for (i = 0; i < (Ipp32s)curEnc->m_SeqParamSet->frame_width_in_mbs; i++)
        {
            Ipp32s curMbAddr;
            Ipp32s refMbAddr;

            //if (sd->m_isMBAFF) {
            //    curMbAddr = (j/2 * (curEnc->m_SeqParamSet->frame_width_in_mbs) + i) * 2 + (j & 1);
            //} else {
                curMbAddr = j * (curEnc->m_SeqParamSet->frame_width_in_mbs) + i;
            //}

            if (!curEnc->m_spatial_resolution_change) {
                H264MacroblockLocalInfo *lmbs;
                H264MacroblockGlobalInfo *gmbs;
                H264MacroblockMVs *MV[2];
                H264MacroblockRefIdxs *RefIdxs[2];

                refMbAddr = curMbAddr;

                lmbs = &(mbinfo->mbs[curMbAddr]);
                gmbs = &(gmbinfo->mbs[curMbAddr]);
                MV[0] = &(gmbinfo->MV[0][curMbAddr]);
                MV[1] = &(gmbinfo->MV[1][curMbAddr]);
                RefIdxs[0] = &(gmbinfo->RefIdxs[0][curMbAddr]);
                RefIdxs[1] = &(gmbinfo->RefIdxs[1][curMbAddr]);

                MFX_INTERNAL_CPY(gmbs, &refEnc->m_pCurrentFrame->m_mbinfo.mbs[refMbAddr], sizeof(H264MacroblockGlobalInfo));
                MFX_INTERNAL_CPY(lmbs, &refEnc->m_mbinfo.mbs[refMbAddr], sizeof(H264MacroblockLocalInfo));
                MFX_INTERNAL_CPY(&gmbinfo->RefIdxs[0][curMbAddr], &refEnc->m_pCurrentFrame->m_mbinfo.RefIdxs[0][refMbAddr], sizeof(H264MacroblockRefIdxs));
                MFX_INTERNAL_CPY(&gmbinfo->RefIdxs[1][curMbAddr], &refEnc->m_pCurrentFrame->m_mbinfo.RefIdxs[1][refMbAddr], sizeof(H264MacroblockRefIdxs));
                MFX_INTERNAL_CPY(&gmbinfo->MV[0][curMbAddr], &refEnc->m_pCurrentFrame->m_mbinfo.MV[0][refMbAddr], sizeof(H264MacroblockMVs));
                MFX_INTERNAL_CPY(&gmbinfo->MV[1][curMbAddr], &refEnc->m_pCurrentFrame->m_mbinfo.MV[1][refMbAddr], sizeof(H264MacroblockMVs));

            } else if ((i >= leftOffset) && (i < (Ipp32s)curEnc->m_SeqParamSet->frame_width_in_mbs - rightOffset) &&
                (j >= topOffset) && (j < (Ipp32s)curEnc->m_SeqParamSet->frame_height_in_mbs - bottomOffset))
            {
//                Ipp32s xRef, yRef;
//                xRef = ((i * 16 - leftOffset * 16) * scaleX +  (1<<(shiftX-1))) >> (shiftX+4);
//                yRef = ((j * 16 - topOffset * 16) * scaleY +  (1<<(shiftY-1))) >> (shiftY+4);
//                refMbAddr = yRef*refEnc->m_SeqParamSet->frame_width_in_mbs + xRef;
//                assert(xRef>=0 && xRef<refEnc->m_WidthInMBs);
//                assert(yRef>=0 && yRef<refEnc->m_HeightInMBs);

//                MFX_INTERNAL_CPY(&gmbinfo->RefIdxs[0][curMbAddr], &refEnc->m_pCurrentFrame->m_mbinfo.RefIdxs[0][refMbAddr], sizeof(H264MacroblockRefIdxs));
//                MFX_INTERNAL_CPY(&gmbinfo->RefIdxs[1][curMbAddr], &refEnc->m_pCurrentFrame->m_mbinfo.RefIdxs[1][refMbAddr], sizeof(H264MacroblockRefIdxs));

                Ipp8u cropFlag = pGetMBCropFlag(&gmbinfo->mbs[curMbAddr]);
                memset((void *)&gmbinfo->mbs[curMbAddr], 0, sizeof(H264MacroblockGlobalInfo));
                pSetMBCropFlag(&gmbinfo->mbs[curMbAddr], cropFlag);

 //               gmbinfo->mbs[curMbAddr] = refEnc->m_pCurrentFrame->m_mbinfo.mbs[refMbAddr];
                umc_h264_MB_mv_resampling(
                    pResizeParameter,
                    curEnc,
                    refEnc,
                    i,
                    j,
                    curMbAddr,
                    slice_type,
                    RestrictedSpatialResolutionChangeFlag,
                    0 /*CroppingChangeFlag*/);

                //// draft formula
                //H264MacroblockMVs *MV[2];
                //MV[0] = &(gmbinfo->MV[0][curMbAddr]);
                //MV[1] = &(gmbinfo->MV[1][curMbAddr]);
                //for (Ipp32s jj=0; jj<2; jj++)
                //for (Ipp32s ii=0; ii<sizeof(MV[0]->MotionVectors)/sizeof(MV[0]->MotionVectors[0]); ii++) {
                //    MV[jj]->MotionVectors[ii].mvx =
                //        (refEnc->m_pCurrentFrame->m_mbinfo.MV[jj][refMbAddr].MotionVectors[ii].mvx * scaleMVX + 0x8000) >> 16;
                //    MV[jj]->MotionVectors[ii].mvy =
                //        (refEnc->m_pCurrentFrame->m_mbinfo.MV[jj][refMbAddr].MotionVectors[ii].mvy * scaleMVY + 0x8000) >> 16;
                //}

            } else {
                H264MacroblockLocalInfo *lmbs;
                H264MacroblockGlobalInfo *gmbs;
                H264MacroblockMVs *MV[2];
                H264MacroblockRefIdxs *RefIdxs[2];

                lmbs = &(mbinfo->mbs[curMbAddr]);
                gmbs = &(gmbinfo->mbs[curMbAddr]);
                MV[0] = &(gmbinfo->MV[0][curMbAddr]);
                MV[1] = &(gmbinfo->MV[1][curMbAddr]);
                RefIdxs[0] = &(gmbinfo->RefIdxs[0][curMbAddr]);
                RefIdxs[1] = &(gmbinfo->RefIdxs[1][curMbAddr]);

                memset((void *) RefIdxs[0], -1, sizeof(H264MacroblockRefIdxs));
                memset((void *) RefIdxs[1], -1, sizeof(H264MacroblockRefIdxs));
                memset((void *) MV[0], 0, sizeof(H264MacroblockMVs));
                memset((void *) MV[1], 0, sizeof(H264MacroblockMVs));

                pSetMBSkippedFlag(gmbs);
                pSetMBResidualPredictionFlag(gmbs, 0);
                lmbs->cbp = 0;
                lmbs->cbp_luma = 0;
                lmbs->cbp_chroma = 0;
                //lmbs->cbp4x4_luma = 0;
                //lmbs->cbp4x4_chroma[0] = 0;
                //lmbs->cbp4x4_chroma[1] = 0;
                //memset(lmbs->sbdir, 0, 4);
            }
        }
    }
}

void constrainedIntraResamplingCheckMBs(//const H264SeqParamSet *curSps,
                                        //H264DecoderLayerDescriptor *curLayer,
                                        H264LayerResizeParameter *pResizeParameter,
                                        H264CoreEncoder_8u16s *curEnc,
                                        H264MacroblockGlobalInfo *mbs,
                                        Ipp32s  *buf,
                                        Ipp32s  widthMB,
                                        Ipp32s  heightMB)
{
    //H264DecoderLayerResizeParameter *pResizeParameter = curLayer->m_pResizeParameter;
    Ipp32s leftOffsetLuma, topOffsetLuma, leftOffsetChroma, topOffsetChroma;
    Ipp32s refWLuma, refHLuma, refWChroma, refHChroma;
    Ipp32s scaledW, scaledH;
    Ipp32s shiftXLuma, shiftYLuma, scaleXLuma, scaleYLuma;
    Ipp32s addXLuma, addYLuma;
    Ipp32s shiftXChroma, shiftYChroma, scaleXChroma, scaleYChroma;
    Ipp32s addXChroma, addYChroma, deltaXChroma, deltaYChroma;
    Ipp32s phaseX, phaseY, refPhaseX, refPhaseY;
    Ipp32s chromaShiftW, chromaShiftH;
    Ipp32s i, j, ii, MbAddr;

    phaseX = pResizeParameter->phaseX;
    phaseY = pResizeParameter->phaseY;
    refPhaseX = pResizeParameter->refPhaseX;
    refPhaseY = pResizeParameter->refPhaseY;
    leftOffsetLuma = pResizeParameter->leftOffset;
    topOffsetLuma = pResizeParameter->topOffset;

    scaledW = pResizeParameter->scaled_ref_layer_width;
    scaledH = pResizeParameter->scaled_ref_layer_height;

    refWLuma = pResizeParameter->ref_layer_width;
    refHLuma = pResizeParameter->ref_layer_height;

    shiftXLuma = pResizeParameter->shiftX;
    shiftYLuma = pResizeParameter->shiftY;

    scaleXLuma = pResizeParameter->scaleX;
    scaleYLuma = pResizeParameter->scaleY;

    addXLuma = ((refWLuma << (shiftXLuma - 1)) + (scaledW >> 1)) / scaledW +
        (1 << (shiftXLuma - 5));

    addYLuma = ((refHLuma << (shiftYLuma - 1)) + (scaledH >> 1)) / scaledH +
        (1 << (shiftYLuma - 5));

    /* 4:2:0 */
    chromaShiftW = 1;
    chromaShiftH = 1;

    leftOffsetChroma = leftOffsetLuma >> chromaShiftW;
    refWChroma = refWLuma >> chromaShiftW;
    scaledW >>= chromaShiftW;
    topOffsetChroma = topOffsetLuma >> chromaShiftH;
    refHChroma = refHLuma >> chromaShiftH;
    scaledH >>= chromaShiftH;

    shiftXChroma = ((curEnc->m_SeqParamSet->level_idc <= 30) ? 16 : countShift(refWChroma));
    shiftYChroma = ((curEnc->m_SeqParamSet->level_idc <= 30) ? 16 : countShift(refHChroma));

    scaleXChroma = ((refWChroma << shiftXChroma) + (scaledW >> 1)) / scaledW;
    scaleYChroma = ((refHChroma << shiftYChroma) + (scaledH >> 1)) / scaledH;

    addXChroma = (((refWChroma * (2 + phaseX)) << (shiftXChroma - 2)) + (scaledW >> 1)) / scaledW +
        (1 << (shiftXChroma - 5));

    addYChroma = (((refHChroma * (2 + phaseY)) << (shiftYChroma - 2)) + (scaledH >> 1)) / scaledH +
        (1 << (shiftYChroma - 5));

    deltaXChroma = 4 * (2 + refPhaseX);
    deltaYChroma = 4 * (2 + refPhaseY);

    MbAddr = 0;

    for (j = 0; j < heightMB; j++)
    {
        Ipp32s yRef, refMbAddrMinYLuma, refMbAddrMaxYLuma;
        Ipp32s refMbAddrMinYChroma, refMbAddrMaxYChroma;

        yRef = (((((j * 16 - topOffsetLuma) * scaleYLuma +  addYLuma) >>
            (shiftYLuma - 4)) - 8) >> 4) + 1;

        if (yRef < 0) yRef = 0;
        if (yRef >= refHLuma) yRef = refHLuma - 1;

        refMbAddrMinYLuma = (yRef >> 4) * (refWLuma >> 4);

        yRef = (((((j * 16 - topOffsetLuma + 15) * scaleYLuma +  addYLuma) >>
            (shiftYLuma - 4)) - 8 + 15) >> 4) - 1;

        if (yRef < 0) yRef = 0;
        if (yRef >= refHLuma) yRef = refHLuma - 1;

        refMbAddrMaxYLuma = (yRef >> 4) * (refWLuma >> 4);

        yRef = (((((j * 8 - topOffsetChroma) * scaleYChroma +  addYChroma) >>
            (shiftYChroma - 4)) - deltaYChroma) >> 4) + 1;

        if (yRef < 0) yRef = 0;
        if (yRef >= refHChroma) yRef = refHChroma - 1;

        refMbAddrMinYChroma = (yRef >> 3) * (refWLuma >> 4);

        yRef = (((((j * 8 - topOffsetChroma + 7) * scaleYChroma +  addYChroma) >>
            (shiftYChroma - 4)) - deltaYChroma + 15) >> 4) - 1;

        if (yRef < 0) yRef = 0;
        if (yRef >= refHChroma) yRef = refHChroma - 1;

        refMbAddrMaxYChroma = (yRef >> 3) * (refWLuma >> 4);


        for (i = 0; i < widthMB; i++)
        {
            Ipp32s xRef, clear, index;
            Ipp32s refMbAddr[8];

            xRef = (((((i * 16 - leftOffsetLuma) * scaleXLuma +  addXLuma) >>
                (shiftXLuma - 4)) - 8) >> 4) + 1;

            if (xRef < 0) xRef = 0;
            if (xRef >= refWLuma) xRef = refWLuma - 1;

            refMbAddr[0] = refMbAddrMinYLuma + (xRef >> 4);
            refMbAddr[1] = refMbAddrMaxYLuma + (xRef >> 4);


            xRef = (((((i * 16 - leftOffsetLuma + 15) * scaleXLuma +  addXLuma) >>
                (shiftXLuma - 4)) - 8 + 15) >> 4) - 1;

            if (xRef < 0) xRef = 0;
            if (xRef >= refWLuma) xRef = refWLuma - 1;

            refMbAddr[2] = refMbAddrMinYLuma + (xRef >> 4);
            refMbAddr[3] = refMbAddrMaxYLuma + (xRef >> 4);

            xRef = (((((i * 8 - leftOffsetChroma) * scaleXChroma +  addXChroma) >>
                (shiftXChroma - 4)) - deltaXChroma) >> 4) + 1;

            if (xRef < 0) xRef = 0;
            if (xRef >= refWChroma) xRef = refWChroma - 1;

            refMbAddr[4] = refMbAddrMinYChroma + (xRef >> 3);
            refMbAddr[5] = refMbAddrMaxYChroma + (xRef >> 3);

            xRef = (((((i * 8 - leftOffsetChroma + 7) * scaleXChroma +  addXChroma) >>
                (shiftXChroma - 4)) - deltaXChroma + 15) >> 4) - 1;

            if (xRef < 0) xRef = 0;
            if (xRef >= refWChroma) xRef = refWChroma - 1;

            refMbAddr[6] = refMbAddrMinYChroma + (xRef >> 3);
            refMbAddr[7] = refMbAddrMaxYChroma + (xRef >> 3);

            index = -1;

            for (ii = 0; ii < 8; ii++)
            {
                if (mbs[refMbAddr[ii]].mbtype <= MBTYPE_PCM)
                {
                    index = ii;
                }
                else
                {
                    refMbAddr[ii] = -1;
                }
            }

            clear = 1;

            if (index >= 0)
            {
                Ipp32s slice_id = mbs[refMbAddr[index]].slice_id;

                clear = 0;

                for (ii = index - 1; ii >= 0; ii--)
                {
                    if ((refMbAddr[ii] >= 0) &&
                        (mbs[refMbAddr[ii]].slice_id != slice_id))
                    {
                        clear = 1;
                        break;
                    }
                }
            }


            if (clear)
            {
                buf[MbAddr] = -1;
            }
            else
            {
                buf[MbAddr] = mbs[refMbAddr[index]].slice_id;
            }
            MbAddr++;
        }
    }
}

static void clearMB(PlaneY  *dst,
                    Ipp32s  stride,
                    Ipp32s  sizeX,
                    Ipp32s  sizeY)
{
    Ipp32s i, j;

    for (j = 0; j < sizeY; j++)
    {
        for (i = 0; i < sizeX; i++)
        {
            dst[i] = 0;
        }

        dst = (PlaneY*)((Ipp8u*)dst + stride);
    }
}

static void copySatMB(Ipp32s  *src,
                      Ipp32s  strideSrc,
                      Ipp8u   *dst,
                      Ipp32s  strideDst,
                      Ipp32s  sizeX,
                      Ipp32s  sizeY)
{
    Ipp32s i, j;

    for (j = 0; j < sizeY; j++)
    {
        for (i = 0; i < sizeX; i++)
        {
            Ipp32s tmp = src[i];

            if      (tmp < 0)   dst[i] = 0;
            else if (tmp > 255) dst[i] = 255;
            else                dst[i] = (Ipp8u)tmp;
        }

        src = (Ipp32s*)((Ipp8u*)src + strideSrc);
        dst = (Ipp8u*)((Ipp8u*)dst + strideDst);
    }
}

static void copyMB(Ipp32s  *src,
                   Ipp32s  strideSrc,
                   PlaneY  *dst,
                   Ipp32s  strideDst,
                   Ipp32s  sizeX,
                   Ipp32s  sizeY)
{
    Ipp32s i, j;

    for (j = 0; j < sizeY; j++)
    {
        for (i = 0; i < sizeX; i++)
        {
            dst[i] = (PlaneY)src[i];
        }

        src = (Ipp32s*)((Ipp8u*)src + strideSrc);
        dst = (PlaneY*)((Ipp8u*)dst + strideDst);
    }
}

static void copyMB_NV12(Ipp32s  *src,
                        Ipp32s  strideSrc,
                        PlaneY  *dst,
                        Ipp32s  strideDst,
                        Ipp32s  sizeX,
                        Ipp32s  sizeY)
{
    Ipp32s i, j;

    for (j = 0; j < sizeY; j++)
    {
        for (i = 0; i < sizeX; i++)
        {
            dst[i*2] = (PlaneY)src[i*2];
        }

        src = (Ipp32s*)((Ipp8u*)src + strideSrc);
        dst = (PlaneY*)((Ipp8u*)dst + strideDst);
    }
}

void constrainedIntrainterpolateInterMBs(PlaneY  *srcdstY,
                                         Ipp32s  strideY,
                                         PlaneUV *srcdstU,
                                         Ipp32s  strideU,
                                         PlaneUV *srcdstV,
                                         Ipp32s  strideV,
                                         Ipp32s  startH,
                                         Ipp32s  endH,
                                         Ipp32s  widthMB,
                                         Ipp32s  heightMB,
                                         H264MacroblockGlobalInfo *gmbs,
                                         Ipp32s  curSliceNum)

{
    //    Ipp32s startSlice, endSlice;
    Ipp32s i, j, refMbAddr;
    H264MacroblockGlobalInfo *lmbs = gmbs;
    const Ipp32s mulNV12 = 2; // NV12 only

    refMbAddr = startH * widthMB;

    for (j = startH; j < endH; j++)
    {
        PlaneY *tmpPtrY = (PlaneY*)((Ipp8u*)srcdstY + 16 * j * strideY);
        PlaneUV *tmpPtrU = (PlaneY*)((Ipp8u*)srcdstU + 8 * j * strideU);
        PlaneUV *tmpPtrV = (PlaneY*)((Ipp8u*)srcdstV + 8 * j * strideV);

        for (i = 0; i < widthMB; i++)
        {
            Ipp32s isAboveIntra = 0;
            Ipp32s isBelowIntra = 0;
            Ipp32s isLeftIntra = 0;
            Ipp32s isRightIntra = 0;
            Ipp32s isAboveLeftIntra = 0;
            Ipp32s isAboveRightIntra = 0;
            Ipp32s isBelowLeftIntra = 0;
            Ipp32s isBelowRightIntra = 0;

            if ((lmbs[refMbAddr].mbtype <= MBTYPE_PCM) &&
                (gmbs[refMbAddr].slice_id == curSliceNum))
            {
                tmpPtrY += 16;
                tmpPtrU += 8*mulNV12;
                tmpPtrV += 8*mulNV12;
                refMbAddr++;

                continue;
            }

            if (j > 0)
            {
                if (((lmbs[refMbAddr-widthMB].mbtype) <= MBTYPE_PCM) &&
                    (gmbs[refMbAddr-widthMB].slice_id == curSliceNum))
                {
                    isAboveIntra = 1;
                }

                if ((i > 0) && ((lmbs[refMbAddr-widthMB-1].mbtype) <= MBTYPE_PCM) &&
                    (gmbs[refMbAddr-widthMB-1].slice_id == curSliceNum))
                {
                    isAboveLeftIntra = 1;
                }

                if ((i < widthMB - 1) && ((lmbs[refMbAddr-widthMB+1].mbtype) <= MBTYPE_PCM) &&
                    (gmbs[refMbAddr-widthMB+1].slice_id == curSliceNum))
                {
                    isAboveRightIntra = 1;
                }
            }

            if (j < heightMB - 1)
            {
                if (((lmbs[refMbAddr+widthMB].mbtype) <= MBTYPE_PCM) &&
                    (gmbs[refMbAddr+widthMB].slice_id == curSliceNum))
                {
                    isBelowIntra = 1;
                }

                if ((i > 0) && ((lmbs[refMbAddr+widthMB-1].mbtype) <= MBTYPE_PCM) &&
                    (gmbs[refMbAddr+widthMB-1].slice_id == curSliceNum))
                {
                    isBelowLeftIntra = 1;
                }

                if ((i < widthMB - 1) && ((lmbs[refMbAddr+widthMB+1].mbtype) <= MBTYPE_PCM) &&
                    (gmbs[refMbAddr+widthMB+1].slice_id == curSliceNum))
                {
                    isBelowRightIntra = 1;
                }
            }

            if ((i > 0) && ((lmbs[refMbAddr-1].mbtype) <= MBTYPE_PCM) &&
                (gmbs[refMbAddr-1].slice_id == curSliceNum))
            {
                isLeftIntra = 1;
            }

            if ((i < widthMB - 1) && ((lmbs[refMbAddr+1].mbtype) <= MBTYPE_PCM) &&
                gmbs[refMbAddr+1].slice_id == curSliceNum)
            {
                isRightIntra = 1;
            }

            /* 0 quarter */

            if (isLeftIntra)
            {
                if (isAboveIntra)
                {
                    diagonalConstruction(tmpPtrY, strideY, tmpPtrU, strideU,
                        tmpPtrV, strideV, 0, isAboveLeftIntra, 0);
                }
                else
                {
                    copyPlanesFromLeftRight(tmpPtrY, strideY, tmpPtrU, strideU,
                        tmpPtrV, strideV, 1);

                }
            }
            else if (isAboveIntra)
            {
                copyPlanesFromAboveBelow(tmpPtrY, strideY, tmpPtrU, strideU,
                    tmpPtrV, strideV, 1, 0);
            }
            else if (isAboveLeftIntra)
            {
                fillPlanes(tmpPtrY, strideY, tmpPtrU, strideU, tmpPtrV, strideV,
                    (Ipp32s)(*(((PlaneY*)((Ipp8u*)tmpPtrY - strideY)) - 1)),
                    (Ipp32s)(*(((PlaneUV*)((Ipp8u*)tmpPtrU - strideU)) - 1*mulNV12)),
                    (Ipp32s)(*(((PlaneUV*)((Ipp8u*)tmpPtrV - strideV)) - 1*mulNV12)), 0);
            }
            else
            {
                fillPlanes(tmpPtrY, strideY, tmpPtrU, strideU, tmpPtrV, strideV,
                    0, 0, 0, 0);
            }

            /* 1 quarter */

            if (isRightIntra)
            {
                if (isAboveIntra)
                {
                    diagonalConstruction(tmpPtrY + 8, strideY, tmpPtrU + 4*mulNV12, strideU,
                        tmpPtrV + 4*mulNV12, strideV, 1, isAboveRightIntra, 0);
                }
                else
                {
                    copyPlanesFromLeftRight(tmpPtrY + 8, strideY, tmpPtrU + 4*mulNV12, strideU,
                        tmpPtrV + 4*mulNV12, strideV, 0);

                }
            }
            else if (isAboveIntra)
            {
                copyPlanesFromAboveBelow(tmpPtrY + 8, strideY, tmpPtrU + 4*mulNV12, strideU,
                    tmpPtrV + 4*mulNV12, strideV, 1, 0);
            }
            else if (isAboveRightIntra)
            {
                fillPlanes(tmpPtrY + 8, strideY, tmpPtrU + 4, strideU, tmpPtrV + 4, strideV,
                    (Ipp32s)(*(((PlaneY*)((Ipp8u*)tmpPtrY - strideY)) + 16)),
                    (Ipp32s)(*(((PlaneUV*)((Ipp8u*)tmpPtrU - strideU)) + 8*mulNV12)),
                    (Ipp32s)(*(((PlaneUV*)((Ipp8u*)tmpPtrV - strideV)) + 8*mulNV12)), 0);
            }
            else
            {
                fillPlanes(tmpPtrY + 8, strideY, tmpPtrU + 4*mulNV12, strideU, tmpPtrV + 4*mulNV12, strideV,
                    0, 0, 0, 0);
            }

            tmpPtrY = (PlaneY*)((Ipp8u*)tmpPtrY + 8 * strideY);
            tmpPtrU = (PlaneY*)((Ipp8u*)tmpPtrU + 4 * strideU);
            tmpPtrV = (PlaneY*)((Ipp8u*)tmpPtrV + 4 * strideV);

            /* 2 quarter */

            if (isLeftIntra)
            {
                if (isBelowIntra)
                {
                    diagonalConstruction(tmpPtrY, strideY, tmpPtrU, strideU,
                        tmpPtrV, strideV, 2, isBelowLeftIntra, 0);
                }
                else
                {
                    copyPlanesFromLeftRight(tmpPtrY, strideY, tmpPtrU, strideU,
                        tmpPtrV, strideV, 1);

                }
            }
            else if (isBelowIntra)
            {
                copyPlanesFromAboveBelow(tmpPtrY, strideY, tmpPtrU, strideU,
                    tmpPtrV, strideV, 0, 0);
            }
            else if (isBelowLeftIntra)
            {
                fillPlanes(tmpPtrY, strideY, tmpPtrU, strideU, tmpPtrV, strideV,
                    (Ipp32s)(*(((PlaneY*)((Ipp8u*)tmpPtrY + 8 *strideY)) - 1)),
                    (Ipp32s)(*(((PlaneUV*)((Ipp8u*)tmpPtrU + 4 * strideU)) - 1*mulNV12)),
                    (Ipp32s)(*(((PlaneUV*)((Ipp8u*)tmpPtrV + 4 * strideV)) - 1*mulNV12)), 0);
            }
            else
            {
                fillPlanes(tmpPtrY, strideY, tmpPtrU, strideU, tmpPtrV, strideV,
                    0, 0, 0, 0);
            }

            /* 3 quarter */

            if (isRightIntra)
            {
                if (isBelowIntra)
                {
                    diagonalConstruction(tmpPtrY + 8, strideY, tmpPtrU + 4*mulNV12, strideU,
                        tmpPtrV + 4*mulNV12, strideV, 3, isBelowRightIntra, 0);
                }
                else
                {
                    copyPlanesFromLeftRight(tmpPtrY + 8, strideY, tmpPtrU + 4*mulNV12, strideU,
                        tmpPtrV + 4*mulNV12, strideV, 0);

                }
            }
            else if (isBelowIntra)
            {
                copyPlanesFromAboveBelow(tmpPtrY + 8, strideY, tmpPtrU + 4*mulNV12, strideU,
                    tmpPtrV + 4*mulNV12, strideV, 0, 0);
            }
            else if (isBelowRightIntra)
            {
                fillPlanes(tmpPtrY + 8, strideY, tmpPtrU + 4*mulNV12, strideU, tmpPtrV + 4*mulNV12, strideV,
                    (Ipp32s)(*(((PlaneY*)((Ipp8u*)tmpPtrY + 8 * strideY)) + 16)),
                    (Ipp32s)(*(((PlaneUV*)((Ipp8u*)tmpPtrU + 4 * strideU)) + 8*mulNV12)),
                    (Ipp32s)(*(((PlaneUV*)((Ipp8u*)tmpPtrV + 4 * strideV)) + 8*mulNV12)), 0);
            }
            else
            {
                fillPlanes(tmpPtrY + 8, strideY, tmpPtrU + 4*mulNV12, strideU, tmpPtrV + 4*mulNV12, strideV,
                    0, 0, 0, 0);
            }

            tmpPtrY = ((PlaneY*)((Ipp8u*)tmpPtrY - 8 * strideY)) + 16;
            tmpPtrU = ((PlaneY*)((Ipp8u*)tmpPtrU - 4 * strideU)) + 8*mulNV12;
            tmpPtrV = ((PlaneY*)((Ipp8u*)tmpPtrV - 4 * strideV)) + 8*mulNV12;

            refMbAddr++;
        }
    }
}

void umc_h264_constrainedIntraUpsampling(//H264Slice *curSlice,
                                         //H264DecoderLayerDescriptor *curLayer,
                                         H264LayerResizeParameter *pResizeParameter,
                                         H264CoreEncoder_8u16s *curEnc,
                                         H264MacroblockGlobalInfo *gmbs,
                                         //H264DecoderMacroblockLayerInfo *lmbs,
                                         PlaneY  *pOutYPlane,
                                         PlaneUV *pOutUPlane,
                                         PlaneUV *pOutVPlane,
                                         PlaneY  *pRefYPlane,
                                         PlaneUV *pRefUPlane,
                                         PlaneUV * /*pRefVPlane*/,
                                         Ipp32s  *tmpBuf0,
                                         Ipp32s  *tmpBuf1,
                                         Ipp32s  *tmpBuf2,
                                         Ipp32s  *sliceNumBuf,
                                         Ipp32s   outLumaStride,
                                         Ipp32s   outChromaStride,
                                         Ipp32s   pixel_sz,
                                         Ipp32s   refLumaStride,
                                         Ipp32s   refChromaStride,
                                         Ipp32s   nv12)
{
    //const H264SeqParamSet *curSps = curSlice->GetSeqParam();
    //    H264SliceHeader *curSliceHeader = curSlice->GetSliceHeader();
    //H264DecoderLayerResizeParameter *pResizeParameter = curLayer->m_pResizeParameter;

    Ipp32s leftOffsetLuma, rightOffsetLuma, topOffsetLuma, bottomOffsetLuma;
    Ipp32s refWLuma, refHLuma, scaledWLuma, scaledHLuma;
    Ipp32s shiftXLuma, shiftYLuma, scaleXLuma, scaleYLuma;
    Ipp32s addXLuma, addYLuma, deltaYLuma;
    Ipp32s leftOffsetChroma, rightOffsetChroma, topOffsetChroma, bottomOffsetChroma;
    Ipp32s refWChroma, refHChroma, scaledWChroma, scaledHChroma;
    Ipp32s shiftXChroma, shiftYChroma, scaleXChroma, scaleYChroma;
    Ipp32s addXChroma, addYChroma, deltaXChroma, deltaYChroma;
    Ipp32s chromaShiftW, chromaShiftH;
    Ipp32s firstMB, lastMB, firstRefMB, lastRefMB;
    Ipp32s widthMB, heightMB, refwidthMB, MBAddr;
    Ipp32s MBNum, RefMBNum;
    //    Ipp32s lumaStride, chromaStride;
    Ipp32s startXLuma, endXLuma, startXChroma, endXChroma;
    PlaneY  *pYPlane;
    PlaneUV *pUPlane, *pVPlane, *ptrChromaRef, *prtOutChroma;
    Ipp32s i, j;

    if (nv12) nv12 = 1;
    const Ipp32s mulNV12 = nv12+1; // NV12 only

    leftOffsetLuma = pResizeParameter->leftOffset;
    topOffsetLuma = pResizeParameter->topOffset;
    rightOffsetLuma = pResizeParameter->rightOffset;
    bottomOffsetLuma = pResizeParameter->bottomOffset;

    scaledWLuma = pResizeParameter->scaled_ref_layer_width;
    scaledHLuma = pResizeParameter->scaled_ref_layer_height;

    widthMB = (scaledWLuma + leftOffsetLuma + rightOffsetLuma) >> 4;
    heightMB = (scaledHLuma + topOffsetLuma + bottomOffsetLuma) >> 4;
    MBNum =   heightMB * widthMB;

    refWLuma = pResizeParameter->ref_layer_width;
    refHLuma = pResizeParameter->ref_layer_height;
    refwidthMB = refWLuma >> 4;
    RefMBNum = (refHLuma >> 4) * refwidthMB;

    shiftXLuma = pResizeParameter->shiftX;
    shiftYLuma = pResizeParameter->shiftY;

    scaleXLuma = pResizeParameter->scaleX;
    scaleYLuma = pResizeParameter->scaleY;

    addXLuma = ((refWLuma << (shiftXLuma - 1)) + (scaledWLuma >> 1)) / scaledWLuma +
        (1 << (shiftXLuma- 5));

    addYLuma = ((refHLuma << (shiftYLuma - 1)) + (scaledHLuma >> 1)) / scaledHLuma +
        (1 << (shiftYLuma - 5));

    deltaYLuma = 8;

    /* To Do - fild to fild, fild to frame */

    /* Chroma */

    chromaShiftW = (3 > curEnc->m_SeqParamSet->chroma_format_idc) ? 1 : 0;
    chromaShiftH = (2 > curEnc->m_SeqParamSet->chroma_format_idc) ? 1 : 0;

    leftOffsetChroma = leftOffsetLuma >> chromaShiftW;
    rightOffsetChroma = rightOffsetLuma >> chromaShiftW;
    refWChroma = refWLuma >> chromaShiftW;
    scaledWChroma = scaledWLuma >> chromaShiftW;
    topOffsetChroma = topOffsetLuma >> chromaShiftH;
    bottomOffsetChroma = bottomOffsetLuma >> chromaShiftH;
    refHChroma = refHLuma >> chromaShiftH;
    scaledHChroma = scaledHLuma >> chromaShiftH;

    shiftXChroma = ((curEnc->m_SeqParamSet->level_idc <= 30) ? 16 : countShift(refWChroma));
    shiftYChroma = ((curEnc->m_SeqParamSet->level_idc <= 30) ? 16 : countShift(refHChroma));

    scaleXChroma = ((refWChroma << shiftXChroma) + (scaledWChroma >> 1)) / scaledWChroma;
    scaleYChroma = ((refHChroma << shiftYChroma) + (scaledHChroma >> 1)) / scaledHChroma;

    addXChroma = (((refWChroma * (2 + pResizeParameter->phaseX)) << (shiftXChroma - 2)) + (scaledWChroma >> 1)) /
        scaledWChroma + (1 << (shiftXChroma - 5));

    addYChroma = (((refHChroma * (2 + pResizeParameter->phaseY)) << (shiftYChroma - 2)) + (scaledHChroma >> 1)) /
        scaledHChroma + (1 << (shiftYChroma - 5));

    deltaXChroma = 4 * (2 + pResizeParameter->refPhaseX);
    deltaYChroma = 4 * (2 + pResizeParameter->refPhaseY);

    firstMB = 0;
    lastRefMB = firstRefMB = 0;

    pYPlane = (PlaneY*)tmpBuf2;
    pUPlane = (PlaneUV*)((Ipp8u*)pYPlane + refWLuma * refHLuma * pixel_sz);
    //pVPlane = (PlaneUV*)((Ipp8u*)pUPlane + refWChroma * refHChroma * pixel_sz); //nv12 ??
    pVPlane = pUPlane + 1; //nv12

    MBAddr = 0;
    for (j = 0; j < heightMB * 16; j += 16)
    {
        for (i = 0; i < widthMB * 16; i += 16)
        {
            if (sliceNumBuf[MBAddr] < 0)
            {
                Ipp32s X, Y;
                PlaneY *dstY = ((PlaneY*)((Ipp8u*)pOutYPlane + j * outLumaStride)) + i;
                PlaneUV *dstU;

                X = i >> chromaShiftW;
                Y = j >> chromaShiftH;

                clearMB(dstY, outLumaStride, 16, 16);
                //if (nv12) {
                    dstU = ((PlaneUV*)((Ipp8u*)pOutUPlane + Y * outChromaStride)) + (X << nv12);
                    clearMB(dstU, outChromaStride, 16 >> chromaShiftW << nv12, 16 >> chromaShiftH);
                //} else {
                //    PlaneUV *dstV;
                //    dstU = ((PlaneUV*)((Ipp8u*)pOutUPlane + Y * outChromaStride)) + X;
                //    dstV = ((PlaneUV*)((Ipp8u*)pOutVPlane + Y * outChromaStride)) + X;
                //    clearMB(dstU, outChromaStride, 16 >> chromaShiftW, 16 >> chromaShiftH);
                //    clearMB(dstV, outChromaStride, 16 >> chromaShiftW, 16 >> chromaShiftH);
                //}
            }
            MBAddr++;
        }
    }

    //    startXLuma, endXLuma, startXChroma, endXChroma;

    startXLuma = 0;
    if (leftOffsetLuma < 0)
        startXLuma = -leftOffsetLuma;

    endXLuma = scaledWLuma;
    if (rightOffsetLuma < 0)
        endXLuma = scaledWLuma + rightOffsetLuma;

    startXChroma = startXLuma >> chromaShiftW;
    endXChroma = endXLuma >> chromaShiftW;

    for (;;)
    {
        Ipp32s startY, finishY, curSliceNum;
        Ipp32s startYMB, finishYMB;
        Ipp32s tmpTopOffset, tmpBottomOffset;
        //        Ipp32s firstZeroMB, lastZeroMB;
        Ipp32s ii;

        if (lastRefMB >= RefMBNum)
            break;

        firstRefMB = lastRefMB;

        curSliceNum = gmbs[lastRefMB].slice_id;


        while (gmbs[lastRefMB].slice_id == curSliceNum)
        {
            lastRefMB++;
            if (lastRefMB >= RefMBNum)
                break;
        }


        for (firstMB = 0; firstMB < MBNum; firstMB++)
        {
            if (sliceNumBuf[firstMB] == curSliceNum)
            {
                break;
            }
        }

        if (firstMB >= MBNum)
            continue;

        /* copy reference */
        startY = (firstRefMB / refwidthMB) - 1;
        if (startY < 0) startY = 0;

        finishY = ((lastRefMB + refwidthMB - 1)/refwidthMB) + 1;
        if (finishY >= RefMBNum) finishY = RefMBNum;

        for (i = startY << 4; i < finishY << 4; i++)
        {
            MFX_INTERNAL_CPY(pYPlane + i * refWLuma * pixel_sz,
                pRefYPlane + i * refLumaStride,
                refWLuma * pixel_sz);
        }

        for (i = startY << (4 - chromaShiftH); i < finishY << (4 - chromaShiftH); i++)
        {
            //if (nv12) {
            //    PlaneUV *u, *v, *uvref;
            //    u = pUPlane + i * refWChroma * pixel_sz;
            //    v = pVPlane + i * refWChroma * pixel_sz;
            //    uvref = pRefUPlane + i * refChromaStride;
            //    for (Ipp32s ii = 0; ii < refWChroma; ii++) {
            //    u[ii] = uvref[2*ii];
            //    v[ii] = uvref[2*ii+1];
            //    }
            //} else {
            //    MFX_INTERNAL_CPY(pUPlane + i * refWChroma * pixel_sz,
            //        pRefUPlane + i * refChromaStride,
            //        refWChroma * pixel_sz);
            //    MFX_INTERNAL_CPY(pVPlane + i * refWChroma * pixel_sz,
            //        pRefVPlane + i * refChromaStride,
            //        refWChroma * pixel_sz);
            //}
            MFX_INTERNAL_CPY(pUPlane + i * refWChroma * pixel_sz,
                pRefUPlane + i * refChromaStride,
                refWChroma * pixel_sz << nv12);
        }

        constrainedIntrainterpolateInterMBs(pYPlane, refWLuma * pixel_sz,
            pUPlane, refWChroma * pixel_sz,
            pVPlane, refWChroma * pixel_sz,
            startY, finishY,
            refWLuma >> 4, refHLuma >> 4,
            gmbs, /*lmbs,*/ curSliceNum);

        /* find the first and last dst row */

        lastMB = firstMB;

        for (i = firstMB; i < MBNum; i++)
        {
            if (sliceNumBuf[i] == curSliceNum)
            {
                lastMB = i;
            }
        }

        lastMB += 1;

        /* Luma */

        startYMB = firstMB / widthMB;
        finishYMB = (lastMB + widthMB - 1) / widthMB;

        startY = startYMB * 16;
        finishY = finishYMB * 16;

        startY -= topOffsetLuma;
        if (startY < 0)
            startY = 0;

        finishY -= topOffsetLuma;

        if (finishY > scaledHLuma)
            finishY = scaledHLuma;

        if (finishY > scaledHLuma + bottomOffsetLuma)
            finishY = scaledHLuma + bottomOffsetLuma;

        bicubicLuma(refWLuma, refHLuma, refWLuma * pixel_sz, pYPlane,
            startXLuma, endXLuma, startY, finishY, widthMB * 4 * 16,
            tmpBuf0 + leftOffsetLuma + topOffsetLuma * widthMB * 16,
            scaleXLuma, addXLuma, shiftXLuma,
            scaleYLuma, addYLuma, shiftYLuma, deltaYLuma, tmpBuf1,
            0);

        startY = startYMB * 16;
        finishY = finishYMB * 16;

        tmpTopOffset = topOffsetLuma - startY;
        if (tmpTopOffset < 0) tmpTopOffset = 0;

        tmpBottomOffset = finishY - (scaledHLuma + topOffsetLuma);
        if (tmpBottomOffset < 0) tmpBottomOffset = 0;

        border(endXLuma - startXLuma, finishY - startY - tmpTopOffset - tmpBottomOffset,
            widthMB * 4 * 16, startXLuma + leftOffsetLuma,
            scaledWLuma + rightOffsetLuma - endXLuma,
            tmpTopOffset, tmpBottomOffset, tmpBuf0);

        MBAddr = startYMB * widthMB;
        for (j = startY; j < finishY; j += 16)
        {
            for (i = 0; i < widthMB * 16; i += 16)
            {
                if (sliceNumBuf[MBAddr] == curSliceNum)
                {
                    Ipp32s *src = ((Ipp32s*)((Ipp8u*)tmpBuf0 + j * widthMB * 16 * 4)) + i;
                    PlaneY *dst = ((PlaneY*)((Ipp8u*)pOutYPlane + j * outLumaStride)) + i;

                    copySatMB(src, widthMB * 16 * 4, dst, outLumaStride, 16, 16);
                }
                MBAddr++;
            }
        }

        /* Chroma */

        ptrChromaRef = pUPlane;
        prtOutChroma = pOutUPlane;

        for (ii = 0; ii < 2; ii++)
        {
            startY = startYMB << (4 - chromaShiftH);
            finishY = finishYMB << (4 - chromaShiftH);

            startY -= topOffsetChroma;
            if (startY < 0)
                startY = 0;

            finishY -= topOffsetChroma;

            if (finishY > scaledHChroma)
                finishY = scaledHChroma;

            if (finishY > scaledHChroma + bottomOffsetChroma)
                finishY = scaledHChroma + bottomOffsetChroma;


            bilinearChromaConstrained(refWChroma, refHChroma, (refWChroma) * pixel_sz*mulNV12, ptrChromaRef,
                startXChroma, endXChroma, startY, finishY, widthMB*mulNV12 << (6 - chromaShiftW),
                tmpBuf0 + leftOffsetChroma*mulNV12 + topOffsetChroma * (widthMB*mulNV12 << (4 - chromaShiftW)),
                scaleXChroma, addXChroma, shiftXChroma, deltaXChroma,
                scaleYChroma, addYChroma, shiftYChroma, deltaYChroma, tmpBuf1);

            startY = startYMB << (4 - chromaShiftH);
            finishY = finishYMB << (4 - chromaShiftH);

            tmpTopOffset = topOffsetChroma - startY;
            if (tmpTopOffset < 0) tmpTopOffset = 0;

            tmpBottomOffset = finishY - (scaledHChroma + topOffsetChroma);
            if (tmpBottomOffset < 0) tmpBottomOffset = 0;

            if(nv12) {
                border(scaledWChroma*mulNV12, finishY - startY - tmpTopOffset - tmpBottomOffset,
                    widthMB*mulNV12 << (6 - chromaShiftW), (startXChroma + leftOffsetChroma)*mulNV12,
                    (scaledWChroma + rightOffsetChroma - endXChroma)*mulNV12,
                    tmpTopOffset, tmpBottomOffset, tmpBuf0); // NV12 write for both U and V twice, for a while
            } else {
                border(scaledWChroma, finishY - startY - tmpTopOffset - tmpBottomOffset,
                    widthMB << (6 - chromaShiftW), startXChroma + leftOffsetChroma,
                    scaledWChroma + rightOffsetChroma - endXChroma,
                    tmpTopOffset, tmpBottomOffset, tmpBuf0);
            }

            MBAddr = startYMB * widthMB;
            for (j = startY; j < finishY; j += (1 << (4 - chromaShiftH)))
            {
                for (i = 0; i < widthMB << (4 - chromaShiftW); i += (1 << (4 - chromaShiftW)))
                {
                    if (sliceNumBuf[MBAddr] == curSliceNum)
                    {
                        Ipp32s *src = ((Ipp32s*)((Ipp8u*)tmpBuf0 + j * (widthMB*mulNV12 << (6 - chromaShiftW)))) + i*mulNV12;
                        PlaneY *dst = ((PlaneUV*)((Ipp8u*)prtOutChroma + j * outChromaStride)) + (i << nv12);

                        if (nv12) {
                            copyMB_NV12(src, widthMB*mulNV12 << (6 - chromaShiftW), dst, outChromaStride,
                                16 >> chromaShiftW, 16 >> chromaShiftH);
                        } else {
                            copyMB(src, widthMB << (6 - chromaShiftW), dst, outChromaStride,
                                16 >> chromaShiftW, 16 >> chromaShiftH);
                        }
                    }
                    MBAddr++;
                }
            }

            ptrChromaRef = pVPlane;
            prtOutChroma = pOutVPlane + nv12;
        }
    }
}

void SVCDeblocking(H264CoreEncoder_8u16s *curEnc,
                   H264CoreEncoder_8u16s *refEnc) {
    curEnc->m_deblocking_IL = 1;

    curEnc->ref_enc = refEnc;

    for (curEnc->m_deblocking_stage2 = 0; curEnc->m_deblocking_stage2 < 2; curEnc->m_deblocking_stage2++) {
        refEnc->m_field_index = 0;
        for (Ipp32s slice = (Ipp32s)refEnc->m_info.num_slices*refEnc->m_field_index; slice < refEnc->m_info.num_slices*(refEnc->m_field_index+1); slice++){
            H264CoreEncoder_DeblockSlice_8u16s(curEnc, refEnc->m_Slices + slice, refEnc->m_Slices[slice].m_first_mb_in_slice + refEnc->m_WidthInMBs*refEnc->m_HeightInMBs*refEnc->m_field_index, refEnc->m_Slices[slice].m_MB_Counter);
        }
        Ipp32s deblocking_idc = curEnc->m_Slices[0].m_disable_inter_layer_deblocking_filter_idc;
        if (deblocking_idc != DEBLOCK_FILTER_ON_2_PASS &&
            deblocking_idc != DEBLOCK_FILTER_ON_2_PASS_NO_CHROMA)
            break;
    }
}

void LoadMBInfoFromRefLayer(//H264SegmentDecoderMultiThreaded * sd
                                    H264CoreEncoder_8u16s *curEnc,
                                    H264CoreEncoder_8u16s *refEnc)
{
    //H264Slice *pSlice = sd->m_pSlice;
    H264EncoderFrame_8u16s  *pPredFrame, *pFrame = curEnc->m_pCurrentFrame; // current layer
    H264EncoderFrame_8u16s *pRefFrame, *pRecFrame = refEnc->m_pCurrentFrame; // reference layer
    ////pRefFrame = H264EncoderFrameList_findSVCref_8u16s(
    ////    &refEnc->m_dpb, curEnc->m_pCurrentFrame); // pSlice->GetCurrentFrame();
    if(!refEnc->m_pReconstructFrame)
      refEnc->m_pReconstructFrame = refEnc->m_pReconstructFrame_allocated; // tmp ahead
    pRefFrame = refEnc->m_pReconstructFrame; // were swapped after were encoded
    if(!curEnc->m_pReconstructFrame)
      curEnc->m_pReconstructFrame = curEnc->m_pReconstructFrame_allocated; // tmp ahead
    pPredFrame = curEnc->m_pReconstructFrame;
    //H264SliceHeader *pSliceHeader = pSlice->GetSliceHeader();
    Ipp32s pitch_luma = pFrame->m_pitchBytes; // pitch_luma();
    Ipp32s pitch_chroma = pFrame->m_pitchBytes; //only nv12 //pitch_chroma();
    Ipp32s pixel_sz = 1; //(pFrame->m_bpp > 8) + 1;
    //Ipp32s chromaShift = 0; //only nv12 //(3 > pFrame->m_chroma_format) ? 1 : 0;
    //H264DecoderLayerDescriptor *layerMb, *curlayerMb;
    //const H264SeqParamSet *curSps = &curEnc->m_SeqParamSet; //pSlice->GetSeqParam();
    Ipp32s nv12 = 1; //(pFrame->GetColorFormat() == NV12);

    H264SliceType *pSlice = curEnc->m_Slices;
    sNALUnitHeaderSVCExtension* svc = &curEnc->m_svc_layer.svc_ext;
    H264LayerResizeParameter* resize_param = &curEnc->m_svc_layer.resize;

    if (/*pSlice->m_bIsFirstSliceInDepLayer && (pFrame->m_currentLayer > 0) &&
        (pSliceHeader->ref_layer_dq_id >= 0)*/
        !svc->no_inter_layer_pred_flag && refEnc)
    {
        Ipp32s refWidth  = refEnc->m_info.info.clip_info.width;
        Ipp32s refHeight = refEnc->m_info.info.clip_info.height;
        Ipp32s curWidth  = curEnc->m_info.info.clip_info.width;
        Ipp32s curHeight = curEnc->m_info.info.clip_info.height;
        //Ipp32s refChromaWidth = refWidth >> 1; //((Ipp32s) (3 > pFrame->m_chroma_format));

        //VM_ASSERT((pSliceHeader->ref_layer_dq_id >> 4) < pFrame->m_currentLayer);
        //            VM_ASSERT(pSliceHeader->ref_layer_dq_id == 0);

        //layerMb = &sd->m_mbinfo.layerMbs[(pSliceHeader->ref_layer_dq_id >> 4)];
        //curlayerMb = &sd->m_mbinfo.layerMbs[pFrame->m_currentLayer];

        if (resize_param->m_spatial_resolution_change) {
            MFX_INTERNAL_CPY(curEnc->m_pYDeblockingILPlane, pRefFrame->m_pYPlane, pRefFrame->uHeight*pRefFrame->m_pitchBytes);
            MFX_INTERNAL_CPY(curEnc->m_pUDeblockingILPlane, pRefFrame->m_pUPlane, pRefFrame->uHeight*pRefFrame->m_pitchBytes/2);
            SVCDeblocking(curEnc, refEnc);
        }

        if (resize_param->m_spatial_resolution_change &&
            pSlice->m_constrained_intra_resampling_flag)
        {
            constrainedIntraResamplingCheckMBs(resize_param, curEnc,
                pFrame->m_mbinfo.mbs,
                curEnc->m_tmpBuf2,
                curWidth,
                curHeight);

            umc_h264_constrainedIntraUpsampling(resize_param, curEnc,
                pPredFrame->m_mbinfo.mbs,
                (PlanePtrY) pPredFrame->m_pYPlane,
                (PlanePtrUV)pPredFrame->m_pUPlane,
                (PlanePtrUV)pPredFrame->m_pVPlane,
                (PlanePtrY) pRefFrame->m_pYPlane,
                (PlanePtrUV)pRefFrame->m_pUPlane,
                (PlanePtrUV)pRefFrame->m_pVPlane,
                curEnc->m_tmpBuf0,
                curEnc->m_tmpBuf1,
                curEnc->m_tmpBuf2 +
                (curWidth >> 4) * (curHeight >> 4),
                curEnc->m_tmpBuf2,
                pitch_luma,
                pitch_chroma,
                pixel_sz,
                pRefFrame->m_pitchBytes,
                pRefFrame->m_pitchBytes,
                nv12);
        }
        else
        {
            if (resize_param->m_spatial_resolution_change) {
                interpolateInterMBs(pRecFrame,
                    curEnc->m_pYDeblockingILPlane,
                    curEnc->m_pUDeblockingILPlane,
                    curEnc->m_pVDeblockingILPlane,
                    pRecFrame->m_pitchBytes,
                    refWidth >> 4, refHeight >> 4);

                umc_h264_upsampling(resize_param, curEnc,
                    (PlanePtrY) pPredFrame->m_pYPlane,
                    (PlanePtrUV)pPredFrame->m_pUPlane,
                    resize_param->m_spatial_resolution_change ? (PlanePtrY) curEnc->m_pYDeblockingILPlane : pRefFrame->m_pYPlane,
                    resize_param->m_spatial_resolution_change ? (PlanePtrUV)curEnc->m_pUDeblockingILPlane : pRefFrame->m_pUPlane,
                    pitch_luma,
                    pitch_chroma,
                    pRefFrame->m_pitchBytes,
                    pRefFrame->m_pitchBytes);
            } else {
                MFX_INTERNAL_CPY(pPredFrame->m_pYPlane, pRefFrame->m_pYPlane, pRefFrame->uHeight*pRefFrame->m_pitchBytes);
                MFX_INTERNAL_CPY(pPredFrame->m_pUPlane, pRefFrame->m_pUPlane, pRefFrame->uHeight*pRefFrame->m_pitchBytes/2);
            }
        }

        umc_h264_mv_resampling(resize_param, curEnc, refEnc, &(pPredFrame->m_mbinfo),
            &curEnc->m_mbinfo);

        if (resize_param->m_spatial_resolution_change) {
            upsamplingResidual(resize_param, curEnc, refEnc,
                (Ipp8u *)curEnc->m_tmpBuf2,
                refWidth, refHeight);
        } else {
            Ipp32s mbWidth = curEnc->m_SeqParamSet->frame_width_in_mbs;
            Ipp32s mbHeight = curEnc->m_SeqParamSet->frame_height_in_mbs;
            Ipp32s mbNum = mbWidth * mbHeight;

            MFX_INTERNAL_CPY(curEnc->m_pYInputBaseResidual, refEnc->m_pYInputResidual, mbNum*256*sizeof(COEFFSTYPE));
            MFX_INTERNAL_CPY(curEnc->m_pUInputBaseResidual, refEnc->m_pUInputResidual, mbNum*64*sizeof(COEFFSTYPE));
            MFX_INTERNAL_CPY(curEnc->m_pVInputBaseResidual, refEnc->m_pVInputResidual, mbNum*64*sizeof(COEFFSTYPE));
            MFX_INTERNAL_CPY(curEnc->m_pYInputResidual, refEnc->m_pYInputResidual, mbNum*256*sizeof(COEFFSTYPE));
            MFX_INTERNAL_CPY(curEnc->m_pUInputResidual, refEnc->m_pUInputResidual, mbNum*64*sizeof(COEFFSTYPE));
            MFX_INTERNAL_CPY(curEnc->m_pVInputResidual, refEnc->m_pVInputResidual, mbNum*64*sizeof(COEFFSTYPE));
            MFX_INTERNAL_CPY(curEnc->m_pYInputRefPlane, refEnc->m_pYInputRefPlane, pRefFrame->uHeight*pRefFrame->m_pitchBytes);
            MFX_INTERNAL_CPY(curEnc->m_pUInputRefPlane, refEnc->m_pUInputRefPlane, pRefFrame->uHeight*pRefFrame->m_pitchBytes/2);

//            curEnc->m_SavedMacroblockCoeffs = refEnc->m_SavedMacroblockCoeffs;
//            curEnc->m_SavedMacroblockTCoeffs = refEnc->m_SavedMacroblockTCoeffs;
//            curEnc->m_mbinfo.intra_types = refEnc->m_mbinfo.intra_types;
            MFX_INTERNAL_CPY(curEnc->m_SavedMacroblockCoeffs, refEnc->m_SavedMacroblockCoeffs,
                mbNum*COEFFICIENTS_BUFFER_SIZE*sizeof(COEFFSTYPE));
            MFX_INTERNAL_CPY(curEnc->m_SavedMacroblockTCoeffs, refEnc->m_SavedMacroblockTCoeffs,
                mbNum*COEFFICIENTS_BUFFER_SIZE*sizeof(COEFFSTYPE));
            MFX_INTERNAL_CPY(curEnc->m_mbinfo.intra_types, refEnc->m_mbinfo.intra_types,
                mbNum*sizeof(H264MacroblockIntraTypes));
        }

        ////pFrame->m_use_base_residual = 1; // ?

        ////if (!curEnc->m_spatial_resolution_change &&
        ////    (resize_param.leftOffset ||
        ////    resize_param.topOffset))
        ////{
        ////    H264LayerResizeParameter *pResizeParameter = &resize_param;
        ////    Ipp32s MbW = (Ipp32s)curSps->frame_width_in_mbs;
        ////    Ipp32s MbH = (Ipp32s)curSps->frame_height_in_mbs;
        ////    Ipp32s leftOffset = pResizeParameter->leftOffset;
        ////    Ipp32s rightOffset = pResizeParameter->rightOffset;
        ////    Ipp32s topOffset = pResizeParameter->topOffset;
        ////    Ipp32s bottomOffset = pResizeParameter->bottomOffset;
        ////    //                Ipp32s refMbW = MbW - (leftOffset + rightOffset);
        ////    Ipp32s startH, endH, startW, endW, breakPoint;
        ////    Ipp32s numCoeff;
        ////    Ipp32s i, j, k;
        ////    Ipp32s chroma_format = curEnc->m_SeqParamSet->chroma_format_idc;

        ////    numCoeff = (256 + (2 * 256 >> ((Ipp32s) (3 > chroma_format)))) >>
        ////        ((Ipp32s) (2 > chroma_format));

        ////    startH = (topOffset < 0) ? 0 : topOffset;
        ////    endH = (bottomOffset < 0) ? MbH : MbH - bottomOffset;
        ////    startW = (leftOffset < 0) ? 0 : leftOffset;
        ////    endW = (rightOffset < 0) ? MbW : MbW - rightOffset;
        ////    if ((leftOffset + rightOffset) == 0)
        ////    {
        ////        if ((leftOffset + topOffset * MbW) < 0)
        ////        {
        ////            breakPoint = endH;
        ////        }
        ////        else
        ////        {
        ////            breakPoint = startH;
        ////        }

        ////    }
        ////    else
        ////    {
        ////        breakPoint = topOffset - ((leftOffset + topOffset * MbW)/(leftOffset + rightOffset));
        ////    }

        ////    if (breakPoint < startH)
        ////        breakPoint = startH;

        ////    if (breakPoint > endH)
        ////        breakPoint = endH;

        ////    /* shift < 0 */
        ////    for (j = startH; j < breakPoint; j++)
        ////    {
        ////        Ipp32s MbAddr = j * MbW;
        ////        Ipp32s shift = (j - topOffset) * (leftOffset + rightOffset) + leftOffset + topOffset * MbW;

        ////        for (i = startW; i < endW; i++)
        ////        {
        ////            Ipp16s *pCoeffPtr, *pRefCoeffPtr;
        ////            if (pRefFrame->m_mbinfo.mbs[MbAddr+i-shift].mbtype < MBTYPE_PCM)
        ////            {
        ////                IntraType *pMBIntraTypes = sd->m_pMBIntraTypes + (MbAddr+i)*NUM_INTRA_TYPE_ELEMENTS;
        ////                IntraType *pRefMBIntraTypes = sd->m_pMBIntraTypes + (MbAddr+i-shift)*NUM_INTRA_TYPE_ELEMENTS;

        ////                for (k = 0; k < NUM_INTRA_TYPE_ELEMENTS; k++)
        ////                {
        ////                    pMBIntraTypes[k] = pRefMBIntraTypes[k];
        ////                }
        ////            }

        ////            pCoeffPtr = sd->m_mbinfo.SavedMacroblockTCoeffs + (MbAddr+i)*COEFFICIENTS_BUFFER_SIZE;
        ////            pRefCoeffPtr = sd->m_mbinfo.SavedMacroblockTCoeffs + (MbAddr+i-shift)*COEFFICIENTS_BUFFER_SIZE;

        ////            for (k = 0; k < numCoeff; k++)
        ////            {
        ////                pCoeffPtr[k] = pRefCoeffPtr[k];
        ////            }

        ////            pCoeffPtr = sd->m_mbinfo.SavedMacroblockCoeffs + (MbAddr+i)*COEFFICIENTS_BUFFER_SIZE;
        ////            pRefCoeffPtr = sd->m_mbinfo.SavedMacroblockCoeffs + (MbAddr+i-shift)*COEFFICIENTS_BUFFER_SIZE;

        ////            for (k = 0; k < numCoeff; k++)
        ////            {
        ////                pCoeffPtr[k] = pRefCoeffPtr[k];
        ////            }

        ////            pFrame->m_mbinfo.mbs[MbAddr+i].mbflags = pFrame->m_mbinfo.mbs[MbAddr+i-shift].mbflags;
        ////            sd->m_mbinfo.mbs[MbAddr+i].QP = sd->m_mbinfo.mbs[MbAddr+i-shift].QP;
        ////        }
        ////    }

        ////    /* shift > 0 */

        ////    for (j = endH-1; j >= breakPoint; j--)
        ////    {
        ////        Ipp32s MbAddr = j * MbW;
        ////        Ipp32s shift = (j - topOffset) * (leftOffset + rightOffset) + leftOffset + topOffset * MbW;

        ////        for (i = endW - 1; i >= startW; i--)
        ////        {
        ////            Ipp16s *pCoeffPtr, *pRefCoeffPtr;
        ////            if (pRefFrame->m_mbinfo.mbs[MbAddr+i-shift].mbtype < MBTYPE_PCM)
        ////            {
        ////                IntraType *pMBIntraTypes = sd->m_pMBIntraTypes + (MbAddr+i)*NUM_INTRA_TYPE_ELEMENTS;
        ////                IntraType *pRefMBIntraTypes = sd->m_pMBIntraTypes + (MbAddr+i-shift)*NUM_INTRA_TYPE_ELEMENTS;

        ////                for (k = 0; k < NUM_INTRA_TYPE_ELEMENTS; k++)
        ////                {
        ////                    pMBIntraTypes[k] = pRefMBIntraTypes[k];
        ////                }
        ////            }

        ////            pCoeffPtr = sd->m_mbinfo.SavedMacroblockTCoeffs + (MbAddr+i)*COEFFICIENTS_BUFFER_SIZE;
        ////            pRefCoeffPtr = sd->m_mbinfo.SavedMacroblockTCoeffs + (MbAddr+i-shift)*COEFFICIENTS_BUFFER_SIZE;

        ////            for (k = 0; k < numCoeff; k++)
        ////            {
        ////                pCoeffPtr[k] = pRefCoeffPtr[k];
        ////            }

        ////            pCoeffPtr = sd->m_mbinfo.SavedMacroblockCoeffs + (MbAddr+i)*COEFFICIENTS_BUFFER_SIZE;
        ////            pRefCoeffPtr = sd->m_mbinfo.SavedMacroblockCoeffs + (MbAddr+i-shift)*COEFFICIENTS_BUFFER_SIZE;

        ////            for (k = 0; k < numCoeff; k++)
        ////            {
        ////                pCoeffPtr[k] = pRefCoeffPtr[k];
        ////            }

        ////            pFrame->m_mbinfo.mbs[MbAddr+i].mbflags = pFrame->m_mbinfo.mbs[MbAddr+i-shift].mbflags;
        ////            sd->m_mbinfo.mbs[MbAddr+i].QP = sd->m_mbinfo.mbs[MbAddr+i-shift].QP;
        ////        }
        ////    }
        ////}
    } else /*if (pSlice->m_bIsFirstSliceInDepLayer)*/ {
        //H264Slice *pSlice = sd->m_pSlice;
        //H264DecoderFrame *pFrame = pSlice->GetCurrentFrame();
        IppiSize lumaSize, chromaSize;

        lumaSize.width  = curEnc->m_info.info.clip_info.width;
        lumaSize.height = curEnc->m_info.info.clip_info.height;

        chromaSize.width = lumaSize.width >> 1; //((Ipp32s) (3 > pFrame->m_chroma_format));
        chromaSize.height = lumaSize.height >> 1; //((Ipp32s) (2 > pFrame->m_chroma_format));

        if (curEnc->m_pYResidual) {
            ippsZero_16s(curEnc->m_pYResidual, lumaSize.width * lumaSize.height);
            ippsZero_16s(curEnc->m_pUResidual, chromaSize.width * chromaSize.height);
            ippsZero_16s(curEnc->m_pVResidual, chromaSize.width * chromaSize.height);
        }

        //pFrame->m_use_base_residual = 0;
    }

    /*if (pSlice->m_bIsFirstSliceInDepLayer)*/
    {
        int iii;
        int size = refEnc->m_info.info.clip_info.width *
            refEnc->m_info.info.clip_info.height;

        if (curEnc->m_spatial_resolution_change)
        {
            for (iii = 0; iii < size; iii++)
            {
                curEnc->m_SavedMacroblockCoeffs[iii] = 0;
                curEnc->m_SavedMacroblockTCoeffs[iii] = 0;
            }

            ippsZero_16s(curEnc->m_SavedMacroblockTCoeffs,
                (curEnc->m_info.info.clip_info.width >> 4) *
                (curEnc->m_info.info.clip_info.height >> 4) * COEFFICIENTS_BUFFER_SIZE);
        }

        //            pFrame->InitFrameData(pSlice);
        //            sd->m_pTaskBroker->m_pTaskSupplier->InitFreeFrame(pFrame, pSlice);
    }
}

static Ipp32s minPositive(Ipp32s x, Ipp32s y)
{
    if ((x >= 0) && (y >= 0))
    {
        return ((x < y) ? x : y);
    }
    else
    {
        return ((x > y) ? x : y);
    }
}

static Ipp32s mvDiff(Ipp32s *mv1,
                     Ipp32s *mv2)
{
    Ipp32s tmp0 = mv1[0] - mv2[0];
    Ipp32s tmp1 = mv1[1] - mv2[1];

    if (tmp0 < 0) tmp0 = -tmp0;
    if (tmp1 < 0) tmp1 = -tmp1;

    return (tmp0 + tmp1);
}


static void umc_h264_MB_mv_resampling(//H264SegmentDecoderMultiThreaded *sd,
                                      //H264DecoderLayerDescriptor *curLayerD,
                                      H264LayerResizeParameter *pResizeParameter,
                                      H264CoreEncoder_8u16s *curEnc,
                                      H264CoreEncoder_8u16s *refEnc,
                                      Ipp32s curMB_X,
                                      Ipp32s curMB_Y,
                                      Ipp32s curMBAddr,
                                      Ipp32s slice_type,
                                      //H264DecoderLayerDescriptor *refLayerMb,
                                      Ipp32s RestrictedSpatialResolutionChangeFlag,
                                      Ipp32s CroppingChangeFlag)
{
    H264GlobalMacroblocksDescriptor *refLayerMb = &(refEnc->m_pCurrentFrame->m_mbinfo);
    H264GlobalMacroblocksDescriptor *gmbinfo = &(curEnc->m_pReconstructFrame->m_mbinfo);
    H264LocalMacroblockDescriptor *mbinfo = &curEnc->m_mbinfo;
    Ipp32s curFieldFlag = curEnc->m_SeqParamSet->frame_mbs_only_flag ? 0 : 1;

    Ipp32s refLayerPartIdc[4][4];
    Ipp32s refIdxILPredL[2][2][2];
    Ipp32s tempRefIdxPredL[2][4][4];
    Ipp32s mvILPredL[2][4][4][2];
    Ipp32s tempMvAL[2][2];
    Ipp32s tempMvBL[2][2];
    Direction_t sbdir[4];
    H264MacroblockLocalInfo *lmbs;
    H264MacroblockGlobalInfo *gmbs;
    H264MacroblockMVs *MV[2];
    H264MacroblockRefIdxs *RefIdxs[2];
    //H264DecoderLayerResizeParameter *pResizeParameter;
    Ipp32s refMbType = 0;
    Ipp32s intraILPredFlag;
    Ipp32s mbtype, sbtype, subblock;
    Ipp32s maxX; // directions: 2 for B slice
    Ipp32s x, y, i, j;
    Ipp32s fieldInFrameFlag = 0;
    Ipp32s mbStride = refEnc->m_SeqParamSet->frame_width_in_mbs;
    Ipp32s isRefMbaff = refEnc->m_SliceHeader.MbaffFrameFlag;

    if (curEnc->m_SliceHeader.MbaffFrameFlag && curFieldFlag)
        fieldInFrameFlag = 1;

    lmbs = &(mbinfo->mbs[curMBAddr]);
    gmbs = &(gmbinfo->mbs[curMBAddr]);
    MV[0] = &(gmbinfo->MV[0][curMBAddr]);
    MV[1] = &(gmbinfo->MV[1][curMBAddr]);
    //RefIdxs[0] = GetRefIdxs(gmbinfo, 0, curMBAddr);
    //RefIdxs[1] = GetRefIdxs(gmbinfo, 1, curMBAddr);
    RefIdxs[0] = &(gmbinfo->RefIdxs[0][curMBAddr]);
    RefIdxs[1] = &(gmbinfo->RefIdxs[1][curMBAddr]);
    //pResizeParameter = curLayerD->m_pResizeParameter;

    maxX = 1;
    if (slice_type == BPREDSLICE)
        maxX = 2;

    for (y = 0; y < 4; y++)
    {
        Ipp32s t_curMB_Y = ((curMB_Y >> fieldInFrameFlag) << (4 + fieldInFrameFlag)) + (curMB_Y & fieldInFrameFlag);

        Ipp32s yRef = t_curMB_Y + ((4 * y + 1) << fieldInFrameFlag);
        Ipp32s refMbPartIdxY, refMbAddrY;

        yRef = (yRef * pResizeParameter->scaleY + pResizeParameter->addY) >> pResizeParameter->shiftY;
        refMbAddrY = (yRef >> 4) * mbStride;

        for (x = 0; x < 4; x++)
        {
            Ipp32s xRef = ((curMB_X * 16 + 4 * x + 1) * pResizeParameter->scaleX +
                pResizeParameter->addX) >> pResizeParameter->shiftX;
            Ipp32s refMbAddr = umc_h264_getAddr(isRefMbaff, mbStride, (xRef >> 4), (yRef >> 4));
            Ipp32s refMbPartIdxX, idx8x8Base, idx4x4Base, base8x8MbPartIdx, base4x4SubMbPartIdx;
            //Ipp32s refFieldFlag = GetMBFieldDecodingFlag(refLayerMb->mbs[refMbAddr]);
            Ipp32s refFieldFlag = refEnc->m_SeqParamSet->frame_mbs_only_flag ? 0 : 1;
            Ipp32s mbOffset = 0;

            refMbPartIdxX = xRef & 15;

            if (!curEnc->m_SliceHeader.MbaffFrameFlag && !refFieldFlag) {
                refMbPartIdxY = yRef & 15;
            } else if (refFieldFlag == curFieldFlag) {
                refMbAddr     = mbOffset + (yRef >> 4) * mbStride + (xRef >> 4);
                refMbPartIdxY = ( yRef & ( 15 + 16 * curFieldFlag ) ) >> curFieldFlag;
            } else if (!curFieldFlag) {
                Ipp32s     iBaseTopMbY = ( (yRef >> 4) >> 1 ) << 1;
                Ipp32s topAddr = mbOffset + umc_h264_getAddr(isRefMbaff, mbStride, (xRef >> 4), iBaseTopMbY);
                Ipp32s botAddr = topAddr + 1;
                refMbAddr = IS_INTRA_MBTYPE(refLayerMb->mbs[topAddr].mbtype) ? botAddr: topAddr;
                refMbPartIdxY     = ( ( ( yRef >> 4 ) & 1 ) << 3 ) + ( ( ( yRef & 15 ) >> 3 ) << 2 );
            } else {
                refMbPartIdxY = yRef & 15;
                refMbPartIdxX = xRef & 15;
            }

            idx8x8Base    = ( (   refMbPartIdxY       >> 3 ) << 1 ) + (   refMbPartIdxX       >> 3 );
            idx4x4Base    = ( ( ( refMbPartIdxY & 7 ) >> 2 ) << 1 ) + ( ( refMbPartIdxX & 7 ) >> 2 );
            base8x8MbPartIdx     = 0;
            base4x4SubMbPartIdx  = 0;

            ////Ipp32s eMbModeBase = 0;

            refMbType = refLayerMb->mbs[refMbAddr].mbtype;

            if (refLayerMb->mbs[refMbAddr].mbtype <= MBTYPE_PCM)
            {
                refLayerPartIdc[y][x] = INTRA;
                continue;
            }

            ////if (refLayerMb->mbs[refMbAddr].mbtype == MBTYPE_INTER_16x8)
            ////    eMbModeBase = 2;
            ////else if (refLayerMb->mbs[refMbAddr].mbtype == MBTYPE_INTER_8x16)
            ////    eMbModeBase = 3;
            ////else if (refLayerMb->mbs[refMbAddr].mbtype == MBTYPE_INTER_8x8 ||
            ////    refLayerMb->mbs[refMbAddr].mbtype == MBTYPE_INTER_8x8_REF0)
            ////    eMbModeBase = 4;
            ////else if (refLayerMb->mbs[refMbAddr].mbtype == MBTYPE_BIDIR ||
            ////    refLayerMb->mbs[refMbAddr].mbtype == MBTYPE_BACKWARD ||
            ////    refLayerMb->mbs[refMbAddr].mbtype == MBTYPE_FORWARD)
            ////    eMbModeBase = 1;

            ////if( eMbModeBase == 0 && curr_slice->m_slice_type == BPREDSLICE )
            ////{
            ////    if( curr_slice->m_ref_layer_dq_id == 0 )
            ////    {
            ////        base8x8MbPartIdx     = idx8x8Base;
            ////        base4x4SubMbPartIdx  = idx4x4Base;
            ////    }
            ////} else {
            ////    base8x8MbPartIdx = NxNPartIdx[ eMbModeBase ][ idx8x8Base ];

            ////    if( refLayerMb->mbs[refMbAddr].mbtype == MBTYPE_INTER_8x8 ||
            ////        refLayerMb->mbs[refMbAddr].mbtype == MBTYPE_INTER_8x8_REF0 )
            ////    {
            ////        Ipp32s blkModeBase = 0;

            ////        if (refLayerMb->mbs[refMbAddr].sbtype[idx8x8Base] == SBTYPE_8x8)
            ////            blkModeBase = 1;
            ////        else if (refLayerMb->mbs[refMbAddr].sbtype[idx8x8Base] == SBTYPE_8x4)
            ////            blkModeBase = 2;
            ////        if (refLayerMb->mbs[refMbAddr].sbtype[idx8x8Base] == SBTYPE_4x8)
            ////            blkModeBase = 3;
            ////        if (refLayerMb->mbs[refMbAddr].sbtype[idx8x8Base] == SBTYPE_4x4)
            ////            blkModeBase = 4;

            ////        //if(/* blkModeBase == 0*/refLayerMb->mbs[refMbAddr].sbdir[idx8x8Base] >= D_DIR_DIRECT)
            ////        if(refLayerMb->mbs[refMbAddr].mbtype == MBTYPE_DIRECT)
            ////        {
            ////            if( curr_slice->m_ref_layer_dq_id == 0 )
            ////            {
            ////                base4x4SubMbPartIdx = idx4x4Base;
            ////            }
            ////        }
            ////        else
            ////        {
            ////            base4x4SubMbPartIdx = NxNPartIdx[ blkModeBase ][ idx4x4Base ];
            ////        }
            ////    }
            ////}

            ////Ipp32s base4x4BlkX    = ( ( base8x8MbPartIdx  & 1 ) << 1 ) + ( base4x4SubMbPartIdx  & 1 );
            ////Ipp32s base4x4BlkY    = ( ( base8x8MbPartIdx >> 1 ) << 1 ) + ( base4x4SubMbPartIdx >> 1 );
            ////Ipp32s base4x4BlkIdx  = ( base4x4BlkY << 2 ) + base4x4BlkX;
            ////refLayerPartIdc[y][x]           = 16 * refMbAddr + base4x4BlkIdx; // KL to simplify: for raster order
            refLayerPartIdc[y][x] = 16 * refMbAddr + (refMbPartIdxY&0xC) + (refMbPartIdxX>>2); // KL simplified for raster order
        }
    }

    intraILPredFlag = 1;

    for (y = 0; y < 4; y++)
    {
        for (x = 0; x < 4; x++)
        {
            if (refLayerPartIdc[y][x] != INTRA)
            {
                intraILPredFlag = 0;
                break;
            }
        }
        if (intraILPredFlag == 0)
        {
            break;
        }
    }

    if (intraILPredFlag == 1) // all refs are intra
    {
        if (curEnc->m_spatial_resolution_change)
        {
            gmbs->mbtype = MBTYPE_INTRA;
        }
        else
        {
            gmbs->mbtype = (Ipp8s)refMbType;
        }

        memset((void *) MV[0]->MotionVectors, 0, sizeof(H264MotionVector) * 16);
        memset((void *) MV[1]->MotionVectors, 0, sizeof(H264MotionVector) * 16);

        memset((void *) RefIdxs[0], -1, sizeof(H264MacroblockRefIdxs));
        memset((void *) RefIdxs[1], -1, sizeof(H264MacroblockRefIdxs));

        //memset(lmbs->sbdir, 0, 4); // ? KL anything instead?

        return;
    }

    if ((intraILPredFlag == 0) && (RestrictedSpatialResolutionChangeFlag == 0))
    {
        Ipp32s restored[2][2];

        for (y = 0; y < 4; y += 2)
        {
            for (x = 0; x < 4; x += 2)
            {
                restored[0][0] = 0;
                restored[0][1] = 0;
                restored[1][0] = 0;
                restored[1][1] = 0;

                for (j = 0; j < 2; j++)
                {
                    for (i = 0; i < 2; i++)
                    {
                        if (refLayerPartIdc[y + j][x + i] == INTRA)
                        {
                            restored[j][i] = 1;

                            /* look at the right (left) */
                            if ((refLayerPartIdc[y + j][x + 1 - i] != INTRA) &&
                                (!restored[j][1 - i]))
                            {
                                refLayerPartIdc[y + j][x + i] =
                                    refLayerPartIdc[y + j][x + 1 - i];

                            }
                            /* look at the down (up) */
                            else if ((refLayerPartIdc[y + 1 - j][x + i] != INTRA) &&
                                (!restored[1 - j][i]))
                            {
                                refLayerPartIdc[y + j][x + i] =
                                    refLayerPartIdc[y + 1 - j][x + i];
                            }
                            /* diagonal */
                            else if ((refLayerPartIdc[y + 1 - j][x + 1 - i] != INTRA) &&
                                (!restored[1 - j][1 - i]))
                            {
                                refLayerPartIdc[y + j][x + i] =
                                    refLayerPartIdc[y + 1 - j][x + 1 - i];
                            }
                        }
                    }
                }
            }
        }

        restored[0][0] = 0;
        restored[0][1] = 0;
        restored[1][0] = 0;
        restored[1][1] = 0;

        for (y = 0; y < 2; y++)
        {
            for (x = 0; x < 2; x++)
            {
                if (refLayerPartIdc[2 * y][2 * x] == INTRA)
                {
                    restored[y][x] = 1;

                    if ((refLayerPartIdc[2 * y][2 - x] != INTRA) &&
                        (!restored[y][1 - x]))
                    {
                        for (j = 0; j < 2; j++)
                        {
                            for (i = 0; i < 2; i++)
                            {
                                refLayerPartIdc[2 * y + j][2 * x + i] =
                                    refLayerPartIdc[2 * y + j][2 - x];
                            }
                        }
                    }
                    else if ((refLayerPartIdc[2 - y][2 * x] != INTRA) &&
                        (!restored[1 - y][x]))
                    {
                        for (j = 0; j < 2; j++)
                        {
                            for (i = 0; i < 2; i++)
                            {
                                refLayerPartIdc[2 * y + j][2 * x + i] =
                                    refLayerPartIdc[2 - y][2 * x + i];
                            }
                        }
                    }
                    else if ((refLayerPartIdc[2 - y][2 - x] != INTRA) &&
                        (!restored[1 - y][1 - x]))
                    {
                        for (j = 0; j < 2; j++)
                        {
                            for (i = 0; i < 2; i++)
                            {
                                refLayerPartIdc[2 * y + j][2 * x + i] =
                                    refLayerPartIdc[2 - y][2 - x];
                            }
                        }
                    }
                }
            }
        }
    }

    for (y = 0; y < 4; y++)
    {
        for (x = 0; x < 4; x++)
        {
            Ipp32s aMv[2][2];
            Ipp32s refMbAddr, refMbPartIdx;

            refMbAddr = refLayerPartIdc[y][x] / 16;
            refMbPartIdx = refLayerPartIdc[y][x] % 16;

            Ipp32s refFieldFlag = GetMBFieldDecodingFlag(refLayerMb->mbs[refMbAddr]);

            for (i = 0; i < maxX; i++)
            {
                Ipp32s scaleX;
                Ipp32s scaleY;
                Ipp32s curWidth = pResizeParameter->scaled_ref_layer_width;
                Ipp32s curHeigh = pResizeParameter->scaled_ref_layer_height;
                Ipp32s refWidth = pResizeParameter->ref_layer_width;
                Ipp32s refHeigh = pResizeParameter->ref_layer_height;
                Ipp32s dOX, dOY, dSW, dSH; // KL to be unused (offsets included)
                //Ipp32s refLayerRefIdx = refLayerMb->mbs[refMbAddr].refIdxs[i].refIndexs[subblock_block_membership[refMbPartIdx]];
                Ipp32s refLayerRefIdx = refLayerMb->RefIdxs[i][refMbAddr].RefIdxs[refMbPartIdx]; // no subblock_block_mapping[]
                tempRefIdxPredL[i][y][x]  = ( ( ( refLayerRefIdx ) << ( curFieldFlag/* - m_cMvScale.m_iCurrField */) ) >> ( refFieldFlag/* - m_cMvScale.m_iBaseField*/ ) );

                if (tempRefIdxPredL[i][y][x] < 0) {
                    mvILPredL[i][y][x][0] = 0;
                    mvILPredL[i][y][x][1] = 0;
                    continue;
                }

                aMv[i][0] = refLayerMb->MV[i][refMbAddr].MotionVectors[refMbPartIdx].mvx;
                aMv[i][1] = refLayerMb->MV[i][refMbAddr].MotionVectors[refMbPartIdx].mvy * ( 1 + refFieldFlag );

                ////if ((CroppingChangeFlag == 0) || //// KL no cropping change for a while
                ////    !sd->m_pRefPicList[i] ||
                ////    !sd->m_pRefPicList[i][tempRefIdxPredL[i][y][x]])
                ////{
                    dOX = dOY = dSW = dSH = 0; // KL to be unused (offsets included)
                ////}
                ////else
                ////{
                ////    Ipp32s did = curEnc->m_svc_layer.svc_ext.dependency_id;

                ////    dOX = pResizeParameter->leftOffset - sd->m_pRefPicList[i][tempRefIdxPredL[i][y][x]]->m_offsets[did][0];
                ////    dOY = pResizeParameter->topOffset - sd->m_pRefPicList[i][tempRefIdxPredL[i][y][x]]->m_offsets[did][1];
                ////    dSW = pResizeParameter->rightOffset - sd->m_pRefPicList[i][tempRefIdxPredL[i][y][x]]->m_offsets[did][2] + dOX;
                ////    dSH = pResizeParameter->bottomOffset - sd->m_pRefPicList[i][tempRefIdxPredL[i][y][x]]->m_offsets[did][3] + dOY;
                ////}

                scaleX = (((curWidth + dSW) << 16) + (refWidth >> 1)) / refWidth;
                scaleY = (((curHeigh + dSH) << 16) + (refHeigh >> 1)) / refHeigh;

                aMv[i][0] = (aMv[i][0] * scaleX + 32768) >> 16;
                aMv[i][1] = (aMv[i][1] * scaleY + 32768) >> 16;

                if (CroppingChangeFlag == 1)
                {
                    Ipp32s xFrm = (curMB_X * 16 + (4 * x + 1));
                    Ipp32s yFrm = (curMB_Y * 16 + (4 * y + 1));

                    scaleX = (((4 * dSW) << 16) + (curWidth >> 1)) / curWidth;
                    scaleY = (((4 * dSH) << 16) + (curHeigh >> 1)) / curHeigh;

                    aMv[i][0] += (((xFrm - pResizeParameter->leftOffset) * scaleX + 32768 ) >> 16 ) - 4 * dOX;
                    aMv[i][1] += (((yFrm - pResizeParameter->topOffset) * scaleY + 32768 ) >> 16 ) - 4 * dOY;

                }

                mvILPredL[i][y][x][0] = aMv[i][0];
                mvILPredL[i][y][x][1] = (aMv[i][1] ) / (1 + curFieldFlag);
            }
        }
    }

    refIdxILPredL[0][0][0] = tempRefIdxPredL[0][0][0];
    refIdxILPredL[0][0][1] = tempRefIdxPredL[0][0][2];
    refIdxILPredL[0][1][0] = tempRefIdxPredL[0][2][0];
    refIdxILPredL[0][1][1] = tempRefIdxPredL[0][2][2];

    refIdxILPredL[1][0][0] = tempRefIdxPredL[1][0][0];
    refIdxILPredL[1][0][1] = tempRefIdxPredL[1][0][2];
    refIdxILPredL[1][1][0] = tempRefIdxPredL[1][2][0];
    refIdxILPredL[1][1][1] = tempRefIdxPredL[1][2][2];

    if (RestrictedSpatialResolutionChangeFlag == 0)
    {
        Ipp32s xP, yP, xS, yS;

        for (yP = 0; yP < 2; yP++)
        {
            for (xP = 0; xP < 2; xP++)
            {
                for (yS = 0; yS < 2; yS++)
                {
                    for (xS = 0; xS < 2; xS++)
                    {
                        for (i = 0; i < 2; i++)
                        {
                            refIdxILPredL[i][yP][xP] =
                                minPositive(refIdxILPredL[i][yP][xP],
                                tempRefIdxPredL[i][2 * yP + yS][2 * xP + xS]);
                        }
                    }
                }

                for (yS = 0; yS < 2; yS++)
                {
                    for (xS = 0; xS < 2; xS++)
                    {
                        for (i = 0; i < maxX; i++)
                        {
                            if (tempRefIdxPredL[i][2 * yP + yS][2 * xP + xS] !=
                                refIdxILPredL[i][yP][xP])
                            {
                                if (tempRefIdxPredL[i][2 * yP + yS][2 * xP + 1 - xS] ==
                                    refIdxILPredL[i][yP][xP])
                                {
                                    mvILPredL[i][2 * yP + yS][2 * xP + xS][0] =
                                        mvILPredL[i][2 * yP + yS][2 * xP + 1 - xS][0];

                                    mvILPredL[i][2 * yP + yS][2 * xP + xS][1] =
                                        mvILPredL[i][2 * yP + yS][2 * xP + 1 - xS][1];
                                }
                                else if (tempRefIdxPredL[i][2 * yP + 1 - yS][2 * xP + xS] ==
                                    refIdxILPredL[i][yP][xP])
                                {
                                    mvILPredL[i][2 * yP + yS][2 * xP + xS][0] =
                                        mvILPredL[i][2 * yP + 1 - yS][2 * xP + xS][0];

                                    mvILPredL[i][2 * yP + yS][2 * xP + xS][1] =
                                        mvILPredL[i][2 * yP + 1 - yS][2 * xP + xS][1];
                                }
                                else if (tempRefIdxPredL[i][2 * yP + 1 - yS][2 * xP + 1 - xS] ==
                                    refIdxILPredL[i][yP][xP])
                                {
                                    mvILPredL[i][2 * yP + yS][2 * xP + xS][0] =
                                        mvILPredL[i][2 * yP + 1 - yS][2 * xP + 1 - xS][0];

                                    mvILPredL[i][2 * yP + yS][2 * xP + xS][1] =
                                        mvILPredL[i][2 * yP + 1 - yS][2 * xP + 1 - xS][1];
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (RestrictedSpatialResolutionChangeFlag == 0)
    {
        Ipp32s yO, xO;

        for (yO = 0; yO < 4; yO += 2)
        {
            for (xO = 0; xO < 4; xO += 2)
            {
                if (curEnc->m_SeqParamSet->direct_8x8_inference_flag && slice_type == BPREDSLICE) {
                    for (i = 0; i < maxX; i++)
                    {
                        Ipp32s aMv[2];
                        Ipp32s iXC = ( xO >> 1 ) * 3;
                        Ipp32s iYC = ( yO >> 1 ) * 3;
                        aMv[0] = mvILPredL[i][iYC][iXC][0];
                        aMv[1] = mvILPredL[i][iYC][iXC][1];
                        mvILPredL[i][yO  ][xO  ][0]  = aMv[0];
                        mvILPredL[i][yO  ][xO  ][1]  = aMv[1];
                        mvILPredL[i][yO+1][xO  ][0]  = aMv[0];
                        mvILPredL[i][yO+1][xO  ][1]  = aMv[1];
                        mvILPredL[i][yO  ][xO+1][0]  = aMv[0];
                        mvILPredL[i][yO  ][xO+1][1]  = aMv[1];
                        mvILPredL[i][yO+1][xO+1][0]  = aMv[0];
                        mvILPredL[i][yO+1][xO+1][1]  = aMv[1];
                    }
                }

                sbtype = SBTYPE_8x8;
                for (i = 0; i < maxX; i++)
                {
                    if (((mvDiff(mvILPredL[i][yO][xO], mvILPredL[i][yO][xO + 1])) > 1) ||
                        ((mvDiff(mvILPredL[i][yO][xO], mvILPredL[i][yO + 1][xO])) > 1) ||
                        ((mvDiff(mvILPredL[i][yO][xO], mvILPredL[i][yO + 1][xO + 1])) > 1))
                    {
                        sbtype = SBTYPE_UNDEF;
                        break;
                    }
                }

                if (sbtype == SBTYPE_UNDEF)
                {
                    for (i = 0; i < maxX; i++)
                    {
                        sbtype = SBTYPE_8x4;

                        if (((mvDiff(mvILPredL[i][yO][xO], mvILPredL[i][yO][xO + 1])) > 1) ||
                            ((mvDiff(mvILPredL[i][yO + 1][xO], mvILPredL[i][yO + 1][xO + 1])) > 1))
                        {
                            sbtype = SBTYPE_UNDEF;
                            break;
                        }
                    }
                }

                if (sbtype == SBTYPE_UNDEF)
                {
                    for (i = 0; i < maxX; i++)
                    {
                        sbtype = SBTYPE_4x8;

                        if (((mvDiff(mvILPredL[i][yO][xO], mvILPredL[i][yO + 1][xO])) > 1) ||
                            ((mvDiff(mvILPredL[i][yO][xO + 1], mvILPredL[i][yO + 1][xO + 1])) > 1))
                        {
                            sbtype = SBTYPE_4x4;
                            break;
                        }
                    }
                }

                if (sbtype == SBTYPE_8x8)
                {
                    for (i = 0; i < maxX; i++)
                    {
                        tempMvAL[i][0] = (mvILPredL[i][yO][xO][0] +
                            mvILPredL[i][yO][xO + 1][0] +
                            mvILPredL[i][yO + 1][xO][0] +
                            mvILPredL[i][yO + 1][xO + 1][0] + 2) >> 2;

                        tempMvAL[i][1] = (mvILPredL[i][yO][xO][1] +
                            mvILPredL[i][yO][xO + 1][1] +
                            mvILPredL[i][yO + 1][xO][1] +
                            mvILPredL[i][yO + 1][xO + 1][1] + 2) >> 2;

                        mvILPredL[i][yO][xO][0] = tempMvAL[i][0];
                        mvILPredL[i][yO][xO + 1][0] = tempMvAL[i][0];
                        mvILPredL[i][yO + 1][xO][0] = tempMvAL[i][0];
                        mvILPredL[i][yO + 1][xO + 1][0] = tempMvAL[i][0];

                        mvILPredL[i][yO][xO][1] = tempMvAL[i][1];
                        mvILPredL[i][yO][xO + 1][1] = tempMvAL[i][1];
                        mvILPredL[i][yO + 1][xO][1] = tempMvAL[i][1];
                        mvILPredL[i][yO + 1][xO + 1][1] = tempMvAL[i][1];

                    }
                }
                else if (sbtype == SBTYPE_8x4)
                {
                    for (i = 0; i < maxX; i++)
                    {
                        tempMvAL[i][0] = (mvILPredL[i][yO][xO][0] +
                            mvILPredL[i][yO][xO + 1][0] + 1) >> 1;

                        tempMvAL[i][1] = (mvILPredL[i][yO][xO][1] +
                            mvILPredL[i][yO][xO + 1][1] + 1) >> 1;

                        tempMvBL[i][0] = (mvILPredL[i][yO + 1][xO][0] +
                            mvILPredL[i][yO + 1][xO + 1][0] + 1) >> 1;

                        tempMvBL[i][1] = (mvILPredL[i][yO + 1][xO][1] +
                            mvILPredL[i][yO + 1][xO + 1][1] + 1) >> 1;

                        mvILPredL[i][yO][xO][0] = tempMvAL[i][0];
                        mvILPredL[i][yO][xO + 1][0] = tempMvAL[i][0];

                        mvILPredL[i][yO][xO][1] = tempMvAL[i][1];
                        mvILPredL[i][yO][xO + 1][1] = tempMvAL[i][1];

                        mvILPredL[i][yO + 1][xO][0] = tempMvBL[i][0];
                        mvILPredL[i][yO + 1][xO + 1][0] = tempMvBL[i][0];

                        mvILPredL[i][yO + 1][xO][1] = tempMvBL[i][1];
                        mvILPredL[i][yO + 1][xO + 1][1] = tempMvBL[i][1];
                    }
                }
                else if (sbtype == SBTYPE_4x8)
                {
                    for (i = 0; i < maxX; i++)
                    {
                        tempMvAL[i][0] = (mvILPredL[i][yO][xO][0] +
                            mvILPredL[i][yO + 1][xO][0] + 1) >> 1;

                        tempMvAL[i][1] = (mvILPredL[i][yO][xO][1] +
                            mvILPredL[i][yO + 1][xO][1] + 1) >> 1;

                        tempMvBL[i][0] = (mvILPredL[i][yO][xO + 1][0] +
                            mvILPredL[i][yO + 1][xO + 1][0] + 1) >> 1;

                        tempMvBL[i][1] = (mvILPredL[i][yO][xO + 1][1] +
                            mvILPredL[i][yO + 1][xO + 1][1] + 1) >> 1;

                        mvILPredL[i][yO][xO][0]     = tempMvAL[i][0];
                        mvILPredL[i][yO + 1][xO][0] = tempMvAL[i][0];

                        mvILPredL[i][yO][xO][1]     = tempMvAL[i][1];
                        mvILPredL[i][yO + 1][xO][1] = tempMvAL[i][1];

                        mvILPredL[i][yO][xO + 1][0]     = tempMvBL[i][0];
                        mvILPredL[i][yO + 1][xO + 1][0] = tempMvBL[i][0];

                        mvILPredL[i][yO][xO + 1][1]     = tempMvBL[i][1];
                        mvILPredL[i][yO + 1][xO + 1][1] = tempMvBL[i][1];
                    }
                }
            }
        }
    }

    if ((slice_type == INTRASLICE) || (slice_type == S_INTRASLICE))
        return;

    for (i = 0; i < 4; i++)
    {
        sbdir[i] = (Direction_t)0;
        gmbs->i_pred[i] = 0;
    }

    mbtype = MBTYPE_FORWARD;

    for (i = 0; i < maxX; i++)
    {
        Ipp32s tmp0, tmp1;

        if ((refIdxILPredL[i][0][0] != refIdxILPredL[i][0][1]) ||
            (refIdxILPredL[i][0][0] != refIdxILPredL[i][1][0]) ||
            (refIdxILPredL[i][0][0] != refIdxILPredL[i][1][1]))
        {
            mbtype = MBTYPE_UNDEF;
            break;
        }

        tmp0 = mvILPredL[i][0][0][0];
        tmp1 = mvILPredL[i][0][0][1];

        for (y = 0; y < 4; y++)
        {
            for (x = 0; x < 4; x++)
            {
                if ((mvILPredL[i][y][x][0] != tmp0) ||
                    (mvILPredL[i][y][x][1] != tmp1))
                {
                    mbtype = MBTYPE_UNDEF;
                    break;
                }
            }
            if (mbtype == MBTYPE_UNDEF)
                break;
        }

        if (mbtype == MBTYPE_UNDEF)
            break;
    }

    if (mbtype == MBTYPE_UNDEF)
    {
        mbtype = MBTYPE_INTER_16x8;
        for (i = 0; i < maxX; i++)
        {
            Ipp32s tmp0, tmp1;

            if ((refIdxILPredL[i][0][0] != refIdxILPredL[i][0][1]) ||
                (refIdxILPredL[i][1][0] != refIdxILPredL[i][1][1]))
            {
                mbtype = MBTYPE_UNDEF;
                break;
            }

            tmp0 = mvILPredL[i][0][0][0];
            tmp1 = mvILPredL[i][0][0][1];

            for (x = 0; x < 4; x++)
            {
                if ((mvILPredL[i][0][x][0] != tmp0) ||
                    (mvILPredL[i][1][x][0] != tmp0) ||
                    (mvILPredL[i][0][x][1] != tmp1) ||
                    (mvILPredL[i][1][x][1] != tmp1))
                {
                    mbtype = MBTYPE_UNDEF;
                    break;
                }
            }

            if (mbtype == MBTYPE_UNDEF)
                break;

            tmp0 = mvILPredL[i][2][0][0];
            tmp1 = mvILPredL[i][2][0][1];

            for (x = 0; x < 4; x++)
            {
                if ((mvILPredL[i][2][x][0] != tmp0) ||
                    (mvILPredL[i][3][x][0] != tmp0) ||
                    (mvILPredL[i][2][x][1] != tmp1) ||
                    (mvILPredL[i][3][x][1] != tmp1))
                {
                    mbtype = MBTYPE_UNDEF;
                    break;
                }
            }

            if (mbtype == MBTYPE_UNDEF)
                break;
        }
    }

    if (mbtype == MBTYPE_UNDEF)
    {
        mbtype = MBTYPE_INTER_8x16;
        for (i = 0; i < maxX; i++)
        {
            Ipp32s tmp0, tmp1;

            if ((refIdxILPredL[i][0][0] != refIdxILPredL[i][1][0]) ||
                (refIdxILPredL[i][0][1] != refIdxILPredL[i][1][1]))
            {
                mbtype = MBTYPE_UNDEF;
                break;
            }

            tmp0 = mvILPredL[i][0][0][0];
            tmp1 = mvILPredL[i][0][0][1];

            for (y = 0; y < 4; y++)
            {
                if ((mvILPredL[i][y][0][0] != tmp0) ||
                    (mvILPredL[i][y][1][0] != tmp0) ||
                    (mvILPredL[i][y][0][1] != tmp1) ||
                    (mvILPredL[i][y][1][1] != tmp1))
                {
                    mbtype = MBTYPE_UNDEF;
                    break;
                }
            }

            if (mbtype == MBTYPE_UNDEF)
                break;

            tmp0 = mvILPredL[i][0][2][0];
            tmp1 = mvILPredL[i][0][2][1];

            for (y = 0; y < 4; y++)
            {
                if ((mvILPredL[i][y][2][0] != tmp0) ||
                    (mvILPredL[i][y][3][0] != tmp0) ||
                    (mvILPredL[i][y][2][1] != tmp1) ||
                    (mvILPredL[i][y][3][1] != tmp1))
                {
                    mbtype = MBTYPE_UNDEF;
                    break;
                }
            }

            if (mbtype == MBTYPE_UNDEF)
                break;
        }
    }

    if (mbtype == MBTYPE_UNDEF)
        mbtype = MBTYPE_INTER_8x8;

    if (slice_type == BPREDSLICE)
    {
        Ipp32s ind0, ind1;
        if (mbtype != MBTYPE_INTER_8x8)
        {
            ind0 = (refIdxILPredL[1][0][0] >= 0 ? 2 : 0) +
                (refIdxILPredL[0][0][0] >= 0 ? 1 : 0);

            if (mbtype != MBTYPE_FORWARD)
            {
                ind1 = (refIdxILPredL[1][1][1] >= 0 ? 2 : 0) +
                    (refIdxILPredL[0][1][1] >= 0 ? 1 : 0);

                if (ind0 == 1)
                {
                    sbdir[0] = D_DIR_FWD;
                }
                else if (ind0 == 2)
                {
                    sbdir[0] = D_DIR_BWD;
                }
                else if (ind0 == 3)
                {
                    sbdir[0] = D_DIR_BIDIR;
                }

                if (ind1 == 1)
                {
                    sbdir[1] = D_DIR_FWD;
                    if (ind0 == 1) mbtype = (mbtype==MBTYPE_INTER_16x8)?MBTYPE_FWD_FWD_16x8  :MBTYPE_FWD_FWD_8x16; else
                    if (ind0 == 2) mbtype = (mbtype==MBTYPE_INTER_16x8)?MBTYPE_BWD_FWD_16x8  :MBTYPE_BWD_FWD_8x16; else
                    if (ind0 == 3) mbtype = (mbtype==MBTYPE_INTER_16x8)?MBTYPE_BIDIR_FWD_16x8:MBTYPE_BIDIR_FWD_8x16;
                }
                else if (ind1 == 2)
                {
                    sbdir[1] = D_DIR_BWD;
                    if (ind0 == 1) mbtype = (mbtype==MBTYPE_INTER_16x8)?MBTYPE_FWD_BWD_16x8  :MBTYPE_FWD_BWD_8x16; else
                    if (ind0 == 2) mbtype = (mbtype==MBTYPE_INTER_16x8)?MBTYPE_BWD_BWD_16x8  :MBTYPE_BWD_BWD_8x16; else
                    if (ind0 == 3) mbtype = (mbtype==MBTYPE_INTER_16x8)?MBTYPE_BIDIR_BWD_16x8:MBTYPE_BIDIR_BWD_8x16;
                }
                else if (ind1 == 3)
                {
                    sbdir[1] = D_DIR_BIDIR;
                    if (ind0 == 1) mbtype = (mbtype==MBTYPE_INTER_16x8)?MBTYPE_FWD_BIDIR_16x8  :MBTYPE_FWD_BIDIR_8x16; else
                    if (ind0 == 2) mbtype = (mbtype==MBTYPE_INTER_16x8)?MBTYPE_BWD_BIDIR_16x8  :MBTYPE_BWD_BIDIR_8x16; else
                    if (ind0 == 3) mbtype = (mbtype==MBTYPE_INTER_16x8)?MBTYPE_BIDIR_BIDIR_16x8:MBTYPE_BIDIR_BIDIR_8x16;
                }
            }
            else
            {
                if (ind0 == 2)
                {
                    mbtype = MBTYPE_BACKWARD;

                }
                else if (ind0 == 3)
                {
                    mbtype = MBTYPE_BIDIR;
                }
            }
        }
        else
            mbtype = MBTYPE_B_8x8;
    }

    gmbs->mbtype = (Ipp8s)mbtype;

    if (mbtype == MBTYPE_INTER_8x8 || mbtype == MBTYPE_B_8x8)
    {
        for (subblock = 0; subblock < 4; subblock++)
        {
            Ipp32s yO, xO;
            xO = 2 * (subblock % 2);
            yO = 2 * (subblock / 2);

            sbtype = SBTYPE_8x8;

            for (i = 0; i < maxX; i++)
            {
                Ipp32s tmp0 = mvILPredL[i][yO][xO][0];
                Ipp32s tmp1 = mvILPredL[i][yO][xO][1];

                if ((mvILPredL[i][yO][xO + 1][0] != tmp0) ||
                    (mvILPredL[i][yO + 1][xO][0] != tmp0) ||
                    (mvILPredL[i][yO + 1][xO + 1][0] != tmp0) ||
                    (mvILPredL[i][yO][xO + 1][1] != tmp1) ||
                    (mvILPredL[i][yO + 1][xO][1] != tmp1) ||
                    (mvILPredL[i][yO + 1][xO + 1][1] != tmp1))
                {
                    sbtype = SBTYPE_UNDEF;
                    break;
                }
            }

            if (sbtype == SBTYPE_UNDEF)
            {
                sbtype = SBTYPE_8x4;
                for (i = 0; i < maxX; i++)
                {
                    if ((mvILPredL[i][yO][xO][0] != mvILPredL[i][yO][xO + 1][0]) ||
                        (mvILPredL[i][yO + 1][xO][0] != mvILPredL[i][yO + 1][xO + 1][0]) ||
                        (mvILPredL[i][yO][xO][1] != mvILPredL[i][yO][xO + 1][1]) ||
                        (mvILPredL[i][yO + 1][xO][1] != mvILPredL[i][yO + 1][xO + 1][1]))
                    {
                        sbtype = SBTYPE_UNDEF;
                        break;
                    }
                }
            }

            if (sbtype == SBTYPE_UNDEF)
            {
                sbtype = SBTYPE_4x8;
                for (i = 0; i < maxX; i++)
                {
                    if ((mvILPredL[i][yO][xO][0] != mvILPredL[i][yO + 1][xO][0]) ||
                        (mvILPredL[i][yO][xO + 1][0] != mvILPredL[i][yO + 1][xO + 1][0]) ||
                        (mvILPredL[i][yO][xO][1] != mvILPredL[i][yO + 1][xO][1]) ||
                        (mvILPredL[i][yO][xO + 1][1] != mvILPredL[i][yO + 1][xO + 1][1]))
                    {
                        sbtype = SBTYPE_UNDEF;
                        break;
                    }
                }
            }

            if (sbtype == SBTYPE_UNDEF)
                sbtype = SBTYPE_4x4;

            if (slice_type == BPREDSLICE) {
                Ipp32s ind;
                ind = (refIdxILPredL[1][yO / 2][xO / 2] >= 0 ? 2 : 0) +
                    (refIdxILPredL[0][yO / 2][xO / 2] >= 0 ? 1 : 0);


                if (ind == 1)
                {
                    if (sbtype == SBTYPE_8x8) sbtype = SBTYPE_FORWARD_8x8; else
                    if (sbtype == SBTYPE_8x4) sbtype = SBTYPE_FORWARD_8x4; else
                    if (sbtype == SBTYPE_4x8) sbtype = SBTYPE_FORWARD_4x8; else
                    if (sbtype == SBTYPE_4x4) sbtype = SBTYPE_FORWARD_4x4;
                }
                else if (ind == 2)
                {
                    if (sbtype == SBTYPE_8x8) sbtype = SBTYPE_BACKWARD_8x8; else
                    if (sbtype == SBTYPE_8x4) sbtype = SBTYPE_BACKWARD_8x4; else
                    if (sbtype == SBTYPE_4x8) sbtype = SBTYPE_BACKWARD_4x8; else
                    if (sbtype == SBTYPE_4x4) sbtype = SBTYPE_BACKWARD_4x4;
                }
                else if (ind == 3)
                {
                    if (sbtype == SBTYPE_8x8) sbtype = SBTYPE_BIDIR_8x8; else
                    if (sbtype == SBTYPE_8x4) sbtype = SBTYPE_BIDIR_8x4; else
                    if (sbtype == SBTYPE_4x8) sbtype = SBTYPE_BIDIR_4x8; else
                    if (sbtype == SBTYPE_4x4) sbtype = SBTYPE_BIDIR_4x4;
                }
            }
            gmbs->sbtype[subblock] = (Ipp8u)sbtype;
        }
    }

    if (!isRefMbaff && !curEnc->m_SliceHeader.MbaffFrameFlag //&& pGetMBBaseModeFlag(gmbs)
        && !RestrictedSpatialResolutionChangeFlag
        && curEnc->m_spatial_resolution_change) {
            //RestoreIntraPrediction conditions
            for (i=0; i<2; i++) {
                Ipp32s refX = curEnc->svc_ResizeMap[0][curMB_X].refMB[i];
                if (refX < 0) continue;
                for (j=0; j<2; j++) {
                    Ipp32s ref, refY = curEnc->svc_ResizeMap[1][curMB_Y].refMB[j];
                    if (refY < 0) continue;
                    ref = refX + refY * mbStride;
                    if(IS_INTRA_MBTYPE(refLayerMb->mbs[ref].mbtype))
                        gmbs->i_pred[2*j + i] = 1;
                }
            }
    }

    for (i = 0; i < maxX; i++)
    {
        for (y = 0; y < 4; y++)
        {
            for (x = 0; x < 4; x++)
            {
                MV[i]->MotionVectors[y * 4 + x].mvx = (Ipp16s)mvILPredL[i][y][x][0];
                MV[i]->MotionVectors[y * 4 + x].mvy = (Ipp16s)mvILPredL[i][y][x][1];
                RefIdxs[i]->RefIdxs [y * 4 + x]   = (T_RefIdx)refIdxILPredL[i][y>>1][x>>1]; // KL assign all 16 sb
            }
        }

        //for (y = 0; y < 2; y++)
        //{
        //    for (x = 0; x < 2; x++)
        //    {
        //        RefIdxs[i]->RefIdxs[2*y+x]   = (T_RefIdx)refIdxILPredL[i][y][x]; // KL either assign all 16 sb?
        //    }
        //}
    }
}

//////////virtual void StoreLayersMBInfo(H264CoreEncoder_8u16s *curEnc,
//////////                               H264SegmentDecoderMultiThreaded * sd)
//////////{
//////////    //H264Slice *pSlice = sd->m_pSlice;
//////////    //const H264PicParamSet *pPicParamSet;
//////////    //H264DecoderFrame *pFrame = pSlice->GetCurrentFrame();
//////////    Ipp32s nv12_flag = 1; //pFrame->GetColorFormat() == NV12;
//////////    //        Ipp32s pixel_sz = (pFrame->m_bpp > 8) + 1;
//////////    H264EncoderFrame_8u16s *pFrame = curEnc->m_pCurrentFrame;
//////////
//////////    //pPicParamSet = pSlice->GetPicParam();
//////////    //pFrame->m_chroma_qp_index_offset_base[0] = pPicParamSet->chroma_qp_index_offset[0];
//////////    //pFrame->m_chroma_qp_index_offset_base[1] = pPicParamSet->chroma_qp_index_offset[1];
//////////
//////////    //if (pSlice->m_bIsLastSliceInDepLayer &&
//////////    //    (pFrame->m_currentLayer < pFrame->m_numberOfLayers - 1))
//////////    {
//////////        Ipp32s nMBCount = pFrame->numMBs/*m_MBLayerSize[pFrame->m_currentLayer]*/;
//////////        //H264DecoderLayerDescriptor *layerMb = &sd->m_mbinfo.layerMbs[pFrame->m_currentLayer];
//////////        Ipp32s pitch_luma = pFrame->m_pitchBytes;
//////////        Ipp32s pitch_chroma = pFrame->m_pitchBytes;
//////////
//////////        IppiSize lumaSize, chromaSize;
//////////        Ipp32s i;
//////////
//////////        MFX_INTERNAL_CPY(layerMb->MV[0], pFrame->m_mbinfo.MV[0], sizeof(H264MacroblockMVs) * nMBCount);
//////////        MFX_INTERNAL_CPY(layerMb->MV[1], pFrame->m_mbinfo.MV[1], sizeof(H264MacroblockMVs) * nMBCount);
//////////        //            MFX_INTERNAL_CPY(layerMb->RefIdxs[0], pFrame->m_mbinfo.RefIdxs[0], sizeof(H264DecoderMacroblockRefIdxs) * nMBCount);
//////////        //            MFX_INTERNAL_CPY(layerMb->RefIdxs[1], pFrame->m_mbinfo.RefIdxs[1], sizeof(H264DecoderMacroblockRefIdxs) * nMBCount);
//////////
//////////        for (i = 0; i < nMBCount; i++)
//////////        {
//////////            pFrame->m_mbinfo.RefIdxs;
//////////            pFrame->mbsData.MotionVectors
//////////
//////////
//////////            layerMb->mbs[i].mbtype = pFrame->m_mbinfo.mbs[i].mbtype;
//////////            layerMb->mbs[i].sbtype[0] = pFrame->m_mbinfo.mbs[i].sbtype[0];
//////////            layerMb->mbs[i].sbtype[1] = pFrame->m_mbinfo.mbs[i].sbtype[1];
//////////            layerMb->mbs[i].sbtype[2] = pFrame->m_mbinfo.mbs[i].sbtype[2];
//////////            layerMb->mbs[i].sbtype[3] = pFrame->m_mbinfo.mbs[i].sbtype[3];
//////////            layerMb->mbs[i].mbflags = pFrame->m_mbinfo.mbs[i].mbflags;
//////////            layerMb->mbs[i].sbdir[0] = sd->m_mbinfo.mbs[i].sbdir[0];
//////////            layerMb->mbs[i].sbdir[1] = sd->m_mbinfo.mbs[i].sbdir[1];
//////////            layerMb->mbs[i].sbdir[2] = sd->m_mbinfo.mbs[i].sbdir[2];
//////////            layerMb->mbs[i].sbdir[3] = sd->m_mbinfo.mbs[i].sbdir[3];
//////////            layerMb->mbs[i].refIdxs[0] = pFrame->m_mbinfo.mbs[i].refIdxs[0];
//////////            layerMb->mbs[i].refIdxs[1] = pFrame->m_mbinfo.mbs[i].refIdxs[1];
//////////        }
//////////
//////////        lumaSize.width = pFrame->m_LayerLumaSize[pFrame->m_currentLayer].width;
//////////        lumaSize.height = pFrame->m_LayerLumaSize[pFrame->m_currentLayer].height;
//////////
//////////        chromaSize.width = lumaSize.width >> ((Ipp32s) (3 > pFrame->m_chroma_format));
//////////        chromaSize.height = lumaSize.height >> ((Ipp32s) (2 > pFrame->m_chroma_format));
//////////
//////////        CopyPlane((PlanePtrY)pFrame->m_pYPlane, pitch_luma,
//////////            (PlanePtrY)layerMb->m_pYPlane, lumaSize.width, lumaSize);
//////////
//////////        if (nv12_flag) {
//////////            CopyPlane_NV12((PlanePtrUV)pFrame->m_pUVPlane, pitch_chroma,
//////////                (PlanePtrUV)layerMb->m_pUPlane, (PlanePtrUV)layerMb->m_pVPlane,
//////////                chromaSize.width, chromaSize);
//////////        } else {
//////////            CopyPlane((PlanePtrUV)pFrame->m_pUPlane, pitch_chroma,
//////////                (PlanePtrUV)layerMb->m_pUPlane, chromaSize.width, chromaSize);
//////////
//////////            CopyPlane((PlanePtrUV)pFrame->m_pVPlane, pitch_chroma,
//////////                (PlanePtrUV)layerMb->m_pVPlane, chromaSize.width, chromaSize);
//////////        }
//////////
//////////        //interpolateInterMBs((PlaneY*)layerMb->m_pYPlane, lumaSize.width * pixel_sz,
//////////        //                    (PlaneUV*)layerMb->m_pUPlane, chromaSize.width * pixel_sz,
//////////        //                    (PlaneUV*)layerMb->m_pVPlane, chromaSize.width * pixel_sz,
//////////        //                    lumaSize.width >> 4, lumaSize.height >> 4,
//////////        //                    layerMb->mbs);
//////////    }
//////////}

} //namespace UMC_H264_ENCODER

#undef H264EncoderFrameType
#undef H264CurrentMacroblockDescriptorType
#undef H264SliceType
#undef H264CoreEncoderType
#undef H264BsBaseType
#undef H264BsRealType
#undef H264BsFakeType
#undef H264ENC_MAKE_NAME
#undef COEFFSTYPE
#undef PIXTYPE

#endif //UMC_ENABLE_H264_VIDEO_ENCODER
