//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2003-2018 Intel Corporation. All Rights Reserved.
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


H264Bitstream::H264Bitstream(uint8_t * const pb, const uint32_t maxsize)
     : H264HeadersBitstream(pb, maxsize)
     , m_lcodIRange(0)
     , m_lcodIOffset(0)
     , m_iMagicBits(0)
     , m_pMagicBits(0)
     , startIdx(0)
     , endIdx(0)
{
} // H264Bitstream::H264Bitstream(uint8_t * const pb,

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

// Function 'p8_ippiHuffmanTableFree_32s' is depricated
#if defined(__GNUC__)
#if defined(__INTEL_COMPILER)
    #pragma warning (disable:1478)
#else
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
#elif defined(__clang__)
  #pragma clang diagnostic push
  #pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
void H264Bitstream::ReleaseTables(void)
{
    int32_t i;

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

#if defined(__GNUC__)
  #pragma GCC diagnostic pop // "-Wdeprecated-declarations"
#elif defined(__clang__)
  #pragma clang diagnostic pop // "-Wdeprecated-declarations"
#endif

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
uint32_t H264Bitstream::DecodeSingleBinOnes_CABAC(uint32_t ctxIdx,
                                                int32_t &binIdx)
{
    // See subclause 9.3.3.2.1 of H.264 standard

    uint32_t pStateIdx = context_array[ctxIdx].pStateIdxAndVal;
    uint32_t codIOffset = m_lcodIOffset;
    uint32_t codIRange = m_lcodIRange;
    uint32_t codIRangeLPS;
    uint32_t binVal;

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
            uint32_t numBits = NumBitsToGetTbl[codIRange >> CABAC_MAGIC_BITS];

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

    context_array[ctxIdx].pStateIdxAndVal = (uint8_t) pStateIdx;
    m_lcodIOffset = codIOffset;
    m_lcodIRange = codIRange;

    return binVal;

} // uint32_t H264Bitstream::DecodeSingleBinOnes_CABAC(uint32_t ctxIdx,

inline
uint32_t H264Bitstream::DecodeBypassOnes_CABAC(void)
{
    // See subclause 9.3.3.2.3 of H.264 standard
    uint32_t binVal;// = 0;
    uint32_t binCount = 0;
    uint32_t codIOffset = m_lcodIOffset;
    uint32_t codIRange = m_lcodIRange;

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

        int32_t mask = ((int32_t)(codIRange)-1-(int32_t)(codIOffset))>>31;
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

} // uint32_t H264Bitstream::DecodeBypassOnes_CABAC(void)

static
uint8_t iCtxIdxIncTable[64][2] =
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

int32_t H264Bitstream::DecodeSignedLevel_CABAC(uint32_t localCtxIdxOffset,
                                              uint32_t &numDecodAbsLevelEq1,
                                              uint32_t &numDecodAbsLevelGt1,
                                              uint32_t max_value)
{
    // See subclause 9.3.2.3 of H.264
    uint32_t ctxIdxInc;
    int32_t binIdx;

    // PREFIX BIN(S) STRING DECODING
    // decoding first bin of prefix bin string

    if (5 + numDecodAbsLevelGt1 < max_value)
    {
        ctxIdxInc = iCtxIdxIncTable[numDecodAbsLevelEq1][((uint32_t) -((int32_t) numDecodAbsLevelGt1)) >> 31];

        if (0 == DecodeSingleBin_CABAC(localCtxIdxOffset + ctxIdxInc))
        {
            numDecodAbsLevelEq1 += 1;
            binIdx = 1;
        }
        else
        {
            int32_t binVal;

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
                int32_t leadingZeroBits;
                int32_t codeNum;

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
            int32_t binVal;

            // decoding next bin(s) of prefix bin string
            // we use Truncated Unary binarization with cMax = uCoff;
            binIdx = 1;

            binVal = DecodeSingleBinOnes_CABAC(localCtxIdxOffset + max_value, binIdx);

            // SUFFIX BIN(S) STRING DECODING

            // See subclause 9.1 of H.264 standard
            // we use Exp-Golomb code of 0-th order
            if (binVal)
            {
                int32_t leadingZeroBits;
                int32_t codeNum;

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
        int32_t lcodIOffset = m_lcodIOffset;
        int32_t lcodIRange = m_lcodIRange;

        // See subclause 9.3.3.2.3 of H.264 standard
#if (CABAC_MAGIC_BITS > 0)
        // do shift on 1 bit
        lcodIOffset += lcodIOffset;

        {
            int32_t iMagicBits = m_iMagicBits;

            iMagicBits -= 1;
            if (0 >= iMagicBits)
                RefreshCABACBits(lcodIOffset, m_pMagicBits, iMagicBits);
            m_iMagicBits = iMagicBits;
        }
#else // !(CABAC_MAGIC_BITS > 0)
        lcodIOffset = (lcodIOffset << 1) | Get1Bit();
#endif // (CABAC_MAGIC_BITS > 0)

        int32_t mask = ((int32_t)(lcodIRange) - 1 - (int32_t)(lcodIOffset)) >> 31;
        // conditionally negate level
        binIdx = (binIdx ^ mask) - mask;
        // conditionally subtract range from offset
        lcodIOffset -= lcodIRange & mask;

        m_lcodIOffset = lcodIOffset;
    }

    return binIdx;

} //int32_t H264Bitstream::DecodeSignedLevel_CABAC(int32_t ctxIdxOffset, int32_t &numDecodAbsLevelEq1, int32_t &numDecodAbsLevelGt1)

//
// this is a limited version of the DecodeSignedLevel_CABAC function.
// it decodes single value per block.
//
int32_t H264Bitstream::DecodeSingleSignedLevel_CABAC(uint32_t localCtxIdxOffset)
{
    // See subclause 9.3.2.3 of H.264
    uint32_t ctxIdxInc;
    int32_t binIdx;

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
            int32_t binVal;

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
                int32_t leadingZeroBits;
                int32_t codeNum;

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
        int32_t lcodIOffset = m_lcodIOffset;
        int32_t lcodIRange = m_lcodIRange;

        // See subclause 9.3.3.2.3 of H.264 standard
#if (CABAC_MAGIC_BITS > 0)
        // do shift on 1 bit
        lcodIOffset += lcodIOffset;

        {
            int32_t iMagicBits = m_iMagicBits;

            iMagicBits -= 1;
            if (0 >= iMagicBits)
                RefreshCABACBits(lcodIOffset, m_pMagicBits, iMagicBits);
            m_iMagicBits = iMagicBits;
        }
#else // !(CABAC_MAGIC_BITS > 0)
        lcodIOffset = (lcodIOffset << 1) | Get1Bit();
#endif // (CABAC_MAGIC_BITS > 0)

        int32_t mask = ((int32_t)(lcodIRange) - 1 - (int32_t)(lcodIOffset)) >> 31;
        // conditionally negate level
        binIdx = (binIdx ^ mask) - mask;
        // conditionally subtract range from offset
        lcodIOffset -= lcodIRange & mask;

        m_lcodIOffset = lcodIOffset;
    }

    return binIdx;

} //int32_t H264Bitstream::DecodeSingleSignedLevel_CABAC(int32_t ctxIdxOffset)

} // namespace UMC

#define IPP_NOERROR_RET()  return ippStsNoErr
#define IPP_ERROR_RET( ErrCode )  return (ErrCode)

#define IPP_BADARG_RET( expr, ErrCode )\
            {if (expr) { IPP_ERROR_RET( ErrCode ); }}

#define IPP_BAD_PTR1_RET( ptr )\
            IPP_BADARG_RET( NULL==(ptr), ippStsNullPtrErr )

static const uint32_t vlc_inc[] = {0,3,6,12,24,48,96};

typedef struct{
    int16_t bits;
    int16_t offsets;
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
    uint8_t bits;

    union{
    int8_t q;
    uint8_t qqqqq[3];
    };

} qq;

static int32_t sadd[7]={15,0,0,0,0,0,0};


inline
void _GetBlockCoeffs_CAVLC(uint32_t ** const & ppBitStream,
                           int32_t * & pOffset,
                           int32_t uCoeffIndex,
                           int32_t sNumCoeff,
                           int32_t sNumTrailingOnes,
                           int32_t *CoeffBuf)
{
    uint32_t uCoeffLevel = 0;

    /* 0..6, to select coding method used for each coeff */
    int32_t suffixLength = (sNumCoeff > 10) && (sNumTrailingOnes < 3) ? 1 : 0;

    /* When NumTrOnes is less than 3, need to add 1 to level of first coeff */
    int32_t uFirstAdjust = ((sNumTrailingOnes < 3) ? 1 : 0);

    int32_t NumZeros = -1;
    for (int32_t w = 0; !w; NumZeros++)
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
        uint32_t level_suffix;
        uint32_t levelSuffixSize = NumZeros - 3;
        int32_t levelCode;

        h264GetBits((*ppBitStream), (*pOffset), levelSuffixSize, level_suffix);
        levelCode = ((MFX_MIN(15, NumZeros) << suffixLength) + level_suffix) + uFirstAdjust*2 + sadd[suffixLength];
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
        int32_t zeros = -1;
        for (int32_t w = 0; !w; zeros++)
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
            uint32_t level_suffix;
            uint32_t levelSuffixSize = zeros - 3;
            int32_t levelCode;

            h264GetBits((*ppBitStream), (*pOffset), levelSuffixSize, level_suffix);
            levelCode = ((MFX_MIN(15, zeros) << suffixLength) + level_suffix) + sadd[suffixLength];
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

} /* static void _GetBlockCoeffs_CAVLC(uint32_t **pbs, */

static int32_t bitsToGetTbl16s[7][16] = /*[level][numZeros]*/
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
static int32_t addOffsetTbl16s[7][16] = /*[level][numZeros]*/
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

IppStatus ownippiDecodeCAVLCCoeffs_H264_1u16s (uint32_t **ppBitStream,
                                                     int32_t *pOffset,
                                                     int16_t *pNumCoeff,
                                                     int16_t **ppPosCoefbuf,
                                                     uint32_t uVLCSelect,
                                                     int16_t uMaxNumCoeff,
                                                     const int32_t **ppTblCoeffToken,
                                                     const int32_t **ppTblTotalZeros,
                                                     const int32_t **ppTblRunBefore,
                                                     const int32_t *pScanMatrix) /* buffer to return up to 16 */

{
    int16_t        CoeffBuf[16] = {};    /* Temp buffer to hold coeffs read from bitstream*/
    uint32_t        uVLCIndex        = 2;
    uint32_t        uCoeffIndex        = 0;
    int32_t        sTotalZeros        = 0;
    int32_t        sFirstPos        = 16 - uMaxNumCoeff;
    uint32_t        TrOneSigns = 0;        /* return sign bits (1==neg) in low 3 bits*/
    uint32_t        uTR1Mask;
    int32_t        pos;
    int32_t        sRunBefore;
    int16_t        sNumTrailingOnes;
    int32_t        sNumCoeff = 0;
    uint32_t        table_bits;
    uint8_t         code_len;
    int32_t        i;
    uint32_t  table_pos;
    int32_t  val;
    uint32_t *pbsBackUp;
    int32_t bsoffBackUp;

    /* check error(s) */
    //IPP_BAD_PTR4_RET(ppBitStream,pOffset,ppPosCoefbuf,pNumCoeff)
    //IPP_BAD_PTR4_RET(ppTblCoeffToken,ppTblTotalZeros,ppTblRunBefore,pScanMatrix)
    //IPP_BAD_PTR2_RET(*ppBitStream, *ppPosCoefbuf)
    IPP_BADARG_RET(((sFirstPos != 0 && sFirstPos != 1) || (int32_t)uVLCSelect < 0), ippStsOutOfRangeErr)

    /* make a bit-stream backup */
    pbsBackUp = *ppBitStream;
    bsoffBackUp = *pOffset;

    if (uVLCSelect > 7)
    {
        /* fixed length code 4 bits numCoeff and */
        /* 2 bits for TrailingOnes */
        h264GetBits((*ppBitStream), (*pOffset), 6, sNumCoeff);
        sNumTrailingOnes = (int16_t) (sNumCoeff & 3);
        sNumCoeff         = (sNumCoeff&0x3c)>>2;
        if (sNumCoeff == 0 && sNumTrailingOnes == 3)
            sNumTrailingOnes = 0;
        else
            sNumCoeff++;
    }
    else
    {
        const int32_t *pDecTable;
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
        code_len = (uint8_t) (val);

        while (code_len & 0x80)
        {
            val = val >> 8;
            table_bits = pDecTable[val];
            h264GetBits((*ppBitStream), (*pOffset), table_bits, table_pos);
            val = pDecTable[table_pos + val + 1];
            code_len = (uint8_t) (val & 0xff);
        }

        h264UngetNBits((*ppBitStream), (*pOffset), code_len);

        if ((val>>8) == IPPVC_VLC_FORBIDDEN)
        {
             *ppBitStream = pbsBackUp;
             *pOffset = bsoffBackUp;

             return ippStsH263VLCCodeErr;
        }

        sNumTrailingOnes  = (int16_t) ((val >> 8) & 0xff);
        sNumCoeff = (val >> 16) & 0xff;
    }

    *pNumCoeff = (int16_t) sNumCoeff;

    if (sNumTrailingOnes)
    {
        h264GetBits((*ppBitStream), (*pOffset), sNumTrailingOnes, TrOneSigns);
        uTR1Mask = 1 << (sNumTrailingOnes - 1);
        while (uTR1Mask)
        {
            CoeffBuf[uCoeffIndex++] = (int16_t) ((TrOneSigns & uTR1Mask) == 0 ? 1 : -1);
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
            uint16_t suffixLength = 0;        /* 0..6, to select coding method used for each coeff */
            int16_t lCoeffIndex;
            uint16_t uCoeffLevel = 0;
            int32_t NumZeros;
            uint16_t uBitsToGet;
            uint16_t uFirstAdjust;
            uint16_t uLevelOffset;
            int32_t w;
            int16_t    *lCoeffBuf = &CoeffBuf[uCoeffIndex];

            if ((sNumCoeff > 10) && (sNumTrailingOnes < 3))
                suffixLength = 1;

            /* When NumTrOnes is less than 3, need to add 1 to level of first coeff */
            uFirstAdjust = (uint16_t) ((sNumTrailingOnes < 3) ? 1 : 0);

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
                    uBitsToGet = (int16_t) (bitsToGetTbl16s[suffixLength][NumZeros]);
                    uLevelOffset = (uint16_t) (addOffsetTbl16s[suffixLength][NumZeros]);

                    if (uBitsToGet)
                    {
                        h264GetBits((*ppBitStream), (*pOffset), uBitsToGet, NumZeros);
                    }

                    uCoeffLevel = (uint16_t) ((NumZeros>>1) + uLevelOffset + uFirstAdjust);

                    lCoeffBuf[lCoeffIndex] = (int16_t) ((NumZeros & 1) ? (-((signed) uCoeffLevel)) : (uCoeffLevel));
                }
                else
                {
                    uint32_t level_suffix;
                    uint32_t levelSuffixSize = NumZeros - 3;
                    int32_t levelCode;

                    h264GetBits((*ppBitStream), (*pOffset), levelSuffixSize, level_suffix);
                    levelCode = (uint16_t) ((MFX_MIN(15, NumZeros) << suffixLength) + level_suffix) + uFirstAdjust*2 + sadd[suffixLength];
                    levelCode = (uint16_t) (levelCode + (1 << (NumZeros - 3)) - 4096);

                    lCoeffBuf[lCoeffIndex] = (int16_t) ((levelCode & 1) ?
                                                      ((-levelCode - 1) >> 1) :
                                                      ((levelCode + 2) >> 1));

                    uCoeffLevel = (uint16_t) OWNV_ABS(lCoeffBuf[lCoeffIndex]);
                }

                uFirstAdjust = 0;

            }    /* for uCoeffIndex */

        }
        /* Get TotalZeros if any */
        if (sNumCoeff < uMaxNumCoeff)
        {
            /*ippiVCHuffmanDecodeOne_1u32s(ppBitStream, pOffset,&sTotalZeros, */
            /*                                                ppTblTotalZeros[sNumCoeff]); */
            const int32_t *pDecTable = ppTblTotalZeros[sNumCoeff];

            IPP_BAD_PTR1_RET(ppTblTotalZeros[sNumCoeff]);

            table_bits = pDecTable[0];
            h264GetBits((*ppBitStream), (*pOffset), table_bits, table_pos)

            val = pDecTable[table_pos + 1];
            code_len = (uint8_t) (val & 0xff);
            val = val >> 8;

            while (code_len & 0x80)
            {
                table_bits = pDecTable[val];
                h264GetBits((*ppBitStream), (*pOffset), table_bits, table_pos)

                val = pDecTable[table_pos + val + 1];
                code_len = (uint8_t) (val & 0xff);
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
                const int32_t *pDecTable = ppTblRunBefore[sTotalZeros];

                IPP_BAD_PTR1_RET(ppTblRunBefore[sTotalZeros]);

                table_bits = pDecTable[0];
                h264GetBits((*ppBitStream), (*pOffset), table_bits, table_pos)

                val           = pDecTable[table_pos  + 1];
                code_len   = (uint8_t) (val & 0xff);
                val        = val >> 8;


                while (code_len & 0x80)
                {
                    table_bits = pDecTable[val];
                    h264GetBits((*ppBitStream), (*pOffset), table_bits, table_pos)

                    val           = pDecTable[table_pos + val + 1];
                    code_len   = (uint8_t) (val & 0xff);
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
            sTotalZeros -= MFX_MIN(sTotalZeros, sRunBefore);
            pos             = pScanMatrix[pos];

            (*ppPosCoefbuf)[pos] = CoeffBuf[(uCoeffIndex++)&0x0f];
            sNumCoeff--;
        }
        (*ppPosCoefbuf) += 16;
    }

    return ippStsNoErr;

}

IppStatus MyippiDecodeCAVLCCoeffs_H264_1u16s (uint32_t ** const ppBitStream,
                                                     int32_t *pOffset,
                                                     int16_t *pNumCoeff,
                                                     int16_t **ppPosCoefbuf,
                                                     uint32_t uVLCSelect,
                                                     int16_t ,
                                                     int16_t uMaxNumCoeff,
                                                     const int32_t *pScanMatrix,
                                                     int32_t )

{
    int32_t **ppTblTotalZeros;

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
                                                     (const int32_t **) UMC::H264Bitstream::m_tblCoeffToken,
                                                     (const int32_t **) UMC::H264Bitstream::m_tblTotalZeros,
                                                     (const int32_t **) UMC::H264Bitstream::m_tblRunBefore,
                                                     pScanMatrix);


#if 0
#if 1
    return own_ippiDecodeCAVLCCoeffsIdxs_H264_1u16s(ppBitStream,
                                                     pOffset,
                                                     pNumCoeff,
                                                     ppPosCoefbuf,
                                                     uVLCSelect,
                                                     uMaxNumCoeff,
                                                     (const int32_t **) UMC::H264Bitstream::m_tblCoeffToken,
                                                     (const int32_t **) ppTblTotalZeros,
                                                     (const int32_t **) UMC::H264Bitstream::m_tblRunBefore,
                                                     pScanMatrix,
                                                     scanIdxStart,
                                                     coeffLimit + scanIdxStart - 1);
#else
    int32_t        CoeffBuf[16] = {};    /* Temp buffer to hold coeffs read from bitstream*/
    int32_t        uVLCIndex        = 2;
    int32_t        sTotalZeros        = 0;
    int32_t        sRunBefore;
    int32_t        sNumTrailingOnes;
    int32_t        sNumCoeff = 0;

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

        const int32_t *pDecTable = UMC::H264Bitstream::m_tblCoeffToken[uVLCIndex];

        //IPP_BAD_PTR1_RET(ppTblCoeffToken[uVLCIndex]);

        int32_t table_pos;
        int32_t table_bits = *pDecTable;
        ippiGetNBits((*ppBitStream), (*pOffset), table_bits, table_pos);
        int32_t val = pDecTable[table_pos + 1];
        uint8_t code_len = (uint8_t)val;

        while (code_len & 0x80)
        {
            val = val >> 8;
            table_bits = pDecTable[val];
            ippiGetNBits((*ppBitStream), (*pOffset), table_bits, table_pos);
            val = pDecTable[table_pos + val + 1];
            code_len = (uint8_t)(val & 0xff);
        }

        ippiUngetNBits((*ppBitStream), (*pOffset), code_len);

        sNumTrailingOnes  = ((val >> 8) & 0xff);
        sNumCoeff = (val >> 16) & 0xff;
    }

    *pNumCoeff = (int16_t)sNumCoeff;

    if (sNumCoeff)
    {
        int32_t uCoeffIndex = 0;

        if (sNumTrailingOnes)
        {
            uint32_t TrOneSigns;
            ippiGetNBits((*ppBitStream), (*pOffset), sNumTrailingOnes, TrOneSigns);
            uint32_t uTR1Mask = 1 << (sNumTrailingOnes - 1);
            while (uTR1Mask)
            {
                CoeffBuf[uCoeffIndex++] = ((TrOneSigns & uTR1Mask) == 0 ? 1 : -1);
                uTR1Mask >>= 1;
            }
        }
#ifdef __ICL
#pragma vector always
#endif
        //for (int32_t i = 0; i < 16; i++)
          //  (*ppPosCoefbuf)[i] = 0;

        memset(*ppPosCoefbuf, 0, 16*sizeof(int16_t));

        /* Get the sign bits of any trailing one coeffs */
        /* and put signed coeffs to the buffer */
        /* Get nonzero coeffs which are not Tr1 coeffs */
        if (sNumCoeff != sNumTrailingOnes)
        {
#if 1
            _GetBlockCoeffs_CAVLC(ppBitStream, pOffset, uCoeffIndex, sNumCoeff, sNumTrailingOnes, CoeffBuf);
#else
            uint32_t uCoeffLevel = 0;

            /* 0..6, to select coding method used for each coeff */
            int32_t suffixLength = (sNumCoeff > 10) && (sNumTrailingOnes < 3) ? 1 : 0;

            /* When NumTrOnes is less than 3, need to add 1 to level of first coeff */
            int32_t uFirstAdjust = ((sNumTrailingOnes < 3) ? 1 : 0);

            if (suffixLength < 6)
            {
                if (uCoeffLevel > vlc_inc[suffixLength])
                    suffixLength++;
            }


            int32_t NumZeros = -1;
            for (int32_t w = 0; !w; NumZeros++)
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
                uint32_t level_suffix;
                uint32_t levelSuffixSize = NumZeros - 3;
                int32_t levelCode;

                ippiGetNBits((*ppBitStream), (*pOffset), levelSuffixSize, level_suffix);
                levelCode = ((MFX_MIN(15, NumZeros) << suffixLength) + level_suffix) + uFirstAdjust*2 + sadd[suffixLength];
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
                int32_t NumZeros = -1;
                for (int32_t w = 0; !w; NumZeros++)
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
                    uint32_t level_suffix;
                    uint32_t levelSuffixSize = NumZeros - 3;
                    int32_t levelCode;

                    ippiGetNBits((*ppBitStream), (*pOffset), levelSuffixSize, level_suffix);
                    levelCode = ((MFX_MIN(15, NumZeros) << suffixLength) + level_suffix) + sadd[suffixLength];
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

        int32_t sFirstPos        = 16 - uMaxNumCoeff;

        /* Get TotalZeros if any */
        if (sNumCoeff != uMaxNumCoeff)
        {
            const int32_t *pDecTable = UMC::H264Bitstream::m_tblTotalZeros[sNumCoeff];

            int32_t table_pos;
            int32_t table_bits = pDecTable[0];
            ippiNextBits((*ppBitStream), (*pOffset), table_bits, table_pos)

            const qq * qqq = (qq*)(&pDecTable[table_pos  + 1]);

            ippiSkipNBits((*ppBitStream), (*pOffset), table_bits - qqq->bits)

            sTotalZeros = qqq->q;

            for (; sTotalZeros > 0 && sNumCoeff > 1; sNumCoeff--)
            {
                const int32_t *pDecTable = UMC::H264Bitstream::m_tblRunBefore[sTotalZeros];
                int32_t table_pos;
                int32_t table_bits = pDecTable[0];

                ippiNextBits((*ppBitStream), (*pOffset), table_bits, table_pos)

                const qq * qqq = (qq*)(&pDecTable[table_pos  + 1]);

                ippiSkipNBits((*ppBitStream), (*pOffset), table_bits - qqq->bits)

                sRunBefore =  qqq->q;

                int32_t pos  = sNumCoeff - 1 + sTotalZeros + sFirstPos;
                pos         = pScanMatrix[pos];

                (*ppPosCoefbuf)[pos] = (int16_t)CoeffBuf[uCoeffIndex++];

                sTotalZeros -= sRunBefore;
            }
        }

        for (; sNumCoeff; sNumCoeff--)
        {
            int32_t pos  = sNumCoeff - 1 + sTotalZeros + sFirstPos;
            pos         = pScanMatrix[pos];
            (*ppPosCoefbuf)[pos] = (int16_t)CoeffBuf[uCoeffIndex++];
        }

        (*ppPosCoefbuf) += 16;
    }

    return ippStsNoErr;
#endif
#endif
} /* IPPFUN(IppStatus, ippiDecodeCAVLCCoeffs_H264_1u16s, (uint32_t **ppBitStream, */

