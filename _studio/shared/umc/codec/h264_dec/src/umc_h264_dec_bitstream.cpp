//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2017 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#include "vm_debug.h"
#include "umc_h264_dec.h"
#include "umc_h264_bitstream.h"
#include "umc_h264_dec_coeff_token_map.h"
#include "umc_h264_dec_total_zero.h"
#include "umc_h264_dec_run_before.h"
#include "umc_h264_bitstream_inlines.h"
#include "umc_h264_dec_ippwrap.h"

#include <limits.h>

namespace UMC
{

static
const Ipp32u GetBitsMask[25] =
{
    0x00000000, 0x00000001, 0x00000003, 0x00000007,
    0x0000000f, 0x0000001f, 0x0000003f, 0x0000007f,
    0x000000ff, 0x000001ff, 0x000003ff, 0x000007ff,
    0x00000fff, 0x00001fff, 0x00003fff, 0x00007fff,
    0x0000ffff, 0x0001ffff, 0x0003ffff, 0x0007ffff,
    0x000fffff, 0x001fffff, 0x003fffff, 0x007fffff,
    0x00ffffff
};

IppVCHuffmanSpec_32s *(H264Bitstream::m_tblCoeffToken[5]);
IppVCHuffmanSpec_32s *(H264Bitstream::m_tblRunBefore [16]);
IppVCHuffmanSpec_32s *(H264Bitstream::m_tblTotalZeros[16]);

IppVCHuffmanSpec_32s *(H264Bitstream::m_tblTotalZerosCR[4]);
IppVCHuffmanSpec_32s *(H264Bitstream::m_tblTotalZerosCR422[8]);

bool H264Bitstream::m_bTablesInited = false;                      // (bool) tables have been allocated

class TableInitializer
{
public:
    TableInitializer()
    {
        H264Bitstream::InitTables();
    }

