/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2008-2013 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#ifndef __UMC_H264_RESAMPLING_TEMPLATES_H
#define __UMC_H264_RESAMPLING_TEMPLATES_H

#include <ippi.h>
#include "umc_h264_dec.h"
#include "umc_h264_slice_decoding.h"
#include <math.h>

namespace UMC
{

#define IS_REF_INTRA(refMbAddr) ((mbs[refMbAddr].mbtype) <= MBTYPE_PCM)
#define IS_REF_INTRA_INSLICE(refMbAddr, sliceId) ((mbs[refMbAddr].mbtype) <= MBTYPE_PCM && (((sliceId) < 0 || gmbs[refMbAddr].slice_id == (sliceId))))

template<typename Plane>
void saturateAndBorder(Ipp32s width,
                          Ipp32s height,
                          Plane *dst,
                          Ipp32s dstStride,
                          Ipp32s *src,
                          Ipp32s srcStride,
                          Ipp32s leftOffset,
                          Ipp32s rightOffset,
                          Ipp32s topOffset,
                          Ipp32s bottomOffset,
                          Ipp32s verticalInterpolationFlag,
                          Ipp32s yBorder,
                          Ipp32s nv12_flag)
{
    if (nv12_flag) nv12_flag = 1;

    for (Ipp32s j = 0; j < topOffset; j++)
    {
        Plane*  outPtr = dst + j * dstStride;

        for (Ipp32s i = 0; i < width + leftOffset + rightOffset; i++)
        {
            outPtr[i << nv12_flag] = 128;
        }
    }

    for (Ipp32s j = IPP_MAX(0, topOffset); j < height + topOffset; j++)
    {
        Plane*  outPtr = dst + j * dstStride;

        for (Ipp32s i = 0; i < leftOffset; i++)
        {
            outPtr[i << nv12_flag] = 128;
        }

        if (src)
        {
            if (verticalInterpolationFlag)
            {
                Ipp32s* intPtr = (Ipp32s*)((Ipp8u*)src + (yBorder + ((j - topOffset) >> 1)) * srcStride);
                for (Ipp32s i = leftOffset; i < width + leftOffset; i++)
                {
                    if (((j - topOffset) % 2 ) == 0)
                    {
                        outPtr[i] = (Ipp8u)intPtr[i - leftOffset]; // already saturated
                    }
                    else
                    {
                        int k = i - leftOffset;
                        Ipp32s acc;

                        acc   = 16;
                        acc  += 19 * (intPtr[ k                  ] + intPtr[k +     1 * srcStride/4]);
                        acc  -=  3 * (intPtr[ k - 1 * srcStride/4] + intPtr[k + 2 * srcStride/4]);
                        acc >>=  5;

                        if (acc > 255) acc = 255;
                        else if (acc < 0) acc = 0;

                        outPtr[i] = (Ipp8u)acc;
                    }
                }
            } else {
                Ipp32s* intPtr = (Ipp32s*)((Ipp8u*)src + (j - topOffset) * srcStride);
                for (Ipp32s i = IPP_MAX(0, leftOffset); i < width + leftOffset; i++)
                {
                    Ipp32s tmp = intPtr[i - leftOffset];

                    if      (tmp < 0)   outPtr[i] = 0;
                    else if (tmp > 255) outPtr[i] = 255;
                    else                outPtr[i] = (Ipp8u)tmp;
                }
            }
        }

        for (Ipp32s i = 0; i < rightOffset; i++)
        {
            outPtr[(width + leftOffset + i) << nv12_flag] = 128;
        }
    }

    for (Ipp32s j = IPP_MAX(0, height + topOffset); j < height + topOffset + bottomOffset; j++)
    {
        Plane*  outPtr = dst + j * dstStride;

        for (Ipp32s i = 0; i < width + leftOffset + rightOffset; i++)
        {
            outPtr[i << nv12_flag] = 128;
        }
    }
}

template<typename Plane>
void saturateAndBorder1(Ipp32s width,
                          Ipp32s height,
                          Plane *dst,
                          Ipp32s dstStride,
                          Ipp32s *src,
                          Ipp32s srcStride,
                          Ipp32s leftOffset,
                          Ipp32s rightOffset,
                          Ipp32s topOffset,
                          Ipp32s bottomOffset,
                          Ipp32s verticalInterpolationFlag,
                          Ipp32s yBorder,
                          Ipp32s bottom_field_flag,
                          bool   isChroma,
                          Ipp32s nv12)
{
    if (!src)
        return;

    for (Ipp32s j = 0; j < height; j++)
    {
        Plane*  outPtr = dst + j * dstStride;

        if (src)
        {
            if (verticalInterpolationFlag)
            {
                Ipp32s* intPtr = (Ipp32s*)((Ipp8u*)src + (yBorder + (j >> 1)) * srcStride);
                if ((j % 2 ) != bottom_field_flag)
                    intPtr = (Ipp32s*)((Ipp8u*)src + (yBorder + (j >> 1) - bottom_field_flag) * srcStride);

                for (Ipp32s i = 0; i < width; i++)
                {
                    if (i < leftOffset || i >= width - rightOffset ||
                        j < topOffset || j >= height - bottomOffset)
                    {
                        outPtr[i << nv12] = 128; // set to mid gray
                        continue;
                    }

                    if ((j % 2 ) == bottom_field_flag)
                    {
                        outPtr[i << nv12] = (Ipp8u)intPtr[i]; // already saturated
                    }
                    else
                    {
                        if (isChroma)
                        {
                            int k = i;
                            Ipp32s acc;

                            acc  = (intPtr[k] + intPtr[k + 1 * srcStride/4] + 1) >> 1;

                            if (acc > 255) acc = 255;
                            else if (acc < 0) acc = 0;

                            outPtr[i << nv12] = (Ipp8u)acc;
                        }
                        else
                        {
                            int k = i;
                            Ipp32s acc;

                            acc   = 16;
                            acc  += 19 * (intPtr[ k                  ] + intPtr[k +     1 * srcStride/4]);
                            acc  -=  3 * (intPtr[ k - 1 * srcStride/4] + intPtr[k + 2 * srcStride/4]);
                            acc >>=  5;

                            if (acc > 255) acc = 255;
                            else if (acc < 0) acc = 0;

                            outPtr[i] = (Ipp8u)acc;
                        }
                    }
                }
            } else {
                Ipp32s* intPtr = (Ipp32s*)((Ipp8u*)src + (j) * srcStride);
                for (Ipp32s i = 0; i < width; i++)
                {
                    if (i < leftOffset || i >= width - rightOffset ||
                        j < topOffset || j >= height - bottomOffset)
                    {
                        outPtr[i << nv12] = 128; // set to mid gray
                        continue;
                    }

                    VM_ASSERT(intPtr[i] >=0 && intPtr[i] <= 255);
                    outPtr[i << nv12] = (Ipp8u)intPtr[i];
                }
            }
        }
    }
}

template <typename Plane>
void bicubicPlane(Ipp32s srcWidth, Ipp32s srcHeight,
                        Ipp32s srcStride, Plane *src,
                        Ipp32s leftOffset, Ipp32s topOffset, Ipp32s rightOffset, Ipp32s bottomOffset,
                        Ipp32s dstWidth, Ipp32s dstHeight,
                        Ipp32s dstStride, Ipp32s *dst,
                        Ipp32s scaleX, Ipp32s addX, Ipp32s shiftX, Ipp32s deltaX, Ipp32s offsetX,
                        Ipp32s scaleY, Ipp32s addY, Ipp32s shiftY, Ipp32s deltaY, Ipp32s offsetY,
                        Ipp32s *tmpBuf,
                        Ipp32s  yBorder,
                        Ipp32s isChroma)
{
    Ipp32s margin = isChroma ?  1 : 2;
    Ipp32s nv12 = 0;

    for (Ipp32s j = -margin; j < srcHeight + margin; j++)
    {
        Plane* intPtr = (Plane*)((Ipp8u*)src + j * srcStride);
        Ipp32s* outPtr = (Ipp32s*)((Ipp8u*)dst + (j + margin) * dstStride);

        for (Ipp32s i = 0; i < dstWidth; i++)
        {
            if (i < leftOffset || i > dstWidth - rightOffset)
            {
                outPtr[i] = 128;
                continue;
            }

            Ipp32s xRef16 = (( (i - offsetX) * scaleX + addX) >> (shiftX - 4)) - deltaX;
            Ipp32s xRef = xRef16 >> 4;
            Ipp32s xPhase = xRef16 & 0xf;

            Ipp32s acc = 0;

            if (isChroma)
            {
                for (Ipp32s k = 0; k < 2; k++)
                {
                    Ipp32s ind = xRef + k;
                    if (ind < -margin) ind = -margin;
                    else if (ind > (srcWidth + margin - 1)) ind = srcWidth + margin - 1;

                    acc += bilinearFilter[xPhase][k] * intPtr[ind];
                }
            }
            else
            {
                for (Ipp32s k = 0; k < 4; k++)
                {
                    Ipp32s ind = xRef - 1 + k;
                    if (ind < -margin) ind = -margin;
                    else if (ind > (srcWidth + margin - 1)) ind = srcWidth + margin - 1;

                    acc += bicubicFilter[xPhase][k]*intPtr[ind];
                }
            }
            outPtr[i] = acc;
        }
    }

    for (Ipp32s i = 0; i < dstWidth; i++)
    {
        Plane*  intPtr = (Plane *)(dst + i) + margin * dstStride;

        for (Ipp32s j = - yBorder; j < dstHeight + yBorder; j++)
        {
            if (i < leftOffset || i >= dstWidth - rightOffset ||
                j < topOffset - yBorder || j >= dstHeight - bottomOffset + yBorder)
            {
                tmpBuf[j + yBorder] = 128;
                continue;
            }

            Ipp32s yRef16 = (( (j - offsetY) * scaleY + addY) >> (shiftY - 4)) - deltaY;
            Ipp32s yRef = yRef16 >> 4;
            Ipp32s yPhase = yRef16 & 0xf;

            Ipp32s acc = 0;
            if (isChroma)
            {
                for (Ipp32s k = 0; k < 2; k++)
                {
                    Ipp32s ind = yRef + k;
                    if (ind < -margin) ind = -margin;
                    else if (ind > (srcHeight + margin - 1)) ind = srcHeight + margin - 1;

                    acc += bilinearFilter[yPhase][k] * (*((Ipp32s*)((Ipp8u*)intPtr + ind * dstStride)));
                 }
            }
            else
            {
                for (Ipp32s k = 0; k < 4; k++)
                {
                    Ipp32s ind = yRef - 1 + k;
                    if (ind < -margin) ind = -margin;
                    else if (ind > (srcHeight + margin - 1)) ind = srcHeight + margin - 1;

                    acc += bicubicFilter[yPhase][k]* (*((Ipp32s*)((Ipp8u*)intPtr + ind * dstStride)));
                }
            }

            tmpBuf[j + yBorder] = (acc + 512) >> 10;
            if (tmpBuf[j + yBorder] > 255) tmpBuf[j + yBorder] = 255;
            else if (tmpBuf[j + yBorder] < 0) tmpBuf[j + yBorder] = 0;
        }

        intPtr = (Ipp8u *)(dst + (i << nv12));
        for (Ipp32s j = - yBorder; j < dstHeight + yBorder; j++)
        {
            *(Ipp32s*)intPtr = tmpBuf[j + yBorder];
            intPtr += dstStride;
        }
    }
}

template <typename Plane>
void bicubicLuma(Ipp32s srcWidth, Ipp32s srcHeight,
                        Ipp32s srcStride, Plane *src,
                        Ipp32s dstXstart, Ipp32s dstXend,
                        Ipp32s dstYstart, Ipp32s dstYend,
                        Ipp32s dstStride, Ipp32s *dst,
                        Ipp32s scaleX, Ipp32s addX, Ipp32s shiftX, Ipp32s deltaX,
                        Ipp32s scaleY, Ipp32s addY, Ipp32s shiftY, Ipp32s deltaY,
                        Ipp32s *tmpBuf,
                        Ipp32s  yBorder,
                        bool isChroma)
{
    Ipp32s srcYstart, srcYend;
    Ipp32s margin = isChroma ?  1 : 2;

    Ipp32s i = ((dstYstart * scaleY + addY) >> (shiftY - 4)) - deltaY;
    srcYstart = (i >> 4) - (isChroma ?  0 : 1);
    if (srcYstart < 0) srcYstart = 0;

    i = (((dstYend - 1) * scaleY + addY) >> (shiftY - 4)) - deltaY;
    srcYend = (i >> 4) + (isChroma ?  2 : 3);
    if (srcYend >= srcHeight) srcYend = srcHeight;

    for (Ipp32s j = srcYstart - margin; j < srcYend + margin; j++)
    {
        Plane* intPtr = (Plane*)((Ipp8u*)src + j * srcStride);
        Ipp32s* outPtr = (Ipp32s*)((Ipp8u*)dst + (j + margin) * dstStride);

        for (Ipp32s i = dstXstart; i < dstXend; i++)
        {
            Ipp32s xRef16 = ((i * scaleX + addX) >> (shiftX - 4)) - deltaX;
            Ipp32s xRef = xRef16 >> 4;
            Ipp32s xPhase = xRef16 & 0xf;

            Ipp32s acc = 0;

            if (isChroma)
            {
                for (Ipp32s k = 0; k < 2; k++)
                {
                    Ipp32s ind = xRef + k;
                    if (ind < -margin) ind = -margin;
                    else if (ind > (srcWidth + margin - 1)) ind = srcWidth + margin - 1;

                    acc += bilinearFilter[xPhase][k] * intPtr[ind];
                }
            }
            else
            {
                for (Ipp32s k = 0; k < 4; k++)
                {
                    Ipp32s ind = xRef - 1 + k;
                    if (ind < -margin) ind = -margin;
                    else if (ind > (srcWidth + margin - 1)) ind = srcWidth + margin - 1;

                    acc += bicubicFilter[xPhase][k]*intPtr[ind];
                }
            }
            outPtr[i] = acc;
        }
    }

    for (Ipp32s i = dstXstart; i < dstXend; i++)
    {
        Plane*  intPtr = (Plane *)(dst + i) + margin * dstStride;

        for (Ipp32s j = dstYstart - yBorder; j < dstYend + yBorder; j++)
        {
            Ipp32s yRef16 = ((j * scaleY + addY) >> (shiftY - 4)) - deltaY;
            Ipp32s yRef = yRef16 >> 4;
            Ipp32s yPhase = yRef16 & 0xf;

            Ipp32s acc = 0;
            if (isChroma)
            {
                for (Ipp32s k = 0; k < 2; k++)
                {
                    Ipp32s ind = yRef + k;
                    if (ind < -margin) ind = -margin;
                    else if (ind > (srcHeight + margin - 1)) ind = srcHeight + margin - 1;

                    acc += bilinearFilter[yPhase][k] * (*((Ipp32s*)((Ipp8u*)intPtr + ind * dstStride)));
                 }
            }
            else
            {
                for (Ipp32s k = 0; k < 4; k++)
                {
                    Ipp32s ind = yRef - 1 + k;
                    if (ind < -margin) ind = -margin;
                    else if (ind > (srcHeight + margin - 1)) ind = srcHeight + margin - 1;

                    acc += bicubicFilter[yPhase][k]* (*((Ipp32s*)((Ipp8u*)intPtr + ind * dstStride)));
                }
            }

            tmpBuf[j + yBorder] = (acc + 512) >> 10;
            if (tmpBuf[j + yBorder] > 255) tmpBuf[j + yBorder] = 255;
            else if (tmpBuf[j + yBorder] < 0) tmpBuf[j + yBorder] = 0;
        }

        intPtr = (Ipp8u *)(dst + i);
        intPtr += (dstYstart) * dstStride;

        for (Ipp32s j = dstYstart - yBorder; j < dstYend + yBorder; j++)
        {
            *(Ipp32s*)intPtr = tmpBuf[j + yBorder]; // PK
            intPtr += dstStride;
        }
    }
}

template <typename Plane>
void bilinearChromaConstrained(Ipp32s  srcWidth, Ipp32s  srcHeight,
                                      Ipp32s  srcStride, Plane *src,
                                      Ipp32s  dstXstart, Ipp32s  dstXend,
                                      Ipp32s  dstYstart, Ipp32s  dstYend,
                                      Ipp32s  dstStride, Ipp32s  *dst,
                                      Ipp32s  scaleX, Ipp32s  addX, Ipp32s  shiftX, Ipp32s  deltaX,
                                      Ipp32s  scaleY, Ipp32s  addY, Ipp32s  shiftY, Ipp32s  deltaY,
                                      Ipp32s  *tmpBuf1)
{
    Ipp32s srcYstart, srcYend;
    Ipp32s margin = 1;

    Ipp32s i = ((dstYstart * scaleY + addY) >> (shiftY - 4)) - deltaY;
    srcYstart = (i >> 4);
    if (srcYstart < 0) srcYstart = 0;


    i = (((dstYend - 1) * scaleY + addY) >> (shiftY - 4)) - deltaY;
    srcYend = (i >> 4) + 2;
    if (srcYend >= srcHeight) srcYend = srcHeight;

    for (Ipp32s j = srcYstart - margin; j < srcYend + margin; j++)
    {
        Plane* intPtr = (Plane*)((Ipp8u*)src + j * srcStride);
        Ipp32s*  outPtr = (Ipp32s*) ((Ipp8u*)dst + (j + margin) * dstStride);

        for (Ipp32s i = dstXstart; i < dstXend; i++)
        {
            Ipp32s xRef16 = ((i * scaleX + addX) >> (shiftX - 4)) - deltaX;
            Ipp32s xRef = xRef16 >> 4;
            Ipp32s xPhase = xRef16 & 0xf;

            Ipp32s acc = 0;
            for (Ipp32s k = 0; k < 2; k++)
            {
                Ipp32s ind = xRef + k;
                if (ind < -margin) ind = -margin;
                else if (ind > (srcWidth + margin - 1)) ind = srcWidth + margin - 1;

                acc += bilinearFilter[xPhase][k] * intPtr[ind];
            }

            outPtr[i] = acc;
        }
    }

    for (Ipp32s i = dstXstart; i < dstXend; i++)
    {
        Ipp32s*  intPtr = (Ipp32s*) ((Ipp8u*)dst + (margin) * dstStride) + i;

        for (Ipp32s j = dstYstart; j < dstYend; j++)
        {
            Ipp32s yRef16 = ((j * scaleY + addY) >> (shiftY - 4)) - deltaY;
            Ipp32s yRef = yRef16 >> 4;
            Ipp32s yPhase = yRef16 & 0xf;

            Ipp32s acc = 0;
            for (Ipp32s k = 0; k < 2; k++)
            {
                Ipp32s ind = yRef + k;
                if (ind < -margin) ind = -margin;
                else if (ind > (srcHeight + margin - 1)) ind = srcHeight + margin - 1;

                acc += bilinearFilter[yPhase][k] * intPtr[ind*dstStride/4];
            }

            tmpBuf1[j] = (acc + 512) >> 10;
            if (tmpBuf1[j] > 255) tmpBuf1[j] = 255;
            else if (tmpBuf1[j] < 0) tmpBuf1[j] = 0;
        }

        intPtr = dst + i;
        intPtr += dstYstart * (dstStride / 4);

        for (Ipp32s j = dstYstart; j < dstYend; j++)
        {
            *(Ipp32s*)intPtr = tmpBuf1[j];
            intPtr += (dstStride / 4);
        }
    }
}

template <typename Plane>
void bilinearChroma(Ipp32s  srcWidth, Ipp32s  srcHeight,
                           Ipp32s  srcStride, Plane *src,
                           Ipp32s  dstXstart, Ipp32s  dstXend,
                           Ipp32s  dstYstart, Ipp32s  dstYend,
                           Ipp32s  dstStride, Plane *dst,
                           Ipp32s  scaleX, Ipp32s  addX, Ipp32s  shiftX, Ipp32s  deltaX,
                           Ipp32s  scaleY, Ipp32s  addY, Ipp32s  shiftY, Ipp32s  deltaY,
                           Ipp32s  *tmpBuf,
                           Ipp32s  *tmpBuf1,
                           Ipp32s  tmpBufStride,
                           Ipp32s  verticalInterpolationFlag,
                           Ipp32s  yBorder,
                           Ipp32s nv12)
{
    tmpBufStride;tmpBuf;
    Ipp32s srcYstart, srcYend;
    Ipp32s *interp_buf = tmpBuf1;
    Ipp32s margin = 1;

    Ipp32s i = ((dstYstart * scaleY + addY) >> (shiftY - 4)) - deltaY;
    srcYstart = (i >> 4);
    if (srcYstart < 0) srcYstart = 0;


    i = (((dstYend - 1) * scaleY + addY) >> (shiftY - 4)) - deltaY;
    srcYend = (i >> 4) + 2;
    if (srcYend >= srcHeight) srcYend = srcHeight;

    for (Ipp32s j = srcYstart - margin; j < srcYend + margin; j++)
    {
        Plane* intPtr = (Plane*)((Ipp8u*)src + j * srcStride);
        Ipp32s*  outPtr = (Ipp32s*) ((Ipp8u*)tmpBuf + (j + margin) * tmpBufStride);

        for (Ipp32s i = dstXstart; i < dstXend; i++)
        {
            Ipp32s xRef16 = ((i * scaleX + addX) >> (shiftX - 4)) - deltaX;
            Ipp32s xRef = xRef16 >> 4;
            Ipp32s xPhase = xRef16 & 0xf;

            Ipp32s acc = 0;
            for (Ipp32s k = 0; k < 2; k++)
            {
                Ipp32s ind = xRef + k;
                if (ind < -margin) ind = -margin;
                else if (ind > (srcWidth + margin - 1)) ind = srcWidth + margin - 1;

                acc += bilinearFilter[xPhase][k] * intPtr[ind];
            }

            outPtr[i] = acc;
        }
    }

    for (Ipp32s i = dstXstart; i < dstXend; i++)
    {
        Ipp32s*  intPtr = tmpBuf + i + margin * tmpBufStride/4;

        for (Ipp32s j = dstYstart; j < dstYend + yBorder; j++)
        {
            Ipp32s yRef16 = ((j * scaleY + addY) >> (shiftY - 4)) - deltaY;
            Ipp32s yRef = yRef16 >> 4;
            Ipp32s yPhase = yRef16 & 0xf;

            Ipp32s acc = 0;
            for (Ipp32s k = 0; k < 2; k++)
            {
                Ipp32s ind = yRef + k;
                if (ind < -margin) ind = -margin;
                else if (ind > (srcHeight + margin - 1)) ind = srcHeight + margin - 1;

                acc += bilinearFilter[yPhase][k] * intPtr[ind*tmpBufStride/4];
            }

            interp_buf[j] = (acc + 512) >> 10;
            if (interp_buf[j] > 255) interp_buf[j] = 255;
            else if (interp_buf[j] < 0) interp_buf[j] = 0;
        }

        Plane* outPtr = dst + (i << nv12);

        if (!verticalInterpolationFlag)
        {
            for (Ipp32s j = dstYstart; j < dstYend; j++)
            {
                *(Plane*)((Ipp8u*)outPtr + j * dstStride) = (Plane)interp_buf[j];
            }
        } else {
            for(Ipp32s j = dstYstart << 1; j < dstYend << 1; j++)
            {
                if( ( j % 2 ) == 0/*iBotField */ || j == (dstYend << 1) - 1 )
                {
                    Ipp32s k = ( ( j >> 1 )/* + yBorder */);
                    *(Plane*)((Ipp8u*)outPtr + j * dstStride) = (Plane)interp_buf[k];
                }
                else
                {
                    Ipp32s k = ( ( j >> 1 )/* + yBorder */- 0/*iBotField*/ );
                    *(Plane*)((Ipp8u*)outPtr + j * dstStride) = (Plane)((interp_buf[k] + interp_buf[k+1] + 1 ) >> 1);
                }
            }
        }
    }
}

template <typename Coeffs>
void bilinearResidual(Ipp32s  srcWidth,
                             Ipp32s  srcHeight,
                             Coeffs  *src,
                             Ipp32s  dstWidth,
                             Ipp32s  dstHeight,
                             Coeffs  *dst,
                             Ipp32s  leftOffset,
                             Ipp32s  topOffset,
                             Ipp32s  rightOffset,
                             Ipp32s  bottomOffset,
                             Ipp32s  scaleX,
                             Ipp32s  addX,
                             Ipp32s  shiftX,
                             Ipp32s  deltaX,
                             Ipp32s  scaleY,
                             Ipp32s  addY,
                             Ipp32s  shiftY,
                             Ipp32s  deltaY,
                             Ipp32s  *tmpBuf,
                             Ipp32s  *tmpBuf1,
                             Ipp32s  *tmpBuf0,
                             H264DecoderLayerDescriptor *refLayerMb,
                             Ipp32s  chromaFlag,
                             Ipp32s  verticalInterpolationFlag,
                             Ipp32s  yBorder,
                             Ipp32s  bottom_field_flag)
{
    Ipp32s mbWidth, mbHeight;
    Ipp32s srcWidthInMbs, srcHeightInMbs;
    Ipp32s shiftH;
    Ipp32s stride;
    Ipp32s *interp_buf = tmpBuf1;
    Ipp32s baseFieldFlag  = refLayerMb->frame_mbs_only_flag ? 0 : 1;

    if ( chromaFlag ) {
        mbWidth = 8;
        mbHeight = 8;
        srcWidthInMbs = srcWidth >> 3;
        srcHeightInMbs = srcHeight >> 3;
    } else {
        mbWidth = 16;
        mbHeight = 16;
        srcWidthInMbs = srcWidth >> 4;
        srcHeightInMbs = srcHeight >> 4;
    }

    Ipp32u refMBOffset = 0;
    H264DecoderMacroblockLayerInfo * refLayerLocalInfo = refLayerMb->mbs;
    if (refLayerMb->field_pic_flag && refLayerMb->bottom_field_flag)
    {
        //refLayerLocalInfo += srcWidthInMbs * srcHeightInMbs;
        refMBOffset = srcWidthInMbs * srcHeightInMbs;
    }

    if (!refLayerMb->is_MbAff)
    {
        for (Ipp32s j = 0; j < srcHeightInMbs; j++)
        for (Ipp32s i = 0; i < srcWidthInMbs; i++)
        {
            Ipp32s  iMbAddr   = j * srcWidthInMbs + i + refMBOffset;
            Ipp32s    flag8x8      = chromaFlag ? false :
                GetMB8x8TSFlag(refLayerLocalInfo[iMbAddr]);

            Ipp32s*  bufPtr = tmpBuf0 + j * mbHeight * srcWidth + i * mbWidth;

            shiftH = 0; // todo
            if( flag8x8 )
              {
                for(Ipp32s jj = 0; jj < mbHeight; jj++, bufPtr += srcWidth )
                for(Ipp32s ii = 0; ii < mbWidth; ii++ )
                {
                  bufPtr[ii]   = ( ( jj >> ( 3 - shiftH ) ) << 1 ) + ( ii >> 3 );
                  bufPtr[ii]  += iMbAddr << 2;
                  bufPtr[ii]   = ( bufPtr[ii] << 1 ) + 1;
                }
              }
              else
              {
                for(Ipp32s jj = 0; jj < mbHeight; jj++, bufPtr += srcWidth )
                for(Ipp32s ii = 0; ii < mbWidth; ii++ )
                {
                  bufPtr[ii]   = ( ( jj >> ( 2 - shiftH ) ) << 2 ) + ( ii >> 2 );
                  bufPtr[ii]  += iMbAddr << 4;
                  bufPtr[ii]   = ( bufPtr[ii] << 1 );
                }
              }
        }
    } else {
        for(Ipp32s j = 0; j < srcHeightInMbs; j++ )
            for(Ipp32s i = 0; i < srcWidthInMbs; i++ )
            {
                Ipp32s  iMbTopAddr   = (j * srcWidthInMbs + i) * 2 + refMBOffset;
                Ipp32s fieldFlag = GetMBFieldDecodingFlag(refLayerLocalInfo[iMbTopAddr]);

                if (fieldFlag) {
                    Ipp32s k = 0;

                    Ipp32s flag8x8 = chromaFlag ? false : GetMB8x8TSFlag(refLayerLocalInfo[iMbTopAddr]);
                    Ipp32s* bufPtr = tmpBuf0 + (j) * mbHeight * srcWidth + i * mbWidth;
                    Ipp32s iMbAddr = iMbTopAddr + k;

                    shiftH = 0; // todo
                    if( flag8x8 )
                    {
                        for(Ipp32s jj = 0; jj < mbHeight; jj++, bufPtr += srcWidth )
                            for(Ipp32s ii = 0; ii < mbWidth; ii++ )
                            {
                                bufPtr[ii]   = ( ( jj >> 3 ) << 1 ) + ( ii >> 3 );
                                bufPtr[ii]  += iMbAddr << 2;
                                bufPtr[ii]   = ( bufPtr[ii] << 1 ) + 1;
                            }
                    }
                    else
                    {
                        for(Ipp32s jj = 0; jj < mbHeight; jj++, bufPtr += srcWidth )
                            for(Ipp32s ii = 0; ii < mbWidth; ii++ )
                            {
                                bufPtr[ii]   = ( ( jj >> 2 ) << 2 ) + ( ii >> 2 );
                                bufPtr[ii]  += iMbAddr << 4;
                                bufPtr[ii]   = ( bufPtr[ii] << 1 );
                            }
                    }
                } else {
                    Ipp32s* bufPtr = tmpBuf0 + j * (mbHeight) * srcWidth + i * mbWidth;
                    for (Ipp32s k = 0; k < 2; k++)
                    {
                        Ipp32s flag8x8 = chromaFlag ? false : GetMB8x8TSFlag(refLayerLocalInfo[iMbTopAddr + k]);
                        Ipp32s iMbAddr = iMbTopAddr + k;

                        shiftH = 0; // todo
                        if( flag8x8 )
                        {
                            for(Ipp32s jj = 0; jj < mbHeight >> 1; jj++, bufPtr += srcWidth )
                                for(Ipp32s ii = 0; ii < mbWidth; ii++ )
                                {
                                    bufPtr[ii]   = ( ( jj >> 2 ) << 1 ) + ( ii >> 3 );
                                    bufPtr[ii]  += iMbAddr << 2;
                                    bufPtr[ii]   = ( bufPtr[ii] << 1 ) + 1;
                                }
                        }
                        else
                        {
                            for(Ipp32s jj = 0; jj < mbHeight >> 1; jj++, bufPtr += srcWidth )
                                for(Ipp32s ii = 0; ii < mbWidth; ii++ )
                                {
                                    bufPtr[ii]   = ( ( jj >> 1 ) << 2 ) + ( ii >> 2 );
                                    bufPtr[ii]  += iMbAddr << 4;
                                    bufPtr[ii]   = ( bufPtr[ii] << 1 );
                                }
                        }
                    }
                }
            }
    }

    for (Ipp32s j = 0; j < srcHeight; j++)
    {
        Coeffs* intPtr;
        Ipp32s* outPtr = tmpBuf + j * dstWidth;
        Ipp32s*  bufPtr = tmpBuf0 + j * srcWidth;
        intPtr = src + j * (srcWidth << baseFieldFlag);

        for (Ipp32s i = 0; i < dstWidth; i++)
        {
            Ipp32s xRef16 = (Ipp32s)((Ipp32u)((i - leftOffset) * scaleX + addX) >> (shiftX - 4)) - deltaX;
            Ipp32s xRef = xRef16 >> 4;
            Ipp32s xPhase = xRef16 & 0xf;
            Ipp32s ind0, ind1;
            Ipp32s idc0, idc1;

            if( i < leftOffset || i >= dstWidth - rightOffset )
            {
              outPtr[i] = 0;
              continue;
            }

            ind0   = xRef;
            ind1 = xRef + 1;
            if (ind0 < 0) ind0 = 0;
            else if (ind0 > srcWidth - 1) ind0 =  srcWidth - 1;
            if (ind1 < 0) ind1 = 0;
            else if (ind1 > srcWidth - 1) ind1 =  srcWidth - 1;

            idc0   = bufPtr[ind0];
            idc1   = bufPtr[ind1];

            if( idc0 == idc1 )
            {
              outPtr[i] = ( 16 - xPhase ) * intPtr[ind0] + xPhase * intPtr[ind1];
            }
            else
            {
              outPtr[i] = intPtr[ xPhase < 8 ? ind0 : ind1 ] << 4;
            }

        }
    }

    stride = dstWidth;

    for (Ipp32s i = 0; i < dstWidth; i++)
    {
        Ipp32s* intPtr = tmpBuf + i;
        Coeffs* outPtr = dst + i;
        Ipp32s*  bufPtr = tmpBuf0;

        Ipp32s xRef16 = (Ipp32s)((Ipp32u)((i - leftOffset) * scaleX + addX) >> (shiftX - 4)) - deltaX;
        Ipp32s xRef = ( xRef16 >> 4 ) + ( ( xRef16 & 15 ) >> 3 );
        if (xRef < 0) xRef = 0;
        else if (xRef > srcWidth - 1) xRef = srcWidth - 1;

        for (Ipp32s j = -yBorder; j < dstHeight + yBorder; j++/*, outPtr += stride*/)
        {
            Ipp32s yRef16, yRef, yPhase;
            Ipp32s ind0, ind1;
            Ipp32s idc0, idc1;
            Ipp32s ty0;

            if( i < leftOffset            || i >= dstWidth - rightOffset           ||
                j < topOffset - yBorder || j >= dstHeight - bottomOffset + yBorder  )
            {
              interp_buf[j + yBorder] = 0;
              continue;
            }

            ty0 = (j - topOffset) * scaleY + addY;
            ty0 = j >= topOffset ? (Ipp32s)((Ipp32u)ty0 >> (shiftY - 4) ) : (ty0 >> (shiftY - 4));
            yRef16 = ty0 - deltaY;
            yRef = yRef16 >> 4;
            yPhase = yRef16 & 0xf;

            ind0 = yRef;
            ind1 = yRef + 1;

            if (ind0 < 0) ind0 = 0;
            else if (ind0 > srcHeight - 1) ind0 =  srcHeight - 1;
            if (ind1 < 0) ind1 = 0;
            else if (ind1 > srcHeight - 1) ind1 =  srcHeight - 1;

            idc0   = bufPtr[ ind0 * srcWidth + xRef ];
            idc1   = bufPtr[ ind1 * srcWidth + xRef ];

            if( idc0 == idc1 )
            {
              interp_buf[j + yBorder] = ( ( 16 - yPhase ) * intPtr[ind0*dstWidth] + yPhase * intPtr[ind1*dstWidth] + 128 ) >> 8;
            }
            else
            {
              interp_buf[j + yBorder] = ( intPtr[ ( yPhase < 8 ? ind0 : ind1 ) * dstWidth ] + 8 ) >> 4;
            }
        }

        if (verticalInterpolationFlag) {
            for (Ipp32s j = 0; j < dstHeight << 1; j++, outPtr += dstWidth )
            {
                if( i < leftOffset || i >= dstWidth - rightOffset ||
                    j < (topOffset << 1) || j >= ((dstHeight - bottomOffset) << 1)   )
                {
                    outPtr[0] = 0; // set to zero
                    continue;
                }

                if( ( j % 2 ) == bottom_field_flag)
                {
                    Ipp32s k = ( ( j >> 1 ) + yBorder );
                    outPtr[0] = (Ipp16s)interp_buf[k];
                }
                else
                {
                    Ipp32s k = ( ( j >> 1 ) + yBorder - bottom_field_flag);
                    outPtr[0] = (Ipp16s)(( interp_buf[k] + interp_buf[k+1] + 1 ) >> 1);
                }
            }
        } else {
            for(Ipp32s  j = 0; j < dstHeight; j++, outPtr += dstWidth ) {
                outPtr[0] = (Ipp16s)interp_buf[j];
            }
        }
    }
}

template <typename Coeffs>
void upsamplingResidual(H264Slice *curSlice,
                        H264DecoderLayerDescriptor *refLayer,
                        H264DecoderLayerDescriptor *curLayer,
                        Ipp32s *tmpBuf0,
                        Ipp32s *tmpBuf1,
                        Coeffs *tmpBuf2,
                        IppiSize refLumaSize)
    {
        const H264SeqParamSet *curSps = curSlice->GetSeqParam();
        H264DecoderLayerResizeParameter *resizeParams = curLayer->m_pResizeParameter;

        Ipp32s chromaFlag = 0;

        Ipp32s baseFieldFlag  = refLayer->frame_mbs_only_flag ? 0 : 1;
        Ipp32s curFieldFlag = (refLayer->frame_mbs_only_flag && curLayer->frame_mbs_only_flag) ? 0 : 1;

        Ipp32s frameBasedResamplingFlag   = curLayer->frame_mbs_only_flag && refLayer->frame_mbs_only_flag;
        Ipp32s verticalInterpolationFlag  = !frameBasedResamplingFlag && !curLayer->field_pic_flag;
        Ipp32s yBorder = verticalInterpolationFlag ? 2 : 0;

        Ipp32s phaseX = chromaFlag ? resizeParams->phaseX : 0;
        Ipp32s phaseY = chromaFlag ? resizeParams->phaseY : 0;
        Ipp32s refPhaseX = chromaFlag ? resizeParams->refPhaseX : 0;
        Ipp32s refPhaseY = chromaFlag ? resizeParams->refPhaseY : 0;

        Ipp32s refW = refLumaSize.width >> chromaFlag;
        Ipp32s refH = refLumaSize.height >> chromaFlag;

        Ipp32s scaledW = resizeParams->scaled_ref_layer_width >> chromaFlag;
        Ipp32s scaledH = resizeParams->scaled_ref_layer_height >> chromaFlag;
        Ipp32s fullW = curSps->frame_width_in_mbs * 16 >> chromaFlag;
        Ipp32s fullH = curSps->frame_height_in_mbs * 16 >> chromaFlag;

        Ipp32s leftOffset = resizeParams->leftOffset >> chromaFlag;
        Ipp32s topOffset = resizeParams->topOffset >> (chromaFlag + curFieldFlag);
        Ipp32s rightOffset = fullW - leftOffset - scaledW;
        Ipp32s bottomOffset = (fullH >> curFieldFlag) - topOffset - (scaledH >> curFieldFlag);

        if (!curLayer->frame_mbs_only_flag && refLayer->frame_mbs_only_flag)
            scaledH >>= 1;

        Ipp32s shiftX = ((curSps->level_idc <= 30) ? 16 : countShift(refW));
        Ipp32s shiftY = ((curSps->level_idc <= 30) ? 16 : countShift(refH));

        Ipp32s scaleX = (((Ipp32u)refW << shiftX) + (scaledW >> 1)) / scaledW;
        Ipp32s scaleY = (((Ipp32u)refH << shiftY) + (scaledH >> 1)) / scaledH;

        Ipp32s bottom_field_flag = refLayer->frame_mbs_only_flag ? 0 : curLayer->field_pic_flag ? curLayer->bottom_field_flag : refLayer->field_pic_flag ? refLayer->bottom_field_flag : 0;
            
        if (curLayer->frame_mbs_only_flag == 0 || refLayer->frame_mbs_only_flag == 0)
        {
            if (refLayer->frame_mbs_only_flag)
            {
                phaseY    = phaseY + 4 * bottom_field_flag + 2;
                refPhaseY = 2 * refPhaseY + 2;
            }
            else
            {
                phaseY    = phaseY    + 4 * bottom_field_flag;
                refPhaseY = refPhaseY + 4 * bottom_field_flag;
            }
        }

        Ipp32s addX = (((refW * ( 2 + phaseX )) << (shiftX - 2)) + (scaledW >> 1)) / scaledW + (1 << (shiftX - 5));
        Ipp32s addY = (((refH * ( 2 + phaseY )) << (shiftY - 2)) + (scaledH >> 1)) / scaledH + (1 << (shiftY - 5));

        Ipp32s deltaX = 4 * ( 2 + refPhaseX );
        Ipp32s deltaY = 4 * ( 2 + refPhaseY );

        if (curLayer->frame_mbs_only_flag == 0 || refLayer->frame_mbs_only_flag == 0)
        {
            addY   = (((refH * (2 + phaseY) ) << (shiftY - 3)) + (scaledH >> 1) ) / scaledH + (1 << (shiftY - 5) );
            deltaY = 2 * (2 + refPhaseY);
        }

        bilinearResidual(refW, refH >> baseFieldFlag, refLayer->m_pYResidual,
                         fullW, fullH >> curFieldFlag, curLayer->m_pYResidual,
                         leftOffset, topOffset, rightOffset, bottomOffset,
                         scaleX, addX, shiftX, deltaX, scaleY, addY, shiftY, deltaY,
                         tmpBuf0, tmpBuf1, tmpBuf2, refLayer, 0, verticalInterpolationFlag, yBorder, bottom_field_flag);


        chromaFlag = 1;

        yBorder = verticalInterpolationFlag ? 1 : 0;

        phaseX = chromaFlag ? resizeParams->phaseX : 0;
        phaseY = chromaFlag ? resizeParams->phaseY : 0;
        refPhaseX = chromaFlag ? resizeParams->refPhaseX : 0;
        refPhaseY = chromaFlag ? resizeParams->refPhaseY : 0;
        refW = refLumaSize.width >> chromaFlag;
        refH = refLumaSize.height >> chromaFlag;
        scaledW = resizeParams->scaled_ref_layer_width >> chromaFlag;
        scaledH = resizeParams->scaled_ref_layer_height >> chromaFlag;
        fullW = curSps->frame_width_in_mbs * 16 >> chromaFlag;
        fullH = curSps->frame_height_in_mbs * 16 >> chromaFlag;

        leftOffset = resizeParams->leftOffset >> chromaFlag;
        topOffset = resizeParams->topOffset >> (chromaFlag + curFieldFlag);
        rightOffset = fullW - leftOffset - scaledW;
        bottomOffset = (fullH >> curFieldFlag) - topOffset - (scaledH >> curFieldFlag);

        if (curLayer->frame_mbs_only_flag == 0 && refLayer->frame_mbs_only_flag == 1)
        {
            scaledH >>= 1;
        }

        shiftX = ((curSps->level_idc <= 30) ? 16 : countShift(refW));
        shiftY = ((curSps->level_idc <= 30) ? 16 : countShift(refH));

        scaleX = (((Ipp32u)refW << shiftX) + (scaledW >> 1)) / scaledW;
        scaleY = (((Ipp32u)refH << shiftY) + (scaledH >> 1)) / scaledH;

        addX = (((refW * ( 2 + phaseX )) << (shiftX - 2)) + (scaledW >> 1)) / scaledW +
               (1 << (shiftX - 5));

        addY = (((refH * ( 2 + phaseY )) << (shiftY - 2)) + (scaledH >> 1)) / scaledH +
               (1 << (shiftY - 5));

        if (curLayer->frame_mbs_only_flag == 0 || refLayer->frame_mbs_only_flag == 0)
        {
            if( refLayer->frame_mbs_only_flag )
            {
                phaseY       = phaseY + 4 * bottom_field_flag + 1;
                refPhaseY    = 2 * refPhaseY + 2;
            }
            else
            {
                phaseY       = phaseY    + 4 * bottom_field_flag;
                refPhaseY    = refPhaseY + 4 * bottom_field_flag;
            }
        }

        deltaX     = 4 * ( 2 + refPhaseX );
        deltaY     = 4 * ( 2 + refPhaseY );

        if (curLayer->frame_mbs_only_flag == 0 || refLayer->frame_mbs_only_flag == 0)
        {
            addY           = ( ( ( refH * ( 2 + phaseY ) ) << ( shiftY - 3 ) ) + ( scaledH >> 1 ) ) / scaledH + ( 1 << ( shiftY - 5 ) );
            deltaY         = 2 * ( 2 + refPhaseY );
        }

        bilinearResidual(refW, refH >> baseFieldFlag, refLayer->m_pUResidual,
                         fullW, fullH >> curFieldFlag, curLayer->m_pUResidual,
                         leftOffset, topOffset,
                         rightOffset, bottomOffset,
                         scaleX, addX, shiftX, deltaX, scaleY, addY, shiftY, deltaY,
                         tmpBuf0, tmpBuf1, tmpBuf2, refLayer, 1, verticalInterpolationFlag, yBorder, bottom_field_flag);

        bilinearResidual(refW, refH >> baseFieldFlag, refLayer->m_pVResidual,
                         fullW, fullH >> curFieldFlag, curLayer->m_pVResidual,
                         leftOffset, topOffset,
                         rightOffset, bottomOffset,
                         scaleX, addX, shiftX, deltaX, scaleY, addY, shiftY, deltaY,
                         tmpBuf0, tmpBuf1, tmpBuf2, refLayer, 1, verticalInterpolationFlag, yBorder, bottom_field_flag);
    }

/* only 4:2:0 is supported now */
template <typename PlaneY, typename PlaneUV>
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
        srcdstU += 3;
        srcdstV += 3;
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

template <typename PlaneY, typename PlaneUV>
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
            srcdstU[i] = (PlaneUV)u;
        }

        srcdstU = (PlaneUV*)((Ipp8u*)srcdstU + strideU);
    }

    for (j = 0; j < (4 >> half); j++)
    {
        for (i = 0; i < 4; i++)
        {
            srcdstV[i] = (PlaneUV)v;
        }

        srcdstV = (PlaneUV*)((Ipp8u*)srcdstV + strideV);
    }
}

