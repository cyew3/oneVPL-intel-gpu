//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2004-2017 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"

#if defined (UMC_ENABLE_VC1_VIDEO_DECODER)

#include "umc_vc1_huffman.h"

#include <cstdlib>
using namespace UMC;

#ifdef OPEN_SOURCE
#define VLC_FORBIDDEN 0xf0f1

static Ipp32u bit_mask[33] =
{
    0x0,
    0x01,       0x03,       0x07,       0x0f,
    0x01f,      0x03f,      0x07f,      0x0ff,
    0x01ff,     0x03ff,     0x07ff,     0x0fff,
    0x01fff,    0x03fff,    0x07fff,    0x0ffff,
    0x01ffff,   0x03ffff,   0x07ffff,   0x0fffff,
    0x01fffff,  0x03fffff,  0x07fffff,  0x0ffffff,
    0x01ffffff, 0x03ffffff, 0x07ffffff, 0x0fffffff,
    0x1fffffff, 0x3fffffff, 0x7fffffff, 0xffffffff
};

#define GetNBits(current_data, bit_ptr, nbits, pData,type)              \
{                                                                       \
    register Ipp32u x;                                                  \
                                                                        \
    bit_ptr -= nbits;                                                   \
                                                                        \
    if (bit_ptr < 0)                                                     \
    {                                                                   \
        bit_ptr += 32;                                                  \
                                                                        \
        x = (current_data)[1] >> (bit_ptr);                             \
        x >>= 1;                                                        \
        x += (current_data)[0] << (31 - bit_ptr);                       \
        (current_data)++;                                               \
    }                                                                   \
    else                                                                \
    {                                                                   \
        x = (current_data)[0] >> (bit_ptr + 1);                         \
    }                                                                   \
                                                                        \
    pData = (type)(x & bit_mask[nbits]);                                \
}

#define UngetNBits(current_data, bit_ptr, nbits)                        \
{                                                                       \
    bit_ptr += nbits;                                                   \
    if (bit_ptr > 31)                                                    \
    {                                                                   \
        bit_ptr -= 32;                                                  \
        (current_data)--;                                               \
    }                                                                   \
}

static Ipp32s HuffmanTableSize(Ipp32s rl, const Ipp32s *pSrcTable)
{
    typedef struct
    {
        Ipp32s code;
        Ipp32s Nc;
    } CodeWord;

    CodeWord *Code;
    Ipp32s nWord, SubTblLength, CodeLength, n, i, j, s, index, dstTableSize;
    const Ipp32s *src;

    /* find number of code words */
    src = pSrcTable + pSrcTable[1] + 2;
    for (nWord = 0;;) {
        if ((n = *src++) < 0) break;
        src += n*(rl == 0 ? 2 : 3);
        nWord += n;
    }

    /* allocate temporary buffer */
    Code = (CodeWord *)malloc(nWord * sizeof(CodeWord));
    if (((CodeWord *)0) == Code)
        return 0;

    /* find destination table size */
    src = pSrcTable + pSrcTable[1] + 2;
    nWord = 0;
    dstTableSize = (1 << pSrcTable[2]) + 1;
    for (CodeLength = 1; ; CodeLength++) {
        /* process all code which length is CodeLength */
        if ((n = *src++) < 0) break;

        for (i = 0; i < n; i++) {
            SubTblLength = 0;
            for (s = 0; s < pSrcTable[1]; s++) {/* for all subtables */
                SubTblLength += pSrcTable[2 + s];
                if (CodeLength <= SubTblLength) break;
                /* find already processed code with the same prefix */
                index = *src >> (CodeLength - SubTblLength);
                for (j = 0; j<nWord; j++) {
                    if (Code[j].Nc <= SubTblLength) continue;
                    if ((Code[j].code >> (Code[j].Nc - SubTblLength)) == index) break;
                }
                if (j >= nWord) {
                    /* there is not code words with the same prefix, create new subtable */
                    dstTableSize += (1 << pSrcTable[2 + s + 1]) + 1;
                }
            }
            /* put word in array */
            Code[nWord].code = *src++;
            Code[nWord++].Nc = CodeLength;
            src += rl == 0 ? 1 : 2;
        }
    }

    free(Code);

    return dstTableSize;
}

