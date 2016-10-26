//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2008-2015 Intel Corporation. All Rights Reserved.
//

#include "mfx_common.h"

/* makefiles need to be updated to build on Linux (e.g. add -msse4.2) */
#if defined(_WIN32) || defined(_WIN64)

#if defined (MFX_ENABLE_VPP)

#include "mfx_color_space_conversion_vpp.h"

#include <immintrin.h>

IppStatus cc_P010_to_NV12_sse( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                           mfxFrameData* outData, mfxFrameInfo* /*outInfo*/,
                            mfxFrameData* /*yv12Data*/)
{
    mfxU32 inPitch, outPitch;
    mfxU16 *inPtr;
    mfxU8  *outPtr;
    mfxI32 i, j, width, height;
    __m128i xmm0, xmm1;

    IppStatus sts = ippStsNoErr;

    // assume width multiple of 16
    VM_ASSERT( (inInfo->Width & 0x0f) == 0 );

    if(MFX_PICSTRUCT_PROGRESSIVE & inInfo->PicStruct)
    {
        width  = inInfo->Width;
        height = inInfo->Height;

        // pitch in bytes
        inPtr    = (mfxU16 *)inData->Y;
        inPitch  = inData->Pitch / 2;   // 16-bit input
        outPtr   = (mfxU8  *)outData->Y;
        outPitch = outData->Pitch;      // 8-bit output

        for (j = 0; j < height; j++) {
            for (i = 0; i < width; i += 16) {
                xmm0 = _mm_loadu_si128((__m128i *)(inPtr + i + 0));
                xmm1 = _mm_loadu_si128((__m128i *)(inPtr + i + 8));

                // TODO - round here? (C version does not)
                //xmm0 = _mm_add_epi16(xmm0, _mm_set1_epi16(0x02));
                //xmm1 = _mm_add_epi16(xmm1, _mm_set1_epi16(0x02));

                xmm0 = _mm_srli_epi16(xmm0, 2);
                xmm1 = _mm_srli_epi16(xmm1, 2);

                xmm0 = _mm_packus_epi16(xmm0, xmm1);
                _mm_storeu_si128((__m128i *)(outPtr + i), xmm0);
            }
            inPtr  += inPitch;
            outPtr += outPitch;
        }

        inPtr    = (mfxU16 *)inData->UV;
        outPtr   = (mfxU8  *)outData->UV;
        for (j = 0; j < height / 2; j++) {
            for (i = 0; i < width; i += 16) {
                xmm0 = _mm_loadu_si128((__m128i *)(inPtr + i + 0));
                xmm1 = _mm_loadu_si128((__m128i *)(inPtr + i + 8));

                // TODO - round here? (C version does not)
                //xmm0 = _mm_add_epi16(xmm0, _mm_set1_epi16(0x02));
                //xmm1 = _mm_add_epi16(xmm1, _mm_set1_epi16(0x02));

                xmm0 = _mm_srli_epi16(xmm0, 2);
                xmm1 = _mm_srli_epi16(xmm1, 2);

                xmm0 = _mm_packus_epi16(xmm0, xmm1);
                _mm_storeu_si128((__m128i *)(outPtr + i), xmm0);
            }
            inPtr  += inPitch;
            outPtr += outPitch;
        }
    }
    else
    {
        /* Interlaced content is not supported... yet. */
        return ippStsErr;
    }

    return sts;
} // IppStatus cc_P010_to_NV12( mfxFrameData* inData,  mfxFrameInfo* inInfo,...)

