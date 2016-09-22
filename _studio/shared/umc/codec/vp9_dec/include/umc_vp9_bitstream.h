//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2016 Intel Corporation. All Rights Reserved.
//

#pragma once

#include "umc_vp9_dec_defs.h"

#ifdef UMC_ENABLE_VP9_VIDEO_DECODER

#ifndef __UMC_VP9_BITSTREAM_H_
#define __UMC_VP9_BITSTREAM_H_

namespace UMC_VP9_DECODER
{
    class VP9DecoderFrame;
    struct Loopfilter;

    class VP9Bitstream
    {

    public:

        VP9Bitstream();
        VP9Bitstream(Ipp8u * const pb, const Ipp32u maxsize);

        // Reset the bitstream with new data pointer 
        void Reset(Ipp8u * const pb, const Ipp32u maxsize);
        // Reset the bitstream with new data pointer and bit offset
        void Reset(Ipp8u * const pb, Ipp32s offset, const Ipp32u maxsize); 
        
        // Returns number of decoded bytes since last reset
        size_t BytesDecoded() const
        {
            return
                static_cast<size_t>(m_pbs - m_pbsBase) + (m_bitOffset / 8); 
        }

        // Returns number of decoded bits since last reset
        size_t BitsDecoded() const
        {
            return
                static_cast<size_t>(m_pbs - m_pbsBase) * 8 + m_bitOffset;
        }

        // Returns number of bytes left in bitstream array
        size_t BytesLeft() const
        {
            return
                (Ipp32s)m_maxBsSize - (Ipp32s) BytesDecoded(); 
        }
 
        // Return bitstream array base address and size
        void GetOrg(Ipp8u **pbs, Ipp32u *size);

        Ipp32u GetBit()
        {
            if (m_pbs >= m_pbsBase + m_maxBsSize)
                throw vp9_exception(UMC::UMC_ERR_NOT_ENOUGH_DATA);

            Ipp32u const bit = (*m_pbs >> (7 - m_bitOffset)) & 1;
            if (++m_bitOffset == 8)
            {
                ++m_pbs;
                m_bitOffset = 0;
            }
            
            return bit;
        }

        Ipp32u GetBits(Ipp32u nbits);
        Ipp32u GetUe();
        Ipp32s GetSe();

    private:

        Ipp8u* m_pbs;                                              // pointer to the current position of the buffer.
        Ipp32s m_bitOffset;                                        // the bit position (0 to 31) in the dword pointed by m_pbs.
        Ipp8u* m_pbsBase;                                          // pointer to the first byte of the buffer.
        Ipp32u m_maxBsSize;                                        // maximum buffer size in bytes. 
    };

    inline
    bool CheckSyncCode(VP9Bitstream* bs)
    {
        return 
            bs->GetBits(8) == VP9_SYNC_CODE_0 &&
            bs->GetBits(8) == VP9_SYNC_CODE_1 &&
            bs->GetBits(8) == VP9_SYNC_CODE_2
            ;
    }

    void GetDisplaySize(VP9Bitstream*, VP9DecoderFrame*);
    void GetFrameSize(VP9Bitstream*, VP9DecoderFrame*);
    void GetBitDepthAndColorSpace(VP9Bitstream*, VP9DecoderFrame*);
    void GetFrameSizeWithRefs(VP9Bitstream*, VP9DecoderFrame*);
    void SetupLoopFilter(VP9Bitstream *pBs, Loopfilter*);

} // namespace UMC_VP9_DECODER

#endif // __UMC_VP9_BITSTREAM_H_

#endif // UMC_ENABLE_VP9_VIDEO_DECODER