static int HuffmanInitAlloc(Ipp32s rl, const Ipp32s* pSrcTable, Ipp32s** ppDstSpec)
{
    Ipp32s          i, k, l, j, commLen = 0;
    Ipp32s          nCodes, code, szblen, offset, shift, mask, value1, value2 = 0, size;

    Ipp32s          *szBuff;
    Ipp32s          *table;

    if (!pSrcTable || !ppDstSpec)
        return -1;

    size = HuffmanTableSize(rl, pSrcTable);
    if (0 == size)
        return -1;

    szblen = pSrcTable[1];
    offset = (1 << pSrcTable[2]) + 1;
    szBuff = (Ipp32s*)&pSrcTable[2];

    table = (Ipp32s*)malloc(size * sizeof(Ipp32s));
    if (0 == table)
        return -1;
    *ppDstSpec = table;

    for (i = 0; i < size; i++)
    {
        table[i] = (VLC_FORBIDDEN << 8) | 1;
    }
    table[0] = pSrcTable[2];

    for (i = 1, k = 2 + pSrcTable[1]; pSrcTable[k] >= 0; i++)
    {
        nCodes = pSrcTable[k++] * (rl == 0 ? 2 : 3);

        for (nCodes += k; k < nCodes; k += (rl == 0 ? 2 : 3))
        {
            commLen = 0;
            table = *ppDstSpec;
            for (l = 0; l < szblen; l++)
            {
                commLen += szBuff[l];
                if (commLen >= i)
                {
                    mask = ((1 << (i - commLen + szBuff[l])) - 1);
                    shift = (commLen - i);
                    code = (pSrcTable[k] & mask) << shift;
                    value1 = pSrcTable[k + 1];
                    if (rl == 1) value2 = pSrcTable[k + 2];

                    for (j = 0; j < (1 << (commLen - i)); j++)
                        if (rl == 0) table[code + j + 1] = (value1 << 8) | (commLen - i);
                        else      table[code + j + 1] = ((Ipp16s)value2 << 16) | ((Ipp8u)value1 << 8) | (commLen - i);

                        break;
                }
                else
                {
                    mask = (1 << szBuff[l]) - 1;
                    shift = (i - commLen);
                    code = (pSrcTable[k] >> shift) & mask;

                    if (table[code + 1] != (long)((VLC_FORBIDDEN << 8) | 1))
                    {
                        if (((table[code + 1] & 0xff) == 0x80) && (rl == 1 || (table[code + 1] & 0xff00)))
                        {
                            table = (Ipp32s*)*ppDstSpec + (table[code + 1] >> 8);
                        }
                    }
                    else
                    {
                        table[code + 1] = (offset << 8) | 0x80;
                        table = (*ppDstSpec) + offset;
                        table[0] = szBuff[l + 1];
                        offset += (1 << szBuff[l + 1]) + 1;
                    }

                }
            }
        }
    }

    return 0;
}

int DecodeHuffmanOne(Ipp32u**  pBitStream, int* pOffset,
    Ipp32s*  pDst, const Ipp32s* pDecodeTable)
{
    Ipp32u table_bits, code_len;
    register Ipp32u  pos;
    register Ipp32s  val;

    if (!pBitStream || !pOffset || !pDecodeTable || !*pBitStream || !pDst)
        return -1;

    table_bits = *pDecodeTable;
    GetNBits((*pBitStream), (*pOffset), table_bits, pos, Ipp32u)

    val = pDecodeTable[pos + 1];
    code_len = val & 0xff;
    val = val >> 8;

    while (code_len & 0x80)
    {
        table_bits = pDecodeTable[val];
        GetNBits((*pBitStream), (*pOffset), table_bits, pos, Ipp32u)

        val = pDecodeTable[pos + val + 1];
        code_len = val & 0xff;
        val = val >> 8;
    }

    if (val == VLC_FORBIDDEN)
    {
        *pDst = val;
        return -1;
    }

    UngetNBits(*pBitStream, *pOffset, code_len)

    *pDst = val;

    return 0;
}