template <typename PlaneY, typename PlaneUV>
void fillPlanes_wh(PlaneY  *srcdstY,
                Ipp32s  strideY,
                PlaneUV *srcdstU,
                Ipp32s  strideU,
                PlaneUV *srcdstV,
                Ipp32s  strideV,
                Ipp32s  y,
                Ipp32s  u,
                Ipp32s  v,
                Ipp32s width, Ipp32s height)
{
    Ipp32s i, j;

    /* Luma */

    for (j = 0; j < height; j++)
    {
        for (i = 0; i < width; i++)
        {
            srcdstY[i] = (PlaneY)y;
        }

        srcdstY = (PlaneY*)((Ipp8u*)srcdstY + strideY);
    }

    width >>= 1;
    height >>= 1;

    /* Chroma */

    for (j = 0; j < height; j++)
    {
        for (i = 0; i < width; i++)
        {
            srcdstU[i] = (PlaneUV)u;
        }

        srcdstU = (PlaneUV*)((Ipp8u*)srcdstU + strideU);
    }

    for (j = 0; j < height; j++)
    {
        for (i = 0; i < width; i++)
        {
            srcdstV[i] = (PlaneUV)v;
        }

        srcdstV = (PlaneUV*)((Ipp8u*)srcdstV + strideV);
    }
}

