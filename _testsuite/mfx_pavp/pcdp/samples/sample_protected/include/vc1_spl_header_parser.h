//* ////////////////////////////////////////////////////////////////////////////// */
//*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2010-2013 Intel Corporation. All Rights Reserved.
//
//
//*/

#ifndef _VC1_SPL_HEADER_PARSER_H__
#define _VC1_SPL_HEADER_PARSER_H__

#include "vc1_spl_defs.h"

namespace ProtectedLibrary
{

class VC1_SPL_Headers
{
    public:
        VC1_SPL_Headers();
        virtual ~VC1_SPL_Headers();

        mfxStatus ParseSeqHeader(mfxU32* bs, mfxU32* len, mfxU32 dataSize);
        mfxStatus ParseEntryPoint(mfxU32* bs, mfxU32* len, mfxU32 dataSize);
        mfxStatus ParsePictureHeader(mfxU32* bs, mfxU32* len, mfxU32 dataSize);
        mfxStatus ParseSliceHeader(mfxU32* bs, mfxU32* len, mfxU32 dataSize);
        mfxStatus ParseFieldHeader(mfxU32* bs, mfxU32* len, mfxU32 dataSize);
        mfxStatus DecodePictHeaderParams_InterlaceFieldPicture();
        mfxStatus DecodePictHeaderParams();
        mfxStatus GetPicType(SliceTypeCode *type);
    protected:
        VC1SequenceHeader SH;
        VC1PictureLayerHeader PicH;
        mfxU8 SHInit;
        std::vector<mfxU8>  m_pSwapBuffer;
        std::vector<mfxU8>  m_pBitplane;
        VC1SplBitstream  m_bs;

        mfxStatus Init();
        mfxStatus Close();
        mfxStatus Reset();

        mfxU32* PrepareData(mfxU32* bs,mfxU32 dataSize);

        mfxStatus DecodePictHeaderParams_ProgressiveIpicture_Adv();
        mfxStatus DecodePictHeaderParams_InterlaceIpicture_Adv();
        mfxStatus DecodeFieldHeaderParams_InterlaceFieldIpicture_Adv();
        mfxStatus DecodePictHeaderParams_InterlacePpicture_Adv();
        mfxStatus DecodePictHeaderParams_ProgressivePpicture_Adv();
        mfxStatus DecodeFieldHeaderParams_InterlaceFieldPpicture_Adv();
        mfxStatus DecodePictHeaderParams_ProgressiveBpicture_Adv();
        mfxStatus DecodePictHeaderParams_InterlaceBpicture_Adv();
        mfxStatus DecodeFieldHeaderParams_InterlaceFieldBpicture_Adv();
        mfxStatus DecodeSkippicture();
        mfxStatus VOPDQuant();
        void DecodeBitplane(VC1SplBitplane* pBitplane, mfxI32 width, mfxI32 height,mfxI32 offset);
        void Norm6ModeDecode(VC1SplBitplane* pBitplane, mfxI32 width, mfxI32 height);
        void Norm2ModeDecode(VC1SplBitplane* pBitplane, mfxI32 width, mfxI32 height);
        void InverseBitplane(VC1SplBitplane* pBitplane, mfxI32 size);
        void InverseDiff(VC1SplBitplane* pBitplane, mfxI32 widthMB, mfxI32 heightMB);

        mfxStatus MVRangeDecode();
        mfxStatus DMVRangeDecode();

        mfxU32 GetPicHeaderSize(mfxU8* pOriginalData, mfxU32 Offset/*, Ipp8u& Flag_03*/);

};

void SwapData(mfxU8 *src, mfxU8 *dst, mfxU32 dataSize);

#define __VC1GetBits(current_data, offset, nbits, data)                 \
{                                                                       \
    mfxU32 x;                                                           \
                                                                        \
    assert((nbits) >= 0 && (nbits) <= 32);                              \
    assert(offset >= 0 && offset <= 31);                                \
                                                                        \
    offset -= (nbits);                                                  \
                                                                        \
    if(offset >= 0)                                                     \
    {                                                                   \
        x = current_data[0] >> (offset + 1);                            \
    }                                                                   \
    else                                                                \
    {                                                                   \
        offset += 32;                                                   \
                                                                        \
        x = current_data[1] >> (offset);                                \
        x >>= 1;                                                        \
        x += current_data[0] << (31 - offset);                          \
        current_data++;                                                 \
    }                                                                   \
                                                                        \
    assert(offset >= 0 && offset <= 31);                                \
                                                                        \
    (data) = x & ((0x00000001 << (nbits&0x1F)) - 1);                    \
}

#define VC1GetNBits(current_data, offset, nbits, data) __VC1GetBits(current_data, offset, nbits, data);
#define VC1_SPL_GET_BITS(num_bits, value)  VC1GetNBits(m_bs.pBitstream, m_bs.bitOffset, num_bits, value);

}

#endif //_VC1_SPL_HEADER_PARSER_H__