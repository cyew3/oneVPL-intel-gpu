/* /////////////////////////////////////////////////////////////////////////////
//
//                  INTEL CORPORATION PROPRIETARY INFORMATION
//     This software is supplied under the terms of a license agreement or
//     nondisclosure agreement with Intel Corporation and may not be copied
//     or disclosed except in accordance with the terms of that agreement.
//          Copyright(c) 2008 - 2015 Intel Corporation. All Rights Reserved.
//
//
//          ColorSpaceConversion Video Pre\Post Processing
//
*/

/* NOTE: compile with ICL and set /QxCORE-AVX2 to avoid SSE/AVX2 transition penalties */

#include "mfx_dispatcher.h"

#if defined (MFX_ENABLE_VPP)

#include <immintrin.h>

#define mm128(s)               _mm256_castsi256_si128(s)     /* cast xmm = low 128 of ymm */
#define mm256(s)               _mm256_castsi128_si256(s)     /* cast ymm = [xmm | undefined] */

namespace MFX_PP
{

IppStatus MFX_Dispatcher_avx2::cc_P010_to_NV12( mfxFrameData* inData,  mfxFrameInfo* inInfo,
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

IppStatus MFX_Dispatcher_avx2::cc_P010_to_P210( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                           mfxFrameData* outData, mfxFrameInfo*)
{
    mfxU32 inPitch, outPitch;
    mfxU16 *inPtr;
    mfxU16 *outPtr;
    mfxI32 i, j, width, height;
    __m256i ymm0, ymm1, ymm2, ymm3;

    _mm256_zeroupper();

    IppStatus sts = ippStsNoErr;

    // assume width multiple of 16
    VM_ASSERT( (inInfo->Width & 0x0f) == 0 );

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
            for (i = 0; i < width; i += 16) {
                ymm0 = _mm256_loadu_si256((__m256i *)(inPtr + i));
                _mm256_storeu_si256((__m256i *)(outPtr + i), ymm0);
            }
            inPtr  += inPitch;
            outPtr += outPitch;
        }

        // Chroma is interpolated using nearest points that gives closer match with ffmpeg
        inPtr    = (mfxU16 *)inData->U16;
        outPtr   = (mfxU16 *)outData->U16;

        // first row just copy
        for (i = 0; i < width; i += 16) {
            // 8 UV pairs per pass
            ymm0 = _mm256_loadu_si256((__m256i *)(inPtr + i));
            _mm256_storeu_si256((__m256i *)(outPtr + i), ymm0);
        }

        // inner rows = 3*r0 + 1*r1 (odd) and 1*r0 + 3*r1 (even)
        for (j = 1; j < height - 1; j += 2) {
            inPtr =  (mfxU16 *)inData->U16 +  inPitch  * (j/2);
            outPtr = (mfxU16 *)outData->U16 + outPitch * (j);

            for (i = 0; i < width; i += 16) {
                // 8 UV pairs per pass
                ymm0 = _mm256_loadu_si256((__m256i *)(inPtr + i + 0*inPitch));
                ymm1 = _mm256_loadu_si256((__m256i *)(inPtr + i + 1*inPitch));

                ymm2 = _mm256_add_epi16(ymm0, _mm256_slli_epi16(ymm0, 1));  // ymm2 = 3*ymm0
                ymm3 = _mm256_add_epi16(ymm1, _mm256_slli_epi16(ymm1, 1));  // ymm3 = 3*ymm1
                ymm2 = _mm256_add_epi16(ymm2, ymm1);
                ymm3 = _mm256_add_epi16(ymm3, ymm0);

                ymm2 = _mm256_srli_epi16(ymm2, 2);
                ymm3 = _mm256_srli_epi16(ymm3, 2);
                ymm2 = _mm256_min_epi16(ymm2, _mm256_set1_epi16(1023));
                ymm3 = _mm256_min_epi16(ymm3, _mm256_set1_epi16(1023));

                _mm256_storeu_si256((__m256i *)(outPtr + 0*outPitch + i), ymm2);
                _mm256_storeu_si256((__m256i *)(outPtr + 1*outPitch + i), ymm3);
            }
        }

        // last row just copy
        inPtr =  (mfxU16 *)inData->UV +  inPitch  * (height/2 - 1);
        outPtr = (mfxU16 *)outData->UV + outPitch * (height - 1);
        for (i = 0; i < width; i += 16) {
            // 8 UV pairs per pass
            ymm0 = _mm256_loadu_si256((__m256i *)(inPtr + i));
            _mm256_storeu_si256((__m256i *)(outPtr + i), ymm0);
        }
  }
  else
  {
      /* Interlaced content is not supported... yet. */
     return ippStsErr;
  }