template <typename PlaneY, typename PlaneUV>
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
            srcdstU[i] = ptrU[i];
        }

        srcdstU = (PlaneUV*)((Ipp8u*)srcdstU + strideU);
    }

    for (j = 0; j < (4 >> half); j++)
    {
        for (i = 0; i < 4; i++)
        {
            srcdstV[i] = ptrV[i];
        }

        srcdstV = (PlaneUV*)((Ipp8u*)srcdstV + strideV);
    }
}

template <typename PlaneY, typename PlaneUV>
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

    if (left)
    {
        ptrY = srcdstY - 1;
        ptrU = srcdstU - 1;
        ptrV = srcdstV - 1;
    }
    else
    {
        ptrY = srcdstY + 8;
        ptrU = srcdstU + 4;
        ptrV = srcdstV + 4;
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
            srcdstU[i] = ptrU[0];
        }

        srcdstU = (PlaneUV*)((Ipp8u*)srcdstU + strideU);
        ptrU = (PlaneUV*)((Ipp8u*)ptrU + strideU);
    }

    for (j = 0; j < 4; j++)
    {
        for (i = 0; i < 4; i++)
        {
            srcdstV[i] = ptrV[0];
        }

        srcdstV = (PlaneUV*)((Ipp8u*)srcdstV + strideV);
        ptrV = (PlaneUV*)((Ipp8u*)ptrV + strideV);
    }
}

