// Copyright (c) 2003-2018 Intel Corporation
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

#include "umc_deinterlacing.h"
#include "umc_video_data.h"
#include "ippi.h"
#include "ippvc.h"

using namespace UMC;

Deinterlacing::Deinterlacing()
{
  mMethod = DEINTERLACING_DUPLICATE;
}

Status Deinterlacing::SetMethod(DeinterlacingMethod method)
{
  mMethod = method;
  return UMC_OK;
}

static void DeinterlacingEdgeDetect(Ipp8u *psrc,
                                    Ipp32s iSrcPitch,
                                    Ipp8u *pdst,
                                    Ipp32s iDstPitch,
                                    Ipp32s w,
                                    Ipp32s h);

Status Deinterlacing::GetFrame(MediaData *input, MediaData *output)
{
  VideoData *in = DynamicCast<VideoData>(input);
  VideoData *out = DynamicCast<VideoData>(output);
  DeinterlacingMethod method = mMethod;
  int k;

  if (NULL == in || NULL == out) {
    return UMC_ERR_NULL_PTR;
  }

  ColorFormat cFormat = in->GetColorFormat();
  if (out->GetColorFormat() != cFormat) {
    return UMC_ERR_INVALID_PARAMS;
  }
  Ipp32s in_Width = in->GetWidth();
  Ipp32s in_Height = in->GetHeight();
  Ipp32s out_Width = out->GetWidth();
  Ipp32s out_Height = out->GetHeight();
  if ( (in_Width != out_Width) || (in_Height != out_Height) ) {
    return UMC_ERR_INVALID_PARAMS;
  }

  for (k = 0; k < in->GetNumPlanes(); k++) {
    VideoData::PlaneInfo srcPlane;
    const Ipp8u *pSrc0; //, *pSrc1;
    Ipp8u *pDst0, *pDst1;
    int srcPitch, dstPitch;
    IppiSize size;

    in->GetPlaneInfo(&srcPlane, k);
    pSrc0 = (const Ipp8u*)in->GetPlanePointer(k);
    srcPitch = (Ipp32s)in->GetPlanePitch(k);
    pDst0 = (Ipp8u*)out->GetPlanePointer(k);
    dstPitch = (Ipp32s)out->GetPlanePitch(k);
    size.width = srcPlane.m_ippSize.width * srcPlane.m_iSamples * srcPlane.m_iSampleSize;
    size.height = srcPlane.m_ippSize.height;

    if (method == DEINTERLACING_BLEND) {
      if (srcPlane.m_iSampleSize != 1) {
        //return UMC_ERR_UNSUPPORTED;
        method = DEINTERLACING_DUPLICATE;
      }
      ippiDeinterlaceFilterTriangle_8u_C1R(pSrc0, srcPitch,
                                           pDst0, dstPitch,
                                           size,
                                           128,
                                           IPP_LOWER | IPP_UPPER | IPP_CENTER);
      continue;
    }

    if (method == DEINTERLACING_SPATIAL) {
      if (k == 0) { // Y
        if (srcPlane.m_iSampleSize != 1 || srcPlane.m_iSamples != 1) {
          //return UMC_ERR_UNSUPPORTED;
          method = DEINTERLACING_DUPLICATE;
        }
        DeinterlacingEdgeDetect((Ipp8u*)pSrc0, srcPitch,
                                pDst0, dstPitch,
                                size.width, size.height);
        continue;
      } else { // U,V
        method = DEINTERLACING_DUPLICATE;
      }
    }

    // DEINTERLACING_DUPLICATE
    //pSrc1 = pSrc0 += srcPitch;
    pSrc0 += srcPitch;
    srcPitch *= 2;
    pDst1 = pDst0 + dstPitch;
    dstPitch *= 2;
    size.height /= 2;
    ippiCopy_8u_C1R(pSrc0, srcPitch, pDst0, dstPitch, size);
    ippiCopy_8u_C1R(pSrc0, srcPitch, pDst1, dstPitch, size);
  }

  return UMC_OK;
}

static void DeinterlacingEdgeDetect(Ipp8u *psrc,
                                    Ipp32s iSrcPitch,
                                    Ipp8u *pdst,
                                    Ipp32s iDstPitch,
                                    Ipp32s w,
                                    Ipp32s h)
{
  Ipp32s /*hi, lo,*/ x, y;
  Ipp32s res;
  Ipp8u *pInA, *pInB, *pInC, *pInD, *pInE, *pInF;
  Ipp32s dif1, dif2, dif3;
  IppiSize roi = {w, h/2};

  // copy even lines
  ippiCopy_8u_C1R(psrc, 2*iSrcPitch, pdst, 2*iDstPitch, roi);

  // then line #1 and line before last
  std::copy(psrc + iSrcPitch, psrc + 2*iSrcPitch, pdst + iDstPitch);
  std::copy(psrc + (h - 2) * iSrcPitch, psrc + (h - 2) * iSrcPitch + w, pdst + (h - 1)*iDstPitch);

  psrc += 3*iSrcPitch;
  pdst += 3*iDstPitch;
  pInB = psrc - iSrcPitch;
  pInA = pInB - 1;
  pInC = pInB + 1;
  pInE = psrc + iSrcPitch;
  pInD = pInE - 1;
  pInF = pInE + 1;

#if 0
  hi = 240;
  lo = 16;
#endif

  for (y = 3; y <= h - 3; y += 2) {
    pdst[0] = psrc[0];
    pdst[w - 1] = psrc[w - 1];

    for (x = 1; x < w - 1; x++)
    {
      dif1 = abs((Ipp32s)pInA[x] - (Ipp32s)pInF[x]);
      dif2 = abs((Ipp32s)pInC[x] - (Ipp32s)pInD[x]);
      dif3 = abs((Ipp32s)pInB[x] - (Ipp32s)pInE[x]);

      if (dif1 < dif2) {
        if (dif1 < dif3) {
          res = ((Ipp32s)pInA[x] + (Ipp32s)pInF[x]) >> 1; //1
        } else {
          res = ((Ipp32s)pInB[x] + (Ipp32s)pInE[x]) >> 1; //3
        }
      } else {
        if (dif2 < dif3) {
          res = ((Ipp32s)pInC[x] + (Ipp32s)pInD[x]) >> 1; //2
        } else {
          res = ((Ipp32s)pInB[x] + (Ipp32s)pInE[x]) >> 1; //3
        }
      }
#if 0
      if (res > hi) res = hi;
      else if (res < lo) res = lo;
#endif
      pdst[x] = (Ipp8u)res;
    }
    psrc += 2*iSrcPitch;
    pdst += 2*iDstPitch;
    pInA += 2*iSrcPitch;
    pInB += 2*iSrcPitch;
    pInC += 2*iSrcPitch;
    pInD += 2*iSrcPitch;
    pInE += 2*iSrcPitch;
    pInF += 2*iSrcPitch;
  }
}
