/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2003-2016 Intel Corporation. All Rights Reserved.
//
//
*/
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

IppStatus MyippiDecodeCAVLCCoeffs_H264_1u16s (Ipp32u ** const ppBitStream,
                                                    Ipp32s *pOffset,
                                                    Ipp16s *pNumCoeff,
                                                    Ipp16s **ppPosCoefbuf,
                                                    Ipp32u uVLCSelect,
                                                    Ipp16s coeffLimit,
                                                    Ipp16s uMaxNumCoeff,
                                                    const Ipp32s *pScanMatrix,
                                                    Ipp32s scanIdxStart);

IppStatus MyippiDecodeCAVLCCoeffs_H264_1u32s (Ipp32u ** const ppBitStream,
                                                    Ipp32s *pOffset,
                                                    Ipp16s *pNumCoeff,
                                                    Ipp32s **ppPosCoefbuf,
                                                    Ipp32u uVLCSelect,
                                                    Ipp16s uMaxNumCoeff,
                                                    const Ipp32s *pScanMatrix);

IppStatus MyippiDecodeCAVLCChromaDcCoeffs_H264_1u16s(Ipp32u **ppBitStream,
                                                        Ipp32s *pOffset,
                                                        Ipp16s *pNumCoeff,
                                                        Ipp16s **ppPosCoefbuf);


IppStatus MyippiDecodeCAVLCChromaDcCoeffs_H264_1u32s(Ipp32u **ppBitStream,
                                                        Ipp32s *pOffset,
                                                        Ipp16s *pNumCoeff,
                                                        Ipp32s **ppPosCoefbuf);

namespace UMC
{
    inline IppStatus MyDecodeCAVLCCoeffs_H264(Ipp32u **ppBitStream, Ipp32s *pOffset,
                                            Ipp16s *pNumCoeff, Ipp16s **ppDstCoeffs,
                                            Ipp32u uVLCSelect, Ipp16s coeffLimit,
                                            Ipp16s uMaxNumCoeff, const Ipp32s *pScanMatrix, Ipp32s scanIdxStart)
    {
        return MyippiDecodeCAVLCCoeffs_H264_1u16s(ppBitStream, pOffset,
                                                pNumCoeff, ppDstCoeffs,
                                                uVLCSelect, coeffLimit,
                                                uMaxNumCoeff, pScanMatrix, scanIdxStart);
    }

    inline IppStatus MyDecodeCAVLCCoeffs_H264(Ipp32u **ppBitStream, Ipp32s *pOffset,
                                            Ipp16s *pNumCoeff, Ipp32s **ppDstCoeffs,
                                            Ipp32u uVLCSelect, Ipp16s ,
                                            Ipp16s uMaxNumCoeff, const Ipp32s *pScanMatrix, Ipp32s )
    {
        return MyippiDecodeCAVLCCoeffs_H264_1u32s(ppBitStream, pOffset,
                                                pNumCoeff, ppDstCoeffs,
                                                uVLCSelect, uMaxNumCoeff, pScanMatrix);
    }

    inline IppStatus MyDecodeCAVLCChromaDcCoeffs_H264(Ipp32u **ppBitStream,
                                                    Ipp32s *pOffset,
                                                    Ipp16s *pNumCoeff,
                                                    Ipp16s **ppDstCoeffs)
    {
        return MyippiDecodeCAVLCChromaDcCoeffs_H264_1u16s(ppBitStream,
                                                        pOffset,
                                                        pNumCoeff,
                                                        ppDstCoeffs);
    }

    inline IppStatus MyDecodeCAVLCChromaDcCoeffs_H264(Ipp32u **ppBitStream,
                                                    Ipp32s *pOffset,
                                                    Ipp16s *pNumCoeff,
                                                    Ipp32s **ppDstCoeffs)
    {
        return MyippiDecodeCAVLCChromaDcCoeffs_H264_1u32s(ppBitStream,
                                                        pOffset,
                                                        pNumCoeff,
                                                        ppDstCoeffs);
    }

    inline IppStatus DecodeCAVLCChromaDcCoeffs422_H264(Ipp32u **ppBitStream,
                                                       Ipp32s *pOffset,
                                                       Ipp16s *pNumCoeff,
                                                       Ipp16s **ppDstCoeffs,
                                                       const Ipp32s *pTblCoeffToken,
                                                       const Ipp32s **ppTblTotalZerosCR,
                                                       const Ipp32s **ppTblRunBefore)
    {
        return ippiDecodeCAVLCChroma422DcCoeffs_H264_1u16s(ppBitStream,
                                                            pOffset,
                                                            pNumCoeff,
                                                            ppDstCoeffs,
                                                            pTblCoeffToken,
                                                            ppTblTotalZerosCR,
                                                            ppTblRunBefore);
    }