template <typename PlaneY, typename PlaneUV>
void xpad8x8_MBAFF( PlaneY *tmpPtrY, PlaneUV *tmpPtrU, PlaneUV *tmpPtrV,
                            Ipp32s strideY, Ipp32s strideU, Ipp32s strideV,
                            Ipp32s cIdx, Ipp32s bVer, Ipp32s bHor, Ipp32s bCorner,
                            Ipp32s half, Ipp32s bFromAbove, Ipp32s bFromLeft )
{
    if (cIdx & 1) {
        tmpPtrY += 8;
        tmpPtrU += 4;
        tmpPtrV += 4;
    }

    if (cIdx & 2) {
        tmpPtrY = (PlaneY*)((Ipp8u*)tmpPtrY + 8 * strideY);
        tmpPtrU = (PlaneY*)((Ipp8u*)tmpPtrU + 4 * strideU);
        tmpPtrV = (PlaneY*)((Ipp8u*)tmpPtrV + 4 * strideV);
    }

    if( bVer && bHor )
    {
        if( bFromAbove && bFromLeft )
        {
            diagonalConstruction(tmpPtrY, strideY, tmpPtrU, strideU,
                tmpPtrV, strideV, 0, bCorner, half);
        }
        else if( bFromAbove )
        {
            diagonalConstruction(tmpPtrY, strideY, tmpPtrU, strideU,
                tmpPtrV, strideV, 1, bCorner, half);
        }
        else if( bFromLeft )
        {
            diagonalConstruction(tmpPtrY, strideY, tmpPtrU, strideU,
                tmpPtrV, strideV, 2, bCorner, half);
        }
        else
        {
            diagonalConstruction(tmpPtrY, strideY, tmpPtrU, strideU,
                tmpPtrV, strideV, 3, bCorner, half);
        }
    }
    else if( bVer )
    {
        if( bFromAbove )
        {
            copyPlanesFromAboveBelow(tmpPtrY, strideY, tmpPtrU, strideU,
                tmpPtrV, strideV, 1, half);
        }
        else
        {
            copyPlanesFromAboveBelow(tmpPtrY, strideY, tmpPtrU, strideU,
                tmpPtrV, strideV, 0, half);
        }
    }
    else if( bHor )
    {
        if( bFromLeft )
        {
            copyPlanesFromLeftRight(tmpPtrY, strideY, tmpPtrU, strideU,
                tmpPtrV, strideV, 1);
        }
        else
        {
            copyPlanesFromLeftRight(tmpPtrY, strideY, tmpPtrU, strideU,
                tmpPtrV, strideV, 0);
        }
    }
    else if( bCorner )
    {
        PlaneY *ptry = tmpPtrY;
        PlaneUV *ptru = tmpPtrU;
        PlaneUV *ptrv = tmpPtrV;

        if( bFromAbove && bFromLeft )
        {
            fillPlanes(ptry, strideY, ptru, strideU, ptrv, strideV,
                (Ipp32s)(*(((PlaneY*)((Ipp8u*)tmpPtrY - strideY)) - 1)),
                (Ipp32s)(*(((PlaneUV*)((Ipp8u*)tmpPtrU - strideU)) - 1)),
                (Ipp32s)(*(((PlaneUV*)((Ipp8u*)tmpPtrV - strideV)) - 1)),
                half);
        }
        else if( bFromAbove )
        {
            fillPlanes(ptry, strideY, ptru, strideU, ptrv, strideV,
                (Ipp32s)(*(((PlaneY*)((Ipp8u*)tmpPtrY - strideY)) + 8)),
                (Ipp32s)(*(((PlaneUV*)((Ipp8u*)tmpPtrU - strideU)) + 4)),
                (Ipp32s)(*(((PlaneUV*)((Ipp8u*)tmpPtrV - strideV)) + 4)),
                half);
        }
        else if( bFromLeft )
        {
            if (half) {
                ptry = (PlaneY*)((Ipp8u*)tmpPtrY + 4 *strideY);
                ptru = (PlaneUV*)((Ipp8u*)tmpPtrU + 2 * strideU);
                ptrv = (PlaneUV*)((Ipp8u*)tmpPtrV + 2 * strideV);
            }
            fillPlanes(ptry, strideY, ptru, strideU, ptrv, strideV,
                (Ipp32s)(*(((PlaneY*)((Ipp8u*)tmpPtrY + 8 *strideY)) - 1)),
                (Ipp32s)(*(((PlaneUV*)((Ipp8u*)tmpPtrU + 4 * strideU)) - 1)),
                (Ipp32s)(*(((PlaneUV*)((Ipp8u*)tmpPtrV + 4 * strideV)) - 1)),
                half);
        }
        else
        {
            if (half) {
                ptry = (PlaneY*)((Ipp8u*)tmpPtrY + 4 *strideY);
                ptru = (PlaneUV*)((Ipp8u*)tmpPtrU + 2 * strideU);
                ptrv = (PlaneUV*)((Ipp8u*)tmpPtrV + 2 * strideV);
            }
            fillPlanes(ptry, strideY, ptru, strideU, ptrv, strideV,
                (Ipp32s)(*(((PlaneY*)((Ipp8u*)tmpPtrY + 8 * strideY)) + 8)),
                (Ipp32s)(*(((PlaneUV*)((Ipp8u*)tmpPtrU + 4 * strideU)) + 4)),
                (Ipp32s)(*(((PlaneUV*)((Ipp8u*)tmpPtrV + 4 * strideV)) + 4)),
                half);
        }
    }
    else
    {
        PlaneY *ptry = tmpPtrY;
        PlaneUV *ptru = tmpPtrU;
        PlaneUV *ptrv = tmpPtrV;

        if (half && !bFromAbove) {
            ptry = (PlaneY*)((Ipp8u*)tmpPtrY + 4 *strideY);
            ptru = (PlaneUV*)((Ipp8u*)tmpPtrU + 2 * strideU);
            ptrv = (PlaneUV*)((Ipp8u*)tmpPtrV + 2 * strideV);
        }
        fillPlanes(ptry, strideY,
            ptru, strideU,
            ptrv, strideV,
            0, 0, 0,
            half);
    }
}

template <typename PlaneY, typename PlaneUV>
void pad8x8_MBAFF( PlaneY *tmpPtrY, PlaneUV *tmpPtrU, PlaneUV *tmpPtrV,
                             Ipp32s strideY, Ipp32s strideU, Ipp32s strideV,
                             Ipp32s blk8x8, Ipp32s bV0, Ipp32s bV1, Ipp32s bH, Ipp32s bC0, Ipp32s bC1 )
{
    Ipp32s    switch_flag     = ( !bV0 && bV1 && bH ) || ( !bV0 && !bH && !bC0 && ( bV1 || bC1 ) );
    Ipp32s    double_flag     = ( bV0 && bV1 ) || ( ( bV0 || bC0 ) && !bH && ( bV1 || bC1 ) );
    if ( switch_flag && double_flag ) return;
    Ipp32s    fromAbove  = ( blk8x8 < 2 );
    Ipp32s    fromLeft   = ( blk8x8 % 2 == 0 );
    Ipp32s cIdx = blk8x8;

    if( double_flag )
    {
        xpad8x8_MBAFF( tmpPtrY, tmpPtrU, tmpPtrV, strideY, strideU, strideV, cIdx, bV0, bH, bC0, true,   fromAbove, fromLeft );
        xpad8x8_MBAFF( tmpPtrY, tmpPtrU, tmpPtrV, strideY, strideU, strideV, cIdx, bV1, bH, bC1, true,  !fromAbove, fromLeft );
    }
    else if( switch_flag )
    {
        xpad8x8_MBAFF( tmpPtrY, tmpPtrU, tmpPtrV, strideY, strideU, strideV, cIdx, bV1, bH, bC1, false, !fromAbove, fromLeft );
    }
    else
    {
        xpad8x8_MBAFF( tmpPtrY, tmpPtrU, tmpPtrV, strideY, strideU, strideV, cIdx, bV0, bH, bC0, false,  fromAbove, fromLeft );
    }
}

template <typename PlaneY, typename PlaneUV>
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
            (Ipp32s)(*(((PlaneUV*)((Ipp8u*)tmpPtrU - strideU)) - 1)),
            (Ipp32s)(*(((PlaneUV*)((Ipp8u*)tmpPtrV - strideV)) - 1)), 0);
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
            diagonalConstruction(tmpPtrY + 8, strideY, tmpPtrU + 4, strideU,
                tmpPtrV + 4, strideV, 1, isAboveRightIntra, 0);
        }
        else
        {
            copyPlanesFromLeftRight(tmpPtrY + 8, strideY, tmpPtrU + 4, strideU,
                tmpPtrV + 4, strideV, 0);

        }
    }
    else if (isAboveIntra)
    {
        copyPlanesFromAboveBelow(tmpPtrY + 8, strideY, tmpPtrU + 4, strideU,
            tmpPtrV + 4, strideV, 1, 0);
    }
    else if (isAboveRightIntra)
    {
        fillPlanes(tmpPtrY + 8, strideY, tmpPtrU + 4, strideU, tmpPtrV + 4, strideV,
            (Ipp32s)(*(((PlaneY*)((Ipp8u*)tmpPtrY - strideY)) + 16)),
            (Ipp32s)(*(((PlaneUV*)((Ipp8u*)tmpPtrU - strideU)) + 8)),
            (Ipp32s)(*(((PlaneUV*)((Ipp8u*)tmpPtrV - strideV)) + 8)), 0);
    }
    else
    {
        fillPlanes(tmpPtrY + 8, strideY, tmpPtrU + 4, strideU, tmpPtrV + 4, strideV,
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
            (Ipp32s)(*(((PlaneUV*)((Ipp8u*)tmpPtrU + 4 * strideU)) - 1)),
            (Ipp32s)(*(((PlaneUV*)((Ipp8u*)tmpPtrV + 4 * strideV)) - 1)), 0);
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
            diagonalConstruction(tmpPtrY + 8, strideY, tmpPtrU + 4, strideU,
                tmpPtrV + 4, strideV, 3, isBelowRightIntra, 0);
        }
        else
        {
            copyPlanesFromLeftRight(tmpPtrY + 8, strideY, tmpPtrU + 4, strideU,
                tmpPtrV + 4, strideV, 0);

        }
    }
    else if (isBelowIntra)
    {
        copyPlanesFromAboveBelow(tmpPtrY + 8, strideY, tmpPtrU + 4, strideU,
            tmpPtrV + 4, strideV, 0, 0);
    }
    else if (isBelowRightIntra)
    {
        fillPlanes(tmpPtrY + 8, strideY, tmpPtrU + 4, strideU, tmpPtrV + 4, strideV,
            (Ipp32s)(*(((PlaneY*)((Ipp8u*)tmpPtrY + 8 * strideY)) + 16)),
            (Ipp32s)(*(((PlaneUV*)((Ipp8u*)tmpPtrU + 4 * strideU)) + 8)),
            (Ipp32s)(*(((PlaneUV*)((Ipp8u*)tmpPtrV + 4 * strideV)) + 8)), 0);
    }
    else
    {
        fillPlanes(tmpPtrY + 8, strideY, tmpPtrU + 4, strideU, tmpPtrV + 4, strideV,
            0, 0, 0, 0);
    }
}