  return sts;

} // IppStatus cc_P210_to_P010( mfxFrameData* inData,  mfxFrameInfo* inInfo,...)

IppStatus MFX_Dispatcher_avx2::cc_NV16_to_NV12( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                           mfxFrameData* outData, mfxFrameInfo* /*outInfo*/)
{
    mfxU32 inPitch, outPitch;
    mfxU8 *inPtr;
    mfxU8 *outPtr;
    mfxI32 i, j, width, height;
    __m128i xmm0;
    __m256i ymm0, ymm1;

    _mm256_zeroupper();

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
            for (i = 0; i < width; i += 16) {
                // process 8 UV pairs at a time
                ymm0 = _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i *)(inPtr + 0*inPitch + i)));
                ymm1 = _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i *)(inPtr + 1*inPitch + i)));

                ymm0 = _mm256_add_epi16(ymm0, ymm1);
                ymm0 = _mm256_add_epi16(ymm0, _mm256_set1_epi16(1));
                ymm0 = _mm256_srli_epi16(ymm0, 1);
                ymm0 = _mm256_packus_epi16(ymm0, ymm0);
                ymm0 = _mm256_permute4x64_epi64(ymm0, 0xd8);

                // store averaged line
                _mm_storeu_si128((__m128i *)(outPtr + i), mm128(ymm0));
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

IppStatus MFX_Dispatcher_avx2::cc_NV16_to_P210( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                           mfxFrameData* outData, mfxFrameInfo* /*outInfo*/)
{
    mfxU32 inPitch, outPitch;
    mfxU8 *inPtr;
    mfxU16 *outPtr;
    mfxI32 i, j, width, height;
    __m256i ymm0;

    _mm256_zeroupper();

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

        for (j = 0; j < height; j ++) {
            for (i = 0; i < width; i += 16) {
                ymm0 = _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i *)(inPtr + i)));
                ymm0 = _mm256_slli_epi16(ymm0, 2);
                _mm256_storeu_si256((__m256i *)(outPtr + i), ymm0);
            }
            inPtr  += inPitch;
            outPtr += outPitch;
        }

        inPtr    = (mfxU8 *)inData->UV;
        outPtr   = (mfxU16 *)outData->UV;

        for (j = 0; j < height; j++) {
            for (i = 0; i < width; i += 16) {
                ymm0 = _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i *)(inPtr + i)));
                ymm0 = _mm256_slli_epi16(ymm0, 2);
                _mm256_storeu_si256((__m256i *)(outPtr + i), ymm0);
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