IppStatus cc_P010_to_P210_sse( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                           mfxFrameData* outData, mfxFrameInfo*)
{
    mfxU32 inPitch, outPitch;
    mfxU16 *inPtr;
    mfxU16 *outPtr;
    mfxI32 i, j, width, height;
    __m128i xmm0, xmm1, xmm2, xmm3;

    IppStatus sts = ippStsNoErr;

    // assume width multiple of 8
    VM_ASSERT( (inInfo->Width & 0x07) == 0 );

    if(MFX_PICSTRUCT_PROGRESSIVE & inInfo->PicStruct)
    {
        width  = inInfo->Width;
        height = inInfo->Height;

        // pitch in bytes
        inPtr    = (mfxU16 *)inData->Y;
        inPitch  = inData->Pitch / 2;       // 16-bit input
        outPtr   = (mfxU16 *)outData->Y;
        outPitch = outData->Pitch / 2;      // 16-bit output

        // Copy Y plane as is
        for (j = 0; j < height; j++) {
            for (i = 0; i < width; i += 8) {
                xmm0 = _mm_loadu_si128((__m128i *)(inPtr + i));
                _mm_storeu_si128((__m128i *)(outPtr + i), xmm0);
            }
            inPtr  += inPitch;
            outPtr += outPitch;
        }

        // Chroma is interpolated using nearest points that gives closer match with ffmpeg
        inPtr    = (mfxU16 *)inData->U16;
        outPtr   = (mfxU16 *)outData->U16;

        // first row just copy
        for (i = 0; i < width; i += 8) {
            // 4 UV pairs per pass
            xmm0 = _mm_loadu_si128((__m128i *)(inPtr + i));
            _mm_storeu_si128((__m128i *)(outPtr + i), xmm0);
        }

        // inner rows = 3*r0 + 1*r1 (odd) and 1*r0 + 3*r1 (even)
        for (j = 1; j < height - 1; j += 2) {
            inPtr =  (mfxU16 *)inData->U16 +  inPitch  * (j/2);
            outPtr = (mfxU16 *)outData->U16 + outPitch * (j);

            for (i = 0; i < width; i += 8) {
                // 4 UV pairs per pass
                xmm0 = _mm_loadu_si128((__m128i *)(inPtr + i + 0*inPitch));
                xmm1 = _mm_loadu_si128((__m128i *)(inPtr + i + 1*inPitch));

                xmm2 = _mm_add_epi16(xmm0, _mm_slli_epi16(xmm0, 1));  // xmm2 = 3*xmm0
                xmm3 = _mm_add_epi16(xmm1, _mm_slli_epi16(xmm1, 1));  // xmm3 = 3*xmm1
                xmm2 = _mm_add_epi16(xmm2, xmm1);
                xmm3 = _mm_add_epi16(xmm3, xmm0);

                xmm2 = _mm_srli_epi16(xmm2, 2);
                xmm3 = _mm_srli_epi16(xmm3, 2);
                xmm2 = _mm_min_epi16(xmm2, _mm_set1_epi16(1023));
                xmm3 = _mm_min_epi16(xmm3, _mm_set1_epi16(1023));

                _mm_storeu_si128((__m128i *)(outPtr + 0*outPitch + i), xmm2);
                _mm_storeu_si128((__m128i *)(outPtr + 1*outPitch + i), xmm3);
            }
        }

        // last row just copy
        inPtr =  (mfxU16 *)inData->UV +  inPitch  * (height/2 - 1);
        outPtr = (mfxU16 *)outData->UV + outPitch * (height - 1);
        for (i = 0; i < width; i += 8) {
            // 4 UV pairs per pass
            xmm0 = _mm_loadu_si128((__m128i *)(inPtr + i));
            _mm_storeu_si128((__m128i *)(outPtr + i), xmm0);
        }
  }
  else
  {
      /* Interlaced content is not supported... yet. */
     return ippStsErr;
  }

  return sts;

} // IppStatus cc_P210_to_P010( mfxFrameData* inData,  mfxFrameInfo* inInfo,...)