int DecodeHuffmanPair(Ipp32u **pBitStream, Ipp32s *pBitOffset, const Ipp32s *pTable,
    Ipp8s *pFirst, Ipp16s *pSecond)
{
    Ipp32s val;
    Ipp32u table_bits, pos;
    Ipp8u code_len;
    Ipp32u *tmp_pbs = 0;
    Ipp32s tmp_offs = 0;

    /* check error(s) */
    if (!pBitStream || !pBitOffset || !pTable || !pFirst || !pSecond || !*pBitStream)
        return -1;

    tmp_pbs = *pBitStream;
    tmp_offs = *pBitOffset;
    table_bits = *pTable;
    GetNBits((*pBitStream), (*pBitOffset), table_bits, pos, Ipp32u);
    val = pTable[pos + 1];
    code_len = (Ipp8u)(val);

    while (code_len & 0x80)
    {
        val = val >> 8;
        table_bits = pTable[val];
        GetNBits((*pBitStream), (*pBitOffset), table_bits, pos, Ipp32u);
        val = pTable[pos + val + 1];
        code_len = (Ipp8u)(val);
    }

    UngetNBits((*pBitStream), (*pBitOffset), code_len);

    if ((val >> 8) == VLC_FORBIDDEN)
    {
        *pBitStream = tmp_pbs;
        *pBitOffset = tmp_offs;
        return -1;
    }

    *pFirst = (Ipp8s)((val >> 8) & 0xff);
    *pSecond = (Ipp16s)((val >> 16) & 0xffff);

    return 0;
}

int HuffmanTableInitAlloc(const Ipp32s* pSrcTable, Ipp32s** ppDstSpec)
{
    return HuffmanInitAlloc(0, pSrcTable, ppDstSpec);
}

int HuffmanRunLevelTableInitAlloc(const Ipp32s* pSrcTable, Ipp32s** ppDstSpec)
{
    return HuffmanInitAlloc(1, pSrcTable, ppDstSpec);
}

void HuffmanTableFree(Ipp32s *pDecodeTable)
{
    free((void*)pDecodeTable);
}

#else // #ifdef OPEN_SOURCE

#include "ippvc.h"
#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
#pragma warning(disable:4996)

int DecodeHuffmanOne(Ipp32u**  pBitStream, int* pOffset,
    Ipp32s*  pDst, const Ipp32s* pDecodeTable)
{
    return (ippStsNoErr != ippiDecodeHuffmanOne_1u32s(pBitStream, pOffset, pDst, pDecodeTable)) ? -1 : 0;
}

int DecodeHuffmanPair(Ipp32u **pBitStream, Ipp32s *pBitOffset,
    const Ipp32s *pTable, Ipp8s *pFirst, Ipp16s *pSecond)
{
    return (ippStsNoErr != ippiDecodeHuffmanPair_1u16s(pBitStream, pBitOffset, pTable, pFirst, pSecond)) ? -1 : 0;
}

int HuffmanTableInitAlloc(const Ipp32s* pSrcTable, Ipp32s** ppDstSpec)
{
    return (ippStsNoErr != ippiHuffmanTableInitAlloc_32s(pSrcTable, ppDstSpec)) ? -1 : 0;
}

int HuffmanRunLevelTableInitAlloc(const Ipp32s* pSrcTable, Ipp32s** ppDstSpec)
{
    return (ippStsNoErr != ippiHuffmanRunLevelTableInitAlloc_32s(pSrcTable, ppDstSpec)) ? -1 : 0;
}

void HuffmanTableFree(Ipp32s *pDecodeTable)
{
    ippiHuffmanTableFree_32s(pDecodeTable);
}

#endif

#endif // #if defined (UMC_ENABLE_VC1_VIDEO_DECODER)
