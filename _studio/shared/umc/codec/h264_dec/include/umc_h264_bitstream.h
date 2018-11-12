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

#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#ifndef __UMC_H264_BITSTREAM_H_
#define __UMC_H264_BITSTREAM_H_

#include "ippvc.h"
#include "umc_structures.h"
#include "umc_dynamic_cast.h"
#include "umc_h264_dec_defs_dec.h"
#include "umc_h264_dec_tables.h"
#include "umc_h264_dec_internal_cabac.h"
#include "umc_h264_bitstream_headers.h"

#define h264Peek1Bit(current_data, offset) \
    ((current_data[0] >> (offset)) & 1)

#define h264Drop1Bit(current_data, offset) \
{ \
    offset -= 1; \
    if (offset < 0) \
    { \
        offset = 31; \
        current_data += 1; \
    } \
}

IppStatus MyippiDecodeCAVLCCoeffs_H264_1u16s (uint32_t ** const ppBitStream,
                                                    int32_t *pOffset,
                                                    int16_t *pNumCoeff,
                                                    int16_t **ppPosCoefbuf,
                                                    uint32_t uVLCSelect,
                                                    int16_t coeffLimit,
                                                    int16_t uMaxNumCoeff,
                                                    const int32_t *pScanMatrix,
                                                    int32_t scanIdxStart);

IppStatus MyippiDecodeCAVLCCoeffs_H264_1u32s (uint32_t ** const ppBitStream,
                                                    int32_t *pOffset,
                                                    int16_t *pNumCoeff,
                                                    int32_t **ppPosCoefbuf,
                                                    uint32_t uVLCSelect,
                                                    int16_t uMaxNumCoeff,
                                                    const int32_t *pScanMatrix);

IppStatus MyippiDecodeCAVLCChromaDcCoeffs_H264_1u16s(uint32_t **ppBitStream,
                                                        int32_t *pOffset,
                                                        int16_t *pNumCoeff,
                                                        int16_t **ppPosCoefbuf);


IppStatus MyippiDecodeCAVLCChromaDcCoeffs_H264_1u32s(uint32_t **ppBitStream,
                                                        int32_t *pOffset,
                                                        int16_t *pNumCoeff,
                                                        int32_t **ppPosCoefbuf);

namespace UMC
{
    inline IppStatus MyDecodeCAVLCCoeffs_H264(uint32_t **ppBitStream, int32_t *pOffset,
                                            int16_t *pNumCoeff, int16_t **ppDstCoeffs,
                                            uint32_t uVLCSelect, int16_t coeffLimit,
                                            int16_t uMaxNumCoeff, const int32_t *pScanMatrix, int32_t scanIdxStart)
    {
        return MyippiDecodeCAVLCCoeffs_H264_1u16s(ppBitStream, pOffset,
                                                pNumCoeff, ppDstCoeffs,
                                                uVLCSelect, coeffLimit,
                                                uMaxNumCoeff, pScanMatrix, scanIdxStart);
    }

    inline IppStatus MyDecodeCAVLCCoeffs_H264(uint32_t **ppBitStream, int32_t *pOffset,
                                            int16_t *pNumCoeff, int32_t **ppDstCoeffs,
                                            uint32_t uVLCSelect, int16_t ,
                                            int16_t uMaxNumCoeff, const int32_t *pScanMatrix, int32_t )
    {
        return MyippiDecodeCAVLCCoeffs_H264_1u32s(ppBitStream, pOffset,
                                                pNumCoeff, ppDstCoeffs,
                                                uVLCSelect, uMaxNumCoeff, pScanMatrix);
    }

    inline IppStatus MyDecodeCAVLCChromaDcCoeffs_H264(uint32_t **ppBitStream,
                                                    int32_t *pOffset,
                                                    int16_t *pNumCoeff,
                                                    int16_t **ppDstCoeffs)
    {
        return MyippiDecodeCAVLCChromaDcCoeffs_H264_1u16s(ppBitStream,
                                                        pOffset,
                                                        pNumCoeff,
                                                        ppDstCoeffs);
    }