#define h264PeekBitsNoCheck(current_data, offset, nbits, data) \
{ \
    uint32_t x; \
    x = current_data[0] >> (offset - nbits + 1); \
    (data) = x; \
}

#define h264PeekBits(current_data, offset, nbits, data) \
{ \
    uint32_t x; \
    int32_t off; \
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
int8_t ChromaDCRunTable[] =
{
    3, 5,
    2, 5,
    1, 4, 1, 4,
    0, 3, 0, 3, 0, 3, 0, 3
};

static
IppStatus _GetBlockCoeffs_CAVLC(uint32_t **pbs,
                           int32_t *bitOffset,
                           int16_t sNumCoeff,
                           int16_t sNumTrOnes,
                           int16_t *CoeffBuf)
{
    uint16_t suffixLength = 0;        /* 0..6, to select coding method used for each coeff */
    int16_t uCoeffIndex;
    uint32_t uCoeffLevel = 0;
    int32_t NumZeros;
    uint16_t uBitsToGet;
    uint16_t uFirstAdjust;
    uint16_t uLevelOffset;
    int32_t w;

    if ((sNumCoeff > 10) && (sNumTrOnes < 3))
        suffixLength = 1;

    /* When NumTrOnes is less than 3, need to add 1 to level of first coeff */
    uFirstAdjust = (uint16_t)((sNumTrOnes < 3) ? 1 : 0);

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
            uBitsToGet     = (uint16_t)(bitsToGetTbl16s[suffixLength][NumZeros]);
            uLevelOffset   = (uint16_t)(addOffsetTbl16s[suffixLength][NumZeros]);

            if (uBitsToGet)
            {
                h264GetBits((*pbs), (*bitOffset), uBitsToGet, NumZeros);
            }

            uCoeffLevel = (NumZeros>>1) + uLevelOffset + uFirstAdjust;

            CoeffBuf[uCoeffIndex] = (int16_t) ((NumZeros & 1) ? (-((signed) uCoeffLevel)) : (uCoeffLevel));
        }
        else
        {
            uint32_t level_suffix;
            uint32_t levelSuffixSize = NumZeros - 3;
            int32_t levelCode;

            h264GetBits((*pbs), (*bitOffset), levelSuffixSize, level_suffix);
            levelCode = (uint16_t) ((MFX_MIN(15, NumZeros) << suffixLength) + level_suffix) + uFirstAdjust*2 + sadd[suffixLength];
            levelCode = (uint16_t) (levelCode + (1 << (NumZeros - 3)) - 4096);

            CoeffBuf[uCoeffIndex] = (int16_t) ((levelCode & 1) ?
                                              ((-levelCode - 1) >> 1) :
                                              ((levelCode + 2) >> 1));

            uCoeffLevel = OWNV_ABS(CoeffBuf[uCoeffIndex]);
        }

        uFirstAdjust = 0;

    } /* for uCoeffIndex*/

    return ippStsNoErr;

} /* static void _GetBlockCoeffs_CAVLC(uint32_t **pbs, */