IppStatus cc_NV16_to_NV12_sse( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                           mfxFrameData* outData, mfxFrameInfo* /*outInfo*/)
{
    mfxU32 inPitch, outPitch;
    mfxU8 *inPtr;
    mfxU8 *outPtr;
    mfxI32 i, j, width, height;
    __m128i xmm0, xmm1;

    IppStatus sts = ippStsNoErr;

    // assume width multiple of 16
    VM_ASSERT( (inInfo->Width & 0x0f) == 0 );

    if(MFX_PICSTRUCT_PROGRESSIVE & inInfo->PicStruct)
    {
        width  = inInfo->Width;
        height = inInfo->Height;

        // pitch in bytes
        inPtr    = (mfxU8 *)inData->Y;
        inPitch  = inData->Pitch;       // 8-bit input
        outPtr   = (mfxU8 *)outData->Y;
        outPitch = outData->Pitch;      // 8-bit output

        for (j = 0; j < height; j++) {
            for (i = 0; i < width; i += 16) {
                xmm0 = _mm_loadu_si128((__m128i *)(inPtr + i));
                _mm_storeu_si128((__m128i *)(outPtr + i), xmm0);
            }
            inPtr  += inPitch;
            outPtr += outPitch;
        }

        inPtr    = (mfxU8 *)inData->UV;
        inPitch  = inData->Pitch;       // 8-bit input
        outPtr   = (mfxU8 *)outData->UV;
        outPitch = outData->Pitch;      // 8-bit output

        // first row just copy
        for (i = 0; i < width; i += 16) {
            // 8 UV pairs per pass
            xmm0 = _mm_loadu_si128((__m128i *)(inPtr + i));
            _mm_storeu_si128((__m128i *)(outPtr + i), xmm0);
        }
        inPtr  += 2*inPitch;
        outPtr += outPitch;

        for (j = 2; j < height; j += 2) {
            for (i = 0; i < width; i += 8) {
                // process 4 UV pairs at a time
                xmm0 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(inPtr + 0*inPitch + i)));
                xmm1 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(inPtr + 1*inPitch + i)));

                xmm0 = _mm_add_epi16(xmm0, xmm1);
                xmm0 = _mm_add_epi16(xmm0, _mm_set1_epi16(1));
                xmm0 = _mm_srli_epi16(xmm0, 1);
                xmm0 = _mm_packus_epi16(xmm0, xmm0);

                // store averaged line
                _mm_storel_epi64((__m128i *)(outPtr + i), xmm0);
            }
            inPtr  += 2*inPitch;
            outPtr += outPitch;
        }
    }
    else
    {
        /* Interlaced content is not supported... yet. */
        return ippStsErr;
    }

    return sts;
} // IppStatus cc_NV16_to_NV12( mfxFrameData* inData,  mfxFrameInfo* inInfo,...)

IppStatus cc_NV16_to_P210_sse( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                           mfxFrameData* outData, mfxFrameInfo* /*outInfo*/)
{
    mfxU32 inPitch, outPitch;
    mfxU8 *inPtr;
    mfxU16 *outPtr;
    mfxI32 i, j, width, height;
    __m128i xmm0, xmm1;

    IppStatus sts = ippStsNoErr;

    // assume width multiple of 16
    VM_ASSERT( (inInfo->Width & 0x0f) == 0 );

    if(MFX_PICSTRUCT_PROGRESSIVE & inInfo->PicStruct)
    {
        width  = inInfo->Width;
        height = inInfo->Height;

        // pitch in bytes
        inPtr    = (mfxU8 *)inData->Y;
        inPitch  = inData->Pitch;         // 8-bit input
        outPtr   = (mfxU16 *)outData->Y;
        outPitch = outData->Pitch / 2;    // 16-bit output

        for (j = 0; j < height; j++) {
            for (i = 0; i < width; i += 16) {
                xmm0 = _mm_loadu_si128((__m128i *)(inPtr + i));
                xmm1 = _mm_unpackhi_epi64(xmm0, xmm0);

                xmm0 = _mm_cvtepu8_epi16(xmm0);
                xmm1 = _mm_cvtepu8_epi16(xmm1);

                xmm0 = _mm_slli_epi16(xmm0, 2);
                xmm1 = _mm_slli_epi16(xmm1, 2);

                _mm_storeu_si128((__m128i *)(outPtr + i + 0), xmm0);
                _mm_storeu_si128((__m128i *)(outPtr + i + 8), xmm1);
            }
            inPtr  += inPitch;
            outPtr += outPitch;
        }

        inPtr    = (mfxU8 *)inData->UV;
        outPtr   = (mfxU16 *)outData->UV;

        for (j = 0; j < height; j++) {
            for (i = 0; i < width; i += 16) {
                xmm0 = _mm_loadu_si128((__m128i *)(inPtr + i));
                xmm1 = _mm_unpackhi_epi64(xmm0, xmm0);

                xmm0 = _mm_cvtepu8_epi16(xmm0);
                xmm1 = _mm_cvtepu8_epi16(xmm1);

                xmm0 = _mm_slli_epi16(xmm0, 2);
                xmm1 = _mm_slli_epi16(xmm1, 2);

                _mm_storeu_si128((__m128i *)(outPtr + i + 0), xmm0);
                _mm_storeu_si128((__m128i *)(outPtr + i + 8), xmm1);
            }
            inPtr  += inPitch;
            outPtr += outPitch;
        }
    }
    else
    {
        /* Interlaced content is not supported... yet. */
        return ippStsErr;
    }

    return sts;
} // IppStatus cc_NV16_to_P210( mfxFrameData* inData,  mfxFrameInfo* inInfo,...)

