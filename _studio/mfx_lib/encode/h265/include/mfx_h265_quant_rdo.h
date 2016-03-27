//
//               INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in accordance with the terms of that agreement.
//        Copyright (c) 2013 - 2016 Intel Corporation. All Rights Reserved.
//

#if defined (MFX_ENABLE_H265_VIDEO_ENCODE)

#ifndef __MFX_H265_QUANT_RDO_H__
#define __MFX_H265_QUANT_RDO_H__

#include "mfx_h265_defs.h"
#include "mfx_h265_ctb.h"
#include "mfx_h265_cabac.h"

namespace H265Enc {

#define NUM_SIG_CG_FLAG_CTX           2       ///< number of context models for MULTI_LEVEL_SIGNIFICANCE
#define NUM_SIG_FLAG_CTX              42      ///< number of context models for sig flag

// for fast cmp with HM
typedef struct
{
  Ipp32s significantCoeffGroupBits[NUM_SIG_CG_FLAG_CTX][2];
  Ipp32s significantBits[27][2];
  Ipp32s lastXBits[15];
  Ipp32s lastYBits[15];
  Ipp32s greaterOneBits[NUM_ONE_FLAG_CTX_LUMA][2];
  Ipp32s levelAbsBits[NUM_ABS_FLAG_CTX_LUMA][2];

  Ipp32s blkCbfBits[NUM_QT_CBF_CTX][2];
  Ipp32s blkRootCbfBits[1][2];
} CabacBits;


struct RDOQCabacState
{
    Ipp32u    ctx_set;
    Ipp32u    c1;
    Ipp32u    c2;
    Ipp32u    c1Idx;
    Ipp32u    c2Idx;
    Ipp32u    go_rice_param;
};

template <typename PixType>
class RDOQuant
{
public:
    RDOQuant(H265CU<PixType>* pCU, H265BsFake* bs, EnumTextType type);
     ~RDOQuant();

     template <Ipp8u rdoqCGZ, Ipp8u SBH>
     void DoAlgorithm(
         Ipp16s* pSrc,
         Ipp16s* pDst,
         Ipp32s  log2_tr_size,
         Ipp32s  bit_depth,
         Ipp32s  is_slice_i,
         Ipp32s  is_blk_i,
         EnumTextType type,
         Ipp32u  abs_part_idx,
         Ipp32s  QP);

private:

    Ipp32u      GetCoefScanIdx(Ipp32u abs_part_idx, Ipp32u log2Width, bool bIsLuma)
    {
        Ipp32u scanIdx = ((H265CU<PixType>*)m_pCU)->GetCoefScanIdx(abs_part_idx, log2Width, bIsLuma, ((H265CU<PixType>*)m_pCU)->IsIntra(abs_part_idx) );

        if (scanIdx == COEFF_SCAN_ZIGZAG)
        {
            scanIdx = COEFF_SCAN_DIAG;
        }

        return scanIdx;
    }

    Ipp32u GetBestQuantLevel (
        Ipp64f& rd64CodedCost,
        Ipp64f& cost_level_0,
        Ipp64f& cost_sig,
        Ipp32s  level_float,
        Ipp32u  max_abs_level,
        Ipp32u  ctx_sig_inc,
        const RDOQCabacState& local_cabac,
        Ipp32s  qbits,
        Ipp64f  errScale,
        bool    isLast);

    Ipp64f GetCost_EncodeOneCoeff(
        Ipp32u  qlevel,
        const RDOQCabacState & cabac);

    Ipp64f GetCost_SigCoef(
        Ipp16u  code,
        Ipp32u  ctx_inc) { return GetCost( m_cabacBits.significantBits[ ctx_inc ][ code ] ); }

    Ipp64f GetCost_SigCoeffGroup(
        Ipp16u  code,
        Ipp32u  ctx_inc) {return GetCost( m_cabacBits.significantCoeffGroupBits[ ctx_inc ][ code ] );}

    Ipp64f GetCost_Cbf(
        Ipp16u  code,
        Ipp32u  ctx_inc,
        bool    isRootCbf);

    Ipp64f GetCost_LastXY(
        Ipp32u  pos_x,
        Ipp32u  pos_y);

    Ipp64f GetCost( Ipp64f bit_cost ) { return m_lambda * bit_cost; }

    void EstimateCabacBits( Ipp32s log2_tr_size );

    void EstimateLastXYBits(
        CabacBits* pBits,
        Ipp32s width);

    H265CU<PixType>*       m_pCU;
    H265BsFake* m_bs;
    Ipp64f      m_lambda;
    CabacBits   m_cabacBits;
    Ipp8u m_bitDepth;
    EnumTextType m_textType;
};

} // namespace

#endif // __MFX_H265_QUANT_RDO_H__
#endif // (MFX_ENABLE_H265_VIDEO_ENCODE)
/* EOF */