IppStatus MFX_Dispatcher_avx2::cc_P210_to_P010( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                           mfxFrameData* outData, mfxFrameInfo* /*outInfo*/)
{
    mfxU32 inPitch, outPitch;
    mfxU16 *inPtr;
    mfxU16 *outPtr;
    mfxI32 i, j, width, height;
    __m256i ymm0, ymm1;

    _mm256_zeroupper();

    IppStatus sts = ippStsNoErr;

    // assume width multiple of 16
    VM_ASSERT( (inInfo->Width & 0x0f) == 0 );

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
            for (i = 0; i < width; i += 16) {
                ymm0 = _mm256_loadu_si256((__m256i *)(inPtr + i));
                _mm256_storeu_si256((__m256i *)(outPtr + i), ymm0);
            }
            inPtr  += inPitch;
            outPtr += outPitch;
        }
        inPtr    = (mfxU16 *)inData->U16;
        outPtr   = (mfxU16 *)outData->U16;

        // first row just copy
        for (i = 0; i < width; i += 16) {
            // 8 UV pairs per pass
            ymm0 = _mm256_loadu_si256((__m256i *)(inPtr + i));
            _mm256_storeu_si256((__m256i *)(outPtr + i), ymm0);
        }
        inPtr  += 2*inPitch;
        outPtr += outPitch;

        for (j = 2; j < height; j += 2) {
            for (i = 0; i < width; i += 16) {
                // process 8 UV pairs at a time
                ymm0 = _mm256_loadu_si256((__m256i *)(inPtr + 0*inPitch + i));
                ymm1 = _mm256_loadu_si256((__m256i *)(inPtr + 1*inPitch + i));

                ymm0 = _mm256_add_epi16(ymm0, ymm1);
                ymm0 = _mm256_add_epi16(ymm0, _mm256_set1_epi16(1));
                ymm0 = _mm256_srli_epi16(ymm0, 1);
                ymm0 = _mm256_min_epi16(ymm0, _mm256_set1_epi16(1023));

                // store averaged line
                _mm256_storeu_si256((__m256i *)(outPtr + i), ymm0);
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

IppStatus MFX_Dispatcher_avx2::cc_P210_to_NV12( mfxFrameData* inData,  mfxFrameInfo* inInfo,
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

IppStatus MFX_Dispatcher_avx2::cc_P210_to_NV16( mfxFrameData* inData,  mfxFrameInfo* inInfo,
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

IppStatus MFX_Dispatcher_avx2::cc_NV12_to_P010( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                           mfxFrameData* outData, mfxFrameInfo* /*outInfo*/)
{
    mfxU32 inPitch, outPitch;
    mfxU8 *inPtr;
    mfxU16 *outPtr;
    mfxI32 i, j, width, height;
    __m256i ymm0;

    _mm256_zeroupper();

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
                ymm0 = _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i *)(inPtr + i)));
                ymm0 = _mm256_slli_epi16(ymm0, 2);
                _mm256_storeu_si256((__m256i *)(outPtr + i), ymm0);
            }
            inPtr  += inPitch;
            outPtr += outPitch;
        }

        inPtr    = (mfxU8 *)inData->UV;
        outPtr   = (mfxU16 *)outData->UV;

        for (j = 0; j < height / 2; j++) {
            for (i = 0; i < width; i += 16) {
                ymm0 = _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i *)(inPtr + i)));
                ymm0 = _mm256_slli_epi16(ymm0, 2);
                _mm256_storeu_si256((__m256i *)(outPtr + i), ymm0);
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

IppStatus MFX_Dispatcher_avx2::cc_NV12_to_NV16( mfxFrameData* inData,  mfxFrameInfo* inInfo,
                           mfxFrameData* outData, mfxFrameInfo*)
{
    mfxU32 inPitch, outPitch;
    mfxU8 *inPtr;
    mfxU8 *outPtr;
    mfxI32 i, j, width, height;
    __m128i xmm0;
    __m256i ymm0, ymm1, ymm2, ymm3;

    _mm256_zeroupper();

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

            for (i = 0; i < width; i += 16) {
                // 8 UV pairs per pass
                ymm0 = _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i *)(inPtr + i + 0*inPitch)));
                ymm1 = _mm256_cvtepu8_epi16(_mm_loadu_si128((__m128i *)(inPtr + i + 1*inPitch)));

                ymm2 = _mm256_add_epi16(ymm0, _mm256_slli_epi16(ymm0, 1));  // ymm2 = 3*ymm0
                ymm3 = _mm256_add_epi16(ymm1, _mm256_slli_epi16(ymm1, 1));  // ymm3 = 3*ymm1
                ymm2 = _mm256_add_epi16(ymm2, ymm1);
                ymm3 = _mm256_add_epi16(ymm3, ymm0);

                ymm2 = _mm256_srli_epi16(ymm2, 2);
                ymm3 = _mm256_srli_epi16(ymm3, 2);

                ymm2 = _mm256_packus_epi16(ymm2, ymm3);
                ymm2 = _mm256_permute4x64_epi64(ymm2, 0xd8);

                _mm_storeu_si128((__m128i *)(outPtr + 0*outPitch + i), mm128(ymm2));
                ymm2 = _mm256_permute2x128_si256(ymm2, ymm2, 0x01);
                _mm_storeu_si128((__m128i *)(outPtr + 1*outPitch + i), mm128(ymm2));
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