template <typename PlaneY, typename PlaneUV>
void fillMB_MBAFF(PlaneY *ty,
                         Ipp32s tsy,
                         PlaneUV *tu,
                         Ipp32s tsu,
                         PlaneUV *tv,
                         Ipp32s tsv,
                         Ipp32s mask)
{
    Ipp32s isAvailable_TopLeft  = 0 != (mask & 0x001);
    Ipp32s isAvailable_Top      = 0 != (mask & 0x002);
    Ipp32s isAvailable_TopRight = 0 != (mask & 0x004);
    Ipp32s isAvailable_LeftTop  = 0 != (mask & 0x008);
    Ipp32s isAvailable_LeftBot  = 0 != (mask & 0x010);
    Ipp32s isAvailable_CurTop   = 0 != (mask & 0x020);
    Ipp32s isAvailable_CurBot   = 0 != (mask & 0x040);
    Ipp32s isAvailable_RightTop = 0 != (mask & 0x080);
    Ipp32s isAvailable_RightBot = 0 != (mask & 0x100);
    Ipp32s isAvailable_BotLeft  = 0 != (mask & 0x200);
    Ipp32s isAvailable_Bot      = 0 != (mask & 0x400);
    Ipp32s isAvailable_BotRight = 0 != (mask & 0x800);

    if (!mask) {
        fillPlanes_wh(ty, tsy,
            tu, tsu,
            tv, tsv,
            0, 0, 0,
            16, 16);
        return;
    }

    if( ! isAvailable_CurTop )
    {
        pad8x8_MBAFF( ty, tu, tv, tsy, tsu, tsv,
            0,
            isAvailable_Top,
            isAvailable_CurBot,
            isAvailable_LeftTop,
            isAvailable_TopLeft,
            isAvailable_LeftBot );
    }

    if( ! isAvailable_CurTop )
    {
        pad8x8_MBAFF( ty, tu, tv, tsy, tsu, tsv,
            1,
            isAvailable_Top,
            isAvailable_CurBot,
            isAvailable_RightTop,
            isAvailable_TopRight,
            isAvailable_RightBot );
    }

    if( ! isAvailable_CurBot )
    {
        pad8x8_MBAFF( ty, tu, tv, tsy, tsu, tsv,
            2,
            isAvailable_Bot,
            isAvailable_CurTop,
            isAvailable_LeftBot,
            isAvailable_BotLeft,
            isAvailable_LeftTop );
    }

    if( ! isAvailable_CurBot )
    {
        pad8x8_MBAFF( ty, tu, tv, tsy, tsu, tsv,
            3,
            isAvailable_Bot,
            isAvailable_CurTop,
            isAvailable_RightBot,
            isAvailable_BotRight,
            isAvailable_RightTop );
    }
}

template <typename PlaneIn, typename PlaneOut = PlaneIn>
class PlaneOperations
{
public:
    static void clearMB(PlaneOut  *dst,
                        Ipp32s  stride,
                        Ipp32s  sizeX,
                        Ipp32s  sizeY)
    {
        for (Ipp32s j = 0; j < sizeY; j++)
        {
            for (Ipp32s i = 0; i < sizeX; i++)
            {
                dst[i] = 0;
            }

            dst = dst + stride;
        }
    }

    static void copySatMB(PlaneIn  *src,
                          Ipp32s  strideSrc,
                          PlaneOut *dst,
                          Ipp32s  strideDst,
                          Ipp32s  sizeX,
                          Ipp32s  sizeY)
    {
        for (Ipp32s j = 0; j < sizeY; j++)
        {
            for (Ipp32s i = 0; i < sizeX; i++)
            {
                Ipp32s tmp = src[i];

                if      (tmp < 0)   dst[i] = 0;
                else if (tmp > 255) dst[i] = 255;
                else                dst[i] = (Ipp8u)tmp;
            }

            src = src + strideSrc;
            dst = dst + strideDst;
        }
    }

    static void copyMB(PlaneIn  *src,
                       Ipp32s  strideSrc,
                       PlaneOut *dst,
                       Ipp32s  strideDst,
                       Ipp32s  sizeX,
                       Ipp32s  sizeY)
    {
        for (Ipp32s j = 0; j < sizeY; j++)
        {
            for (Ipp32s i = 0; i < sizeX; i++)
            {
                dst[i] = (PlaneOut)src[i];
            }

            src = src + strideSrc;
            dst = dst + strideDst;
        }
    }

    static void copyMB_NV12(PlaneIn  *src,
                       Ipp32s  strideSrc,
                       PlaneOut  *dst,
                       Ipp32s  strideDst,
                       Ipp32s  sizeX,
                       Ipp32s  sizeY)
    {
        for (Ipp32s j = 0; j < sizeY; j++)
        {
            for (Ipp32s i = 0; i < sizeX; i++)
            {
                dst[i*2] = (PlaneOut)src[i];
            }

            src = src + strideSrc;
            dst = dst + strideDst;
        }
    }

    static void copyRef(PlaneIn  *src,
                        Ipp32s  strideSrc,
                        PlaneOut  *dst,
                        Ipp32s  strideDst,
                        Ipp32s  sizeX,
                        Ipp32s  sizeY)
    {
        for (Ipp32s j = 0; j < sizeY; j++)
        {
            for (Ipp32s i = 0; i < sizeX; i++)
            {
                dst[i] = (PlaneOut)src[i];
            }

            src = src + strideSrc;
            dst = dst + strideDst;
        }
    }
};

template <typename PlaneY=Ipp8u, typename PlaneUV=Ipp8u>
class SVCResampling
{
    typedef PlaneY * PlanePtrY;
    typedef PlaneUV * PlanePtrUV;

public:


    static void umc_h264_upsampling(H264Slice *curSlice,
                             H264DecoderLayerDescriptor *refLayer,
                             H264DecoderLayerDescriptor *curLayer,
                             H264DecoderFrame *pFrame,
                             Ipp32s  *tmpBuf0,
                             Ipp32s  *tmpBuf1,
                             Ipp32s   outLumaStride,
                             Ipp32s   outChromaStride,
                             Ipp32s   refLumaStride,
                             Ipp32s   refChromaStride,
                             Ipp32s   nv12)
    {
        const H264SeqParamSet *curSps = curSlice->GetSeqParam();
        H264DecoderLayerResizeParameter *resizeParams = curLayer->m_pResizeParameter;

        Ipp32s baseFieldFlag  = refLayer->frame_mbs_only_flag ? 0 : 1;
        Ipp32s curFieldFlag = (refLayer->frame_mbs_only_flag && curLayer->frame_mbs_only_flag) ? 0 : 1;

        if (nv12) nv12 = 1;

        PlaneY  *pOutYPlane = (PlanePtrY)pFrame->m_pYPlane;
        PlaneUV *pOutUPlane = nv12 ? (PlanePtrUV)pFrame->m_pUVPlane : (PlanePtrUV)pFrame->m_pUPlane;
        PlaneUV *pOutVPlane = nv12 ? (PlanePtrUV)pFrame->m_pUVPlane : (PlanePtrUV)pFrame->m_pVPlane;

        PlaneY  *pRefYPlane = (PlanePtrY) refLayer->m_pYPlane;
        PlaneUV *pRefUPlane = (PlanePtrUV)refLayer->m_pUPlane;
        PlaneUV *pRefVPlane = (PlanePtrUV)refLayer->m_pVPlane;

        Ipp32s frameBasedResamplingFlag   = curLayer->frame_mbs_only_flag && refLayer->frame_mbs_only_flag;
        Ipp32s verticalInterpolationFlag  = !frameBasedResamplingFlag && !curLayer->field_pic_flag;
        Ipp32s yBorder = verticalInterpolationFlag ? 2 : 0;

        Ipp32s phaseX = resizeParams->phaseX;
        Ipp32s phaseY = resizeParams->phaseY;
        Ipp32s refPhaseX = resizeParams->refPhaseX;
        Ipp32s refPhaseY = resizeParams->refPhaseY;

        Ipp32s scaledW = resizeParams->scaled_ref_layer_width;
        Ipp32s scaledH = resizeParams->scaled_ref_layer_height;

        if (!curLayer->frame_mbs_only_flag && refLayer->frame_mbs_only_flag)
            scaledH >>= 1;

        Ipp32s leftOffset = resizeParams->leftOffset;
        Ipp32s topOffset = resizeParams->topOffset >> curFieldFlag;
        Ipp32s rightOffset = resizeParams->rightOffset;
        Ipp32s bottomOffset = resizeParams->bottomOffset >> curFieldFlag;

        Ipp32s refW = resizeParams->ref_layer_width;
        Ipp32s refH = resizeParams->ref_layer_height;

        Ipp32s shiftX = ((resizeParams->level_idc <= 30) ? 16 : countShift(refW));
        Ipp32s shiftY = ((resizeParams->level_idc <= 30) ? 16 : countShift(refH));

        Ipp32s scaleX = (((Ipp32u)refW << shiftX) + (scaledW >> 1)) / scaledW;
        Ipp32s scaleY = (((Ipp32u)refH << shiftY) + (scaledH >> 1)) / scaledH;

        Ipp32s bottom_field_flag = refLayer->frame_mbs_only_flag ? 0 : curLayer->field_pic_flag ? curLayer->bottom_field_flag : refLayer->field_pic_flag ? refLayer->bottom_field_flag : 0;
            
        if (curLayer->frame_mbs_only_flag == 0 || refLayer->frame_mbs_only_flag == 0)
        {
            if (refLayer->frame_mbs_only_flag)
            {
                phaseY    = phaseY + 4 * bottom_field_flag + 2;
                refPhaseY = 2 * refPhaseY + 2;
            }
            else
            {
                phaseY    = phaseY    + 4 * bottom_field_flag;
                refPhaseY = refPhaseY + 4 * bottom_field_flag;
            }
        }

        Ipp32s addX = ((refW << (shiftX - 1)) + (scaledW >> 1)) / scaledW + (1 << (shiftX - 5));
        Ipp32s addY = ((refH << (shiftY - 1)) + (scaledH >> 1)) / scaledH + (1 << (shiftY - 5));
        Ipp32s deltaY = 8;
        Ipp32s deltaX = 8;

        if (!curLayer->frame_mbs_only_flag || !refLayer->frame_mbs_only_flag)
        {
            addY   = (((refH * (2 + phaseY) ) << (shiftY - 3)) + (scaledH >> 1) ) / scaledH + (1 << (shiftY - 5) );
            deltaY = 2 * (2 + refPhaseY);
        }

        /* Luma */
        Ipp32s startX = 0;
        if (leftOffset < 0)
            startX = -leftOffset;

        Ipp32s endX = scaledW;
        if (rightOffset < 0)
            endX = scaledW + rightOffset;

        Ipp32s startY = 0;
        if (topOffset < 0)
            startY = -topOffset;

        Ipp32s endY = resizeParams->scaled_ref_layer_height >> curFieldFlag;
        if (bottomOffset < 0)
            endY = (resizeParams->scaled_ref_layer_height >> curFieldFlag) + bottomOffset;

        Ipp32u dstStride = (endX + 4)* 4;
#if 0
        bicubicLuma(refW, refH >> baseFieldFlag, refLumaStride << baseFieldFlag, pRefYPlane,
                    startX, endX, startY, endY,
                    dstStride, tmpBuf0, scaleX, addX, shiftX, deltaX,
                    scaleY, addY, shiftY, deltaY, tmpBuf1,
                    yBorder, false);

        saturateAndBorder<Ipp8u>(endX - startX, (endY - startY) << curFieldFlag,
                      pOutYPlane, outLumaStride,
                      tmpBuf0, dstStride,
                      startX + leftOffset, scaledW + rightOffset - endX,(startY + topOffset) << curFieldFlag, resizeParams->scaled_ref_layer_height + ((bottomOffset - endY) << curFieldFlag),
                      verticalInterpolationFlag, yBorder, 0);
#else

        Ipp32s offsetX = leftOffset;
        Ipp32s offsetY = topOffset;

        dstStride = (resizeParams->cur_layer_width + 4)* 4;

        bicubicPlane(refW, refH >> baseFieldFlag,
                        refLumaStride << baseFieldFlag, pRefYPlane,
                        leftOffset, topOffset, rightOffset, bottomOffset,
                        resizeParams->cur_layer_width, resizeParams->cur_layer_height >> curFieldFlag,
                        dstStride, tmpBuf0,
                        scaleX, addX, shiftX, deltaX, offsetX,
                        scaleY, addY, shiftY, deltaY, offsetY,
                        tmpBuf1,
                        yBorder, 0);

        saturateAndBorder1<Ipp8u>(resizeParams->cur_layer_width, resizeParams->cur_layer_height,
                      pOutYPlane, outLumaStride,
                      tmpBuf0, dstStride,
                      leftOffset, rightOffset, topOffset, bottomOffset,
                      verticalInterpolationFlag, yBorder, bottom_field_flag, 0, 0);
#endif

        phaseX = resizeParams->phaseX;
        phaseY = resizeParams->phaseY;
        refPhaseX = resizeParams->refPhaseX;
        refPhaseY = resizeParams->refPhaseY;
        scaledH = resizeParams->scaled_ref_layer_height;

    /* Chroma */

        yBorder = verticalInterpolationFlag ? 1 : 0;

        Ipp32s chromaShiftW = (3 > curSps->chroma_format_idc) ? 1 : 0;
        Ipp32s chromaShiftH = (2 > curSps->chroma_format_idc) ? 1 : 0;

        leftOffset >>= chromaShiftW;
        rightOffset >>= chromaShiftW;
        refW >>= chromaShiftW;
        scaledW >>= chromaShiftW;
        topOffset >>= chromaShiftH;
        bottomOffset >>= chromaShiftH;
        refH >>= chromaShiftH;
        scaledH >>= chromaShiftH;

        Ipp32s scaledH_ini = scaledH;

        if ((curLayer->frame_mbs_only_flag == 0) && (refLayer->frame_mbs_only_flag == 1))
        {
            scaledH >>= 1;
        }

        shiftX = ((curSps->level_idc <= 30) ? 16 : countShift(refW));
        shiftY = ((curSps->level_idc <= 30) ? 16 : countShift(refH));

        scaleX = (((Ipp32u)refW << shiftX) + (scaledW >> 1)) / scaledW;
        scaleY = (((Ipp32u)refH << shiftY) + (scaledH >> 1)) / scaledH;

        addX = (((refW * (2 + phaseX)) << (shiftX - 2)) + (scaledW >> 1)) / scaledW +
               (1 << (shiftX - 5));

        addY = (((refH * (2 + phaseY)) << (shiftY - 2)) + (scaledH >> 1)) / scaledH +
               (1 << (shiftY - 5));

        if ((curLayer->frame_mbs_only_flag == 0) || (refLayer->frame_mbs_only_flag == 0))
        {
            if (refLayer->frame_mbs_only_flag)
            {
                phaseY       = phaseY + 4 * bottom_field_flag + 1;
                refPhaseY    = 2 * refPhaseY + 2;
            }
            else
            {
                phaseY       = phaseY    + 4 * bottom_field_flag;
                refPhaseY    = refPhaseY + 4 * bottom_field_flag;
            }
        }

        deltaX = 4 * (2 + refPhaseX);
        deltaY = 4 * (2 + refPhaseY);

        if (curLayer->frame_mbs_only_flag == 0 || refLayer->frame_mbs_only_flag == 0)
        {
            addY   = ((( refH * (2 + phaseY) ) << (shiftY - 3) ) + (scaledH >> 1) ) / scaledH + (1 << (shiftY - 5));
            deltaY = 2 * ( 2 + refPhaseY );
        }

        startX = 0;
        if (leftOffset < 0)
            startX = -leftOffset;

        endX = scaledW;
        if (rightOffset < 0)
            endX = scaledW + rightOffset;

        startY = 0;
        if (topOffset < 0)
            startY = -topOffset;

        endY = scaledH_ini >> curFieldFlag;
        if (bottomOffset < 0)
            endY = (scaledH_ini >> curFieldFlag) + bottomOffset;

#if 0
        dstStride = (endX)* 4;
        bilinearChroma(refW, refH >> baseFieldFlag, refChromaStride << baseFieldFlag, pRefUPlane,
                       startX, endX, startY, endY, outChromaStride,
                       pOutUPlane + (leftOffset << nv12) + (topOffset << curFieldFlag) * outChromaStride,
                       scaleX, addX, shiftX, deltaX,
                       scaleY, addY, shiftY, deltaY, tmpBuf0, tmpBuf1, dstStride,
                       verticalInterpolationFlag, yBorder, nv12);

        saturateAndBorder(endX - startX, (endY - startY) << curFieldFlag, 
               pOutUPlane, outChromaStride,
               0, 0,
               startX + leftOffset, scaledW + rightOffset - endX,
               (startY + topOffset) << curFieldFlag,
               scaledH_ini + ((bottomOffset - endY) << curFieldFlag),
               0, 0,
               nv12);

        bilinearChroma(refW, refH >> baseFieldFlag, refChromaStride << baseFieldFlag, pRefVPlane,
                       startX, endX, startY, endY, outChromaStride,
                       pOutVPlane + nv12 + (leftOffset << nv12) + (topOffset << curFieldFlag) * outChromaStride,
                       scaleX, addX, shiftX, deltaX,
                       scaleY, addY, shiftY, deltaY, tmpBuf0, tmpBuf1, dstStride,
                       verticalInterpolationFlag, yBorder, nv12);

        saturateAndBorder(endX - startX, (endY - startY) << curFieldFlag, 
               pOutVPlane + nv12, outChromaStride,
               0, 0,
               startX + leftOffset, scaledW + rightOffset - endX,
               (startY + topOffset) << curFieldFlag,
               scaledH_ini + ((bottomOffset - endY) << curFieldFlag),
               0, 0,
               nv12);
#else

        offsetX = leftOffset;
        offsetY = topOffset;

        dstStride = (resizeParams->cur_layer_width + 4)* 4;

        bicubicPlane(refW, refH >> baseFieldFlag,
                        refChromaStride << baseFieldFlag, pRefUPlane,
                        leftOffset, topOffset, rightOffset, bottomOffset,
                        resizeParams->cur_layer_width >> chromaShiftW, (resizeParams->cur_layer_height >> curFieldFlag) >> chromaShiftH,
                        dstStride, tmpBuf0,
                        scaleX, addX, shiftX, deltaX, offsetX,
                        scaleY, addY, shiftY, deltaY, offsetY,
                        tmpBuf1,
                        yBorder, 1);

        saturateAndBorder1<Ipp8u>(resizeParams->cur_layer_width >> chromaShiftW, resizeParams->cur_layer_height >> chromaShiftH,
                      pOutUPlane, outChromaStride,
                      tmpBuf0, dstStride,
                      leftOffset, rightOffset, topOffset, bottomOffset,
                      verticalInterpolationFlag, yBorder, bottom_field_flag, 1, nv12);

        bicubicPlane(refW, refH >> baseFieldFlag,
                        refChromaStride << baseFieldFlag, pRefVPlane,
                        leftOffset, topOffset, rightOffset, bottomOffset,
                        resizeParams->cur_layer_width >> chromaShiftW, (resizeParams->cur_layer_height >> curFieldFlag) >> chromaShiftH,
                        dstStride, tmpBuf0,
                        scaleX, addX, shiftX, deltaX, offsetX,
                        scaleY, addY, shiftY, deltaY, offsetY,
                        tmpBuf1,
                        yBorder, 1);

        saturateAndBorder1<Ipp8u>(resizeParams->cur_layer_width >> chromaShiftW, resizeParams->cur_layer_height >> chromaShiftH,
                      pOutVPlane + nv12, outChromaStride,
                      tmpBuf0, dstStride,
                      leftOffset, rightOffset, topOffset, bottomOffset,
                      verticalInterpolationFlag, yBorder, bottom_field_flag, 1, nv12);
#endif
    }