    inline IppStatus MyDecodeCAVLCChromaDcCoeffs_H264(uint32_t **ppBitStream,
                                                    int32_t *pOffset,
                                                    int16_t *pNumCoeff,
                                                    int32_t **ppDstCoeffs)
    {
        return MyippiDecodeCAVLCChromaDcCoeffs_H264_1u32s(ppBitStream,
                                                        pOffset,
                                                        pNumCoeff,
                                                        ppDstCoeffs);
    }

    inline IppStatus DecodeCAVLCChromaDcCoeffs422_H264(uint32_t **ppBitStream,
                                                       int32_t *pOffset,
                                                       int16_t *pNumCoeff,
                                                       int16_t **ppDstCoeffs,
                                                       const int32_t *pTblCoeffToken,
                                                       const int32_t **ppTblTotalZerosCR,
                                                       const int32_t **ppTblRunBefore)
    {
        return ippiDecodeCAVLCChroma422DcCoeffs_H264_1u16s(ppBitStream,
                                                            pOffset,
                                                            pNumCoeff,
                                                            ppDstCoeffs,
                                                            pTblCoeffToken,
                                                            ppTblTotalZerosCR,
                                                            ppTblRunBefore);
    }

    inline IppStatus DecodeCAVLCChromaDcCoeffs422_H264(uint32_t **ppBitStream,
                                                       int32_t *pOffset,
                                                       int16_t *pNumCoeff,
                                                       int32_t **ppDstCoeffs,
                                                       const int32_t *pTblCoeffToken,
                                                       const int32_t **ppTblTotalZerosCR,
                                                       const int32_t **ppTblRunBefore)
    {
        return ippiDecodeCAVLCChroma422DcCoeffs_H264_1u32s(ppBitStream,
                                                            pOffset,
                                                            pNumCoeff,
                                                            ppDstCoeffs,
                                                            pTblCoeffToken,
                                                            ppTblTotalZerosCR,
                                                            ppTblRunBefore);
    }

class Headers;

// CABAC magic mode switcher.
// Just put it to 0 to switch fast CABAC decoding off.
#define CABAC_MAGIC_BITS 16

#define NUM_BLOCK_TYPES        8
#define NUM_MB_TYPE_CTX       11
#define NUM_BLOCK_TYPE_CTX     9
#define NUM_MVD_CTX           10
#define NUM_REF_NO_CTX         6
#define NUM_DELTA_QAUNT_CTX    4
#define NUM_MB_AFF_CTX         4

#define NUM_INTRA_PRED_CTX             2
#define NUM_CR_INTRA_PRED_CTX          4
#define NUM_CBP_CTX                    4
#define NUM_BLOCK_CBP_CTX              4
#define NUM_SIGN_COEFF_FLAG_CTX       15
#define NUM_LAST_SIGN_COEFF_FLAG_CTX  15
#define NUM_COEF_ONE_LEVEL_CTX         5
#define NUM_COEF_ABS_LEVEL_CTX         5

// Declare CABAC context type
#pragma pack(push, 1)
typedef struct CABAC_CONTEXT
{
    uint8_t pStateIdxAndVal;                                      // (uint8_t) probability state index and value of most probable symbol

} CABAC_CONTEXT;

#pragma pack(pop)

class H264Bitstream  : public H264HeadersBitstream
{
public:

    H264Bitstream(void);
    H264Bitstream(uint8_t * const pb, const uint32_t maxsize);
    virtual ~H264Bitstream(void);

    H264Bitstream& operator=(const H264HeadersBitstream& value)
    {
        *((H264HeadersBitstream*)this) = value;
        return *this;
    }