IppStatus ownippiDecodeCAVLCChromaDcCoeffs_H264_1u16s (uint32_t **ppBitStream,
                                                             int32_t *pOffset,
                                                             int16_t *pNumCoeff,
                                                             int16_t **ppPosCoefbuf,
                                                             const int32_t *pTblCoeffToken,
                                                             const int32_t **ppTblTotalZerosCR,
                                                             const int32_t **ppTblRunBefore)

{
    /* check the most frequently used parameters */
    //IPP_BAD_PTR3_RET(ppBitStream, pOffset, pNumCoeff)

    if (4 < *pOffset)
    {
        uint32_t code;

        h264PeekBitsNoCheck((*ppBitStream), (*pOffset), 5, code);
        /* the shortes DC code */
        if (code & 0x10)
        {
            int32_t iValue;
            int16_t *pCoeffs;

            IPP_BAD_PTR1_RET(ppPosCoefbuf)

            /* advance coeffs buffer */
            pCoeffs = *ppPosCoefbuf;
            *ppPosCoefbuf += 4;
            /* save coeffs */
            pCoeffs[0] =
            pCoeffs[1] =
            pCoeffs[2] =
            pCoeffs[3] = 0;
            iValue = -(((((int32_t) code) >> 3) & 1) * 2 - 1);
            pCoeffs[ChromaDCRunTable[(code & 0x07) * 2]] = (int16_t) iValue;
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
        uint32_t code;

        h264PeekBits((*ppBitStream), (*pOffset), 5, code);
        /* the shortes DC code */
        if (code & 0x10)
        {
            int32_t iValue;
            int16_t *pCoeffs;

            IPP_BAD_PTR1_RET(ppPosCoefbuf)

            /* advance coeffs buffer */
            pCoeffs = *ppPosCoefbuf;
            *ppPosCoefbuf += 4;
            /* save coeffs */
            pCoeffs[0] =
            pCoeffs[1] =
            pCoeffs[2] =
            pCoeffs[3] = 0;
            iValue = -(((((int32_t) code) >> 3) & 1) * 2 - 1);
            pCoeffs[ChromaDCRunTable[(code & 0x07) * 2]] = (int16_t) iValue;
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


    int16_t        CoeffBuf[16] = {};        /* Temp buffer to hold coeffs read from bitstream*/
    uint32_t        uTR1Mask;
    uint32_t        TrOneSigns;            /* return sign bits (1==neg) in low 3 bits*/
    uint32_t        uCoeffIndex            = 0;
    int32_t        sTotalZeros            = 0;
    int32_t        sRunBefore;
    int16_t        sNumTrailingOnes;
    int16_t        sNumCoeff = 0;
    int32_t        pos;
    int32_t        i;

    /* check for the only codeword of all zeros*/


    /*ippiDecodeVLCPair_32s(ppBitStream, pOffset, pTblCoeffToken, */
    /*                              &sNumTrailingOnes,&sNumCoeff);*/
    int32_t table_pos;
    int32_t val;
    uint32_t          table_bits;
    uint8_t           code_len;
    uint32_t *pbsBackUp;
    int32_t bsoffBackUp;

    /* check error(s) */
    //IPP_BAD_PTR4_RET(ppPosCoefbuf, pTblCoeffToken, ppTblTotalZerosCR, ppTblRunBefore)

    /* create bit stream backup */
    pbsBackUp = *ppBitStream;
    bsoffBackUp = *pOffset;

    table_bits = *pTblCoeffToken;
    h264GetBits((*ppBitStream), (*pOffset), table_bits, table_pos);
    val           = pTblCoeffToken[table_pos  + 1];
    code_len   = (uint8_t) (val);

    while (code_len & 0x80)
    {
        val        = val >> 8;
        table_bits = pTblCoeffToken[val];
        h264GetBits((*ppBitStream), (*pOffset), table_bits, table_pos);
        val           = pTblCoeffToken[table_pos + val  + 1];
        code_len   = (uint8_t) (val & 0xff);
    }

    h264UngetNBits((*ppBitStream), (*pOffset), code_len);

    if ((val>>8) == IPPVC_VLC_FORBIDDEN)
    {
         *ppBitStream = pbsBackUp;
         *pOffset    = bsoffBackUp;

         return ippStsH263VLCCodeErr;
    }
    sNumTrailingOnes  = (int16_t) ((val >> 8) &0xff);
    sNumCoeff = (int16_t) ((val >> 16) & 0xff);

    *pNumCoeff = sNumCoeff;

    if (sNumTrailingOnes)
    {
        h264GetBits((*ppBitStream), (*pOffset), sNumTrailingOnes, TrOneSigns);
        uTR1Mask = 1 << (sNumTrailingOnes - 1);
        while (uTR1Mask)
        {
            CoeffBuf[uCoeffIndex++] = (int16_t) ((TrOneSigns & uTR1Mask) == 0 ? 1 : -1);
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
        /*((int32_t*)(*ppPosCoefbuf))[0] = 0; */
        /*((int32_t*)(*ppPosCoefbuf))[1] = 0; */
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
            const int32_t *pDecTable = ppTblTotalZerosCR[sNumCoeff];

            IPP_BAD_PTR1_RET(ppTblTotalZerosCR[sNumCoeff])

            table_bits = pDecTable[0];
            h264GetBits((*ppBitStream), (*pOffset), table_bits, table_pos)

            val           = pDecTable[table_pos  + 1];
            code_len   = (uint8_t) (val & 0xff);
            val        = val >> 8;


            while (code_len & 0x80)
            {
                table_bits = pDecTable[val];
                h264GetBits((*ppBitStream), (*pOffset), table_bits, table_pos)

                val           = pDecTable[table_pos + val + 1];
                code_len   = (uint8_t) (val & 0xff);
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
                const int32_t *pDecTable = ppTblRunBefore[sTotalZeros];

                IPP_BAD_PTR1_RET(ppTblRunBefore[sTotalZeros])

                table_bits = pDecTable[0];
                h264GetBits((*ppBitStream), (*pOffset), table_bits, table_pos)

                val           = pDecTable[table_pos  + 1];
                code_len   = (uint8_t) (val & 0xff);
                val        = val >> 8;


                while (code_len & 0x80)
                {
                    table_bits = pDecTable[val];
                    h264GetBits((*ppBitStream), (*pOffset), table_bits, table_pos)

                    val           = pDecTable[table_pos + val + 1];
                    code_len   = (uint8_t) (val & 0xff);
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

} /* IPPFUN(IppStatus, ippiDecodeCAVLCChromaDcCoeffs_H264_1u16s , (uint32_t **ppBitStream, */
IppStatus MyippiDecodeCAVLCChromaDcCoeffs_H264_1u16s(uint32_t **ppBitStream,
                                                         int32_t *pOffset,
                                                         int16_t *pNumCoeff,
                                                         int16_t **ppPosCoefbuf)

{

#if 1
    return ownippiDecodeCAVLCChromaDcCoeffs_H264_1u16s (ppBitStream,
                                                         pOffset,
                                                         pNumCoeff,
                                                         ppPosCoefbuf,
                                                         UMC::H264Bitstream::m_tblCoeffToken[3],
                                                         (const int32_t **) UMC::H264Bitstream::m_tblTotalZerosCR,
                                                         (const int32_t **) UMC::H264Bitstream::m_tblRunBefore);
#else
    /* check the most frequently used parameters */
    //IPP_BAD_PTR3_RET(ppBitStream, pOffset, pNumCoeff)

#if 1
    if (4 < *pOffset)
    {
        uint32_t code;

        h264PeekBitsNoCheck((*ppBitStream), (*pOffset), 5, code);
        /* the shortes DC code */
        if (code & 0x10)
        {
            int32_t iValue;
            int16_t *pCoeffs;

            /* advance coeffs buffer */
            pCoeffs = *ppPosCoefbuf;
            *ppPosCoefbuf += 4;
            /* save coeffs */
            pCoeffs[0] =
            pCoeffs[1] =
            pCoeffs[2] =
            pCoeffs[3] = 0;
            iValue = -(((((int32_t) code) >> 3) & 1) * 2 - 1);
            pCoeffs[ChromaDCRunTable[(code & 0x07) * 2]] = (int16_t) iValue;
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
        uint32_t code;

        h264PeekBits((*ppBitStream), (*pOffset), 5, code);
        /* the shortes DC code */
        if (code & 0x10)
        {
            int32_t iValue;

            /* advance coeffs buffer */
            int16_t *pCoeffs = *ppPosCoefbuf;
            *ppPosCoefbuf += 4;
            /* save coeffs */
            pCoeffs[0] =
            pCoeffs[1] =
            pCoeffs[2] =
            pCoeffs[3] = 0;
            iValue = -(((((int32_t) code) >> 3) & 1) * 2 - 1);
            pCoeffs[ChromaDCRunTable[(code & 0x07) * 2]] = (int16_t) iValue;
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

    int32_t        CoeffBuf[16] = {};        /* Temp buffer to hold coeffs read from bitstream*/
    uint32_t        TrOneSigns;            /* return sign bits (1==neg) in low 3 bits*/
    uint32_t        uCoeffIndex            = 0;
    int32_t        sTotalZeros            = 0;
    int32_t        sRunBefore;
    int32_t        sNumTrailingOnes;
    int32_t        sNumCoeff = 0;
    int32_t        i;

    /* check for the only codeword of all zeros*/

    int32_t table_pos;
    int32_t val;
    uint32_t table_bits;
    uint8_t  code_len;

    const int32_t *pDecTable = UMC::H264Bitstream::m_tblCoeffToken[3];

    table_bits = *pDecTable;
    ippiGetNBits((*ppBitStream), (*pOffset), table_bits, table_pos);
    val = pDecTable[table_pos  + 1];
    code_len   = (uint8_t) (val);

    while (code_len & 0x80)
    {
        val        = val >> 8;
        table_bits = pDecTable[val];
        ippiGetNBits((*ppBitStream), (*pOffset), table_bits, table_pos);
        val       = pDecTable[table_pos + val  + 1];
        code_len   = (uint8_t) (val & 0xff);
    }

    ippiUngetNBits((*ppBitStream), (*pOffset), code_len);

    sNumTrailingOnes  = ((val >> 8) &0xff);
    sNumCoeff = ((val >> 16) & 0xff);

    *pNumCoeff = (int16_t)sNumCoeff;

    /* Get the sign bits of any trailing one coeffs */
    if (sNumCoeff)
    {
        if (sNumTrailingOnes)
        {
            ippiGetNBits((*ppBitStream), (*pOffset), sNumTrailingOnes, TrOneSigns);
            uint32_t uTR1Mask = 1 << (sNumTrailingOnes - 1);
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

        //memset((*ppPosCoefbuf), 0, 4*sizeof(int16_t));

        /* get nonzero coeffs which are not Tr1 coeffs */
        if (sNumCoeff > sNumTrailingOnes)
        {
            _GetBlockCoeffs_CAVLC(ppBitStream, pOffset, uCoeffIndex, sNumCoeff,
                                         sNumTrailingOnes, CoeffBuf);
        }

        uCoeffIndex = 0;

        if (sNumCoeff < 4)
        {
            const int32_t *pDecTable = UMC::H264Bitstream::m_tblTotalZerosCR[sNumCoeff];

            int32_t table_pos;
            int32_t table_bits = pDecTable[0];
            ippiNextBits((*ppBitStream), (*pOffset), table_bits, table_pos)

            const qq * qqq = (qq*)(&pDecTable[table_pos  + 1]);

            ippiSkipNBits((*ppBitStream), (*pOffset), table_bits - qqq->bits)

            sTotalZeros = qqq->q;

            for (; sTotalZeros > 0 && sNumCoeff > 1; sNumCoeff--)
            {
                const int32_t *pDecTable = UMC::H264Bitstream::m_tblRunBefore[sTotalZeros];
                int32_t table_pos;
                int32_t table_bits = pDecTable[0];

                ippiNextBits((*ppBitStream), (*pOffset), table_bits, table_pos)

                const qq * qqq = (qq*)(&pDecTable[table_pos  + 1]);

                ippiSkipNBits((*ppBitStream), (*pOffset), table_bits - qqq->bits)

                sRunBefore =  qqq->q;

                int32_t pos  = sNumCoeff - 1 + sTotalZeros;
                (*ppPosCoefbuf)[pos] = (int16_t)CoeffBuf[uCoeffIndex++];

                sTotalZeros -= sRunBefore;
            }
        }

        for (; sNumCoeff; sNumCoeff--)
        {
            int32_t pos  = sNumCoeff - 1 + sTotalZeros;
            (*ppPosCoefbuf)[pos] = (int16_t)CoeffBuf[uCoeffIndex++];
        }

        (*ppPosCoefbuf) += 4;
    }
    }

    return ippStsNoErr;
#endif
}

IppStatus MyippiDecodeCAVLCCoeffs_H264_1u32s (uint32_t ** const ppBitStream,
                                                     int32_t *pOffset,
                                                     int16_t *pNumCoeff,
                                                     int32_t **ppPosCoefbuf,
                                                     uint32_t uVLCSelect,
                                                     int16_t uMaxNumCoeff,
                                                     const int32_t *pScanMatrix)

{
    return ippiDecodeCAVLCCoeffs_H264_1u32s(ppBitStream,
                                                     pOffset,
                                                     pNumCoeff,
                                                     ppPosCoefbuf,
                                                     uVLCSelect,
                                                     uMaxNumCoeff,
                                                     (const int32_t **) UMC::H264Bitstream::m_tblCoeffToken,
                                                     (const int32_t **) UMC::H264Bitstream::m_tblTotalZeros,
                                                     (const int32_t **) UMC::H264Bitstream::m_tblRunBefore,
                                                     pScanMatrix);
}


IppStatus MyippiDecodeCAVLCChromaDcCoeffs_H264_1u32s(uint32_t **ppBitStream,
                                                         int32_t *pOffset,
                                                         int16_t *pNumCoeff,
                                                         int32_t **ppPosCoefbuf)

{
    return ippiDecodeCAVLCChromaDcCoeffs_H264_1u32s(ppBitStream,
                                                         pOffset,
                                                         pNumCoeff,
                                                         ppPosCoefbuf,
                                                         UMC::H264Bitstream::m_tblCoeffToken[3],
                                                         (const int32_t **) UMC::H264Bitstream::m_tblTotalZerosCR,
                                                         (const int32_t **) UMC::H264Bitstream::m_tblRunBefore);
}

#endif // UMC_ENABLE_H264_VIDEO_DECODER