ALIGN_DECL(32) static const signed int yTab[8]  = { 0x000129fa, 0x000129fa, 0x000129fa, 0x000129fa, 0x000129fa, 0x000129fa, 0x000129fa, 0x000129fa };
ALIGN_DECL(32) static const signed int rTab[8]  = { 0x00019891, 0x00019891, 0x00019891, 0x00019891, 0x00019891, 0x00019891, 0x00019891, 0x00019891 };
ALIGN_DECL(32) static const signed int bTab[8]  = { 0x00020458, 0x00020458, 0x00020458, 0x00020458, 0x00020458, 0x00020458, 0x00020458, 0x00020458 };
ALIGN_DECL(32) static const signed int grTab[8] = { 0x0000d01f, 0x0000d01f, 0x0000d01f, 0x0000d01f, 0x0000d01f, 0x0000d01f, 0x0000d01f, 0x0000d01f };
ALIGN_DECL(32) static const signed int gbTab[8] = { 0x00006459, 0x00006459, 0x00006459, 0x00006459, 0x00006459, 0x00006459, 0x00006459, 0x00006459 };

/* assume width = multiple of 8 */
IppStatus MFX_Dispatcher_avx2::cc_P010_to_A2RGB10( mfxFrameData* inData,  mfxFrameInfo* inInfo, mfxFrameData* outData, mfxFrameInfo* outInfo)
{
    __m128i xmm0, xmm1;
    __m256i ymm0, ymm1, ymm2, ymm3, ymm4, ymm5;
    __m256i ymmMin, ymmMax, ymmRnd, ymmOff, y1mmOff;

    mfxU32 inPitch, outPitch;
    mfxU16 *ptr_y, *ptr_uv;
    mfxU32 *out, *out_ptr;
    mfxI32 i, j, width, height;

    VM_ASSERT( (inInfo->Width & 0x07) == 0 );
    outInfo;

    if ((MFX_PICSTRUCT_PROGRESSIVE & inInfo->PicStruct) == 0)
        return ippStsErr;  /* interlaced not supported */

    _mm256_zeroupper();

    width  = inInfo->Width;
    height = inInfo->Height;

    inPitch  = (inData->Pitch >> 1);      /* 16-bit input */
    outPitch = (outData->Pitch >> 2);     /* 32-bit output */

    ptr_y  = (mfxU16*)inData->Y;
    ptr_uv = (mfxU16*)inData->UV;
    out    = (mfxU32*)IPP_MIN( IPP_MIN(outData->R, outData->G), outData->B );

    ymmMin  = _mm256_set1_epi32(0);
    ymmMax  = _mm256_set1_epi32(1023);
    ymmRnd  = _mm256_set1_epi32(1 << 15);
    ymmOff  = _mm256_set1_epi32(512);
    y1mmOff = _mm256_set1_epi32(64);

    for (j = 0; j < height; j++) {
        out_ptr = out;
        for (i = 0; i < width; i += 8) {
            /* inner loop should fit entirely in registers in x64 build */
            xmm0 = _mm_loadu_si128((__m128i *)(ptr_y + i));     // [Y0 Y1 Y2 Y3...] (16-bit)
            xmm1 = _mm_loadu_si128((__m128i *)(ptr_uv + i));    // [U0 V0 U1 V1...] (16-bit)

            ymm0 = _mm256_cvtepu16_epi32(xmm0);                 // [Y0 Y1 Y2 Y3...] (32-bit)
            ymm0 = _mm256_sub_epi32(ymm0, y1mmOff);             // -64
            ymm0 = _mm256_mullo_epi32(ymm0, *(__m256i *)yTab);  // Y = [Y0 Y1 Y2 Y3...] * 0x000129fa
            ymm0 = _mm256_add_epi32(ymm0, ymmRnd);              // include round factor

            ymm1 = _mm256_cvtepu16_epi32(xmm1);                 // [U0 V0 U1 V1...] (32-bit)
            ymm1 = _mm256_sub_epi32(ymm1, ymmOff);              // -512
            ymm2 = _mm256_shuffle_epi32(ymm1, 0xF5);            // Cr = [V0 V0 V1 V1...] - 512
            ymm1 = _mm256_shuffle_epi32(ymm1, 0xA0);            // Cb = [U0 U0 U1 U1...] - 512

            /* R = Y + 0x00019891*Cr + rnd */
            ymm3 = _mm256_mullo_epi32(ymm2, *(__m256i *)rTab);
            ymm3 = _mm256_add_epi32(ymm3, ymm0);

            /* G = Y - 0x00006459*Cb - 0x0000d01f*Cr + rnd */
            ymm4 = _mm256_mullo_epi32(ymm1, *(__m256i *)gbTab);
            ymm5 = _mm256_mullo_epi32(ymm2, *(__m256i *)grTab);
            ymm4 = _mm256_sub_epi32(ymm0, ymm4);
            ymm4 = _mm256_sub_epi32(ymm4, ymm5);

            /* B = Y + 0x00020458*Cb + rnd */
            ymm5 = _mm256_mullo_epi32(ymm1, *(__m256i *)bTab);
            ymm5 = _mm256_add_epi32(ymm5, ymm0);

            /* scale back to 16-bit, clip to [0, 2^10 - 1] */
            ymm3 = _mm256_srai_epi32(ymm3, 16);
            ymm4 = _mm256_srai_epi32(ymm4, 16);
            ymm5 = _mm256_srai_epi32(ymm5, 16);

            ymm3 = _mm256_max_epi32(ymm3, ymmMin);
            ymm4 = _mm256_max_epi32(ymm4, ymmMin);
            ymm5 = _mm256_max_epi32(ymm5, ymmMin);

            ymm3 = _mm256_min_epi32(ymm3, ymmMax);
            ymm4 = _mm256_min_epi32(ymm4, ymmMax);
            ymm5 = _mm256_min_epi32(ymm5, ymmMax);

            /* 32-bit output = (R << 20) | (G << 10) | (B << 0) */
            ymm3 = _mm256_slli_epi32(ymm3, 20);
            ymm4 = _mm256_slli_epi32(ymm4, 10);
            ymm5 = _mm256_or_si256(ymm5, ymm3);
            ymm5 = _mm256_or_si256(ymm5, ymm4);

            /* store 8 32-bit RGB values */
            _mm256_storeu_si256((__m256i *)(out_ptr), ymm5);
            out_ptr += 8;
        }

        /* update pointers (UV plane subsampled by 2) */
        out   += outPitch;
        ptr_y += inPitch;
        if (j & 0x01)
            ptr_uv += inPitch;
    }

    return ippStsNoErr;

} // IppStatus cc_P010_to_A2RGB10( mfxFrameData* inData,  mfxFrameInfo* inInfo,...)