    template <typename Coeffs> inline
    void GetCAVLCInfoLuma(uint32_t uVLCSelect, // N, obtained from num coeffs of above/left blocks
                            int32_t uMaxNumCoeff,
                            int16_t &sNumCoeff,
                            Coeffs **ppPosCoefbuf, // buffer to return up to 16
                            int32_t field_flag)
    {
        // Calls CAVLC bitstream decoding functions to obtain nonzero coefficients
        // and related information, returning in passed buffers and passed-by-reference
        // parameters.
        // Bitstream pointer and offset are updated by called functions and are
        // updated on return.

        if (uVLCSelect < 2)
        {
            if (h264Peek1Bit(m_pbs, m_bitOffset))
            {
                h264Drop1Bit(m_pbs, m_bitOffset);
                sNumCoeff = 0;
                return;
            }
        }

        IppStatus ippRes;

        ippRes = MyDecodeCAVLCCoeffs_H264(&m_pbs,
                                        &m_bitOffset,
                                        &sNumCoeff,
                                        ppPosCoefbuf,
                                        uVLCSelect,
                                        (int16_t)(endIdx - startIdx + 1),
                                        (int16_t)uMaxNumCoeff,
                                        (int32_t*) mp_scan4x4[field_flag],
                                        startIdx);

        if (ippStsNoErr > ippRes)
            throw h264_exception(UMC_ERR_INVALID_STREAM);

    } // void GetCAVLCInfoLuma(uint32_t uVLCSelect,

    template <typename Coeffs>
    void GetCAVLCInfoChroma0(int16_t &sNumCoeff, Coeffs **ppPosCoefbuf)
    {
        IppStatus ippRes = MyDecodeCAVLCChromaDcCoeffs_H264(&m_pbs,
                                                          &m_bitOffset,
                                                          &sNumCoeff,
                                                          ppPosCoefbuf);

        if (ippStsNoErr > ippRes)
            throw h264_exception(UMC_ERR_INVALID_STREAM);

    } // void GetCAVLCInfoChroma0(int16_t &sNumCoeff,


    template <typename Coeffs>
    void GetCAVLCInfoChroma2(int16_t &sNumCoeff, Coeffs **ppPosCoefbuf)
    {
        IppStatus ippRes = DecodeCAVLCChromaDcCoeffs422_H264(&m_pbs,
                                                             &m_bitOffset,
                                                             &sNumCoeff,
                                                             ppPosCoefbuf,
                                                             m_tblCoeffToken[4],
                                                             (const int32_t **) m_tblTotalZerosCR422,
                                                             (const int32_t **) m_tblRunBefore);
        if (ippStsNoErr > ippRes)
            throw h264_exception(UMC_ERR_INVALID_STREAM);
    } // void GetCAVLCInfoChroma2(int16_t &sNumCoeff,

    inline void Drop1Bit();
    inline uint32_t Peek1Bit();

    bool NextBit();

    inline size_t GetAllBitsCount()
    {
        return m_maxBsSize;
    }


    //
    // CABAC decoding function(s)
    //

    // Initialize CABAC decoding engine
    void InitializeDecodingEngine_CABAC(void);
    // Terminate CABAC decoding engine, rollback prereaded bits
    void TerminateDecode_CABAC(void);

    void UpdateCABACPointers();

    // Initialize CABAC context(s) in intra slices
    void InitializeContextVariablesIntra_CABAC(int32_t SliceQPy);

    // Initialize CABAC context(s) in inter slices
    void InitializeContextVariablesInter_CABAC(int32_t SliceQPy,
                                               int32_t cabac_init_idc);

    // Decode order of single bins
    uint32_t DecodeSingleBinOnes_CABAC(uint32_t ctxIdx,
                                     int32_t &binIdx);

    // Decode int32_t coefficient value
    int32_t DecodeSignedLevel_CABAC(uint32_t ctxIdxOffset,
                                   uint32_t &numDecodAbsLevelEq1,
                                   uint32_t &numDecodAbsLevelGt1,
                                   uint32_t max_value);
    int32_t DecodeSingleSignedLevel_CABAC(uint32_t ctxIdxOffset);