IppStatus cc_P210_to_P010_sse( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                           mfxFrameData* outData, mfxFrameInfo* /*outInfo*/)
{
    mfxU32 inPitch, outPitch;
    mfxU16 *inPtr;
    mfxU16 *outPtr;
    mfxI32 i, j, width, height;
    __m128i xmm0, xmm1;

    IppStatus sts = ippStsNoErr;

    // assume width multiple of 8
    VM_ASSERT( (inInfo->Width & 0x07) == 0 );

    if(MFX_PICSTRUCT_PROGRESSIVE & inInfo->PicStruct)
    {
        width  = inInfo->Width;
        height = inInfo->Height;

        // pitch in bytes
        inPtr    = (mfxU16 *)inData->Y;
        inPitch  = inData->Pitch / 2;       // 16-bit input
        outPtr   = (mfxU16 *)outData->Y;
        outPitch = outData->Pitch / 2;      // 16-bit output

        for (j = 0; j < height; j++) {
            for (i = 0; i < width; i += 8) {
                xmm0 = _mm_loadu_si128((__m128i *)(inPtr + i));
                _mm_storeu_si128((__m128i *)(outPtr + i), xmm0);
            }
            inPtr  += inPitch;
            outPtr += outPitch;
        }
        inPtr    = (mfxU16 *)inData->U16;
        outPtr   = (mfxU16 *)outData->U16;

        // first row just copy
        for (i = 0; i < width; i += 8) {
            // 4 UV pairs per pass
            xmm0 = _mm_loadu_si128((__m128i *)(inPtr + i));
            _mm_storeu_si128((__m128i *)(outPtr + i), xmm0);
        }
        inPtr  += 2*inPitch;
        outPtr += outPitch;

        for (j = 2; j < height; j += 2) {
            for (i = 0; i < width; i += 8) {
                // process 4 UV pairs at a time
                xmm0 = _mm_loadu_si128((__m128i *)(inPtr + 0*inPitch + i));
                xmm1 = _mm_loadu_si128((__m128i *)(inPtr + 1*inPitch + i));

                xmm0 = _mm_add_epi16(xmm0, xmm1);
                xmm0 = _mm_add_epi16(xmm0, _mm_set1_epi16(1));
                xmm0 = _mm_srli_epi16(xmm0, 1);
                xmm0 = _mm_min_epi16(xmm0, _mm_set1_epi16(1023));

                // store averaged line
                _mm_storeu_si128((__m128i *)(outPtr + i), xmm0);
            }
            inPtr  += 2*inPitch;
            outPtr += outPitch;
        }
    }
    else
    {
        /* Interlaced content is not supported... yet. */
        return ippStsErr;
    }

    return sts;
} // IppStatus cc_P210_to_P010( mfxFrameData* inData,  mfxFrameInfo* inInfo,...)