/* assume width = multiple of 8 */
IppStatus MFX_Dispatcher_avx2::cc_P210_to_A2RGB10( mfxFrameData* inData,  mfxFrameInfo* inInfo, mfxFrameData* outData, mfxFrameInfo* outInfo)
{
    __m128i xmm0, xmm1;
    __m256i ymm0, ymm1, ymm2, ymm3, ymm4, ymm5;
    __m256i ymmMin, ymmMax, ymmRnd, ymmOff, y1mmOff;

    mfxU32 inPitch, outPitch;
    mfxU16 *ptr_y, *ptr_uv;
    mfxU32 *out, *out_ptr;
    mfxI32 i, j, width, height;

    VM_ASSERT( (inInfo->Width & 0x07) == 0 );
    outInfo;

    if ((MFX_PICSTRUCT_PROGRESSIVE & inInfo->PicStruct) == 0)
        return ippStsErr;  /* interlaced not supported */

    _mm256_zeroupper();

    width  = inInfo->Width;
    height = inInfo->Height;

    inPitch  = (inData->Pitch >> 1);      /* 16-bit input */
    outPitch = (outData->Pitch >> 2);     /* 32-bit output */

    ptr_y  = (mfxU16*)inData->Y;
    ptr_uv = (mfxU16*)inData->UV;
    out    = (mfxU32*)IPP_MIN( IPP_MIN(outData->R, outData->G), outData->B );

    ymmMin  = _mm256_set1_epi32(0);
    ymmMax  = _mm256_set1_epi32(1023);
    ymmRnd  = _mm256_set1_epi32(1 << 15);
    ymmOff  = _mm256_set1_epi32(512);
    y1mmOff = _mm256_set1_epi32(64);

    for (j = 0; j < height; j++) {
        out_ptr = out;
        for (i = 0; i < width; i += 8) {
            /* inner loop should fit entirely in registers in x64 build */
            xmm0 = _mm_loadu_si128((__m128i *)(ptr_y + i));     // [Y0 Y1 Y2 Y3...] (16-bit)
            xmm1 = _mm_loadu_si128((__m128i *)(ptr_uv + i));    // [U0 V0 U1 V1...] (16-bit)

            ymm0 = _mm256_cvtepu16_epi32(xmm0);                 // [Y0 Y1 Y2 Y3...] (32-bit)
            ymm0 = _mm256_sub_epi32(ymm0, y1mmOff);             // -64
            ymm0 = _mm256_mullo_epi32(ymm0, *(__m256i *)yTab);  // Y = [Y0 Y1 Y2 Y3...] * 0x000129fa
            ymm0 = _mm256_add_epi32(ymm0, ymmRnd);              // include round factor

            ymm1 = _mm256_cvtepu16_epi32(xmm1);                 // [U0 V0 U1 V1...] (32-bit)
            ymm1 = _mm256_sub_epi32(ymm1, ymmOff);              // -512
            ymm2 = _mm256_shuffle_epi32(ymm1, 0xF5);            // Cr = [V0 V0 V1 V1...] - 512
            ymm1 = _mm256_shuffle_epi32(ymm1, 0xA0);            // Cb = [U0 U0 U1 U1...] - 512

            /* R = Y + 0x00019891*Cr + rnd */
            ymm3 = _mm256_mullo_epi32(ymm2, *(__m256i *)rTab);
            ymm3 = _mm256_add_epi32(ymm3, ymm0);

            /* G = Y - 0x00006459*Cb - 0x0000d01f*Cr + rnd */
            ymm4 = _mm256_mullo_epi32(ymm1, *(__m256i *)gbTab);
            ymm5 = _mm256_mullo_epi32(ymm2, *(__m256i *)grTab);
            ymm4 = _mm256_sub_epi32(ymm0, ymm4);
            ymm4 = _mm256_sub_epi32(ymm4, ymm5);

            /* B = Y + 0x00020458*Cb + rnd */
            ymm5 = _mm256_mullo_epi32(ymm1, *(__m256i *)bTab);
            ymm5 = _mm256_add_epi32(ymm5, ymm0);

            /* scale back to 16-bit, clip to [0, 2^10 - 1] */
            ymm3 = _mm256_srai_epi32(ymm3, 16);
            ymm4 = _mm256_srai_epi32(ymm4, 16);
            ymm5 = _mm256_srai_epi32(ymm5, 16);

            ymm3 = _mm256_max_epi32(ymm3, ymmMin);
            ymm4 = _mm256_max_epi32(ymm4, ymmMin);
            ymm5 = _mm256_max_epi32(ymm5, ymmMin);

            ymm3 = _mm256_min_epi32(ymm3, ymmMax);
            ymm4 = _mm256_min_epi32(ymm4, ymmMax);
            ymm5 = _mm256_min_epi32(ymm5, ymmMax);

            /* 32-bit output = (R << 20) | (G << 10) | (B << 0) */
            ymm3 = _mm256_slli_epi32(ymm3, 20);
            ymm4 = _mm256_slli_epi32(ymm4, 10);
            ymm5 = _mm256_or_si256(ymm5, ymm3);
            ymm5 = _mm256_or_si256(ymm5, ymm4);

            /* store 8 32-bit RGB values */
            _mm256_storeu_si256((__m256i *)(out_ptr), ymm5);
            out_ptr += 8;
        }

        /* update pointers (UV plane subsampled by 2) */
        out   += outPitch;
        ptr_y += inPitch;
        ptr_uv += inPitch;
    }

    return ippStsNoErr;

} // IppStatus cc_P210_to_A2RGB10( mfxFrameData* inData,  mfxFrameInfo* inInfo,...)

} // namespace MFX_PP

#endif // MFX_ENABLE_VPP

/* EOF */