    ~TableInitializer()
    {
        H264Bitstream::ReleaseTables();
    }
};

static TableInitializer tableInitializer;


H264Bitstream::H264Bitstream(Ipp8u * const pb, const Ipp32u maxsize)
     : H264HeadersBitstream(pb, maxsize)
     , m_lcodIRange(0)
     , m_lcodIOffset(0)
     , m_iMagicBits(0)
     , m_pMagicBits(0)
     , startIdx(0)
     , endIdx(0)
{
} // H264Bitstream::H264Bitstream(Ipp8u * const pb,

H264Bitstream::H264Bitstream()
    : H264HeadersBitstream()
    , m_lcodIRange(0)
    , m_lcodIOffset(0)
    , m_iMagicBits(0)
    , m_pMagicBits(0)
    , startIdx(0)
    , endIdx(0)
{
} // H264Bitstream::H264Bitstream(void)

H264Bitstream::~H264Bitstream()
{
} // H264Bitstream::~H264Bitstream()

void H264Bitstream::ReleaseTables(void)
{
    Ipp32s i;

    if (!m_bTablesInited)
        return;

    for (i = 0; i <= 4; i++ )
    {
        if (m_tblCoeffToken[i])
        {
            ippiHuffmanTableFree_32s(m_tblCoeffToken[i]);
            m_tblCoeffToken[i] = NULL;
        }
    }

    for (i = 1; i <= 15; i++)
    {
        if (m_tblTotalZeros[i])
        {
            ippiHuffmanTableFree_32s(m_tblTotalZeros[i]);
            m_tblTotalZeros[i] = NULL;
        }
    }

    for(i = 1; i <= 3; i++)
    {
        if(m_tblTotalZerosCR[i])
        {
            ippiHuffmanTableFree_32s(m_tblTotalZerosCR[i]);
            m_tblTotalZerosCR[i] = NULL;
        }
    }

    for(i = 1; i <= 7; i++)
    {
        if(m_tblTotalZerosCR422[i])
        {
            ippiHuffmanTableFree_32s(m_tblTotalZerosCR422[i]);
            m_tblTotalZerosCR422[i] = NULL;
        }
    }

    for(i = 1; i <= 7; i++)
    {
        if(m_tblRunBefore[i])
        {
            ippiHuffmanTableFree_32s(m_tblRunBefore[i]);
            m_tblRunBefore[i] = NULL;
        }
    }

    for(; i <= 15; i++)
    {
        m_tblRunBefore[i] = NULL;
    }

    m_bTablesInited = false;

} // void H264Bitstream::ReleaseTable(void)

Status H264Bitstream::InitTables(void)
{
    IppStatus ippSts;

    // check tables allocation status
    if (m_bTablesInited)
        return UMC_OK;

    // release tables before initialization
    ReleaseTables();

    memset(m_tblCoeffToken, 0, sizeof(m_tblCoeffToken));
    memset(m_tblTotalZerosCR, 0, sizeof(m_tblTotalZerosCR));
    memset(m_tblTotalZerosCR422, 0, sizeof(m_tblTotalZerosCR422));
    memset(m_tblTotalZeros, 0, sizeof(m_tblTotalZeros));
    memset(m_tblRunBefore, 0, sizeof(m_tblRunBefore));

    // number Coeffs and Trailing Ones map tables allocation
    ippSts = ippiHuffmanRunLevelTableInitAlloc_32s(coeff_token_map_02, &m_tblCoeffToken[0]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanRunLevelTableInitAlloc_32s(coeff_token_map_24, &m_tblCoeffToken[1]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanRunLevelTableInitAlloc_32s(coeff_token_map_48, &m_tblCoeffToken[2]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanRunLevelTableInitAlloc_32s(coeff_token_map_cr, &m_tblCoeffToken[3]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanRunLevelTableInitAlloc_32s(coeff_token_map_cr2, &m_tblCoeffToken[4]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    //
    // TotalZeros tables allocation
    //

    ippSts = ippiHuffmanTableInitAlloc_32s(total_zeros_map_cr1, &m_tblTotalZerosCR[1]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(total_zeros_map_cr2, &m_tblTotalZerosCR[2]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(total_zeros_map_cr3, &m_tblTotalZerosCR[3]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(total_zeros_map_cr422_1, &m_tblTotalZerosCR422[1]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(total_zeros_map_cr422_2, &m_tblTotalZerosCR422[2]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(total_zeros_map_cr422_3, &m_tblTotalZerosCR422[3]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(total_zeros_map_cr422_4, &m_tblTotalZerosCR422[4]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(total_zeros_map_cr422_5, &m_tblTotalZerosCR422[5]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(total_zeros_map_cr422_6, &m_tblTotalZerosCR422[6]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(total_zeros_map_cr422_7, &m_tblTotalZerosCR422[7]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(total_zeros_map_1, &m_tblTotalZeros[1]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(total_zeros_map_2, &m_tblTotalZeros[2]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(total_zeros_map_3, &m_tblTotalZeros[3]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(total_zeros_map_4, &m_tblTotalZeros[4]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(total_zeros_map_5, &m_tblTotalZeros[5]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(total_zeros_map_6, &m_tblTotalZeros[6]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(total_zeros_map_7, &m_tblTotalZeros[7]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(total_zeros_map_8, &m_tblTotalZeros[8]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(total_zeros_map_9, &m_tblTotalZeros[9]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(total_zeros_map_10, &m_tblTotalZeros[10]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(total_zeros_map_11, &m_tblTotalZeros[11]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(total_zeros_map_12, &m_tblTotalZeros[12]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(total_zeros_map_13, &m_tblTotalZeros[13]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(total_zeros_map_14, &m_tblTotalZeros[14]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(total_zeros_map_15, &m_tblTotalZeros[15]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    //
    // Run Befores tables allocation
    //

    ippSts = ippiHuffmanTableInitAlloc_32s(run_before_map_1, &m_tblRunBefore[1]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(run_before_map_2, &m_tblRunBefore[2]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(run_before_map_3, &m_tblRunBefore[3]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(run_before_map_4, &m_tblRunBefore[4]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(run_before_map_5, &m_tblRunBefore[5]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(run_before_map_6, &m_tblRunBefore[6]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    ippSts = ippiHuffmanTableInitAlloc_32s(run_before_map_6p, &m_tblRunBefore[7]);
    if (ippStsNoErr != ippSts)
        return UMC_ERR_ALLOC;

    m_tblRunBefore[8]  = m_tblRunBefore[7];
    m_tblRunBefore[9]  = m_tblRunBefore[7];
    m_tblRunBefore[10] = m_tblRunBefore[7];
    m_tblRunBefore[11] = m_tblRunBefore[7];
    m_tblRunBefore[12] = m_tblRunBefore[7];
    m_tblRunBefore[13] = m_tblRunBefore[7];
    m_tblRunBefore[14] = m_tblRunBefore[7];
    m_tblRunBefore[15] = m_tblRunBefore[7];

    m_bTablesInited = true;

    return UMC_OK;

} // Status H264Bitstream::InitTables(void)

inline
Ipp32u H264Bitstream::DecodeSingleBinOnes_CABAC(Ipp32u ctxIdx,
                                                Ipp32s &binIdx)
{
    // See subclause 9.3.3.2.1 of H.264 standard

    Ipp32u pStateIdx = context_array[ctxIdx].pStateIdxAndVal;
    Ipp32u codIOffset = m_lcodIOffset;
    Ipp32u codIRange = m_lcodIRange;
    Ipp32u codIRangeLPS;
    Ipp32u binVal;

    do
    {
        binIdx += 1;
        codIRangeLPS = rangeTabLPS[pStateIdx][(codIRange >> (6 + CABAC_MAGIC_BITS)) - 4];
        codIRange -= codIRangeLPS << CABAC_MAGIC_BITS;

        // most probably state.
        // it is more likely to decode most probably value.
        if (codIOffset < codIRange)
        {
            binVal = pStateIdx & 1;
            pStateIdx = transIdxMPS[pStateIdx];
        }
        else
        {
            codIOffset -= codIRange;
            codIRange = codIRangeLPS << CABAC_MAGIC_BITS;

            binVal = (pStateIdx & 1) ^ 1;
            pStateIdx = transIdxLPS[pStateIdx];
        }

        // Renormalization process
        // See subclause 9.3.3.2.2 of H.264
        {
            Ipp32u numBits = NumBitsToGetTbl[codIRange >> CABAC_MAGIC_BITS];

            codIRange <<= numBits;
            codIOffset <<= numBits;

#if (CABAC_MAGIC_BITS > 0)
            m_iMagicBits -= numBits;
            if (0 >= m_iMagicBits)
                RefreshCABACBits(codIOffset, m_pMagicBits, m_iMagicBits);
#else // !(CABAC_MAGIC_BITS > 0)
            codIOffset |= GetBits(numBits);
#endif // (CABAC_MAGIC_BITS > 0)
        }

#ifdef STORE_CABAC_BITS
        PRINT_CABAC_VALUES(binVal, codIRange>>CABAC_MAGIC_BITS);
#endif

    } while (binVal && (binIdx < 14));

    context_array[ctxIdx].pStateIdxAndVal = (Ipp8u) pStateIdx;
    m_lcodIOffset = codIOffset;
    m_lcodIRange = codIRange;

    return binVal;

} // Ipp32u H264Bitstream::DecodeSingleBinOnes_CABAC(Ipp32u ctxIdx,

inline
Ipp32u H264Bitstream::DecodeBypassOnes_CABAC(void)
{
    // See subclause 9.3.3.2.3 of H.264 standard
    Ipp32u binVal;// = 0;
    Ipp32u binCount = 0;
    Ipp32u codIOffset = m_lcodIOffset;
    Ipp32u codIRange = m_lcodIRange;

    do
    {
#if (CABAC_MAGIC_BITS > 0)
        codIOffset = (codIOffset << 1);

        m_iMagicBits -= 1;
        if (0 >= m_iMagicBits)
            RefreshCABACBits(codIOffset, m_pMagicBits, m_iMagicBits);
#else // !(CABAC_MAGIC_BITS > 0)
        codIOffset = (codIOffset << 1) | Get1Bit();
#endif // (CABAC_MAGIC_BITS > 0)

        Ipp32s mask = ((Ipp32s)(codIRange)-1-(Ipp32s)(codIOffset))>>31;
        // conditionally negate level
        binVal = mask & 1;
        binCount += binVal;
        // conditionally subtract range from offset
        codIOffset -= codIRange & mask;

        if (binCount > 16) // too large prefix part
            throw h264_exception(UMC_ERR_INVALID_STREAM);

    } while(binVal);

    m_lcodIOffset = codIOffset;
    m_lcodIRange = codIRange;

    return binCount;

} // Ipp32u H264Bitstream::DecodeBypassOnes_CABAC(void)

static
Ipp8u iCtxIdxIncTable[64][2] =
{
    {1, 0},
    {2, 0},
    {3, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0},
    {4, 0}
};

Ipp32s H264Bitstream::DecodeSignedLevel_CABAC(Ipp32u localCtxIdxOffset,
                                              Ipp32u &numDecodAbsLevelEq1,
                                              Ipp32u &numDecodAbsLevelGt1,
                                              Ipp32u max_value)
{
    // See subclause 9.3.2.3 of H.264
    Ipp32u ctxIdxInc;
    Ipp32s binIdx;

    // PREFIX BIN(S) STRING DECODING
    // decoding first bin of prefix bin string

    if (5 + numDecodAbsLevelGt1 < max_value)
    {
        ctxIdxInc = iCtxIdxIncTable[numDecodAbsLevelEq1][((Ipp32u) -((Ipp32s) numDecodAbsLevelGt1)) >> 31];

        if (0 == DecodeSingleBin_CABAC(localCtxIdxOffset + ctxIdxInc))
        {
            numDecodAbsLevelEq1 += 1;
            binIdx = 1;
        }
        else
        {
            Ipp32s binVal;

            // decoding next bin(s) of prefix bin string
            // we use Truncated Unary binarization with cMax = uCoff;
            ctxIdxInc = 5 + numDecodAbsLevelGt1;
            binIdx = 1;

            binVal = DecodeSingleBinOnes_CABAC(localCtxIdxOffset + ctxIdxInc, binIdx);

            // SUFFIX BIN(S) STRING DECODING

            // See subclause 9.1 of H.264 standard
            // we use Exp-Golomb code of 0-th order
            if (binVal)
            {
                Ipp32s leadingZeroBits;
                Ipp32s codeNum;

                // counting leading 1' before 0
                leadingZeroBits = DecodeBypassOnes_CABAC();

                // create codeNum
                codeNum = 1;
                while (leadingZeroBits--)
                    codeNum = (codeNum << 1) | DecodeBypass_CABAC();

                // update syntax element
                binIdx += codeNum;

            }

            numDecodAbsLevelGt1 += 1;
        }
    }
    else
    {
        if (0 == DecodeSingleBin_CABAC(localCtxIdxOffset + 0))
        {
            numDecodAbsLevelEq1 += 1;
            binIdx = 1;
        }
        else
        {
            Ipp32s binVal;

            // decoding next bin(s) of prefix bin string
            // we use Truncated Unary binarization with cMax = uCoff;
            binIdx = 1;

            binVal = DecodeSingleBinOnes_CABAC(localCtxIdxOffset + max_value, binIdx);

            // SUFFIX BIN(S) STRING DECODING

            // See subclause 9.1 of H.264 standard
            // we use Exp-Golomb code of 0-th order
            if (binVal)
            {
                Ipp32s leadingZeroBits;
                Ipp32s codeNum;

                // counting leading 1' before 0
                leadingZeroBits = DecodeBypassOnes_CABAC();

                // create codeNum
                codeNum = 1;
                while (leadingZeroBits--)
                    codeNum = (codeNum << 1) | DecodeBypass_CABAC();

                // update syntax element
                binIdx += codeNum;

            }

            numDecodAbsLevelGt1 += 1;
        }
    }

    {
        Ipp32s lcodIOffset = m_lcodIOffset;
        Ipp32s lcodIRange = m_lcodIRange;

        // See subclause 9.3.3.2.3 of H.264 standard
#if (CABAC_MAGIC_BITS > 0)
        // do shift on 1 bit
        lcodIOffset += lcodIOffset;

        {
            Ipp32s iMagicBits = m_iMagicBits;

            iMagicBits -= 1;
            if (0 >= iMagicBits)
                RefreshCABACBits(lcodIOffset, m_pMagicBits, iMagicBits);
            m_iMagicBits = iMagicBits;
        }
#else // !(CABAC_MAGIC_BITS > 0)
        lcodIOffset = (lcodIOffset << 1) | Get1Bit();
#endif // (CABAC_MAGIC_BITS > 0)

        Ipp32s mask = ((Ipp32s)(lcodIRange) - 1 - (Ipp32s)(lcodIOffset)) >> 31;
        // conditionally negate level
        binIdx = (binIdx ^ mask) - mask;
        // conditionally subtract range from offset
        lcodIOffset -= lcodIRange & mask;

        m_lcodIOffset = lcodIOffset;
    }

    return binIdx;

} //Ipp32s H264Bitstream::DecodeSignedLevel_CABAC(Ipp32s ctxIdxOffset, Ipp32s &numDecodAbsLevelEq1, Ipp32s &numDecodAbsLevelGt1)

//
// this is a limited version of the DecodeSignedLevel_CABAC function.
// it decodes single value per block.
//
Ipp32s H264Bitstream::DecodeSingleSignedLevel_CABAC(Ipp32u localCtxIdxOffset)
{
    // See subclause 9.3.2.3 of H.264
    Ipp32u ctxIdxInc;
    Ipp32s binIdx;

    // PREFIX BIN(S) STRING DECODING
    // decoding first bin of prefix bin string

    {
        ctxIdxInc = iCtxIdxIncTable[0][0];

        if (0 == DecodeSingleBin_CABAC(localCtxIdxOffset + ctxIdxInc))
        {
            binIdx = 1;
        }
        else
        {
            Ipp32s binVal;

            // decoding next bin(s) of prefix bin string
            // we use Truncated Unary binarization with cMax = uCoff;
            ctxIdxInc = 5;
            binIdx = 1;

            binVal = DecodeSingleBinOnes_CABAC(localCtxIdxOffset + ctxIdxInc, binIdx);

            // SUFFIX BIN(S) STRING DECODING

            // See subclause 9.1 of H.264 standard
            // we use Exp-Golomb code of 0-th order
            if (binVal)
            {
                Ipp32s leadingZeroBits;
                Ipp32s codeNum;

                // counting leading 1' before 0
                leadingZeroBits = DecodeBypassOnes_CABAC();

                // create codeNum
                codeNum = 1;
                while (leadingZeroBits--)
                    codeNum = (codeNum << 1) | DecodeBypass_CABAC();

                // update syntax element
                binIdx += codeNum;

            }
        }
    }

    {
        Ipp32s lcodIOffset = m_lcodIOffset;
        Ipp32s lcodIRange = m_lcodIRange;

        // See subclause 9.3.3.2.3 of H.264 standard
#if (CABAC_MAGIC_BITS > 0)
        // do shift on 1 bit
        lcodIOffset += lcodIOffset;

        {
            Ipp32s iMagicBits = m_iMagicBits;

            iMagicBits -= 1;
            if (0 >= iMagicBits)
                RefreshCABACBits(lcodIOffset, m_pMagicBits, iMagicBits);
            m_iMagicBits = iMagicBits;
        }
#else // !(CABAC_MAGIC_BITS > 0)
        lcodIOffset = (lcodIOffset << 1) | Get1Bit();
#endif // (CABAC_MAGIC_BITS > 0)

        Ipp32s mask = ((Ipp32s)(lcodIRange) - 1 - (Ipp32s)(lcodIOffset)) >> 31;
        // conditionally negate level
        binIdx = (binIdx ^ mask) - mask;
        // conditionally subtract range from offset
        lcodIOffset -= lcodIRange & mask;

        m_lcodIOffset = lcodIOffset;
    }

    return binIdx;

} //Ipp32s H264Bitstream::DecodeSingleSignedLevel_CABAC(Ipp32s ctxIdxOffset)

} // namespace UMC

#define IPP_NOERROR_RET()  return ippStsNoErr
#define IPP_ERROR_RET( ErrCode )  return (ErrCode)

#define IPP_BADARG_RET( expr, ErrCode )\
            {if (expr) { IPP_ERROR_RET( ErrCode ); }}

#define IPP_BAD_PTR1_RET( ptr )\
            IPP_BADARG_RET( NULL==(ptr), ippStsNullPtrErr )

static const Ipp32u vlc_inc[] = {0,3,6,12,24,48,96};

typedef struct{
    Ipp16s bits;
    Ipp16s offsets;
}  BitsAndOffsets;

static BitsAndOffsets bitsAndOffsets[7][16] = /*[level][numZeros]*/
{
/*         0  1  2  3  4  5  6  7  8  9  10 11 12 13 14 15        */
    /*0*/    {{0, 1}, {0, 1},  {0, 1},  {0, 1},  {0, 1},   {0, 1},   {0, 1},   {0, 1},   {0, 1},   {0, 1},   {0, 1},   {0, 1},   {0, 1},   {0, 1},   {4, 8},   {12, 16}, },
    /*1*/    {{1, 1}, {1, 2},  {1, 3},  {1, 4},  {1, 5},   {1, 6},   {1, 7},   {1, 8},   {1, 9},   {1, 10},  {1, 11},  {1, 12},  {1, 13},  {1, 14},  {1, 15},  {12, 16}, },
    /*2*/    {{2, 1}, {2, 3},  {2, 5},  {2, 7},  {2, 9},   {2, 11},  {2, 13},  {2, 15},  {2, 17},  {2, 19},  {2, 21},  {2, 23},  {2, 25},  {2, 27},  {2, 29},  {12, 31}, },
    /*3*/    {{3, 1}, {3, 5},  {3, 9},  {3, 13}, {3, 17},  {3, 21},  {3, 25},  {3, 29},  {3, 33},  {3, 37},  {3, 41},  {3, 45},  {3, 49},  {3, 53},  {3, 57},  {12, 61}, },
    /*4*/    {{4, 1}, {4, 9},  {4, 17}, {4, 25}, {4, 33},  {4, 41},  {4, 49},  {4, 57},  {4, 65},  {4, 73},  {4, 81},  {4, 89},  {4, 97},  {4, 105}, {4, 113}, {12, 121}, },
    /*5*/    {{5, 1}, {5, 17}, {5, 33}, {5, 49}, {5, 65},  {5, 81},  {5, 97},  {5, 113}, {5, 129}, {5, 145}, {5, 161}, {5, 177}, {5, 193}, {5, 209}, {5, 225}, {12, 241}, },
    /*6*/    {{6, 1}, {6, 33}, {6, 65}, {6, 97}, {6, 129}, {6, 161}, {6, 193}, {6, 225}, {6, 257}, {6, 289}, {6, 321}, {6, 353}, {6, 385}, {6, 417}, {6, 449}, {12, 481}  }
};


typedef struct{
    Ipp8u bits;

    union{
    Ipp8s q;
    Ipp8u qqqqq[3];
    };

} qq;

static Ipp32s sadd[7]={15,0,0,0,0,0,0};


inline
void _GetBlockCoeffs_CAVLC(Ipp32u ** const & ppBitStream,
                           Ipp32s * & pOffset,
                           Ipp32s uCoeffIndex,
                           Ipp32s sNumCoeff,
                           Ipp32s sNumTrailingOnes,
                           Ipp32s *CoeffBuf)
{
    Ipp32u uCoeffLevel = 0;

    /* 0..6, to select coding method used for each coeff */
    Ipp32s suffixLength = (sNumCoeff > 10) && (sNumTrailingOnes < 3) ? 1 : 0;

    /* When NumTrOnes is less than 3, need to add 1 to level of first coeff */
    Ipp32s uFirstAdjust = ((sNumTrailingOnes < 3) ? 1 : 0);

    Ipp32s NumZeros = -1;
    for (Ipp32s w = 0; !w; NumZeros++)
    {
        GetBits1((*ppBitStream), (*pOffset), w);
    }

    if (15 >= NumZeros)
    {
        const BitsAndOffsets &q = bitsAndOffsets[suffixLength][NumZeros];

        if (q.bits)
        {
            h264GetBits((*ppBitStream), (*pOffset), q.bits, NumZeros);
        }

        uCoeffLevel = ((NumZeros>>1) + q.offsets + uFirstAdjust);

        CoeffBuf[uCoeffIndex] = ((NumZeros & 1) ? (-((signed) uCoeffLevel)) : (uCoeffLevel));
    }
    else
    {
        Ipp32u level_suffix;
        Ipp32u levelSuffixSize = NumZeros - 3;
        Ipp32s levelCode;

        h264GetBits((*ppBitStream), (*pOffset), levelSuffixSize, level_suffix);
        levelCode = ((IPP_MIN(15, NumZeros) << suffixLength) + level_suffix) + uFirstAdjust*2 + sadd[suffixLength];
        levelCode = (levelCode + (1 << levelSuffixSize) - 4096);

        CoeffBuf[uCoeffIndex] = ((levelCode & 1) ?
                                          ((-levelCode - 1) >> 1) :
                                          ((levelCode + 2) >> 1));

        uCoeffLevel = ABSOWN(CoeffBuf[uCoeffIndex]);
    }

    uCoeffIndex++;
    if (uCoeffLevel > 3)
        suffixLength = 2;
    else if (uCoeffLevel > vlc_inc[suffixLength])
       suffixLength++;

    /* read coeffs */
    for (; uCoeffIndex < sNumCoeff; uCoeffIndex++)
    {
        /* Get the number of leading zeros to determine how many more */
        /* bits to read. */
        Ipp32s zeros = -1;
        for (Ipp32s w = 0; !w; zeros++)
        {
            GetBits1((*ppBitStream), (*pOffset), w);
        }

        if (15 >= zeros)
        {
            const BitsAndOffsets &q = bitsAndOffsets[suffixLength][zeros];

            if (q.bits)
            {
                h264GetBits((*ppBitStream), (*pOffset), q.bits, zeros);
            }

            uCoeffLevel = ((zeros>>1) + q.offsets);

            CoeffBuf[uCoeffIndex] = ((zeros & 1) ? (-((signed) uCoeffLevel)) : (uCoeffLevel));
        }
        else
        {
            Ipp32u level_suffix;
            Ipp32u levelSuffixSize = zeros - 3;
            Ipp32s levelCode;

            h264GetBits((*ppBitStream), (*pOffset), levelSuffixSize, level_suffix);
            levelCode = ((IPP_MIN(15, zeros) << suffixLength) + level_suffix) + sadd[suffixLength];
            levelCode = (levelCode + (1 << levelSuffixSize) - 4096);

            CoeffBuf[uCoeffIndex] = ((levelCode & 1) ?
                                              ((-levelCode - 1) >> 1) :
                                              ((levelCode + 2) >> 1));

            uCoeffLevel = ABSOWN(CoeffBuf[uCoeffIndex]);
        }

        if (uCoeffLevel > vlc_inc[suffixLength] && suffixLength < 6)
        {
            suffixLength++;
        }
    }    /* for uCoeffIndex */

} /* static void _GetBlockCoeffs_CAVLC(Ipp32u **pbs, */

static Ipp8s trailing_ones[8][3] =
{
    {1, 1, 1},    // 0, 0, 0
    {1, 1, -1},   // 0, 0, 1
    {1, -1, 1},   // 0, 1, 0
    {1, -1, -1},  // 0, 1, 1
    {-1, 1, 1},   // 1, 0, 0
    {-1, 1, -1},  // 1, 0, 1
    {-1, -1, 1},  // 1, 1, 0
    {-1, -1, -1}, // 1, 1, 1
};

static Ipp8s trailing_ones1[8][3] =
{
    {1, 1, 1},    // 0, 0, 0
    {-1, 1, 1},   // 0, 0, 1
    {1, -1, 1},   // 0, 1, 0
    {-1, -1, 1},  // 0, 1, 1
    {1, 1, -1},   // 1, 0, 0
    {-1, 1, -1},  // 1, 0, 1
    {1, -1, -1},  // 1, 1, 0
    {-1, -1, -1}, // 1, 1, 1
};

static Ipp32s bitsToGetTbl16s[7][16] = /*[level][numZeros]*/
{
/*         0  1  2  3  4  5  6  7  8  9  10 11 12 13 14 15        */
/*0*/    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 12, },
/*1*/    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 12, },
/*2*/    {2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 12, },
/*3*/    {3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 12, },
/*4*/    {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 12, },
/*5*/    {5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 12, },
/*6*/    {6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 12  }
};
static Ipp32s addOffsetTbl16s[7][16] = /*[level][numZeros]*/
{
/*         0   1   2   3   4   5   6   7   8   9   10  11  12  13  14  15    */
/*0*/    {1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  8,  16,},
/*1*/    {1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16,},
/*2*/    {1,  3,  5,  7,  9,  11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31,},
/*3*/    {1,  5,  9,  13, 17, 21, 25, 29, 33, 37, 41, 45, 49, 53, 57, 61,},
/*4*/    {1,  9,  17, 25, 33, 41, 49, 57, 65, 73, 81, 89, 97, 105,113,121,},
/*5*/    {1,  17, 33, 49, 65, 81, 97, 113,129,145,161,177,193,209,225,241,},
/*6*/    {1,  33, 65, 97, 129,161,193,225,257,289,321,353,385,417,449,481,}
};

#define OWNV_ABS(v) \
    (((v) >= 0) ? (v) : -(v))

IppStatus ownippiDecodeCAVLCCoeffs_H264_1u16s (Ipp32u **ppBitStream,
                                                     Ipp32s *pOffset,
                                                     Ipp16s *pNumCoeff,
                                                     Ipp16s **ppPosCoefbuf,
                                                     Ipp32u uVLCSelect,
                                                     Ipp16s uMaxNumCoeff,
                                                     const Ipp32s **ppTblCoeffToken,
                                                     const Ipp32s **ppTblTotalZeros,
                                                     const Ipp32s **ppTblRunBefore,
                                                     const Ipp32s *pScanMatrix) /* buffer to return up to 16 */

{
    Ipp16s        CoeffBuf[16] = {};    /* Temp buffer to hold coeffs read from bitstream*/
    Ipp32u        uVLCIndex        = 2;
    Ipp32u        uCoeffIndex        = 0;
    Ipp32s        sTotalZeros        = 0;
    Ipp32s        sFirstPos        = 16 - uMaxNumCoeff;
    Ipp32u        TrOneSigns = 0;        /* return sign bits (1==neg) in low 3 bits*/
    Ipp32u        uTR1Mask;
    Ipp32s        pos;
    Ipp32s        sRunBefore;
    Ipp16s        sNumTrailingOnes;
    Ipp32s        sNumCoeff = 0;
    Ipp32u        table_bits;
    Ipp8u         code_len;
    Ipp32s        i;
    register Ipp32u  table_pos;
    register Ipp32s  val;
    Ipp32u *pbsBackUp;
    Ipp32s bsoffBackUp;

    /* check error(s) */
    //IPP_BAD_PTR4_RET(ppBitStream,pOffset,ppPosCoefbuf,pNumCoeff)
    //IPP_BAD_PTR4_RET(ppTblCoeffToken,ppTblTotalZeros,ppTblRunBefore,pScanMatrix)
    //IPP_BAD_PTR2_RET(*ppBitStream, *ppPosCoefbuf)
    IPP_BADARG_RET(((sFirstPos != 0 && sFirstPos != 1) || (Ipp32s)uVLCSelect < 0), ippStsOutOfRangeErr)

    /* make a bit-stream backup */
    pbsBackUp = *ppBitStream;
    bsoffBackUp = *pOffset;

    if (uVLCSelect > 7)
    {
        /* fixed length code 4 bits numCoeff and */
        /* 2 bits for TrailingOnes */
        h264GetBits((*ppBitStream), (*pOffset), 6, sNumCoeff);
        sNumTrailingOnes = (Ipp16s) (sNumCoeff & 3);
        sNumCoeff         = (sNumCoeff&0x3c)>>2;
        if (sNumCoeff == 0 && sNumTrailingOnes == 3)
            sNumTrailingOnes = 0;
        else
            sNumCoeff++;
    }
    else
    {
        const Ipp32s *pDecTable;
        /* Use one of 3 luma tables */
        if (uVLCSelect < 4)
            uVLCIndex = uVLCSelect>>1;

        /* check for the only codeword of all zeros */
        /*ippiDecodeVLCPair_32s(ppBitStream, pOffset, ppTblCoeffToken[uVLCIndex], */
        /*                                        &sNumTrailingOnes, &sNumCoeff); */

        pDecTable = ppTblCoeffToken[uVLCIndex];

        IPP_BAD_PTR1_RET(ppTblCoeffToken[uVLCIndex]);

        table_bits = *pDecTable;
        h264GetBits((*ppBitStream), (*pOffset), table_bits, table_pos);
        val = pDecTable[table_pos + 1];
        code_len = (Ipp8u) (val);

        while (code_len & 0x80)
        {
            val = val >> 8;
            table_bits = pDecTable[val];
            h264GetBits((*ppBitStream), (*pOffset), table_bits, table_pos);
            val = pDecTable[table_pos + val + 1];
            code_len = (Ipp8u) (val & 0xff);
        }

        h264UngetNBits((*ppBitStream), (*pOffset), code_len);

        if ((val>>8) == IPPVC_VLC_FORBIDDEN)
        {
             *ppBitStream = pbsBackUp;
             *pOffset = bsoffBackUp;

             return ippStsH263VLCCodeErr;
        }

        sNumTrailingOnes  = (Ipp16s) ((val >> 8) & 0xff);
        sNumCoeff = (val >> 16) & 0xff;
    }

    *pNumCoeff = (Ipp16s) sNumCoeff;

    if (sNumTrailingOnes)
    {
        h264GetBits((*ppBitStream), (*pOffset), sNumTrailingOnes, TrOneSigns);
        uTR1Mask = 1 << (sNumTrailingOnes - 1);
        while (uTR1Mask)
        {
            CoeffBuf[uCoeffIndex++] = (Ipp16s) ((TrOneSigns & uTR1Mask) == 0 ? 1 : -1);
            uTR1Mask >>= 1;
        }
    }
    if (sNumCoeff)
    {
#ifdef __ICL
#pragma vector always
#endif
        for (i = 0; i < 16; i++)
            (*ppPosCoefbuf)[i] = 0;

        /* Get the sign bits of any trailing one coeffs */
        /* and put signed coeffs to the buffer */
        /* Get nonzero coeffs which are not Tr1 coeffs */
        if (sNumCoeff > sNumTrailingOnes)
        {
            /*_GetBlockCoeffs_CAVLC(ppBitStream, pOffset,sNumCoeff,*/
            /*                             sNumTrailingOnes, &CoeffBuf[uCoeffIndex]); */
            Ipp16u suffixLength = 0;        /* 0..6, to select coding method used for each coeff */
            Ipp16s lCoeffIndex;
            Ipp16u uCoeffLevel = 0;
            Ipp32s NumZeros;
            Ipp16u uBitsToGet;
            Ipp16u uFirstAdjust;
            Ipp16u uLevelOffset;
            Ipp32s w;
            Ipp16s    *lCoeffBuf = &CoeffBuf[uCoeffIndex];

            if ((sNumCoeff > 10) && (sNumTrailingOnes < 3))
                suffixLength = 1;

            /* When NumTrOnes is less than 3, need to add 1 to level of first coeff */
            uFirstAdjust = (Ipp16u) ((sNumTrailingOnes < 3) ? 1 : 0);

            /* read coeffs */
            for (lCoeffIndex = 0; lCoeffIndex<(sNumCoeff - sNumTrailingOnes); lCoeffIndex++)
            {
                /* update suffixLength */
                if ((lCoeffIndex == 1) && (uCoeffLevel > 3))
                    suffixLength = 2;
                else if (suffixLength < 6)
                {
                    if (uCoeffLevel > vlc_inc[suffixLength])
                        suffixLength++;
                }

                /* Get the number of leading zeros to determine how many more */
                /* bits to read. */
                NumZeros = -1;
                for (w = 0; !w; NumZeros++)
                {
                    GetBits1((*ppBitStream), (*pOffset), w);

                    if (NumZeros > 32)
                    {
                         *ppBitStream = pbsBackUp;
                         *pOffset = bsoffBackUp;

                         return ippStsH263VLCCodeErr;
                    }
                }

                if (15 >= NumZeros)
                {
                    uBitsToGet = (Ipp16s) (bitsToGetTbl16s[suffixLength][NumZeros]);
                    uLevelOffset = (Ipp16u) (addOffsetTbl16s[suffixLength][NumZeros]);

                    if (uBitsToGet)
                    {
                        h264GetBits((*ppBitStream), (*pOffset), uBitsToGet, NumZeros);
                    }

                    uCoeffLevel = (Ipp16u) ((NumZeros>>1) + uLevelOffset + uFirstAdjust);

                    lCoeffBuf[lCoeffIndex] = (Ipp16s) ((NumZeros & 1) ? (-((signed) uCoeffLevel)) : (uCoeffLevel));
                }
                else
                {
                    Ipp32u level_suffix;
                    Ipp32u levelSuffixSize = NumZeros - 3;
                    Ipp32s levelCode;

                    h264GetBits((*ppBitStream), (*pOffset), levelSuffixSize, level_suffix);
                    levelCode = (Ipp16u) ((IPP_MIN(15, NumZeros) << suffixLength) + level_suffix) + uFirstAdjust*2 + sadd[suffixLength];
                    levelCode = (Ipp16u) (levelCode + (1 << (NumZeros - 3)) - 4096);

                    lCoeffBuf[lCoeffIndex] = (Ipp16s) ((levelCode & 1) ?
                                                      ((-levelCode - 1) >> 1) :
                                                      ((levelCode + 2) >> 1));

                    uCoeffLevel = (Ipp16u) OWNV_ABS(lCoeffBuf[lCoeffIndex]);
                }

                uFirstAdjust = 0;

            }    /* for uCoeffIndex */

        }
        /* Get TotalZeros if any */
        if (sNumCoeff < uMaxNumCoeff)
        {
            /*ippiVCHuffmanDecodeOne_1u32s(ppBitStream, pOffset,&sTotalZeros, */
            /*                                                ppTblTotalZeros[sNumCoeff]); */
            const Ipp32s *pDecTable = ppTblTotalZeros[sNumCoeff];

            IPP_BAD_PTR1_RET(ppTblTotalZeros[sNumCoeff]);

            table_bits = pDecTable[0];
            h264GetBits((*ppBitStream), (*pOffset), table_bits, table_pos)

            val = pDecTable[table_pos + 1];
            code_len = (Ipp8u) (val & 0xff);
            val = val >> 8;

            while (code_len & 0x80)
            {
                table_bits = pDecTable[val];
                h264GetBits((*ppBitStream), (*pOffset), table_bits, table_pos)

                val = pDecTable[table_pos + val + 1];
                code_len = (Ipp8u) (val & 0xff);
                val = val >> 8;
            }

            if (val == IPPVC_VLC_FORBIDDEN)
            {
                *ppBitStream = pbsBackUp;
                *pOffset = bsoffBackUp;

                return ippStsH263VLCCodeErr;
            }

            h264UngetNBits((*ppBitStream), (*pOffset), code_len)

            sTotalZeros = val;
        }

        uCoeffIndex = 0;
        while (sNumCoeff)
        {
            /* Get RunBerore if any */
            if ((sNumCoeff > 1) && (sTotalZeros > 0))
            {
                /*ippiVCHuffmanDecodeOne_1u32s(ppBitStream, pOffset,&sRunBefore, */
                /*                                                ppTblRunBefore[sTotalZeros]); */
                const Ipp32s *pDecTable = ppTblRunBefore[sTotalZeros];

                IPP_BAD_PTR1_RET(ppTblRunBefore[sTotalZeros]);

                table_bits = pDecTable[0];
                h264GetBits((*ppBitStream), (*pOffset), table_bits, table_pos)

                val           = pDecTable[table_pos  + 1];
                code_len   = (Ipp8u) (val & 0xff);
                val        = val >> 8;


                while (code_len & 0x80)
                {
                    table_bits = pDecTable[val];
                    h264GetBits((*ppBitStream), (*pOffset), table_bits, table_pos)

                    val           = pDecTable[table_pos + val + 1];
                    code_len   = (Ipp8u) (val & 0xff);
                    val        = val >> 8;
                }

                if (val == IPPVC_VLC_FORBIDDEN)
                {
                    *ppBitStream = pbsBackUp;
                    *pOffset = bsoffBackUp;

                    return ippStsH263VLCCodeErr;
                }

                h264UngetNBits((*ppBitStream), (*pOffset),code_len)

                sRunBefore =  val;
            }
            else
                sRunBefore = sTotalZeros;

            /*Put coeff to the buffer */
            pos             = sNumCoeff - 1 + sTotalZeros + sFirstPos;
            sTotalZeros -= IPP_MIN(sTotalZeros, sRunBefore);
            pos             = pScanMatrix[pos];

            (*ppPosCoefbuf)[pos] = CoeffBuf[(uCoeffIndex++)&0x0f];
            sNumCoeff--;
        }
        (*ppPosCoefbuf) += 16;
    }

    return ippStsNoErr;

}

IppStatus MyippiDecodeCAVLCCoeffs_H264_1u16s (Ipp32u ** const ppBitStream,
                                                     Ipp32s *pOffset,
                                                     Ipp16s *pNumCoeff,
                                                     Ipp16s **ppPosCoefbuf,
                                                     Ipp32u uVLCSelect,
                                                     Ipp16s ,
                                                     Ipp16s uMaxNumCoeff,
                                                     const Ipp32s *pScanMatrix,
                                                     Ipp32s )

{
    Ipp32s **ppTblTotalZeros;

    if (uMaxNumCoeff <= 4)
    {
        ppTblTotalZeros = UMC::H264Bitstream::m_tblTotalZerosCR;

    }
    else if (uMaxNumCoeff <= 8)
    {
        ppTblTotalZeros = UMC::H264Bitstream::m_tblTotalZerosCR422;
    }
    else
    {
        ppTblTotalZeros = UMC::H264Bitstream::m_tblTotalZeros;
    }

    return ownippiDecodeCAVLCCoeffs_H264_1u16s(ppBitStream,
                                                     pOffset,
                                                     pNumCoeff,
                                                     ppPosCoefbuf,
                                                     uVLCSelect,
                                                     uMaxNumCoeff,
                                                     (const Ipp32s **) UMC::H264Bitstream::m_tblCoeffToken,
                                                     (const Ipp32s **) UMC::H264Bitstream::m_tblTotalZeros,
                                                     (const Ipp32s **) UMC::H264Bitstream::m_tblRunBefore,
                                                     pScanMatrix);


#if 0
#if 1
    return own_ippiDecodeCAVLCCoeffsIdxs_H264_1u16s(ppBitStream,
                                                     pOffset,
                                                     pNumCoeff,
                                                     ppPosCoefbuf,
                                                     uVLCSelect,
                                                     uMaxNumCoeff,
                                                     (const Ipp32s **) UMC::H264Bitstream::m_tblCoeffToken,
                                                     (const Ipp32s **) ppTblTotalZeros,
                                                     (const Ipp32s **) UMC::H264Bitstream::m_tblRunBefore,
                                                     pScanMatrix,
                                                     scanIdxStart,
                                                     coeffLimit + scanIdxStart - 1);
#else
    Ipp32s        CoeffBuf[16] = {};    /* Temp buffer to hold coeffs read from bitstream*/
    Ipp32s        uVLCIndex        = 2;
    Ipp32s        sTotalZeros        = 0;
    Ipp32s        sRunBefore;
    Ipp32s        sNumTrailingOnes;
    Ipp32s        sNumCoeff = 0;

    if (uVLCSelect > 7)
    {
        /* fixed length code 4 bits numCoeff and */
        /* 2 bits for TrailingOnes */
        ippiGetNBits((*ppBitStream), (*pOffset), 6, sNumCoeff);
        sNumTrailingOnes = (sNumCoeff & 3);
        sNumCoeff         = (sNumCoeff&0x3c)>>2;
        if (sNumCoeff == 0 && sNumTrailingOnes == 3)
            sNumTrailingOnes = 0;
        else
            sNumCoeff++;
    }
    else
    {
        /* Use one of 3 luma tables */
        if (uVLCSelect < 4)
            uVLCIndex = uVLCSelect>>1;

        /* check for the only codeword of all zeros */

        const Ipp32s *pDecTable = UMC::H264Bitstream::m_tblCoeffToken[uVLCIndex];

        //IPP_BAD_PTR1_RET(ppTblCoeffToken[uVLCIndex]);

        Ipp32s table_pos;
        Ipp32s table_bits = *pDecTable;
        ippiGetNBits((*ppBitStream), (*pOffset), table_bits, table_pos);
        Ipp32s val = pDecTable[table_pos + 1];
        Ipp8u code_len = (Ipp8u)val;

        while (code_len & 0x80)
        {
            val = val >> 8;
            table_bits = pDecTable[val];
            ippiGetNBits((*ppBitStream), (*pOffset), table_bits, table_pos);
            val = pDecTable[table_pos + val + 1];
            code_len = (Ipp8u)(val & 0xff);
        }

        ippiUngetNBits((*ppBitStream), (*pOffset), code_len);

        sNumTrailingOnes  = ((val >> 8) & 0xff);
        sNumCoeff = (val >> 16) & 0xff;
    }

    *pNumCoeff = (Ipp16s)sNumCoeff;

    if (sNumCoeff)
    {
        Ipp32s uCoeffIndex = 0;

        if (sNumTrailingOnes)
        {
            Ipp32u TrOneSigns;
            ippiGetNBits((*ppBitStream), (*pOffset), sNumTrailingOnes, TrOneSigns);
            Ipp32u uTR1Mask = 1 << (sNumTrailingOnes - 1);
            while (uTR1Mask)
            {
                CoeffBuf[uCoeffIndex++] = ((TrOneSigns & uTR1Mask) == 0 ? 1 : -1);
                uTR1Mask >>= 1;
            }
        }
#ifdef __ICL
#pragma vector always
#endif
        //for (Ipp32s i = 0; i < 16; i++)
          //  (*ppPosCoefbuf)[i] = 0;

        memset(*ppPosCoefbuf, 0, 16*sizeof(Ipp16s));

        /* Get the sign bits of any trailing one coeffs */
        /* and put signed coeffs to the buffer */
        /* Get nonzero coeffs which are not Tr1 coeffs */
        if (sNumCoeff != sNumTrailingOnes)
        {
#if 1
            _GetBlockCoeffs_CAVLC(ppBitStream, pOffset, uCoeffIndex, sNumCoeff, sNumTrailingOnes, CoeffBuf);
#else
            Ipp32u uCoeffLevel = 0;

            /* 0..6, to select coding method used for each coeff */
            Ipp32s suffixLength = (sNumCoeff > 10) && (sNumTrailingOnes < 3) ? 1 : 0;

            /* When NumTrOnes is less than 3, need to add 1 to level of first coeff */
            Ipp32s uFirstAdjust = ((sNumTrailingOnes < 3) ? 1 : 0);

            if (suffixLength < 6)
            {
                if (uCoeffLevel > vlc_inc[suffixLength])
                    suffixLength++;
            }


            Ipp32s NumZeros = -1;
            for (Ipp32s w = 0; !w; NumZeros++)
            {
                ippiGetBits1((*ppBitStream), (*pOffset), w);
            }

            if (15 >= NumZeros)
            {
                const BitsAndOffsets &q = bitsAndOffsets[suffixLength][NumZeros];

                if (q.bits)
                {
                    ippiGetNBits((*ppBitStream), (*pOffset), q.bits, NumZeros);
                }

                uCoeffLevel = ((NumZeros>>1) + q.offsets + uFirstAdjust);

                CoeffBuf[uCoeffIndex] = ((NumZeros & 1) ? (-((signed) uCoeffLevel)) : (uCoeffLevel));
            }
            else
            {
                Ipp32u level_suffix;
                Ipp32u levelSuffixSize = NumZeros - 3;
                Ipp32s levelCode;

                ippiGetNBits((*ppBitStream), (*pOffset), levelSuffixSize, level_suffix);
                levelCode = ((IPP_MIN(15, NumZeros) << suffixLength) + level_suffix) + uFirstAdjust*2 + sadd[suffixLength];
                levelCode = (levelCode + (1 << levelSuffixSize) - 4096);

                CoeffBuf[uCoeffIndex] = ((levelCode & 1) ?
                                                  ((-levelCode - 1) >> 1) :
                                                  ((levelCode + 2) >> 1));

                uCoeffLevel = ABSOWN(CoeffBuf[uCoeffIndex]);
            }

            uCoeffIndex++;
            if (uCoeffLevel > 3)
                suffixLength = 2;
            else if (uCoeffLevel > vlc_inc[suffixLength])
               suffixLength++;

            /* read coeffs */
            for (; uCoeffIndex < sNumCoeff; uCoeffIndex++)
            {
                /* Get the number of leading zeros to determine how many more */
                /* bits to read. */
                Ipp32s NumZeros = -1;
                for (Ipp32s w = 0; !w; NumZeros++)
                {
                    ippiGetBits1((*ppBitStream), (*pOffset), w);
                }

                if (15 >= NumZeros)
                {
                    const BitsAndOffsets &q = bitsAndOffsets[suffixLength][NumZeros];

                    if (q.bits)
                    {
                        ippiGetNBits((*ppBitStream), (*pOffset), q.bits, NumZeros);
                    }

                    uCoeffLevel = ((NumZeros>>1) + q.offsets);

                    CoeffBuf[uCoeffIndex] = ((NumZeros & 1) ? (-((signed) uCoeffLevel)) : (uCoeffLevel));
                }
                else
                {
                    Ipp32u level_suffix;
                    Ipp32u levelSuffixSize = NumZeros - 3;
                    Ipp32s levelCode;

                    ippiGetNBits((*ppBitStream), (*pOffset), levelSuffixSize, level_suffix);
                    levelCode = ((IPP_MIN(15, NumZeros) << suffixLength) + level_suffix) + sadd[suffixLength];
                    levelCode = (levelCode + (1 << levelSuffixSize) - 4096);

                    CoeffBuf[uCoeffIndex] = ((levelCode & 1) ?
                                                      ((-levelCode - 1) >> 1) :
                                                      ((levelCode + 2) >> 1));

                    uCoeffLevel = ABSOWN(CoeffBuf[uCoeffIndex]);
                }

                if (uCoeffLevel > vlc_inc[suffixLength] && suffixLength < 6)
                {
                    suffixLength++;
                }
            }    /* for uCoeffIndex */
#endif
        }

        uCoeffIndex = 0;

        Ipp32s sFirstPos        = 16 - uMaxNumCoeff;

        /* Get TotalZeros if any */
        if (sNumCoeff != uMaxNumCoeff)
        {
            const Ipp32s *pDecTable = UMC::H264Bitstream::m_tblTotalZeros[sNumCoeff];

            Ipp32s table_pos;
            Ipp32s table_bits = pDecTable[0];
            ippiNextBits((*ppBitStream), (*pOffset), table_bits, table_pos)

            const qq * qqq = (qq*)(&pDecTable[table_pos  + 1]);

            ippiSkipNBits((*ppBitStream), (*pOffset), table_bits - qqq->bits)

            sTotalZeros = qqq->q;

            for (; sTotalZeros > 0 && sNumCoeff > 1; sNumCoeff--)
            {
                const Ipp32s *pDecTable = UMC::H264Bitstream::m_tblRunBefore[sTotalZeros];
                Ipp32s table_pos;
                Ipp32s table_bits = pDecTable[0];

                ippiNextBits((*ppBitStream), (*pOffset), table_bits, table_pos)

                const qq * qqq = (qq*)(&pDecTable[table_pos  + 1]);

                ippiSkipNBits((*ppBitStream), (*pOffset), table_bits - qqq->bits)

                sRunBefore =  qqq->q;

                Ipp32s pos  = sNumCoeff - 1 + sTotalZeros + sFirstPos;
                pos         = pScanMatrix[pos];

                (*ppPosCoefbuf)[pos] = (Ipp16s)CoeffBuf[uCoeffIndex++];

                sTotalZeros -= sRunBefore;
            }
        }

        for (; sNumCoeff; sNumCoeff--)
        {
            Ipp32s pos  = sNumCoeff - 1 + sTotalZeros + sFirstPos;
            pos         = pScanMatrix[pos];
            (*ppPosCoefbuf)[pos] = (Ipp16s)CoeffBuf[uCoeffIndex++];
        }

        (*ppPosCoefbuf) += 16;
    }

    return ippStsNoErr;
#endif
#endif
} /* IPPFUN(IppStatus, ippiDecodeCAVLCCoeffs_H264_1u16s, (Ipp32u **ppBitStream, */

#define h264PeekBitsNoCheck(current_data, offset, nbits, data) \
{ \
    Ipp32u x; \
    x = current_data[0] >> (offset - nbits + 1); \
    (data) = x; \
}

#define h264PeekBits(current_data, offset, nbits, data) \
{ \
    Ipp32u x; \
    Ipp32s off; \
    /*removeSCEBP(current_data, offset);*/ \
    off = offset - (nbits); \
    if (off >= 0) \
    { \
        x = (current_data[0] >> (off + 1)); \
    } \
    else \
    { \
        x = (current_data[1] >> (off + 32)); \
        x >>= 1; \
        x |= (current_data[0] << (- 1 + -off)); \
    } \
    (data) = x; \
}

#define h264DropBits(current_data, offset, nbits) \
{ \
    offset -= (nbits); \
    if (offset < 0) \
    { \
        offset += 32; \
        current_data++; \
    } \
}

static
Ipp8s ChromaDCRunTable[] =
{
    3, 5,
    2, 5,
    1, 4, 1, 4,
    0, 3, 0, 3, 0, 3, 0, 3
};

static
IppStatus _GetBlockCoeffs_CAVLC(Ipp32u **pbs,
                           Ipp32s *bitOffset,
                           Ipp16s sNumCoeff,
                           Ipp16s sNumTrOnes,
                           Ipp16s *CoeffBuf)
{
    Ipp16u suffixLength = 0;        /* 0..6, to select coding method used for each coeff */
    Ipp16s uCoeffIndex;
    Ipp32u uCoeffLevel = 0;
    Ipp32s NumZeros;
    Ipp16u uBitsToGet;
    Ipp16u uFirstAdjust;
    Ipp16u uLevelOffset;
    Ipp32s w;

    if ((sNumCoeff > 10) && (sNumTrOnes < 3))
        suffixLength = 1;

    /* When NumTrOnes is less than 3, need to add 1 to level of first coeff */
    uFirstAdjust = (Ipp16u)((sNumTrOnes < 3) ? 1 : 0);

    /* read coeffs */
    for (uCoeffIndex = 0; uCoeffIndex<(sNumCoeff - sNumTrOnes); uCoeffIndex++)
    {
        /* update suffixLength */
        if ((uCoeffIndex == 1) && (uCoeffLevel > 3))
            suffixLength = 2;
        else if (suffixLength < 6)
        {
            if (uCoeffLevel > vlc_inc[suffixLength])
                suffixLength++;
        }

        /* Get the number of leading zeros to determine how many more */
        /* bits to read. */
        NumZeros = -1;
        for(w = 0; !w; NumZeros++)
        {
            GetBits1((*pbs), (*bitOffset), w);
            if (NumZeros > 32)
            {
                return ippStsH263VLCCodeErr;
            }
        }

        if (15 >= NumZeros)
        {
            uBitsToGet     = (Ipp16u)(bitsToGetTbl16s[suffixLength][NumZeros]);
            uLevelOffset   = (Ipp16u)(addOffsetTbl16s[suffixLength][NumZeros]);

            if (uBitsToGet)
            {
                h264GetBits((*pbs), (*bitOffset), uBitsToGet, NumZeros);
            }

            uCoeffLevel = (NumZeros>>1) + uLevelOffset + uFirstAdjust;

            CoeffBuf[uCoeffIndex] = (Ipp16s) ((NumZeros & 1) ? (-((signed) uCoeffLevel)) : (uCoeffLevel));
        }
        else
        {
            Ipp32u level_suffix;
            Ipp32u levelSuffixSize = NumZeros - 3;
            Ipp32s levelCode;

            h264GetBits((*pbs), (*bitOffset), levelSuffixSize, level_suffix);
            levelCode = (Ipp16u) ((IPP_MIN(15, NumZeros) << suffixLength) + level_suffix) + uFirstAdjust*2 + sadd[suffixLength];
            levelCode = (Ipp16u) (levelCode + (1 << (NumZeros - 3)) - 4096);

            CoeffBuf[uCoeffIndex] = (Ipp16s) ((levelCode & 1) ?
                                              ((-levelCode - 1) >> 1) :
                                              ((levelCode + 2) >> 1));

            uCoeffLevel = OWNV_ABS(CoeffBuf[uCoeffIndex]);
        }

        uFirstAdjust = 0;

    } /* for uCoeffIndex*/

    return ippStsNoErr;

} /* static void _GetBlockCoeffs_CAVLC(Ipp32u **pbs, */

IppStatus ownippiDecodeCAVLCChromaDcCoeffs_H264_1u16s (Ipp32u **ppBitStream,
                                                             Ipp32s *pOffset,
                                                             Ipp16s *pNumCoeff,
                                                             Ipp16s **ppPosCoefbuf,
                                                             const Ipp32s *pTblCoeffToken,
                                                             const Ipp32s **ppTblTotalZerosCR,
                                                             const Ipp32s **ppTblRunBefore)

{
    /* check the most frequently used parameters */
    //IPP_BAD_PTR3_RET(ppBitStream, pOffset, pNumCoeff)

    if (4 < *pOffset)
    {
        Ipp32u code;

        h264PeekBitsNoCheck((*ppBitStream), (*pOffset), 5, code);
        /* the shortes DC code */
        if (code & 0x10)
        {
            Ipp32s iValue;
            Ipp16s *pCoeffs;

            IPP_BAD_PTR1_RET(ppPosCoefbuf)

            /* advance coeffs buffer */
            pCoeffs = *ppPosCoefbuf;
            *ppPosCoefbuf += 4;
            /* save coeffs */
            pCoeffs[0] =
            pCoeffs[1] =
            pCoeffs[2] =
            pCoeffs[3] = 0;
            iValue = -(((((Ipp32s) code) >> 3) & 1) * 2 - 1);
            pCoeffs[ChromaDCRunTable[(code & 0x07) * 2]] = (Ipp16s) iValue;
            /* update variables */
            *pOffset -= ChromaDCRunTable[(code & 0x07) * 2 + 1];
            *pNumCoeff = 1;

            return ippStsNoErr;
        }
        /* the "end of block" code */
        else if (code & 0x08)
        {
            /* update variables */
            *pNumCoeff = 0;
            *pOffset -= 2;

            return ippStsNoErr;
        }
    }
    else
    {
        Ipp32u code;

        h264PeekBits((*ppBitStream), (*pOffset), 5, code);
        /* the shortes DC code */
        if (code & 0x10)
        {
            Ipp32s iValue;
            Ipp16s *pCoeffs;

            IPP_BAD_PTR1_RET(ppPosCoefbuf)

            /* advance coeffs buffer */
            pCoeffs = *ppPosCoefbuf;
            *ppPosCoefbuf += 4;
            /* save coeffs */
            pCoeffs[0] =
            pCoeffs[1] =
            pCoeffs[2] =
            pCoeffs[3] = 0;
            iValue = -(((((Ipp32s) code) >> 3) & 1) * 2 - 1);
            pCoeffs[ChromaDCRunTable[(code & 0x07) * 2]] = (Ipp16s) iValue;
            /* update variables */
            h264DropBits((*ppBitStream), (*pOffset), ChromaDCRunTable[(code & 0x07) * 2 + 1]);
            *pNumCoeff = 1;

            return ippStsNoErr;
        }
        /* the "end of block" code */
        else if (code & 0x08)
        {
            /* update variables */
            *pNumCoeff = 0;
            h264DropBits((*ppBitStream), (*pOffset), 2)

            return ippStsNoErr;
        }
    }

    {


    Ipp16s        CoeffBuf[16] = {};        /* Temp buffer to hold coeffs read from bitstream*/
    Ipp32u        uTR1Mask;
    Ipp32u        TrOneSigns;            /* return sign bits (1==neg) in low 3 bits*/
    Ipp32u        uCoeffIndex            = 0;
    Ipp32s        sTotalZeros            = 0;
    Ipp32s        sRunBefore;
    Ipp16s        sNumTrailingOnes;
    Ipp16s        sNumCoeff = 0;
    Ipp32s        pos;
    Ipp32s        i;

    /* check for the only codeword of all zeros*/


    /*ippiDecodeVLCPair_32s(ppBitStream, pOffset, pTblCoeffToken, */
    /*                              &sNumTrailingOnes,&sNumCoeff);*/
    register Ipp32s table_pos;
    register Ipp32s val;
    Ipp32u          table_bits;
    Ipp8u           code_len;
    Ipp32u *pbsBackUp;
    Ipp32s bsoffBackUp;

    /* check error(s) */
    //IPP_BAD_PTR4_RET(ppPosCoefbuf, pTblCoeffToken, ppTblTotalZerosCR, ppTblRunBefore)

    /* create bit stream backup */
    pbsBackUp = *ppBitStream;
    bsoffBackUp = *pOffset;

    table_bits = *pTblCoeffToken;
    h264GetBits((*ppBitStream), (*pOffset), table_bits, table_pos);
    val           = pTblCoeffToken[table_pos  + 1];
    code_len   = (Ipp8u) (val);

    while (code_len & 0x80)
    {
        val        = val >> 8;
        table_bits = pTblCoeffToken[val];
        h264GetBits((*ppBitStream), (*pOffset), table_bits, table_pos);
        val           = pTblCoeffToken[table_pos + val  + 1];
        code_len   = (Ipp8u) (val & 0xff);
    }

    h264UngetNBits((*ppBitStream), (*pOffset), code_len);

    if ((val>>8) == IPPVC_VLC_FORBIDDEN)
    {
         *ppBitStream = pbsBackUp;
         *pOffset    = bsoffBackUp;

         return ippStsH263VLCCodeErr;
    }
    sNumTrailingOnes  = (Ipp16s) ((val >> 8) &0xff);
    sNumCoeff = (Ipp16s) ((val >> 16) & 0xff);

    *pNumCoeff = sNumCoeff;

    if (sNumTrailingOnes)
    {
        h264GetBits((*ppBitStream), (*pOffset), sNumTrailingOnes, TrOneSigns);
        uTR1Mask = 1 << (sNumTrailingOnes - 1);
        while (uTR1Mask)
        {
            CoeffBuf[uCoeffIndex++] = (Ipp16s) ((TrOneSigns & uTR1Mask) == 0 ? 1 : -1);
            uTR1Mask >>= 1;
        }

    }
    /* Get the sign bits of any trailing one coeffs */
    if (sNumCoeff)
    {
        /*memset((*ppPosCoefbuf), 0, 4*sizeof(short)); */
#ifdef __ICL
#pragma vector always
#endif
        for (i = 0; i < 4; i++)
        {
            (*ppPosCoefbuf)[i] = 0;
        }
        /*((Ipp32s*)(*ppPosCoefbuf))[0] = 0; */
        /*((Ipp32s*)(*ppPosCoefbuf))[1] = 0; */
        /* get nonzero coeffs which are not Tr1 coeffs */
        if (sNumCoeff > sNumTrailingOnes)
        {
            IppStatus sts = _GetBlockCoeffs_CAVLC(ppBitStream, pOffset,sNumCoeff,
                                         sNumTrailingOnes, &CoeffBuf[uCoeffIndex]);

            if (sts != ippStsNoErr)
                return sts;

        }
        if (sNumCoeff < 4)
        {
            /*ippiVCHuffmanDecodeOne_1u32s(ppBitStream, pOffset,&sTotalZeros, */
            /*                                                ppTblTotalZerosCR[sNumCoeff]); */
            const Ipp32s *pDecTable = ppTblTotalZerosCR[sNumCoeff];

            IPP_BAD_PTR1_RET(ppTblTotalZerosCR[sNumCoeff])

            table_bits = pDecTable[0];
            h264GetBits((*ppBitStream), (*pOffset), table_bits, table_pos)

            val           = pDecTable[table_pos  + 1];
            code_len   = (Ipp8u) (val & 0xff);
            val        = val >> 8;


            while (code_len & 0x80)
            {
                table_bits = pDecTable[val];
                h264GetBits((*ppBitStream), (*pOffset), table_bits, table_pos)

                val           = pDecTable[table_pos + val + 1];
                code_len   = (Ipp8u) (val & 0xff);
                val        = val >> 8;
            }

            if (val == IPPVC_VLC_FORBIDDEN)
            {
                *ppBitStream = pbsBackUp;
                *pOffset = bsoffBackUp;

                return ippStsH263VLCCodeErr;
            }

            h264UngetNBits((*ppBitStream), (*pOffset),code_len)

            sTotalZeros =  val;

        }
        uCoeffIndex = 0;
        while (sNumCoeff)
        {
            if ((sNumCoeff > 1) && (sTotalZeros > 0))
            {
                /*ippiVCHuffmanDecodeOne_1u32s(ppBitStream, pOffset,&sRunBefore, */
                /*                                                ppTblRunBefore[sTotalZeros]); */
                const Ipp32s *pDecTable = ppTblRunBefore[sTotalZeros];

                IPP_BAD_PTR1_RET(ppTblRunBefore[sTotalZeros])

                table_bits = pDecTable[0];
                h264GetBits((*ppBitStream), (*pOffset), table_bits, table_pos)

                val           = pDecTable[table_pos  + 1];
                code_len   = (Ipp8u) (val & 0xff);
                val        = val >> 8;


                while (code_len & 0x80)
                {
                    table_bits = pDecTable[val];
                    h264GetBits((*ppBitStream), (*pOffset), table_bits, table_pos)

                    val           = pDecTable[table_pos + val + 1];
                    code_len   = (Ipp8u) (val & 0xff);
                    val        = val >> 8;
                }

                if (val == IPPVC_VLC_FORBIDDEN)
                {
                    *ppBitStream = pbsBackUp;
                    *pOffset = bsoffBackUp;

                    return ippStsH263VLCCodeErr;
                }

                h264UngetNBits((*ppBitStream), (*pOffset),code_len)

                sRunBefore =  val;
            }
            else
                sRunBefore = sTotalZeros;

            pos             = sNumCoeff - 1 + sTotalZeros;
            sTotalZeros -= sRunBefore;

            /* The coeff is either in CoeffBuf or is a trailing one */
            (*ppPosCoefbuf)[pos] = CoeffBuf[(uCoeffIndex++)&0x0f];

            sNumCoeff--;
        }
        (*ppPosCoefbuf) += 4;

    }

    }

    return ippStsNoErr;

} /* IPPFUN(IppStatus, ippiDecodeCAVLCChromaDcCoeffs_H264_1u16s , (Ipp32u **ppBitStream, */
IppStatus MyippiDecodeCAVLCChromaDcCoeffs_H264_1u16s(Ipp32u **ppBitStream,
                                                         Ipp32s *pOffset,
                                                         Ipp16s *pNumCoeff,
                                                         Ipp16s **ppPosCoefbuf)

{

#if 1
    return ownippiDecodeCAVLCChromaDcCoeffs_H264_1u16s (ppBitStream,
                                                         pOffset,
                                                         pNumCoeff,
                                                         ppPosCoefbuf,
                                                         UMC::H264Bitstream::m_tblCoeffToken[3],
                                                         (const Ipp32s **) UMC::H264Bitstream::m_tblTotalZerosCR,
                                                         (const Ipp32s **) UMC::H264Bitstream::m_tblRunBefore);
#else
    /* check the most frequently used parameters */
    //IPP_BAD_PTR3_RET(ppBitStream, pOffset, pNumCoeff)

#if 1
    if (4 < *pOffset)
    {
        Ipp32u code;

        h264PeekBitsNoCheck((*ppBitStream), (*pOffset), 5, code);
        /* the shortes DC code */
        if (code & 0x10)
        {
            Ipp32s iValue;
            Ipp16s *pCoeffs;

            /* advance coeffs buffer */
            pCoeffs = *ppPosCoefbuf;
            *ppPosCoefbuf += 4;
            /* save coeffs */
            pCoeffs[0] =
            pCoeffs[1] =
            pCoeffs[2] =
            pCoeffs[3] = 0;
            iValue = -(((((Ipp32s) code) >> 3) & 1) * 2 - 1);
            pCoeffs[ChromaDCRunTable[(code & 0x07) * 2]] = (Ipp16s) iValue;
            /* update variables */
            *pOffset -= ChromaDCRunTable[(code & 0x07) * 2 + 1];
            *pNumCoeff = 1;

            return ippStsNoErr;
        }
        /* the "end of block" code */
        else if (code & 0x08)
        {
            /* update variables */
            *pNumCoeff = 0;
            *pOffset -= 2;

            return ippStsNoErr;
        }
    }
    else
    {
        Ipp32u code;

        h264PeekBits((*ppBitStream), (*pOffset), 5, code);
        /* the shortes DC code */
        if (code & 0x10)
        {
            Ipp32s iValue;

            /* advance coeffs buffer */
            Ipp16s *pCoeffs = *ppPosCoefbuf;
            *ppPosCoefbuf += 4;
            /* save coeffs */
            pCoeffs[0] =
            pCoeffs[1] =
            pCoeffs[2] =
            pCoeffs[3] = 0;
            iValue = -(((((Ipp32s) code) >> 3) & 1) * 2 - 1);
            pCoeffs[ChromaDCRunTable[(code & 0x07) * 2]] = (Ipp16s) iValue;
            /* update variables */
            h264DropBits((*ppBitStream), (*pOffset), ChromaDCRunTable[(code & 0x07) * 2 + 1]);
            *pNumCoeff = 1;

            return ippStsNoErr;
        }
        /* the "end of block" code */
        else if (code & 0x08)
        {
            /* update variables */
            *pNumCoeff = 0;
            h264DropBits((*ppBitStream), (*pOffset), 2)

            return ippStsNoErr;
        }
    }
#endif
    {

    Ipp32s        CoeffBuf[16] = {};        /* Temp buffer to hold coeffs read from bitstream*/
    Ipp32u        TrOneSigns;            /* return sign bits (1==neg) in low 3 bits*/
    Ipp32u        uCoeffIndex            = 0;
    Ipp32s        sTotalZeros            = 0;
    Ipp32s        sRunBefore;
    Ipp32s        sNumTrailingOnes;
    Ipp32s        sNumCoeff = 0;
    Ipp32s        i;

    /* check for the only codeword of all zeros*/

    Ipp32s table_pos;
    Ipp32s val;
    Ipp32u table_bits;
    Ipp8u  code_len;

    const Ipp32s *pDecTable = UMC::H264Bitstream::m_tblCoeffToken[3];

    table_bits = *pDecTable;
    ippiGetNBits((*ppBitStream), (*pOffset), table_bits, table_pos);
    val = pDecTable[table_pos  + 1];
    code_len   = (Ipp8u) (val);

    while (code_len & 0x80)
    {
        val        = val >> 8;
        table_bits = pDecTable[val];
        ippiGetNBits((*ppBitStream), (*pOffset), table_bits, table_pos);
        val       = pDecTable[table_pos + val  + 1];
        code_len   = (Ipp8u) (val & 0xff);
    }

    ippiUngetNBits((*ppBitStream), (*pOffset), code_len);

    sNumTrailingOnes  = ((val >> 8) &0xff);
    sNumCoeff = ((val >> 16) & 0xff);

    *pNumCoeff = (Ipp16s)sNumCoeff;

    /* Get the sign bits of any trailing one coeffs */
    if (sNumCoeff)
    {
        if (sNumTrailingOnes)
        {
            ippiGetNBits((*ppBitStream), (*pOffset), sNumTrailingOnes, TrOneSigns);
            Ipp32u uTR1Mask = 1 << (sNumTrailingOnes - 1);
            while (uTR1Mask)
            {
                CoeffBuf[uCoeffIndex++] = ((TrOneSigns & uTR1Mask) == 0 ? 1 : -1);
                uTR1Mask >>= 1;
            }
        }

#ifdef __ICL
#pragma vector always
#endif
        for (i = 0; i < 4; i++)
            (*ppPosCoefbuf)[i] = 0;

        //memset((*ppPosCoefbuf), 0, 4*sizeof(Ipp16s));

        /* get nonzero coeffs which are not Tr1 coeffs */
        if (sNumCoeff > sNumTrailingOnes)
        {
            _GetBlockCoeffs_CAVLC(ppBitStream, pOffset, uCoeffIndex, sNumCoeff,
                                         sNumTrailingOnes, CoeffBuf);
        }

        uCoeffIndex = 0;

        if (sNumCoeff < 4)
        {
            const Ipp32s *pDecTable = UMC::H264Bitstream::m_tblTotalZerosCR[sNumCoeff];

            Ipp32s table_pos;
            Ipp32s table_bits = pDecTable[0];
            ippiNextBits((*ppBitStream), (*pOffset), table_bits, table_pos)

            const qq * qqq = (qq*)(&pDecTable[table_pos  + 1]);

            ippiSkipNBits((*ppBitStream), (*pOffset), table_bits - qqq->bits)

            sTotalZeros = qqq->q;

            for (; sTotalZeros > 0 && sNumCoeff > 1; sNumCoeff--)
            {
                const Ipp32s *pDecTable = UMC::H264Bitstream::m_tblRunBefore[sTotalZeros];
                Ipp32s table_pos;
                Ipp32s table_bits = pDecTable[0];

                ippiNextBits((*ppBitStream), (*pOffset), table_bits, table_pos)

                const qq * qqq = (qq*)(&pDecTable[table_pos  + 1]);

                ippiSkipNBits((*ppBitStream), (*pOffset), table_bits - qqq->bits)

                sRunBefore =  qqq->q;

                Ipp32s pos  = sNumCoeff - 1 + sTotalZeros;
                (*ppPosCoefbuf)[pos] = (Ipp16s)CoeffBuf[uCoeffIndex++];

                sTotalZeros -= sRunBefore;
            }
        }

        for (; sNumCoeff; sNumCoeff--)
        {
            Ipp32s pos  = sNumCoeff - 1 + sTotalZeros;
            (*ppPosCoefbuf)[pos] = (Ipp16s)CoeffBuf[uCoeffIndex++];
        }

        (*ppPosCoefbuf) += 4;
    }
    }

    return ippStsNoErr;
#endif
}

IppStatus MyippiDecodeCAVLCCoeffs_H264_1u32s (Ipp32u ** const ppBitStream,
                                                     Ipp32s *pOffset,
                                                     Ipp16s *pNumCoeff,
                                                     Ipp32s **ppPosCoefbuf,
                                                     Ipp32u uVLCSelect,
                                                     Ipp16s uMaxNumCoeff,
                                                     const Ipp32s *pScanMatrix)

{
    return ippiDecodeCAVLCCoeffs_H264_1u32s(ppBitStream,
                                                     pOffset,
                                                     pNumCoeff,
                                                     ppPosCoefbuf,
                                                     uVLCSelect,
                                                     uMaxNumCoeff,
                                                     (const Ipp32s **) UMC::H264Bitstream::m_tblCoeffToken,
                                                     (const Ipp32s **) UMC::H264Bitstream::m_tblTotalZeros,
                                                     (const Ipp32s **) UMC::H264Bitstream::m_tblRunBefore,
                                                     pScanMatrix);
}


IppStatus MyippiDecodeCAVLCChromaDcCoeffs_H264_1u32s(Ipp32u **ppBitStream,
                                                         Ipp32s *pOffset,
                                                         Ipp16s *pNumCoeff,
                                                         Ipp32s **ppPosCoefbuf)

{
    return ippiDecodeCAVLCChromaDcCoeffs_H264_1u32s(ppBitStream,
                                                         pOffset,
                                                         pNumCoeff,
                                                         ppPosCoefbuf,
                                                         UMC::H264Bitstream::m_tblCoeffToken[3],
                                                         (const Ipp32s **) UMC::H264Bitstream::m_tblTotalZerosCR,
                                                         (const Ipp32s **) UMC::H264Bitstream::m_tblRunBefore);
}

#endif // UMC_ENABLE_H264_VIDEO_DECODER