    // Decode single bin from stream
    inline
    uint32_t DecodeSingleBin_CABAC(uint32_t ctxIdx);

    // Decode single bin using bypass decoding
    inline
    uint32_t DecodeBypass_CABAC();

    inline
    int32_t DecodeBypassSign_CABAC(int32_t val);

    // Decode multiple bins using bypass decoding until ==1
    inline
    uint32_t DecodeBypassOnes_CABAC();

    // Decode end symbol
    inline
    uint32_t DecodeSymbolEnd_CABAC();

    template <typename Coeffs>
    void ResidualBlock8x8_CABAC_SVC(bool field_decoding_flag,
                                    const int32_t *single_scan,
                                    Coeffs *pPosCoefbuf,
                                    int32_t maxNumCoeffminus1,
                                    int32_t startCoeff)
    {
        // See subclause 7.3.5.3.2 of H.264 standard
        uint32_t localCtxIdxOffset, ctxIdxInc, ctxIdxOffsetLast;
        uint32_t numDecodAbsLevelEq1 = 0, numDecodAbsLevelGt1 = 0;
        const uint32_t *ctxIdxBase;
        const int32_t* pHPFF = hp_CtxIdxInc_sig_coeff_flag[field_decoding_flag];

        uint32_t ncoefs = 0;
        int32_t i = startCoeff;
        int16_t coeffRuns[65];

    #ifdef __ICL
        __assume_aligned(pPosCoefbuf, 8);
    #endif
        memset(pPosCoefbuf, 0, sizeof(Coeffs) * 64);

        // See table 9-32 of H.264 standard
        if (field_decoding_flag)
            ctxIdxBase = ctxIdxOffset8x8FieldCoded;
        else
            ctxIdxBase = ctxIdxOffset8x8FrameCoded;

        localCtxIdxOffset = ctxIdxBase[SIGNIFICANT_COEFF_FLAG] +
            ctxIdxBlockCatOffset[SIGNIFICANT_COEFF_FLAG][BLOCK_LUMA8X8_LEVELS];
        ctxIdxOffsetLast = ctxIdxBase[LAST_SIGNIFICANT_COEFF_FLAG] +
            ctxIdxBlockCatOffset[LAST_SIGNIFICANT_COEFF_FLAG][BLOCK_LUMA8X8_LEVELS];

        for (; i < maxNumCoeffminus1; i++)
        {
            ctxIdxInc = pHPFF[i];
            // get significant_coeff_flag
            if (DecodeSingleBin_CABAC(localCtxIdxOffset+ctxIdxInc))
            {
                // store position of non-zero coeff
                coeffRuns[ncoefs] = (int16_t) i;
                // Intel compiler should use memory form of increment
                ncoefs ++;
                ctxIdxInc = hp_CtxIdxInc_last_sig_coeff_flag[i];
                // get last_significant_coeff_flag
                if (DecodeSingleBin_CABAC(ctxIdxOffsetLast+ctxIdxInc)) break;
            }
        }

        if (i == maxNumCoeffminus1)
        {
            coeffRuns[ncoefs] = (int16_t) i;
            ncoefs ++;
        }

        // calculate last coefficient in block
        localCtxIdxOffset = ctxIdxBase[COEFF_ABS_LEVEL_MINUS1] +
            ctxIdxBlockCatOffset[COEFF_ABS_LEVEL_MINUS1][BLOCK_LUMA8X8_LEVELS];

        for (; ncoefs > 0; ncoefs--)
        {
            int32_t level = DecodeSignedLevel_CABAC(localCtxIdxOffset,
                                                   numDecodAbsLevelEq1,
                                                   numDecodAbsLevelGt1,9);

            // store coeff position and level to coeff buffer
            uint32_t pos = coeffRuns[ncoefs - 1];
            pos = single_scan[pos];

            pPosCoefbuf[pos] = (int16_t) level;
        }

    } // void ResidualBlock8x8_CABAC(bool field_decoding_flag,