    static void interpolateInterMBs(PlaneY  *srcdstY,
                             Ipp32s  strideY,
                             PlaneUV *srcdstU,
                             Ipp32s  strideU,
                             PlaneUV *srcdstV,
                             Ipp32s  strideV,
                             Ipp32s  startH,
                             Ipp32s  endH,
                             Ipp32s  widthMB,
                             Ipp32s  heightMB,
                             H264DecoderMacroblockGlobalInfo *gmbs,
                             H264DecoderLayerDescriptor *layer,
                             Ipp32s  curSliceNum)
    {
        H264DecoderMacroblockLayerInfo *mbs = layer->mbs;

        endH = endH >> layer->field_pic_flag;
        heightMB = heightMB >> layer->field_pic_flag;

        Ipp32s refMbAddr = startH * widthMB;
        if (layer->field_pic_flag && layer->bottom_field_flag)
        {
            Ipp32s mbOffset = widthMB*heightMB;
            refMbAddr += mbOffset;
        }

        strideY = strideY << layer->field_pic_flag;
        strideU = strideU << layer->field_pic_flag;
        strideV = strideV << layer->field_pic_flag;

        for (Ipp32s mb_y = startH; mb_y < endH; mb_y++)
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

                if (!layer->field_pic_flag && !layer->frame_mbs_only_flag)
                {
                    Ipp32s  mb_y0 = (mb_y >> 1) << 1;
                    Ipp32s botMBAddr = layer->is_MbAff ? 1 : widthMB;

                    Ipp32s mbaffRefAddr = (mb_y/2 * widthMB + mb_x) * 2 + (mb_y & 1);
                    refMbAddr = layer->is_MbAff ?  mbaffRefAddr : mb_y0*widthMB + mb_x;
                    Ipp32s xOffset = layer->is_MbAff ? 2 : 1;

                    PlaneY *ty = tmpPtrY;
                    PlaneUV *tu = tmpPtrU;
                    PlaneUV *tv = tmpPtrV;

                    Ipp32s tsy = strideY;
                    Ipp32s tsu = strideU;
                    Ipp32s tsv = strideV;

                    {
                        if (mbaffRefAddr & 1)
                        {
                            ty -= 15 * strideY;
                            tu -= 7 * strideU;
                            tv -= 7 * strideV;
                        }
                        tsy <<= 1;
                        tsu <<= 1;
                        tsv <<= 1;
                    }

                    Ipp32s  isAvailable_TopLeft = 0;
                    Ipp32s  isAvailable_Top = 0;
                    Ipp32s  isAvailable_TopRight = 0;
                    Ipp32s  isAvailable_LeftTop = 0;
                    Ipp32s  isAvailable_LeftBot = 0;
                    Ipp32s  isAvailable_CurTop = 0;
                    Ipp32s  isAvailable_CurBot = 0;
                    Ipp32s  isAvailable_RightTop = 0;
                    Ipp32s  isAvailable_RightBot = 0;
                    Ipp32s  isAvailable_BotLeft = 0;
                    Ipp32s  isAvailable_Bot = 0;
                    Ipp32s  isAvailable_BotRight = 0;

                    Ipp32s  fieldOffset = (mb_y - mb_y0);

                    Ipp32s  isPairAvailable_Top   = (mb_y0 > 0 );
                    Ipp32s  isPairAvailable_Bot   = (mb_y0 < heightMB - 2 );
                    Ipp32s  isPairAvailable_Left  = (mb_x > 0 );
                    Ipp32s  isPairAvailable_Right = (mb_x < widthMB   - 1 );

                    Ipp32s pairIdxCur = refMbAddr;
                    Ipp32s pairIdxTop = pairIdxCur - widthMB * 2;
                    Ipp32s pairIdxBot = pairIdxCur + widthMB * 2;

                    {
                        Ipp32s idxCurrTop = pairIdxCur + (GetMBFieldDecodingFlag(mbs[pairIdxCur]) ? fieldOffset : 0);
                        Ipp32s idxCurrBot = pairIdxCur + (GetMBFieldDecodingFlag(mbs[pairIdxCur]) ? fieldOffset : botMBAddr);
                        isAvailable_CurTop = IS_REF_INTRA_INSLICE(idxCurrTop, curSliceNum);
                        isAvailable_CurBot = IS_REF_INTRA_INSLICE(idxCurrBot, curSliceNum);
                    }

                    Ipp32s isAvailable_mask = 0;
                    Ipp32s edge = (((mb_y >> 1) == 0) || ((mb_y >> 1) == ((heightMB >> 1) - 1)) || (mb_x == 0) || (mb_x == (widthMB - 1)));
                    Ipp32s rbIntra = (isAvailable_CurTop && isAvailable_CurBot);

                    if (isPairAvailable_Left)
                    {
                        Ipp32s idxLeftTop = pairIdxCur - xOffset + (GetMBFieldDecodingFlag(mbs[pairIdxCur - xOffset]) ? fieldOffset : 0);
                        Ipp32s idxLeftBot = pairIdxCur - xOffset + (GetMBFieldDecodingFlag(mbs[pairIdxCur - xOffset]) ? fieldOffset : botMBAddr);
                        isAvailable_LeftTop = IS_REF_INTRA_INSLICE(idxLeftTop, curSliceNum);
                        isAvailable_LeftBot = IS_REF_INTRA_INSLICE(idxLeftBot, curSliceNum);
                    }

                    if (isPairAvailable_Right)
                    {
                        Ipp32s idxRightTop = pairIdxCur + xOffset + (GetMBFieldDecodingFlag(mbs[pairIdxCur + xOffset]) ? fieldOffset : 0);
                        Ipp32s idxRightBot = pairIdxCur + xOffset + (GetMBFieldDecodingFlag(mbs[pairIdxCur + xOffset]) ? fieldOffset : botMBAddr);
                        isAvailable_RightTop = IS_REF_INTRA_INSLICE(idxRightTop, curSliceNum);
                        isAvailable_RightBot = IS_REF_INTRA_INSLICE(idxRightBot, curSliceNum);
                    }

                    if (isPairAvailable_Top)
                    {
                        {
                            Ipp32s idxTop = pairIdxTop + (GetMBFieldDecodingFlag(mbs[pairIdxTop]) ? fieldOffset : botMBAddr);
                            isAvailable_Top = IS_REF_INTRA_INSLICE(idxTop, curSliceNum);
                        }

                        if( isPairAvailable_Left )
                        {
                            Ipp32s idxTopLeft = pairIdxTop - xOffset + (GetMBFieldDecodingFlag(mbs[pairIdxTop - xOffset]) ? fieldOffset : botMBAddr);
                            isAvailable_TopLeft = IS_REF_INTRA_INSLICE(idxTopLeft, curSliceNum);
                        }

                        if( isPairAvailable_Right )
                        {
                            Ipp32s idxTopRight = pairIdxTop + xOffset + (GetMBFieldDecodingFlag(mbs[pairIdxTop + xOffset]) ? fieldOffset : botMBAddr);
                            isAvailable_TopRight = IS_REF_INTRA_INSLICE(idxTopRight, curSliceNum);
                        }
                    }

                    if (isPairAvailable_Bot)
                    {
                        {
                            Ipp32s idxBot = pairIdxBot + (GetMBFieldDecodingFlag(mbs[pairIdxBot]) ? fieldOffset : 0);
                            isAvailable_Bot = IS_REF_INTRA_INSLICE(idxBot, curSliceNum);
                        }

                        if (isPairAvailable_Left)
                        {
                            Ipp32s idxBotLeft = pairIdxBot - xOffset + (GetMBFieldDecodingFlag(mbs[pairIdxBot - xOffset]) ? fieldOffset : 0);
                            isAvailable_BotLeft = IS_REF_INTRA_INSLICE(idxBotLeft, curSliceNum);
                        }

                        if (isPairAvailable_Right)
                        {
                            Ipp32s idxBotRight = pairIdxBot + xOffset + (GetMBFieldDecodingFlag(mbs[pairIdxBot + xOffset]) ? fieldOffset : 0);
                            isAvailable_BotRight = IS_REF_INTRA_INSLICE(idxBotRight, curSliceNum);
                        }
                    }

                    isAvailable_mask |= (isAvailable_TopLeft  ? 0x001 : 0);
                    isAvailable_mask |= (isAvailable_Top      ? 0x002 : 0);
                    isAvailable_mask |= (isAvailable_TopRight ? 0x004 : 0);
                    isAvailable_mask |= (isAvailable_LeftTop  ? 0x008 : 0);
                    isAvailable_mask |= (isAvailable_LeftBot  ? 0x010 : 0);
                    isAvailable_mask |= (isAvailable_CurTop  ? 0x020 : 0);
                    isAvailable_mask |= (isAvailable_CurBot  ? 0x040 : 0);
                    isAvailable_mask |= (isAvailable_RightTop ? 0x080 : 0);
                    isAvailable_mask |= (isAvailable_RightBot ? 0x100 : 0);
                    isAvailable_mask |= (isAvailable_BotLeft  ? 0x200 : 0);
                    isAvailable_mask |= (isAvailable_Bot      ? 0x400 : 0);
                    isAvailable_mask |= (isAvailable_BotRight ? 0x800 : 0);

                    if (!rbIntra)
                    {
                        fillMB_MBAFF(ty, tsy, tu, tsu, tv, tsv, 
                            isAvailable_mask);
                    }

                    if (edge)
                    {
                        Ipp32s edgeMask;

                        if ((mb_y >> 1) == 0)
                        {
                            edgeMask = 0;

                            if (isAvailable_LeftTop)  edgeMask |= 0x200; /* isAvailable_BotLeft */
                            if (isAvailable_CurTop)   edgeMask |= 0x400; /* isAvailable_CurBot */
                            if (isAvailable_RightTop) edgeMask |= 0x800; /* isAvailable_BotRight */

                            fillMB_MBAFF(ty-16*tsy, tsy, tu-8*tsu, tsu, tv-8*tsv, tsv,
                                edgeMask);

                            if (mb_x == 0)
                            {
                                edgeMask = isAvailable_CurTop ? 0x800 : 0; /* isAvailable_BotRight */
                                fillMB_MBAFF(ty-16*tsy-16, tsy, tu-8*tsu-8, tsu, tv-8*tsv-8, tsv,
                                    edgeMask);
                            }

                            if (mb_x == widthMB - 1)
                            {
                                edgeMask = isAvailable_CurTop ? 0x200 : 0; /* isAvailable_BotLeft */;
                                fillMB_MBAFF(ty-16*tsy+16, tsy, tu-8*tsu+8, tsu, tv-8*tsv+8, tsv,
                                    edgeMask);
                            }
                        }

                        if ((mb_y >> 1) == ((heightMB >> 1) - 1))
                        {
                            edgeMask = 0;

                            if (isAvailable_LeftBot)  edgeMask |= 0x001; /* isAvailable_TopLeft */
                            if (isAvailable_CurBot)   edgeMask |= 0x002; /* isAvailable_Top */
                            if (isAvailable_RightBot) edgeMask |= 0x004; /* isAvailable_TopRight */

                            fillMB_MBAFF(ty+16*tsy, tsy, tu+8*tsu, tsu, tv+8*tsv, tsv,
                                edgeMask);

                            if (mb_x == 0)
                            {
                                edgeMask = isAvailable_CurBot ? 0x004 : 0; /* isAvailable_TopRight */
                                fillMB_MBAFF(ty+16*tsy-16, tsy, tu+8*tsu-8, tsu, tv+8*tsv-8, tsv,
                                    edgeMask);
                            }

                            if (mb_x == widthMB - 1)
                            {
                                edgeMask = isAvailable_CurBot ? 0x001 : 0; /* isAvailable_TopLeft */
                                fillMB_MBAFF(ty+16*tsy+16, tsy, tu+8*tsu+8, tsu, tv+8*tsv+8, tsv,
                                    edgeMask);
                            }
                        }

                        if (mb_x == 0)
                        {
                            edgeMask = 0;

                            if (isAvailable_Top)    edgeMask |= 0x004; /* isAboveRightIntra */
                            if (isAvailable_CurTop) edgeMask |= 0x080; /* isRightIntra */
                            if (isAvailable_CurBot) edgeMask |= 0x100; /* isBelowRightIntra */
                            if (isAvailable_Bot)    edgeMask |= 0x800; /* isBelowRightIntra */

                            fillMB_MBAFF(ty-16, tsy, tu-8, tsu, tv-8, tsv,
                                edgeMask);
                        }

                        if (mb_x == widthMB - 1)
                        {
                            edgeMask = 0;

                            if (isAvailable_Top)    edgeMask |= 0x001; /* isAvailable_TopLeft */
                            if (isAvailable_CurTop) edgeMask |= 0x008; /* isAvailable_LeftTop */
                            if (isAvailable_CurBot) edgeMask |= 0x010; /* isAvailable_LeftBot */
                            if (isAvailable_Bot)    edgeMask |= 0x200; /* isAvailable_BotLeft */

                            fillMB_MBAFF(ty+16, tsy, tu+8, tsu, tv+8, tsv,
                                edgeMask);
                        }
                    }

                    tmpPtrY += 16;
                    tmpPtrU += 8;
                    tmpPtrV += 8;
                }
                else
                {
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
                                    tmpPtrU- 8*strideU-8,  strideU,
                                    tmpPtrV- 8*strideV-8,  strideV,
                                    edgeMask);
                            }