    inline IppStatus DecodeCAVLCChromaDcCoeffs422_H264(Ipp32u **ppBitStream,
                                                       Ipp32s *pOffset,
                                                       Ipp16s *pNumCoeff,
                                                       Ipp32s **ppDstCoeffs,
                                                       const Ipp32s *pTblCoeffToken,
                                                       const Ipp32s **ppTblTotalZerosCR,
                                                       const Ipp32s **ppTblRunBefore)
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
    Ipp8u pStateIdxAndVal;                                      // (Ipp8u) probability state index and value of most probable symbol

} CABAC_CONTEXT;

#pragma pack(pop)

class H264Bitstream  : public H264HeadersBitstream
{
public:

    H264Bitstream(void);
    H264Bitstream(Ipp8u * const pb, const Ipp32u maxsize);
    virtual ~H264Bitstream(void);

    H264Bitstream& operator=(const H264HeadersBitstream& value)
    {
        *((H264HeadersBitstream*)this) = value;
        return *this;
    }

    template <typename Coeffs> inline
    void GetCAVLCInfoLuma(Ipp32u uVLCSelect, // N, obtained from num coeffs of above/left blocks
                            Ipp32s uMaxNumCoeff,
                            Ipp16s &sNumCoeff,
                            Coeffs **ppPosCoefbuf, // buffer to return up to 16
                            Ipp32s field_flag)
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
                                        (Ipp16s)(endIdx - startIdx + 1),
                                        (Ipp16s)uMaxNumCoeff,
                                        (Ipp32s*) mp_scan4x4[field_flag],
                                        startIdx);

        if (ippStsNoErr > ippRes)
            throw h264_exception(UMC_ERR_INVALID_STREAM);

    } // void GetCAVLCInfoLuma(Ipp32u uVLCSelect,

    template <typename Coeffs>
    void GetCAVLCInfoChroma0(Ipp16s &sNumCoeff, Coeffs **ppPosCoefbuf)
    {
        IppStatus ippRes = MyDecodeCAVLCChromaDcCoeffs_H264(&m_pbs,
                                                          &m_bitOffset,
                                                          &sNumCoeff,
                                                          ppPosCoefbuf);

        if (ippStsNoErr > ippRes)
            throw h264_exception(UMC_ERR_INVALID_STREAM);

    } // void GetCAVLCInfoChroma0(Ipp16s &sNumCoeff,


    template <typename Coeffs>
    void GetCAVLCInfoChroma2(Ipp16s &sNumCoeff, Coeffs **ppPosCoefbuf)
    {
        IppStatus ippRes = DecodeCAVLCChromaDcCoeffs422_H264(&m_pbs,
                                                             &m_bitOffset,
                                                             &sNumCoeff,
                                                             ppPosCoefbuf,
                                                             m_tblCoeffToken[4],
                                                             (const Ipp32s **) m_tblTotalZerosCR422,
                                                             (const Ipp32s **) m_tblRunBefore);
        if (ippStsNoErr > ippRes)
            throw h264_exception(UMC_ERR_INVALID_STREAM);
    } // void GetCAVLCInfoChroma2(Ipp16s &sNumCoeff,

    inline void Drop1Bit();
    inline Ipp32u Peek1Bit();

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
    void InitializeContextVariablesIntra_CABAC(Ipp32s SliceQPy);

    // Initialize CABAC context(s) in inter slices
    void InitializeContextVariablesInter_CABAC(Ipp32s SliceQPy,
                                               Ipp32s cabac_init_idc);

    // Decode order of single bins
    Ipp32u DecodeSingleBinOnes_CABAC(Ipp32u ctxIdx,
                                     Ipp32s &binIdx);

    // Decode Ipp32s coefficient value
    Ipp32s DecodeSignedLevel_CABAC(Ipp32u ctxIdxOffset,
                                   Ipp32u &numDecodAbsLevelEq1,
                                   Ipp32u &numDecodAbsLevelGt1,
                                   Ipp32u max_value);
    Ipp32s DecodeSingleSignedLevel_CABAC(Ipp32u ctxIdxOffset);

    // Decode single bin from stream
    inline
    Ipp32u DecodeSingleBin_CABAC(Ipp32u ctxIdx);

    // Decode single bin using bypass decoding
    inline
    Ipp32u DecodeBypass_CABAC();

    inline
    Ipp32s DecodeBypassSign_CABAC(Ipp32s val);

    // Decode multiple bins using bypass decoding until ==1
    inline
    Ipp32u DecodeBypassOnes_CABAC();

    // Decode end symbol
    inline
    Ipp32u DecodeSymbolEnd_CABAC();

    template <typename Coeffs>
    void ResidualBlock8x8_CABAC_SVC(bool field_decoding_flag,
                                    const Ipp32s *single_scan,
                                    Coeffs *pPosCoefbuf,
                                    Ipp32s maxNumCoeffminus1,
                                    Ipp32s startCoeff)
    {
        // See subclause 7.3.5.3.2 of H.264 standard
        Ipp32u ctxIdxOffset, ctxIdxInc, ctxIdxOffsetLast;
        Ipp32u numDecodAbsLevelEq1 = 0, numDecodAbsLevelGt1 = 0;
        const Ipp32u *ctxIdxBase;
        const Ipp32s* pHPFF = hp_CtxIdxInc_sig_coeff_flag[field_decoding_flag];

        Ipp32u ncoefs = 0;
        Ipp32s i = startCoeff;
        Ipp16s coeffRuns[65];

    #ifdef __ICL
        __assume_aligned(pPosCoefbuf, 8);
    #endif
        memset(pPosCoefbuf, 0, sizeof(Coeffs) * 64);

        // See table 9-32 of H.264 standard
        if (field_decoding_flag)
            ctxIdxBase = ctxIdxOffset8x8FieldCoded;
        else
            ctxIdxBase = ctxIdxOffset8x8FrameCoded;

        ctxIdxOffset = ctxIdxBase[SIGNIFICANT_COEFF_FLAG] +
            ctxIdxBlockCatOffset[SIGNIFICANT_COEFF_FLAG][BLOCK_LUMA8X8_LEVELS];
        ctxIdxOffsetLast = ctxIdxBase[LAST_SIGNIFICANT_COEFF_FLAG] +
            ctxIdxBlockCatOffset[LAST_SIGNIFICANT_COEFF_FLAG][BLOCK_LUMA8X8_LEVELS];

        for (; i < maxNumCoeffminus1; i++)
        {
            ctxIdxInc = pHPFF[i];
            // get significant_coeff_flag
            if (DecodeSingleBin_CABAC(ctxIdxOffset+ctxIdxInc))
            {
                // store position of non-zero coeff
                coeffRuns[ncoefs] = (Ipp16s) i;
                // Intel compiler should use memory form of increment
                ncoefs ++;
                ctxIdxInc = hp_CtxIdxInc_last_sig_coeff_flag[i];
                // get last_significant_coeff_flag
                if (DecodeSingleBin_CABAC(ctxIdxOffsetLast+ctxIdxInc)) break;
            }
        }

        if (i == maxNumCoeffminus1)
        {
            coeffRuns[ncoefs] = (Ipp16s) i;
            ncoefs ++;
        }

        // calculate last coefficient in block
        ctxIdxOffset = ctxIdxBase[COEFF_ABS_LEVEL_MINUS1] +
            ctxIdxBlockCatOffset[COEFF_ABS_LEVEL_MINUS1][BLOCK_LUMA8X8_LEVELS];

        for (; ncoefs > 0; ncoefs--)
        {
            Ipp32s level = DecodeSignedLevel_CABAC(ctxIdxOffset,
                                                   numDecodAbsLevelEq1,
                                                   numDecodAbsLevelGt1,9);

            // store coeff position and level to coeff buffer
            Ipp32u pos = coeffRuns[ncoefs - 1];
            pos = single_scan[pos];

            pPosCoefbuf[pos] = (Ipp16s) level;
        }

    } // void ResidualBlock8x8_CABAC(bool field_decoding_flag,

    template <typename Coeffs>
    void ResidualBlock8x8_CABAC(bool field_decoding_flag,
                                const Ipp32s *single_scan,
                                Coeffs *pPosCoefbuf)
    {
        ResidualBlock8x8_CABAC_SVC(field_decoding_flag, single_scan,
                                   pPosCoefbuf, endIdx*4 + 3, startIdx * 4);
    } // void ResidualBlock8x8_CABAC


    template <typename Coeffs>
    void ResidualBlock4x4_CABAC_SVC(Ipp32s ctxBlockCat,
                                const Ipp32u *ctxIdxBase,
                                const Ipp32s *pScan,
                                Coeffs *pPosCoefbuf,
                                    Ipp32s maxNumCoeffminus1,
                                    Ipp32s startCoeff)
    {
        // See subclause 7.3.5.3.2 of H.264 standard
        Coeffs coeffRuns[18];
        Ipp32s iNumCoeffs;
        Ipp32s shiftContentIdx = 0;

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
            Ipp32u ctxIdxOffset, ctxIdxOffsetLast;
            Ipp32s i = startCoeff;

            ctxIdxOffset = ctxIdxBase[SIGNIFICANT_COEFF_FLAG] +
                           ctxIdxBlockCatOffset[SIGNIFICANT_COEFF_FLAG][ctxBlockCat];
            ctxIdxOffsetLast = ctxIdxBase[LAST_SIGNIFICANT_COEFF_FLAG] +
                               ctxIdxBlockCatOffset[LAST_SIGNIFICANT_COEFF_FLAG][ctxBlockCat];

            iNumCoeffs = 0;
            if (i < maxNumCoeffminus1)
            {
                do
                {
                    // get significant_coeff_flag
                    if (DecodeSingleBin_CABAC(ctxIdxOffset + i - shiftContentIdx))
                    {
                        // store position of non-zero coeff
                        coeffRuns[iNumCoeffs] = (Ipp16s) i;
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
            coeffRuns[iNumCoeffs] = (Ipp16s) maxNumCoeffminus1;
            iNumCoeffs += 1;
        }

no_more_coefficients_label:

        // calculate last coefficient in block
        Ipp32u ctxIdxOffset = ctxIdxBase[COEFF_ABS_LEVEL_MINUS1] +
                       ctxIdxBlockCatOffset[COEFF_ABS_LEVEL_MINUS1][ctxBlockCat];

        if (1 == iNumCoeffs)
        {
            // store coeff to coeff buffer
            Ipp32s pos = pScan[coeffRuns[0]];
            pPosCoefbuf[pos] = (Coeffs) DecodeSingleSignedLevel_CABAC(ctxIdxOffset);

        }
        // reorder coefficient(s)
        else
        {
            Ipp32u numDecodAbsLevelEq1 = 0, numDecodAbsLevelGt1 = 0;

            do
            {
                Ipp32s level = DecodeSignedLevel_CABAC(ctxIdxOffset,
                                                       numDecodAbsLevelEq1,
                                                       numDecodAbsLevelGt1,
                                                       9);

                // store coeff to coeff buffer
                Ipp32s pos = pScan[coeffRuns[iNumCoeffs - 1]];
                pPosCoefbuf[pos] = (Coeffs) level;

            } while (0 < --iNumCoeffs);
        }

    } // void ResidualBlock4x4_CABAC_SVC

    template <typename Coeffs>
    void ResidualBlock4x4_CABAC(Ipp32s ctxBlockCat,
                                const Ipp32u *ctxIdxBase,
                                const Ipp32s *pScan,
                                Coeffs *pPosCoefbuf,
                                Ipp32s maxNumCoeffminus1)
    {
        Ipp32s tmpStartIdx = startIdx;

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

    void SetIdx (Ipp32s x, Ipp32s y)
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
    Ipp32u m_lcodIRange;                                        // (Ipp32u) arithmetic decoding engine variable
    Ipp32u m_lcodIOffset;                                       // (Ipp32u) arithmetic decoding engine variable
    Ipp32s m_iMagicBits;                                        // (Ipp32s) available extra CABAC bits
    Ipp16u *m_pMagicBits;                                       // (Ipp16u *) pointer to CABAC data

    Ipp32s startIdx;                                            // Start index for coeffs decoding
    Ipp32s endIdx;                                              // Start index for coeffs decoding

    // Decoding SEI message functions
    Ipp32s sei_message(const Headers & headers,Ipp32s current_sps,H264SEIPayLoad *spl);
    Ipp32s sei_payload(const Headers & headers,Ipp32s current_sps,H264SEIPayLoad *spl);
    Ipp32s buffering_period(const Headers & headers,Ipp32s current_sps,H264SEIPayLoad *spl);
    Ipp32s pic_timing(const Headers & headers,Ipp32s current_sps,H264SEIPayLoad *spl);
    void user_data_registered_itu_t_t35(H264SEIPayLoad *spl);
    void recovery_point(H264SEIPayLoad *spl);
    Ipp32s dec_ref_pic_marking_repetition(const Headers & headers, Ipp32s current_sps,H264SEIPayLoad *spl);

    void unparsed_sei_message(H264SEIPayLoad *spl);

    void scalability_info(H264SEIPayLoad *spl);
    void scalable_nesting(H264SEIPayLoad *spl);

    friend class TableInitializer;
};

// disable the "conditional expression is constant" warning
#pragma warning(disable: 4127)

template <typename Coeffs, Ipp32s color_format>
class BitStreamColorSpecific
{
public:
    typedef Coeffs * CoeffsPtr;

    static inline void ResidualChromaDCBlock_CABAC(const Ipp32u *ctxIdxBase, const Ipp32s *single_scan,
                                                   Coeffs *pPosCoefbuf, H264Bitstream * bs)
    {
        // See subclause 7.3.5.3.2 of H.264 standard
        Ipp32s coef_ctr=-1;
        Ipp32s maxNumCoeffminus1;
        Ipp32u ctxIdxOffset, ctxIdxOffsetLast;
        Ipp32u numDecodAbsLevelEq1 = 0, numDecodAbsLevelGt1 = 0;
        Ipp16s coeffRuns[18];

        Ipp32s numOfCoeffs = 0;
        switch (color_format)
        {
        case 1:
            numOfCoeffs = 4;
            break;
        case 2:
            numOfCoeffs = 8;
            break;
        case 3:
            numOfCoeffs = 16;
            break;
        };

        // See table 9-32 of H.264 standard
    #ifdef __ICL
        __assume_aligned(pPosCoefbuf, 8);
    #endif
        memset(pPosCoefbuf, 0, sizeof(Coeffs) * numOfCoeffs);
        maxNumCoeffminus1 = numOfCoeffs - 1;

        ctxIdxOffset = ctxIdxBase[SIGNIFICANT_COEFF_FLAG] +
            ctxIdxBlockCatOffset[SIGNIFICANT_COEFF_FLAG][BLOCK_CHROMA_DC_LEVELS + color_format];
        ctxIdxOffsetLast = ctxIdxBase[LAST_SIGNIFICANT_COEFF_FLAG] +
            ctxIdxBlockCatOffset[LAST_SIGNIFICANT_COEFF_FLAG][BLOCK_CHROMA_DC_LEVELS + color_format];

        Ipp32u ncoefs = 0;
        Ipp32s i = 0;
        for (; i < maxNumCoeffminus1; i++)
        {
            Ipp32s k;

            if (color_format == 1)
                k = i;
            else
                k = IPP_MIN(i >> (3 & (color_format - 1)), 2);

            // get significant_coeff_flag
            if (bs->DecodeSingleBin_CABAC(ctxIdxOffset+k))
            {
                // store position of non-zero coeff
                coeffRuns[ncoefs] = (Ipp16s) i;
                // Intel compiler should use memory form of increment
                ncoefs ++;
                // get last_significant_coeff_flag
                if (bs->DecodeSingleBin_CABAC(ctxIdxOffsetLast + k))
                    break;
            }
        }

        if (i == maxNumCoeffminus1)
        {
            coeffRuns[ncoefs] = (Ipp16s) i;
            ncoefs ++;
        }

        // calculate last coefficient in block
        ctxIdxOffset = ctxIdxBase[COEFF_ABS_LEVEL_MINUS1] +
            ctxIdxBlockCatOffset[COEFF_ABS_LEVEL_MINUS1][BLOCK_CHROMA_DC_LEVELS + color_format];

        for (; ncoefs > 0; ncoefs--)
        {
            Ipp32s level = bs->DecodeSignedLevel_CABAC(ctxIdxOffset,
                                                       numDecodAbsLevelEq1,
                                                       numDecodAbsLevelGt1,8);

            // store coeff position and level to coeff buffer
            Ipp32u pos = coeffRuns[ncoefs - 1] + coef_ctr + 1;

            if (color_format != 1)
                pos = single_scan[pos];

            pPosCoefbuf[pos] = (Coeffs) level;
        }

    } // void H264Bitstream::ResidualChromaDCBlock_CABAC(const Ipp32u *ctxIdxBase,
};
#pragma warning(default: 4127)

} // namespace UMC

#include "umc_h264_bitstream_inlines.h"

#endif // __UMC_H264_BITSTREAM_H_

#endif // UMC_ENABLE_H264_VIDEO_DECODER