    template <typename Coeffs>
    void ResidualBlock8x8_CABAC(bool field_decoding_flag,
                                const int32_t *single_scan,
                                Coeffs *pPosCoefbuf)
    {
        ResidualBlock8x8_CABAC_SVC(field_decoding_flag, single_scan,
                                   pPosCoefbuf, endIdx*4 + 3, startIdx * 4);
    } // void ResidualBlock8x8_CABAC


    template <typename Coeffs>
    void ResidualBlock4x4_CABAC_SVC(int32_t ctxBlockCat,
                                const uint32_t *ctxIdxBase,
                                const int32_t *pScan,
                                Coeffs *pPosCoefbuf,
                                    int32_t maxNumCoeffminus1,
                                    int32_t startCoeff)
    {
        // See subclause 7.3.5.3.2 of H.264 standard
        Coeffs coeffRuns[18];
        int32_t iNumCoeffs;
        int32_t shiftContentIdx = 0;

        if ((BLOCK_CHROMA_AC_LEVELS == ctxBlockCat) || (BLOCK_LUMA_AC_LEVELS == ctxBlockCat))
            shiftContentIdx = 1;

        // See table 9-32 of H.264 standard
    #ifdef __ICL
        __assume_aligned(pPosCoefbuf, 8);
    #endif

        // reset destination block
        memset(pPosCoefbuf, 0, sizeof(Coeffs) * 16);

        // decode coefficient(s)
        {
            uint32_t localCtxIdxOffset, ctxIdxOffsetLast;
            int32_t i = startCoeff;

            localCtxIdxOffset = ctxIdxBase[SIGNIFICANT_COEFF_FLAG] +
                           ctxIdxBlockCatOffset[SIGNIFICANT_COEFF_FLAG][ctxBlockCat];
            ctxIdxOffsetLast = ctxIdxBase[LAST_SIGNIFICANT_COEFF_FLAG] +
                               ctxIdxBlockCatOffset[LAST_SIGNIFICANT_COEFF_FLAG][ctxBlockCat];

            iNumCoeffs = 0;
            if (i < maxNumCoeffminus1)
            {
                do
                {
                    // get significant_coeff_flag
                    if (DecodeSingleBin_CABAC(localCtxIdxOffset + i - shiftContentIdx))
                    {
                        // store position of non-zero coeff
                        coeffRuns[iNumCoeffs] = (int16_t) i;
                        // Intel compiler should use memory form of increment
                        iNumCoeffs += 1;
                        // get last_significant_coeff_flag
                        if (DecodeSingleBin_CABAC(ctxIdxOffsetLast + i - shiftContentIdx))
                        {
                            // I know "label jumping" is bad programming style,
                            // but there is no another opportunity to avoid extra comparison.
                            goto no_more_coefficients_label;
                        }
                    }
                } while (++i < maxNumCoeffminus1);
            }

            // take into account last coefficient
            coeffRuns[iNumCoeffs] = (int16_t) maxNumCoeffminus1;
            iNumCoeffs += 1;
        }

no_more_coefficients_label:

        // calculate last coefficient in block
        uint32_t localCtxIdxOffset = ctxIdxBase[COEFF_ABS_LEVEL_MINUS1] +
                       ctxIdxBlockCatOffset[COEFF_ABS_LEVEL_MINUS1][ctxBlockCat];

        if (1 == iNumCoeffs)
        {
            // store coeff to coeff buffer
            int32_t pos = pScan[coeffRuns[0]];
            pPosCoefbuf[pos] = (Coeffs) DecodeSingleSignedLevel_CABAC(localCtxIdxOffset);

        }
        // reorder coefficient(s)
        else
        {
            uint32_t numDecodAbsLevelEq1 = 0, numDecodAbsLevelGt1 = 0;

            do
            {
                int32_t level = DecodeSignedLevel_CABAC(localCtxIdxOffset,
                                                       numDecodAbsLevelEq1,
                                                       numDecodAbsLevelGt1,
                                                       9);

                // store coeff to coeff buffer
                int32_t pos = pScan[coeffRuns[iNumCoeffs - 1]];
                pPosCoefbuf[pos] = (Coeffs) level;

            } while (0 < --iNumCoeffs);
        }

    } // void ResidualBlock4x4_CABAC_SVC

