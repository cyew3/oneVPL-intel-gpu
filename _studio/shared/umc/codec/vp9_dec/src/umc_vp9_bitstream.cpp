/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2014 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_VP9_VIDEO_DECODER

#include "vm_debug.h"

#include "umc_structures.h"
#include "umc_vp9_bitstream.h"
#include "umc_vp9_frame.h"

namespace UMC_VP9_DECODER
{

VP9Bitstream::VP9Bitstream()
{
    Reset(0, 0);
}

VP9Bitstream::VP9Bitstream(Ipp8u * const pb, const Ipp32u maxsize)
{
    Reset(pb, maxsize);
}

// Reset the bitstream with new data pointer
void VP9Bitstream::Reset(Ipp8u * const pb, const Ipp32u maxsize)
{
    Reset(pb, 0, maxsize);
}

// Reset the bitstream with new data pointer and bit offset
void VP9Bitstream::Reset(Ipp8u * const pb, Ipp32s offset, const Ipp32u maxsize)
{
    m_pbs       = pb;
    m_pbsBase   = pb;
    m_bitOffset = offset;
    m_maxBsSize = maxsize;

}

// Return bitstream array base address and size
void VP9Bitstream::GetOrg(Ipp8u **pbs, Ipp32u *size)
{
    *pbs       = m_pbsBase;
    *size      = m_maxBsSize; 
}

Ipp32u VP9Bitstream::GetBits(Ipp32u nbits)
{
    mfxU32 bits = 0;
    for (; nbits > 0; --nbits)
    {
        bits <<= 1;
        bits |= GetBit();
    }

    return bits;
}

Ipp32u VP9Bitstream::GetUe()
{
    Ipp32u zeroes = 0;
    while (GetBit() == 0)
        ++zeroes;

    return zeroes == 0 ?
        0 : ((1 << zeroes) | GetBits(zeroes)) - 1; 
}

Ipp32s VP9Bitstream::GetSe()
{
    Ipp32s val = GetUe();
    Ipp32u sign = (val & 1);
    val = (val + 1) >> 1;

    return
        sign ? val : -Ipp32s(val); 
}

void GetFrameSize(VP9Bitstream* bs, VP9DecoderFrame* frame)
{
    frame->width = bs->GetBits(16) + 1;
    frame->height = bs->GetBits(16) + 1;

    GetDisplaySize(bs, frame);
}

void GetDisplaySize(VP9Bitstream* bs, VP9DecoderFrame* frame)
{
    frame->displayWidth = frame->width;
    frame->displayHeight = frame->height;

    if (bs->GetBit())
    {
        frame->displayWidth = bs->GetBits(16) + 1;
        frame->displayHeight = bs->GetBits(16) + 1;
    }
}

void GetBitDepthAndColorSpace(VP9Bitstream* bs, VP9DecoderFrame* frame)
{
    if (frame->profile >= 2)
        frame->bit_depth = bs->GetBit() ? 12 : 10;
    else
        frame->bit_depth = 8;

    if (frame->frameType != KEY_FRAME && frame->intraOnly && frame->profile == 0)
    {
        frame->subsamplingY = frame->subsamplingX = 1;
        return;
    }

    Ipp32u const colorspace
        = bs->GetBits(3);

    if (colorspace != SRGB)
    {
        bs->GetBit(); // 0: [16, 235] (i.e. xvYCC), 1: [0, 255]
        if (1 == frame->profile || 3 == frame->profile)
        {
            frame->subsamplingX = bs->GetBit();
            frame->subsamplingY = bs->GetBit();
            bs->GetBit(); // reserved bit
        }
        else
            frame->subsamplingY = frame->subsamplingX = 1;
    }
    else
    {
        if (1 == frame->profile || 3 == frame->profile)
        {
            frame->subsamplingX = 0;
            frame->subsamplingY = 0;
            bs->GetBit(); // reserved bit
        }
        else
            throw vp9_exception(UMC::UMC_ERR_INVALID_STREAM);
    }
}

void GetFrameSizeWithRefs(VP9Bitstream* bs, VP9DecoderFrame* frame)
{
    bool bFound = false;
    for (Ipp8u i = 0; i < REFS_PER_FRAME; ++i)
    {
        if (bs->GetBit())
        {
            bFound = true;
            frame->width = frame->sizesOfRefFrame[frame->activeRefIdx[i]].width;
            frame->height = frame->sizesOfRefFrame[frame->activeRefIdx[i]].height;
            break;
        }
    }

    if (!bFound)
    {
        frame->width = bs->GetBits(16) + 1;
        frame->height = bs->GetBits(16) + 1;
    }

    GetDisplaySize(bs, frame);
}

void SetupLoopFilter(VP9Bitstream* bs, Loopfilter* filter)
{
    filter->filterLevel = bs->GetBits(6);
    filter->sharpnessLevel = bs->GetBits(3);

    filter->modeRefDeltaUpdate = 0;

    mfxI8 value = 0;

    filter->modeRefDeltaEnabled = (Ipp8u)bs->GetBit();
    if (filter->modeRefDeltaEnabled)
    {
        filter->modeRefDeltaUpdate = (Ipp8u)bs->GetBit();

        if (filter->modeRefDeltaUpdate)
        {
            for (Ipp32u i = 0; i < MAX_REF_LF_DELTAS; i++)
            {
                if (bs->GetBit())
                {
                    value = (Ipp8s)bs->GetBits(6);
                    filter->refDeltas[i] = bs->GetBit() ? -value : value;
                }
            }

            for (Ipp32u i = 0; i < MAX_MODE_LF_DELTAS; i++)
            {
                if (bs->GetBit())
                {
                    value = (Ipp8s)bs->GetBits(6);
                    filter->modeDeltas[i] = bs->GetBit() ? -value : value;
                }
            }
        }
    }
}

} // namespace UMC_VP9_DECODER
#endif // UMC_ENABLE_VP9_VIDEO_DECODER