                            if (mb_x == widthMB - 1)
                            {
                                edgeMask = isIntra ? 0x20 : 0; /* isBelowLeftIntra */;
                                fillMB(tmpPtrY-16*strideY+16, strideY,
                                    tmpPtrU- 8*strideU+8,  strideU,
                                    tmpPtrV- 8*strideV+8,  strideV,
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
                                    tmpPtrU+ 8*strideU-8,  strideU,
                                    tmpPtrV+ 8*strideV-8,  strideV,
                                    edgeMask);
                            }

                            if (mb_x == widthMB - 1)
                            {
                                edgeMask = isIntra ? 0x80 : 0; /* isAboveLeftIntra */;
                                fillMB(tmpPtrY+16*strideY+16, strideY,
                                    tmpPtrU+ 8*strideU+8,  strideU,
                                    tmpPtrV+ 8*strideV+8,  strideV,
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
                                tmpPtrU -8, strideU,
                                tmpPtrV -8, strideV,
                                edgeMask);
                        }

                        if (mb_x == widthMB - 1)
                        {
                            edgeMask = 0;

                            if (isAboveIntra) edgeMask |= 0x80; /* isAboveLeftIntra */
                            if (isIntra)      edgeMask |= 0x40; /* isLeftIntra */
                            if (isBelowIntra) edgeMask |= 0x20; /* isBelowLeftIntra */

                            fillMB(tmpPtrY+16, strideY,
                                tmpPtrU +8, strideU,
                                tmpPtrV +8, strideV,
                                edgeMask);
                        }
                    }

                    tmpPtrY += 16;
                    tmpPtrU += 8;
                    tmpPtrV += 8;
                    refMbAddr++;
                }
            }
        }
    }

    static void umc_h264_constrainedIntraUpsampling(H264Slice *curSlice,
                                             H264DecoderLayerDescriptor *curLayer,
                                             H264DecoderMacroblockGlobalInfo *gmbs,
                                             H264DecoderLayerDescriptor *refLayer,
                                             H264DecoderFrame *pFrame,
                                             H264DecoderLocalMacroblockDescriptor & mbinfo,
                                             Ipp32s   outLumaStride,
                                             Ipp32s   outChromaStride,
                                             Ipp32s   pixel_sz,
                                             Ipp32s   refLumaStride,
                                             Ipp32s   refChromaStride,
                                             Ipp32s   nv12)
    {
        Ipp32u mb_width = curSlice->GetMBWidth();
        Ipp32u mb_height = curSlice->GetMBHeight();

        Ipp32s  *tmpBuf0 = mbinfo.m_tmpBuf0;
        Ipp32s  *tmpBuf01 = mbinfo.m_tmpBuf01;
        Ipp32s  *tmpBuf1 = mbinfo.m_tmpBuf1;
        Ipp32s  *tmpBuf2 = mbinfo.m_tmpBuf2 + mb_width*mb_height;
        Ipp32s  *sliceNumBuf = mbinfo.m_tmpBuf2;

        const H264SeqParamSet *curSps = curSlice->GetSeqParam();
        H264DecoderLayerResizeParameter *resizeParams = curLayer->m_pResizeParameter;

        Ipp32s baseFieldFlag  = refLayer->frame_mbs_only_flag ? 0 : 1;
        Ipp32s curFieldFlag = (refLayer->frame_mbs_only_flag && curLayer->frame_mbs_only_flag) ? 0 : 1;

        Ipp32s firstMB, lastMB, firstRefMB, lastRefMB;
        Ipp32s MBAddr;
        Ipp32s startXLuma, endXLuma, startXChroma, endXChroma;
        PlaneY  *pYPlane;
        PlaneUV *pUPlane, *pVPlane, *ptrChromaRef, *prtOutChroma;

        if (nv12) nv12 = 1;

        PlaneY  *pOutYPlane = (PlanePtrY)pFrame->m_pYPlane;
        PlaneUV *pOutUPlane = nv12 ? (PlanePtrUV)pFrame->m_pUVPlane : (PlanePtrUV)pFrame->m_pUPlane;
        PlaneUV *pOutVPlane = nv12 ? (PlanePtrUV)pFrame->m_pUVPlane : (PlanePtrUV)pFrame->m_pVPlane;

        PlaneY  *pRefYPlane = (PlanePtrY) refLayer->m_pYPlane;
        PlaneUV *pRefUPlane = (PlanePtrUV)refLayer->m_pUPlane;
        PlaneUV *pRefVPlane = (PlanePtrUV)refLayer->m_pVPlane;

        Ipp32s frameBasedResamplingFlag   = curLayer->frame_mbs_only_flag && refLayer->frame_mbs_only_flag;
        Ipp32s verticalInterpolationFlag  = !frameBasedResamplingFlag && !curLayer->field_pic_flag;
        Ipp32s yBorder = verticalInterpolationFlag ? 2 : 0;

        Ipp32s phaseX = resizeParams->phaseX;
        Ipp32s phaseY = resizeParams->phaseY;
        Ipp32s refPhaseX = resizeParams->refPhaseX;
        Ipp32s refPhaseY = resizeParams->refPhaseY;
        Ipp32s leftOffsetLuma = resizeParams->leftOffset;
        Ipp32s topOffsetLuma = resizeParams->topOffset >> curFieldFlag;
        Ipp32s rightOffsetLuma = resizeParams->rightOffset;
        Ipp32s bottomOffsetLuma = resizeParams->bottomOffset >> curFieldFlag;

        Ipp32s scaledWLuma = resizeParams->scaled_ref_layer_width;
        Ipp32s scaledHLuma = resizeParams->scaled_ref_layer_height;

        if (!curLayer->frame_mbs_only_flag && refLayer->frame_mbs_only_flag)
            scaledHLuma >>= 1;

        Ipp32s refWLuma = resizeParams->ref_layer_width;
        Ipp32s refHLuma = resizeParams->ref_layer_height;

        Ipp32s shiftXLuma = ((resizeParams->level_idc <= 30) ? 16 : countShift(refWLuma));
        Ipp32s shiftYLuma = ((resizeParams->level_idc <= 30) ? 16 : countShift(refHLuma));

        Ipp32s scaleXLuma = resizeParams->scaleX;
        Ipp32s scaleYLuma = resizeParams->scaleY;

        Ipp32s bottom_field_flag = refLayer->frame_mbs_only_flag ? 0 : curLayer->field_pic_flag ? curLayer->bottom_field_flag : refLayer->field_pic_flag ? refLayer->bottom_field_flag : 0;        
        if (!curLayer->frame_mbs_only_flag || !refLayer->frame_mbs_only_flag)
        {
            if (refLayer->frame_mbs_only_flag)
            {
                phaseY    = phaseY + 4 * bottom_field_flag + 2;
                refPhaseY = 2 * refPhaseY + 2;
            }
            else
            {
                phaseY    = phaseY    + 4 * bottom_field_flag;
                refPhaseY = refPhaseY + 4 * bottom_field_flag;
            }
        }

        Ipp32s addXLuma = ((refWLuma << (shiftXLuma - 1)) + (scaledWLuma >> 1)) / scaledWLuma + (1 << (shiftXLuma- 5));
        Ipp32s addYLuma = ((refHLuma << (shiftYLuma - 1)) + (scaledHLuma >> 1)) / scaledHLuma + (1 << (shiftYLuma - 5));
        Ipp32s deltaXLuma = 8;
        Ipp32s deltaYLuma = 8;

        if (!curLayer->frame_mbs_only_flag || !refLayer->frame_mbs_only_flag)
        {
            addYLuma = (((refHLuma * (2 + phaseY) ) << (shiftYLuma - 3)) + (scaledHLuma >> 1) ) / scaledHLuma + (1 << (shiftYLuma - 5));
            deltaYLuma = 2 * (2 + refPhaseY);
        }

        Ipp32s refwidthMB = refWLuma >> 4;
        Ipp32s RefMBNum = (refHLuma >> 4) * refwidthMB;

        Ipp32s widthMB = (scaledWLuma + leftOffsetLuma + rightOffsetLuma) >> 4;
        Ipp32s heightMB = (scaledHLuma + topOffsetLuma + bottomOffsetLuma) >> 4;
        Ipp32s MBNum =   heightMB * widthMB;

        /* To Do - fild to fild, fild to frame */

        /* Chroma */

        Ipp32s chromaShiftW = (3 > curSps->chroma_format_idc) ? 1 : 0;
        Ipp32s chromaShiftH = (2 > curSps->chroma_format_idc) ? 1 : 0;

        Ipp32s leftOffsetChroma = leftOffsetLuma >> chromaShiftW;
        Ipp32s rightOffsetChroma = rightOffsetLuma >> chromaShiftW;
        Ipp32s refWChroma = refWLuma >> chromaShiftW;
        Ipp32s scaledWChroma = scaledWLuma >> chromaShiftW;
        Ipp32s topOffsetChroma = topOffsetLuma >> chromaShiftH;
        Ipp32s bottomOffsetChroma = bottomOffsetLuma >> chromaShiftH;
        Ipp32s refHChroma = refHLuma >> chromaShiftH;
        Ipp32s scaledHChroma = scaledHLuma >> chromaShiftH;

        Ipp32s shiftXChroma = ((curSps->level_idc <= 30) ? 16 : countShift(refWChroma));
        Ipp32s shiftYChroma = ((curSps->level_idc <= 30) ? 16 : countShift(refHChroma));

        Ipp32s scaleXChroma = (((Ipp32u)refWChroma << shiftXChroma) + (scaledWChroma >> 1)) / scaledWChroma;
        Ipp32s scaleYChroma = (((Ipp32u)refHChroma << shiftYChroma) + (scaledHChroma >> 1)) / scaledHChroma;

        Ipp32s addXChroma = (((refWChroma * (2 + phaseX)) << (shiftXChroma - 2)) + (scaledWChroma >> 1)) /
                        scaledWChroma + (1 << (shiftXChroma - 5));

        Ipp32s addYChroma = (((refHChroma * (2 + phaseY)) << (shiftYChroma - 2)) + (scaledHChroma >> 1)) /
                        scaledHChroma + (1 << (shiftYChroma - 5));

        Ipp32s deltaXChroma = 4 * (2 + refPhaseX);
        Ipp32s deltaYChroma = 4 * (2 + refPhaseY);

        if (!curLayer->frame_mbs_only_flag || !refLayer->frame_mbs_only_flag)
        {
            addYChroma = (((refHChroma * (2 + phaseY)) << (shiftYChroma - 3)) + (scaledHChroma >> 1)) /
                        scaledHChroma + (1 << (shiftYChroma - 5));
            deltaYChroma = 2 * (2 + refPhaseY);
        }

        firstMB = 0;
        lastRefMB = firstRefMB = 0;

        pYPlane = (PlaneY*)tmpBuf2;
        pUPlane = (PlaneUV*)((Ipp8u*)pYPlane + (refWLuma + 32) * (refHLuma + 64) * pixel_sz);
        pVPlane = (PlaneUV*)((Ipp8u*)pUPlane + (refWChroma + 16) * (refHChroma + 32) * pixel_sz);

        pYPlane += 32 * (refWLuma + 32) + 16; // padding
        pUPlane += 16 * (refWChroma + 16) + 8; // padding
        pVPlane += 16 * (refWChroma + 16) + 8; // padding

        MBAddr = 0;
        for (Ipp32s j = 0; j < heightMB * 16; j += 16)
        {
            for (Ipp32s i = 0; i < widthMB * 16; i += 16)
            {
                if (sliceNumBuf[MBAddr] < 0)
                {
                    Ipp32s X, Y;
                    PlaneY *dstY = ((PlaneY*)((Ipp8u*)pOutYPlane + j * outLumaStride)) + i;
                    PlaneUV *dstU, *dstV;

                    X = i >> chromaShiftW;
                    Y = j >> chromaShiftH;

                    PlaneOperations<PlaneY>::clearMB(dstY, outLumaStride, 16, 16);
                    if (nv12) {
                        dstU = ((PlaneUV*)((Ipp8u*)pOutUPlane + Y * outChromaStride)) + (X << nv12);
                        PlaneOperations<PlaneUV>::clearMB(dstU, outChromaStride, 16 >> chromaShiftW << nv12, 16 >> chromaShiftH);
                    } else {
                        dstU = ((PlaneUV*)((Ipp8u*)pOutUPlane + Y * outChromaStride)) + X;
                        dstV = ((PlaneUV*)((Ipp8u*)pOutVPlane + Y * outChromaStride)) + X;
                        PlaneOperations<PlaneUV>::clearMB(dstU, outChromaStride, 16 >> chromaShiftW, 16 >> chromaShiftH);
                        PlaneOperations<PlaneUV>::clearMB(dstV, outChromaStride, 16 >> chromaShiftW, 16 >> chromaShiftH);
                    }
                }
                MBAddr++;
            }
        }

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
            Ipp32s startY, endY, curSliceNum;
            Ipp32s startYMB, endYMB;
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
            if (startY < 0)
                startY = 0;

            endY = ((lastRefMB + refwidthMB - 1)/refwidthMB) + 1;
            if (endY >= (refHLuma >> 4))
                endY = (refHLuma >> 4);

            for (Ipp32s i = 0; i < refHLuma; i++)
            {
                MFX_INTERNAL_CPY(pYPlane + i * (refWLuma + 32) * pixel_sz,
                       pRefYPlane + i * refLumaStride,
                       refWLuma * pixel_sz);
            }

            for (Ipp32s i = 0; i < refHChroma; i++)
            {
    /*            if (nv12) {
                    PlaneUV *u, *v, *uvref;
                    u = pUPlane + i * refWChroma * pixel_sz;
                    v = pVPlane + i * refWChroma * pixel_sz;
                    uvref = pRefUPlane + i * refChromaStride;
                    for (Ipp32s ii = 0; ii < refWChroma; ii++) {
                        u[ii] = uvref[2*ii];
                        v[ii] = uvref[2*ii+1];
                    }
                } else*/ {
                    MFX_INTERNAL_CPY(pUPlane + i * (refWChroma + 16) * pixel_sz,
                        pRefUPlane + i * refChromaStride,
                        refWChroma * pixel_sz);

                    MFX_INTERNAL_CPY(pVPlane + i * (refWChroma + 16) * pixel_sz,
                        pRefVPlane + i * refChromaStride,
                        refWChroma * pixel_sz);
                }
            }

            interpolateInterMBs(pYPlane, ((refWLuma + 32) * pixel_sz),
                pUPlane, (refWChroma + 16) * pixel_sz,
                pVPlane, (refWChroma + 16) * pixel_sz,
                0, (refHLuma >> 4),
                refWLuma >> 4, (refHLuma >> 4),
                gmbs, refLayer, curSliceNum);

             /* find the first and last dst row */

            lastMB = firstMB;

            for (Ipp32s i = firstMB; i < MBNum; i++)
            {
                if (sliceNumBuf[i] == curSliceNum)
                {
                    lastMB = i;
                }
            }

            lastMB += 1;

            /* Luma */

            startYMB = firstMB / widthMB;
            endYMB = (lastMB + widthMB - 1) / widthMB;

            startY = startYMB * 16;
            endY = endYMB * 16;

            startY -= topOffsetLuma;
            if (startY < 0)
                startY = 0;

            endY -= topOffsetLuma;

            if (endY > scaledHLuma)
                endY = scaledHLuma;

            if (endY > scaledHLuma + bottomOffsetLuma)
                endY = scaledHLuma + bottomOffsetLuma;

#if 0
            bicubicLuma(refWLuma, refHLuma >> baseFieldFlag, ((refWLuma + 32) * pixel_sz) << baseFieldFlag, pYPlane,
                        startXLuma, endXLuma, startY, endY, widthMB * 4 * 16,
                        tmpBuf0 + leftOffsetLuma + topOffsetLuma * widthMB * 16,
                        scaleXLuma, addXLuma, shiftXLuma, deltaXLuma,
                        scaleYLuma, addYLuma, shiftYLuma, deltaYLuma, tmpBuf1,
                        0, false);

            startY = startYMB * 16;
            endY = endYMB * 16;

            Ipp32s tmpTopOffset = topOffsetLuma - startY;
            if (tmpTopOffset < 0) tmpTopOffset = 0;

            Ipp32s tmpBottomOffset = endY - (scaledHLuma + topOffsetLuma);
            if (tmpBottomOffset < 0) tmpBottomOffset = 0;

            saturateAndBorder(endXLuma - startXLuma, (endY - startY - tmpTopOffset - tmpBottomOffset) << curFieldFlag,
                   tmpBuf0 + startY * widthMB * 16, widthMB * 16,
                   0, 0,
                   startXLuma + leftOffsetLuma,
                   scaledWLuma + rightOffsetLuma - endXLuma,
                   tmpTopOffset << curFieldFlag, tmpBottomOffset << curFieldFlag,
                   verticalInterpolationFlag, yBorder, 0);

            Ipp32s dstStride = widthMB * 16 * 4;
#else

            Ipp32s offsetX = leftOffsetLuma;
            Ipp32s offsetY = topOffsetLuma;

            Ipp32s dstStride = (resizeParams->cur_layer_width + 4)* 4;

            bicubicPlane(refWLuma, refHLuma >> baseFieldFlag,
                            refLumaStride << baseFieldFlag, pYPlane,
                            leftOffsetLuma, topOffsetLuma, rightOffsetLuma, bottomOffsetLuma,
                            resizeParams->cur_layer_width, resizeParams->cur_layer_height >> curFieldFlag,
                            dstStride, tmpBuf0,
                            scaleXLuma, addXLuma, shiftXLuma, deltaXLuma, offsetX,
                            scaleYLuma, addYLuma, shiftYLuma, deltaYLuma, offsetY,
                            tmpBuf1,
                            yBorder, 0);

            saturateAndBorder1<Ipp8u>(resizeParams->cur_layer_width, resizeParams->cur_layer_height,
                          (Ipp8u*)tmpBuf01, outLumaStride,
                          tmpBuf0, dstStride,
                          leftOffsetLuma, rightOffsetLuma, topOffsetLuma, bottomOffsetLuma,
                          verticalInterpolationFlag, yBorder, bottom_field_flag, 0, 0);

            startY = startYMB * 16;
            endY = endYMB * 16;
#endif

            MBAddr = startYMB * widthMB;
            for (Ipp32s j = startY; j < endY; j += 16)
            {
                for (Ipp32s i = 0; i < widthMB * 16; i += 16)
                {
                    if (sliceNumBuf[MBAddr] == curSliceNum)
                    {
                        Ipp8u *src = (Ipp8u*)tmpBuf01 + j * outLumaStride + i;
                        PlaneY *dst = ((PlaneY*)((Ipp8u*)pOutYPlane + j * outLumaStride)) + i;

                        PlaneOperations<Ipp8u, PlaneY>::copySatMB(src, outLumaStride, dst, outLumaStride, 16, 16);
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
                endY = endYMB << (4 - chromaShiftH);

                startY -= topOffsetChroma;
                if (startY < 0)
                    startY = 0;

                endY -= topOffsetChroma;

                if (endY > scaledHChroma)
                    endY = scaledHChroma;

                if (endY > scaledHChroma + bottomOffsetChroma)
                    endY = scaledHChroma + bottomOffsetChroma;


                /*bilinearChromaConstrained(refWChroma, refHChroma, (refWChroma + 16) * pixel_sz, ptrChromaRef,
                                          startXChroma, endXChroma, startY, endY, widthMB << (6 - chromaShiftW),
                                          tmpBuf0 + leftOffsetChroma + topOffsetChroma * (widthMB << (4 - chromaShiftW)),
                                          scaleXChroma, addXChroma, shiftXChroma, deltaXChroma,
                                          scaleYChroma, addYChroma, shiftYChroma, deltaYChroma, tmpBuf1);*/

#if 0
                Ipp32s dstStride = widthMB << (6 - chromaShiftW);
                bicubicLuma(refWChroma, refHChroma, (refWChroma + 16) * pixel_sz, ptrChromaRef,
                                          startXChroma, endXChroma, startY, endY, dstStride,
                                          tmpBuf0 + leftOffsetChroma + topOffsetChroma * (widthMB << (4 - chromaShiftW)),
                                          scaleXChroma, addXChroma, shiftXChroma, deltaXChroma,
                                          scaleYChroma, addYChroma, shiftYChroma, deltaYChroma, tmpBuf1, 0, true);

                startY = startYMB << (4 - chromaShiftH);
                endY = endYMB << (4 - chromaShiftH);

                tmpTopOffset = topOffsetChroma - startY;
                if (tmpTopOffset < 0) tmpTopOffset = 0;

                tmpBottomOffset = endY - (scaledHChroma + topOffsetChroma);
                if (tmpBottomOffset < 0) tmpBottomOffset = 0;

                saturateAndBorder(scaledWChroma, endY - startY - tmpTopOffset - tmpBottomOffset,
                       tmpBuf0 + startY * (widthMB << (4 - chromaShiftW)), dstStride,
                       0, 0,
                       startXChroma + leftOffsetChroma,
                       scaledWChroma + rightOffsetChroma - endXChroma,
                       tmpTopOffset, tmpBottomOffset, verticalInterpolationFlag, yBorder,  0);
#else

                Ipp32s offsetXChroma = leftOffsetChroma;
                Ipp32s offsetYChroma = topOffsetChroma;

                Ipp32s dstStride = widthMB << (6 - chromaShiftW);
                //Ipp32s dstStride = (resizeParams->cur_layer_width + 4)* 4;

                bicubicPlane(refWChroma, refHChroma >> baseFieldFlag,
                                refChromaStride << baseFieldFlag, ptrChromaRef,
                                leftOffsetChroma, topOffsetChroma, rightOffsetChroma, bottomOffsetChroma,
                                resizeParams->cur_layer_width >> chromaShiftW, (resizeParams->cur_layer_height >> curFieldFlag) >> chromaShiftH,
                                dstStride, tmpBuf0,
                                scaleXChroma, addXChroma, shiftXChroma, deltaXChroma, offsetXChroma,
                                scaleYChroma, addYChroma, shiftYChroma, deltaYChroma, offsetYChroma,
                                tmpBuf1,
                                yBorder, 1);

                saturateAndBorder1<Ipp8u>(resizeParams->cur_layer_width >> chromaShiftW, resizeParams->cur_layer_height >> chromaShiftH,
                              (Ipp8u*)tmpBuf01, outChromaStride,
                              tmpBuf0, dstStride,
                              leftOffsetChroma, rightOffsetChroma, topOffsetChroma, bottomOffsetChroma,
                              verticalInterpolationFlag, yBorder, bottom_field_flag, 1, 0);

                startY = startYMB << (4 - chromaShiftH);
                endY = endYMB << (4 - chromaShiftH);
#endif

                MBAddr = startYMB * widthMB;
                for (Ipp32s j = startY; j < endY; j += (1 << (4 - chromaShiftH)))
                {
                    for (Ipp32s i = 0; i < widthMB << (4 - chromaShiftW); i += (1 << (4 - chromaShiftW)))
                    {
                        if (sliceNumBuf[MBAddr] == curSliceNum)
                        {
                            Ipp8u *src = (Ipp8u*)tmpBuf01 + j * outChromaStride + i;
                            PlaneY *dst = ((PlaneUV*)((Ipp8u*)prtOutChroma + j * outChromaStride)) + (i << nv12);

                            if (nv12) {
                                PlaneOperations<Ipp8u, PlaneUV>::copyMB_NV12(src, outChromaStride, dst, outChromaStride,
                                    16 >> chromaShiftW, 16 >> chromaShiftH);
                            } else {
                                PlaneOperations<Ipp8u, PlaneUV>::copyMB(src, outChromaStride, dst, outChromaStride,
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
}; // class 


extern void umc_h264_mv_resampling(H264SegmentDecoderMultiThreaded *sd,
                            H264Slice *curSlice,
        H264DecoderLayerDescriptor *refLayerMb,
        H264DecoderLayerDescriptor *curLayerMb,
        H264DecoderGlobalMacroblocksDescriptor *gmbinfo,
        H264DecoderLocalMacroblockDescriptor *mbinfo);

extern void umc_h264_mv_resampling_mb(H264SegmentDecoderMultiThreaded *sd,
                               Ipp32s mb_x, Ipp32s mb_y, Ipp32s field);

extern void constrainedIntraResamplingCheckMBs(const H264SeqParamSet *curSps,
                                        H264DecoderLayerDescriptor *curLayer,
                                        H264DecoderLayerDescriptor *refLayer,
                                        H264DecoderMacroblockGlobalInfo *mbs,
                                        Ipp32s  *buf,
                                        Ipp32s  widthMB,
                                        Ipp32s  heightMB);
} // namespace UMC

#endif // __UMC_H264_RESAMPLING_TEMPLATES_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