    template <typename Coeffs>
    void ResidualBlock4x4_CABAC(int32_t ctxBlockCat,
                                const uint32_t *ctxIdxBase,
                                const int32_t *pScan,
                                Coeffs *pPosCoefbuf,
                                int32_t maxNumCoeffminus1)
    {
        int32_t tmpStartIdx = startIdx;

        if (((BLOCK_CHROMA_AC_LEVELS == ctxBlockCat) || (BLOCK_LUMA_AC_LEVELS == ctxBlockCat)) &&
            (tmpStartIdx == 0))
            tmpStartIdx = 1;

        ResidualBlock4x4_CABAC_SVC(ctxBlockCat,ctxIdxBase, pScan,
                                   pPosCoefbuf, maxNumCoeffminus1,
                                   tmpStartIdx);

    } // void ResidualBlock4x4_CABAC(H264BitStream *pBitStream,

    static IppVCHuffmanSpec_32s *(m_tblCoeffToken[5]);
    static IppVCHuffmanSpec_32s *(m_tblRunBefore [16]);
    static IppVCHuffmanSpec_32s *(m_tblTotalZeros[16]);

    static IppVCHuffmanSpec_32s *(m_tblTotalZerosCR[4]);
    static IppVCHuffmanSpec_32s *(m_tblTotalZerosCR422[8]);

    void SetIdx (int32_t x, int32_t y)
    {
        startIdx = x;
        endIdx = y;
    }

protected:

    // Initialize bitstream tables
    static Status InitTables(void);
    // Release bitstream tables
    static void ReleaseTables(void);

    static bool m_bTablesInited;                                       // (bool) tables have been allocated

    CABAC_CONTEXT context_array[467];                           // (CABAC_CONTEXT []) array of cabac context(s)
    uint32_t m_lcodIRange;                                        // (uint32_t) arithmetic decoding engine variable
    uint32_t m_lcodIOffset;                                       // (uint32_t) arithmetic decoding engine variable
    int32_t m_iMagicBits;                                        // (int32_t) available extra CABAC bits
    uint16_t *m_pMagicBits;                                       // (uint16_t *) pointer to CABAC data

    int32_t startIdx;                                            // Start index for coeffs decoding
    int32_t endIdx;                                              // Start index for coeffs decoding

    // Decoding SEI message functions
    int32_t sei_message(const Headers & headers,int32_t current_sps,H264SEIPayLoad *spl);
    int32_t sei_payload(const Headers & headers,int32_t current_sps,H264SEIPayLoad *spl);
    int32_t buffering_period(const Headers & headers,int32_t current_sps,H264SEIPayLoad *spl);
    int32_t pic_timing(const Headers & headers,int32_t current_sps,H264SEIPayLoad *spl);
    void user_data_registered_itu_t_t35(H264SEIPayLoad *spl);
    void recovery_point(H264SEIPayLoad *spl);
    int32_t dec_ref_pic_marking_repetition(const Headers & headers, int32_t current_sps,H264SEIPayLoad *spl);

    void unparsed_sei_message(H264SEIPayLoad *spl);

    void scalability_info(H264SEIPayLoad *spl);
    void scalable_nesting(H264SEIPayLoad *spl);

    friend class TableInitializer;
};

// disable the "conditional expression is constant" warning
#ifdef _MSVC_LANG
#pragma warning(disable: 4127)
#endif

template <typename Coeffs, int32_t color_format>
class BitStreamColorSpecific
{
public:
    typedef Coeffs * CoeffsPtr;