IppStatus cc_P210_to_NV12_sse( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                           mfxFrameData* outData, mfxFrameInfo* /*outInfo*/,
                            mfxFrameData* /*yv12Data*/)
{
    mfxU32 inPitch, outPitch;
    mfxU16 *inPtr;
    mfxU8  *outPtr;
    mfxI32 i, j, width, height;
    __m128i xmm0, xmm1;

    IppStatus sts = ippStsNoErr;

    // assume width multiple of 16
    VM_ASSERT( (inInfo->Width & 0x0f) == 0 );

    if(MFX_PICSTRUCT_PROGRESSIVE & inInfo->PicStruct)
    {
        width  = inInfo->Width;
        height = inInfo->Height;

        // pitch in bytes
        inPtr    = (mfxU16 *)inData->Y;
        inPitch  = inData->Pitch / 2;   // 16-bit input
        outPtr   = (mfxU8  *)outData->Y;
        outPitch = outData->Pitch;      // 8-bit output

        for (j = 0; j < height; j++) {
            for (i = 0; i < width; i += 16) {
                xmm0 = _mm_loadu_si128((__m128i *)(inPtr + i + 0));
                xmm1 = _mm_loadu_si128((__m128i *)(inPtr + i + 8));

                // TODO - round here? (C version does not)
                //xmm0 = _mm_add_epi16(xmm0, _mm_set1_epi16(0x02));
                //xmm1 = _mm_add_epi16(xmm1, _mm_set1_epi16(0x02));

                xmm0 = _mm_srli_epi16(xmm0, 2);
                xmm1 = _mm_srli_epi16(xmm1, 2);

                xmm0 = _mm_packus_epi16(xmm0, xmm1);
                _mm_storeu_si128((__m128i *)(outPtr + i), xmm0);
            }
            inPtr  += inPitch;
            outPtr += outPitch;
        }

        inPtr    = (mfxU16 *)inData->UV;
        outPtr   = (mfxU8  *)outData->UV;
        for (j = 0; j < height / 2; j++) {
            for (i = 0; i < width; i += 16) {
                xmm0 = _mm_loadu_si128((__m128i *)(inPtr + i + 0));
                xmm1 = _mm_loadu_si128((__m128i *)(inPtr + i + 8));

                // TODO - round here? (C version does not)
                //xmm0 = _mm_add_epi16(xmm0, _mm_set1_epi16(0x02));
                //xmm1 = _mm_add_epi16(xmm1, _mm_set1_epi16(0x02));

                xmm0 = _mm_srli_epi16(xmm0, 2);
                xmm1 = _mm_srli_epi16(xmm1, 2);

                xmm0 = _mm_packus_epi16(xmm0, xmm1);
                _mm_storeu_si128((__m128i *)(outPtr + i), xmm0);
            }
            inPtr  += inPitch * 2;      // discard odd rows to match C version
            outPtr += outPitch;
        }
    }
    else
    {
        /* Interlaced content is not supported... yet. */
        return ippStsErr;
    }

    return sts;
} // IppStatus cc_P210_to_NV12( mfxFrameData* inData,  mfxFrameInfo* inInfo,...)

IppStatus cc_P210_to_NV16_sse( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                           mfxFrameData* outData, mfxFrameInfo* /*outInfo*/,
                            mfxFrameData* /*yv12Data*/)
{
    mfxU32 inPitch, outPitch;
    mfxU16 *inPtr;
    mfxU8  *outPtr;
    mfxI32 i, j, width, height;
    __m128i xmm0, xmm1;

    IppStatus sts = ippStsNoErr;

    // assume width multiple of 16
    VM_ASSERT( (inInfo->Width & 0x0f) == 0 );

    if(MFX_PICSTRUCT_PROGRESSIVE & inInfo->PicStruct)
    {
        width  = inInfo->Width;
        height = inInfo->Height;

        // pitch in bytes
        inPtr    = (mfxU16 *)inData->Y;
        inPitch  = inData->Pitch / 2;   // 16-bit input
        outPtr   = (mfxU8  *)outData->Y;
        outPitch = outData->Pitch;      // 8-bit output

        for (j = 0; j < height; j++) {
            for (i = 0; i < width; i += 16) {
                xmm0 = _mm_loadu_si128((__m128i *)(inPtr + i + 0));
                xmm1 = _mm_loadu_si128((__m128i *)(inPtr + i + 8));

                // TODO - round here? (C version does not)
                //xmm0 = _mm_add_epi16(xmm0, _mm_set1_epi16(0x02));
                //xmm1 = _mm_add_epi16(xmm1, _mm_set1_epi16(0x02));

                xmm0 = _mm_srli_epi16(xmm0, 2);
                xmm1 = _mm_srli_epi16(xmm1, 2);

                xmm0 = _mm_packus_epi16(xmm0, xmm1);
                _mm_storeu_si128((__m128i *)(outPtr + i), xmm0);
            }
            inPtr  += inPitch;
            outPtr += outPitch;
        }

        inPtr    = (mfxU16 *)inData->UV;
        outPtr   = (mfxU8  *)outData->UV;
        for (j = 0; j < height; j++) {
            for (i = 0; i < width; i += 16) {
                xmm0 = _mm_loadu_si128((__m128i *)(inPtr + i + 0));
                xmm1 = _mm_loadu_si128((__m128i *)(inPtr + i + 8));

                // TODO - round here? (C version does not)
                //xmm0 = _mm_add_epi16(xmm0, _mm_set1_epi16(0x02));
                //xmm1 = _mm_add_epi16(xmm1, _mm_set1_epi16(0x02));

                xmm0 = _mm_srli_epi16(xmm0, 2);
                xmm1 = _mm_srli_epi16(xmm1, 2);

                xmm0 = _mm_packus_epi16(xmm0, xmm1);
                _mm_storeu_si128((__m128i *)(outPtr + i), xmm0);
            }
            inPtr  += inPitch;
            outPtr += outPitch;
        }
    }
    else
    {
        /* Interlaced content is not supported... yet. */
        return ippStsErr;
    }

    return sts;
} // IppStatus cc_P210_to_NV16( mfxFrameData* inData,  mfxFrameInfo* inInfo,...)

