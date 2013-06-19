/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2012-2013 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef __H265_TR_QUANT_H
#define __H265_TR_QUANT_H

#include "umc_h265_dec_defs_dec.h"
#include "umc_h265_bitstream.h"
#include "h265_coding_unit.h"
#include "h265_global_rom.h"
#include "assert.h"

#define QP_BITS 15

namespace UMC_HEVC_DECODER
{

typedef struct
{
    Ipp32s m_SignificantCoeffGroupBits[NUM_SIG_CG_FLAG_CTX][2];

    Ipp32s m_significantBits[NUM_SIG_FLAG_CTX][2];
    Ipp32s m_LastXBits[32];
    Ipp32s m_LastYBits[32];

    Ipp32s m_GreaterOneBits[NUM_ONE_FLAG_CTX][2];
    Ipp32s m_LevelAbsBits[NUM_ABS_FLAG_CTX][2];

    Ipp32s m_BlockCbpBits[3 * NUM_QT_CBF_CTX][2];
    Ipp32s m_BlockRootCbpBits[4][2];
    Ipp32s m_ScanZigzag[2];            ///< flag for zigzag scan
    Ipp32s m_ScanNonZigzag[2];         ///< flag for non zigzag scan
} EstBitsSbacStruct;

// QP class
class QPParam
{
public:
    QPParam();

    Ipp32s m_QP;
    Ipp32s m_Per;
    Ipp32s m_Rem;

public:

    H265_FORCEINLINE void SetQPParam(Ipp32s qpScaled)
    {
        m_QP   = qpScaled;
        m_Per  = qpScaled / 6;
        m_Rem  = qpScaled % 6;
    }

    void Clear()
    {
        m_QP = 0;
        m_Per = 0;
        m_Rem = 0;
    }


    Ipp32s GetPer() const
    {
        return m_Per;
    }
    Ipp32s GetRem() const
    {
        return m_Rem;
    }

    Ipp32s GetQP()
    {
        return m_QP;
    }
}; // END CLASS DEFINITION QPParam

// transform and quantization class
class H265TrQuant
{
public:
    H265TrQuant();
    ~H265TrQuant();

    // initialize class
    void Init(Ipp32u MaxWidth, Ipp32u MaxHeight, Ipp32u MaxTrSize);

    // transform & inverse transform functions
    template <typename DstCoeffsType>
    void InvTransformNxN(bool transQuantBypass, EnumTextType TxtType, Ipp32u Mode, DstCoeffsType* pResidual, Ipp32u Stride,
        H265CoeffsPtrCommon pCoeff,Ipp32u Width, Ipp32u Height, Ipp32s scalingListType, bool transformSkip);

    void InvRecurTransformNxN(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Size, Ipp32u TrMode);

#if (HEVC_OPT_CHANGES & 128)
    // ML: OPT: allows to propogate convert const shift
    template <int bitDepth, typename DstCoeffsType>
    void InvTransformSkip(H265CoeffsPtrCommon pCoeff, DstCoeffsType* pResidual, Ipp32u Stride, Ipp32u Width, Ipp32u Height);
#else
    template <typename DstCoeffsType>
    void InvTransformSkip(Ipp32s bitDepth, H265CoeffsPtrCommon pCoeff, DstCoeffsType* pResidual, Ipp32u Stride, Ipp32u Width, Ipp32u Height);
#endif

    // Misc functions
    H265_FORCEINLINE void SetQPforQuant(Ipp32s QP, EnumTextType TxtType, Ipp32s qpBdOffset, Ipp32s chromaQPOffset)
    {
        Ipp32s qpScaled;

        if (TxtType == TEXT_LUMA)
        {
            qpScaled = QP + qpBdOffset;
        }
        else
        {
            qpScaled = Clip3(-qpBdOffset, 57, QP + chromaQPOffset);

            if (qpScaled < 0)
            {
                qpScaled = qpScaled + qpBdOffset;
            }
            else
            {
                qpScaled = g_ChromaScale[qpScaled] + qpBdOffset;
            }
        }
        m_QPParam.SetQPParam(qpScaled);
    }

    void SetLambda(Ipp64f Lambda)
    {
        m_Lambda = Lambda;
    }
    void SetRDOQOffset(Ipp32u RDOQOffset)
    {
        m_RDOQOffset = RDOQOffset;
    }

    EstBitsSbacStruct* m_EstBitsSbac;

    static bool bothCGNeighboursOne(const Ipp32u* SigCoeffGroupFlag,
                                    const Ipp32u  CGPosX,
                                    const Ipp32u  CGPosY,
                                    Ipp32u width, Ipp32u height);

    bool m_UseScalingList;

    void setScalingListDec(H265ScalingList *scalingList);
    void setDefaultScalingList();

protected:
    __inline void processScalingListDec(Ipp32s *coeff, Ipp16s *dequantcoeff, Ipp32s invQuantScales, Ipp32u height, Ipp32u width, Ipp32u ratio, Ipp32u sizuNum, Ipp32u dc);

    QPParam m_QPParam;
    Ipp64f m_Lambda;
    Ipp32u m_RDOQOffset;
    Ipp32u m_MaxTrSize;
    bool m_EncFlag;
    bool m_UseRDOQ;

    // ML: OPT: TODO: Check if we really need 32-bit here
    Ipp16s* m_dequantCoef[SCALING_LIST_SIZE_NUM][SCALING_LIST_NUM][SCALING_LIST_REM_NUM]; ///< array of dequantization matrix coefficient 4x4

private:
    void initScalingList();
    void destroyScalingList();

    H265CoeffsPtrCommon m_residualsBuffer;
    H265CoeffsPtrCommon m_residualsBuffer1;
    H265CoeffsPtrCommon m_tempTransformBuffer;

#if (HEVC_OPT_CHANGES & 2)
    H265_FORCEINLINE
#endif
    Ipp16s* getDequantCoeff(Ipp32u list, Ipp32u qp, Ipp32u size)
    {
        return m_dequantCoef[size][list][qp];
    }; //!< get DeQuant Coefficent

    // forward Transform
    void Transform(Ipp32u Mode, Ipp8u* pResidual, Ipp32u Stride, Ipp32s* pCoeff, Ipp32s Width, Ipp32s Height);

#if (HEVC_OPT_CHANGES & 512)
    template< Ipp32u c_Log2TrSize, Ipp32s bitDepth >
    void DeQuant_inner(const H265CoeffsPtrCommon pQCoef, Ipp32u Length, Ipp32s scalingListType);

    // dequantization
    template< Ipp32s bitDepth >
    void DeQuant(H265CoeffsPtrCommon pSrc, Ipp32u Width, Ipp32u Height, Ipp32s scalingListType);
#else
    // dequantization
    void DeQuant(Ipp32s bitDepth, H265CoeffsPtrCommon pSrc, Ipp32u Width, Ipp32u Height, Ipp32s scalingListType);
#endif // (HEVC_OPT_CHANGES & 128)
};// END CLASS DEFINITION H265TrQuant

} // end namespace UMC_HEVC_DECODER

#endif //__H265_TR_QUANT_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