    static inline void ResidualChromaDCBlock_CABAC(const uint32_t *ctxIdxBase, const int32_t *single_scan,
                                                   Coeffs *pPosCoefbuf, H264Bitstream * bs)
    {
        // See subclause 7.3.5.3.2 of H.264 standard
        int32_t coef_ctr=-1;
        int32_t maxNumCoeffminus1;
        uint32_t localCtxIdxOffset, ctxIdxOffsetLast;
        uint32_t numDecodAbsLevelEq1 = 0, numDecodAbsLevelGt1 = 0;
        int16_t coeffRuns[18];

        int32_t numOfCoeffs = 0;
        switch (color_format)
        {
        case CHROMA_FORMAT_400:
            break;
        case CHROMA_FORMAT_420:
            numOfCoeffs = 4;
            break;
        case CHROMA_FORMAT_422:
            numOfCoeffs = 8;
            break;
        case CHROMA_FORMAT_444:
            numOfCoeffs = 16;
            break;
        };

        // See table 9-32 of H.264 standard
    #ifdef __ICL
        __assume_aligned(pPosCoefbuf, 8);
    #endif
        memset(pPosCoefbuf, 0, sizeof(Coeffs) * numOfCoeffs);
        maxNumCoeffminus1 = numOfCoeffs - 1;

        localCtxIdxOffset = ctxIdxBase[SIGNIFICANT_COEFF_FLAG] +
            ctxIdxBlockCatOffset[SIGNIFICANT_COEFF_FLAG][BLOCK_CHROMA_DC_LEVELS + color_format];
        ctxIdxOffsetLast = ctxIdxBase[LAST_SIGNIFICANT_COEFF_FLAG] +
            ctxIdxBlockCatOffset[LAST_SIGNIFICANT_COEFF_FLAG][BLOCK_CHROMA_DC_LEVELS + color_format];

        uint32_t ncoefs = 0;
        int32_t i = 0;
        for (; i < maxNumCoeffminus1; i++)
        {
            int32_t k;

            if (color_format == 1)
                k = i;
            else
                k = MFX_MIN(i >> (3 & (color_format - 1)), 2);

            // get significant_coeff_flag
            if (bs->DecodeSingleBin_CABAC(localCtxIdxOffset+k))
            {
                // store position of non-zero coeff
                coeffRuns[ncoefs] = (int16_t) i;
                // Intel compiler should use memory form of increment
                ncoefs ++;
                // get last_significant_coeff_flag
                if (bs->DecodeSingleBin_CABAC(ctxIdxOffsetLast + k))
                    break;
            }
        }

        if (i == maxNumCoeffminus1)
        {
            coeffRuns[ncoefs] = (int16_t) i;
            ncoefs ++;
        }

        // calculate last coefficient in block
        localCtxIdxOffset = ctxIdxBase[COEFF_ABS_LEVEL_MINUS1] +
            ctxIdxBlockCatOffset[COEFF_ABS_LEVEL_MINUS1][BLOCK_CHROMA_DC_LEVELS + color_format];

        for (; ncoefs > 0; ncoefs--)
        {
            int32_t level = bs->DecodeSignedLevel_CABAC(localCtxIdxOffset,
                                                       numDecodAbsLevelEq1,
                                                       numDecodAbsLevelGt1,8);

            // store coeff position and level to coeff buffer
            uint32_t pos = coeffRuns[ncoefs - 1] + coef_ctr + 1;

            if (color_format != 1)
                pos = single_scan[pos];

            pPosCoefbuf[pos] = (Coeffs) level;
        }

    } // void H264Bitstream::ResidualChromaDCBlock_CABAC(const uint32_t *ctxIdxBase,
};

// restore the "conditional expression is constant" warning
#ifdef _MSVC_LANG
#pragma warning(default: 4127)
#endif

} // namespace UMC

#include "umc_h264_bitstream_inlines.h"

#endif // __UMC_H264_BITSTREAM_H_

#endif // UMC_ENABLE_H264_VIDEO_DECODER