IppStatus cc_NV12_to_P010_sse( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                           mfxFrameData* outData, mfxFrameInfo* /*outInfo*/)
{
    mfxU32 inPitch, outPitch;
    mfxU8 *inPtr;
    mfxU16 *outPtr;
    mfxI32 i, j, width, height;
    __m128i xmm0, xmm1;

    IppStatus sts = ippStsNoErr;

    // assume width multiple of 16
    VM_ASSERT( (inInfo->Width & 0x0f) == 0 );

    if(MFX_PICSTRUCT_PROGRESSIVE & inInfo->PicStruct)
    {
        width  = inInfo->Width;
        height = inInfo->Height;

        // pitch in bytes
        inPtr    = (mfxU8 *)inData->Y;
        inPitch  = inData->Pitch;         // 8-bit input
        outPtr   = (mfxU16 *)outData->Y;
        outPitch = outData->Pitch/ 2;     // 16-bit output

        for (j = 0; j < height; j++) {
            for (i = 0; i < width; i += 16) {
                xmm0 = _mm_loadu_si128((__m128i *)(inPtr + i));
                xmm1 = _mm_unpackhi_epi64(xmm0, xmm0);

                xmm0 = _mm_cvtepu8_epi16(xmm0);
                xmm1 = _mm_cvtepu8_epi16(xmm1);

                xmm0 = _mm_slli_epi16(xmm0, 2);
                xmm1 = _mm_slli_epi16(xmm1, 2);

                _mm_storeu_si128((__m128i *)(outPtr + i + 0), xmm0);
                _mm_storeu_si128((__m128i *)(outPtr + i + 8), xmm1);
            }
            inPtr  += inPitch;
            outPtr += outPitch;
        }

        inPtr    = (mfxU8 *)inData->UV;
        outPtr   = (mfxU16 *)outData->UV;

        for (j = 0; j < height / 2; j++) {
            for (i = 0; i < width; i += 16) {
                xmm0 = _mm_loadu_si128((__m128i *)(inPtr + i));
                xmm1 = _mm_unpackhi_epi64(xmm0, xmm0);

                xmm0 = _mm_cvtepu8_epi16(xmm0);
                xmm1 = _mm_cvtepu8_epi16(xmm1);

                xmm0 = _mm_slli_epi16(xmm0, 2);
                xmm1 = _mm_slli_epi16(xmm1, 2);

                _mm_storeu_si128((__m128i *)(outPtr + i + 0), xmm0);
                _mm_storeu_si128((__m128i *)(outPtr + i + 8), xmm1);
            }
            inPtr  += inPitch;
            outPtr += outPitch;
        }
    }
    else
    {
        /* Interlaced content is not supported... yet. */
        return ippStsErr;
    }

    return sts;
} // IppStatus cc_NV12_to_P010( mfxFrameData* inData,  mfxFrameInfo* inInfo,...)

IppStatus cc_NV12_to_NV16_sse( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                           mfxFrameData* outData, mfxFrameInfo*)
{
    mfxU32 inPitch, outPitch;
    mfxU8 *inPtr;
    mfxU8 *outPtr;
    mfxI32 i, j, width, height;
    __m128i xmm0, xmm1, xmm2, xmm3;

    IppStatus sts = ippStsNoErr;

    // assume width multiple of 16
    VM_ASSERT( (inInfo->Width & 0x0f) == 0 );

    if(MFX_PICSTRUCT_PROGRESSIVE & inInfo->PicStruct)
    {
        width  = inInfo->Width;
        height = inInfo->Height;

        // pitch in bytes
        inPtr    = (mfxU8 *)inData->Y;
        inPitch  = inData->Pitch;       // 8-bit input
        outPtr   = (mfxU8 *)outData->Y;
        outPitch = outData->Pitch;      // 8-bit output

        // Copy Y plane as is
        for (j = 0; j < height; j++) {
            for (i = 0; i < width; i += 16) {
                xmm0 = _mm_loadu_si128((__m128i *)(inPtr + i));
                _mm_storeu_si128((__m128i *)(outPtr + i), xmm0);
            }
            inPtr  += inPitch;
            outPtr += outPitch;
        }

        // Chroma is interpolated using nearest points that gives closer match with ffmpeg
        inPtr    = (mfxU8 *)inData->UV;
        outPtr   = (mfxU8 *)outData->UV;

        // first row just copy
        for (i = 0; i < width; i += 16) {
            // 8 UV pairs per pass
            xmm0 = _mm_loadu_si128((__m128i *)(inPtr + i));
            _mm_storeu_si128((__m128i *)(outPtr + i), xmm0);
        }

        // inner rows = 3*r0 + 1*r1 (odd) and 1*r0 + 3*r1 (even)
        for (j = 1; j < height - 1; j += 2) {
            inPtr =  (mfxU8 *)inData->UV +  inPitch  * (j/2);
            outPtr = (mfxU8 *)outData->UV + outPitch * (j);

            for (i = 0; i < width; i += 8) {
                // 4 UV pairs per pass
                xmm0 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(inPtr + i + 0*inPitch)));
                xmm1 = _mm_cvtepu8_epi16(_mm_loadl_epi64((__m128i *)(inPtr + i + 1*inPitch)));

                xmm2 = _mm_add_epi16(xmm0, _mm_slli_epi16(xmm0, 1));  // xmm2 = 3*xmm0
                xmm3 = _mm_add_epi16(xmm1, _mm_slli_epi16(xmm1, 1));  // xmm3 = 3*xmm1
                xmm2 = _mm_add_epi16(xmm2, xmm1);
                xmm3 = _mm_add_epi16(xmm3, xmm0);

                xmm2 = _mm_srli_epi16(xmm2, 2);
                xmm3 = _mm_srli_epi16(xmm3, 2);

                xmm2 = _mm_packus_epi16(xmm2, xmm2);
                xmm3 = _mm_packus_epi16(xmm3, xmm3);

                _mm_storel_epi64((__m128i *)(outPtr + 0*outPitch + i), xmm2);
                _mm_storel_epi64((__m128i *)(outPtr + 1*outPitch + i), xmm3);
            }
        }

        // last row just copy
        inPtr =  (mfxU8 *)inData->UV +  inPitch  * (height/2 - 1);
        outPtr = (mfxU8 *)outData->UV + outPitch * (height - 1);
        for (i = 0; i < width; i += 16) {
            // 4 UV pairs per pass
            xmm0 = _mm_loadu_si128((__m128i *)(inPtr + i));
            _mm_storeu_si128((__m128i *)(outPtr + i), xmm0);
        }
  }
  else
  {
      /* Interlaced content is not supported... yet. */
     return ippStsErr;
  }

  return sts;
} // IppStatus cc_NV12_to_NV16( mfxFrameData* inData,  mfxFrameInfo* inInfo,...)

#endif // MFX_ENABLE_VPP

#endif // (_WIN32) || (_WIN64)
/* EOF */
