/*
//
//              INTEL CORPORATION PROPRIETARY INFORMATION
//  This software is supplied under the terms of a license  agreement or
//  nondisclosure agreement with Intel Corporation and may not be copied
//  or disclosed except in  accordance  with the terms of that agreement.
//        Copyright (c) 2003-2012 Intel Corporation. All Rights Reserved.
//
//
*/
#include "umc_defs.h"
#if defined (UMC_ENABLE_H264_VIDEO_DECODER)

#ifndef __UMC_H264_DEC_IPP_LEVEL_H
#define __UMC_H264_DEC_IPP_LEVEL_H

#include "ippvc.h"

namespace UMC_H264_DECODER
{

extern void ConvertNV12ToYV12(const Ipp8u *pSrcDstUVPlane, const Ipp32u _srcdstUVStep, Ipp8u *pSrcDstUPlane, Ipp8u *pSrcDstVPlane, const Ipp32u _srcdstUStep, IppiSize roi);
extern void ConvertYV12ToNV12(const Ipp8u *pSrcDstUPlane, const Ipp8u *pSrcDstVPlane, const Ipp32u _srcdstUStep, Ipp8u *pSrcDstUVPlane, const Ipp32u _srcdstUVStep, IppiSize roi);

extern void ConvertNV12ToYV12(const Ipp16u *pSrcDstUVPlane, const Ipp32u _srcdstUVStep, Ipp16u *pSrcDstUPlane, Ipp16u *pSrcDstVPlane, const Ipp32u _srcdstUStep, IppiSize roi);
extern void ConvertYV12ToNV12(const Ipp16u *pSrcDstUPlane, const Ipp16u *pSrcDstVPlane, const Ipp32u _srcdstUStep, Ipp16u *pSrcDstUVPlane, const Ipp32u _srcdstUVStep, IppiSize roi);

#define   IPPFUN(type,name,arg)   extern type __STDCALL name arg

#if 0
IPPFUN(IppStatus, ippiReconstructChromaIntraMB_H264_16s8u_P1R, (Ipp16s **ppSrcCoeff,
                                                                Ipp8u *pSrcDstUVPlane,
                                                                const Ipp32u _srcdstUVStep,
                                                                const IppIntraChromaPredMode_H264 intra_chroma_mode,
                                                                const Ipp32u cbp4x4,
                                                                const Ipp32u ChromaQP,
                                                                const Ipp8u edge_type));

IPPFUN(IppStatus, ippiReconstructChromaInterMB_H264_16s8u_P1R, (Ipp16s **ppSrcCoeff,
                                                                Ipp8u *pSrcDstUVPlane,
                                                                const Ipp32u srcdstUVStep,
                                                                const Ipp32u cbp4x4,
                                                                const Ipp32u ChromaQP));
#endif

IPPFUN(IppStatus, ippiTransformResidualAndAdd_H264_16s8u_C1I_NV12,(const Ipp8u *pPred,
       Ipp16s *pCoeffs,
       Ipp8u *pRec,
       Ipp32s predPitch,
       Ipp32s recPitch));
IPPFUN(IppStatus, ippiTransformResidualAndAdd_H264_16s16s_C1I_NV12, (const Ipp16s *pPred,
       Ipp16s *pCoeffs,
       Ipp16s *pRec,
       Ipp32s predPitch,
       Ipp32s recPitch));
IPPFUN(IppStatus, ippiTransformResidual_H264_16s16s_C1I_NV12,(const Ipp16s *pCoeffs,
       Ipp16s *pRec,
       Ipp32s recPitch));

IPPFUN(IppStatus, ippiUniDirWeightBlock_NV12_H264_8u_C1IR, (Ipp8u *pSrcDst,
                                                      Ipp32u srcDstStep,
                                                      Ipp32u ulog2wd,
                                                      Ipp32s iWeightU,
                                                      Ipp32s iOffsetU,
                                                      Ipp32s iWeightV,
                                                      Ipp32s iOffsetV,
                                                      IppiSize roi));

IPPFUN(IppStatus, ippiBiDirWeightBlock_NV12_H264_8u_P3P1R,( const Ipp8u *pSrc1,
    const Ipp8u *pSrc2,
    Ipp8u *pDst,
    Ipp32u nSrcPitch1,
    Ipp32u nSrcPitch2,
    Ipp32u nDstPitch,
    Ipp32u ulog2wd,    /* log2 weight denominator */
    Ipp32s iWeightU1,
    Ipp32s iOffsetU1,
    Ipp32s iWeightU2,
    Ipp32s iOffsetU2,
    Ipp32s iWeightV1,
    Ipp32s iOffsetV1,
    Ipp32s iWeightV2,
    Ipp32s iOffsetV2,
    IppiSize roi
    ));

IPPFUN(IppStatus, ippiReconstructChromaIntraHalfsMB_NV12_H264_16s8u_P2R, (Ipp16s **ppSrcCoeff,
       Ipp8u *pSrcDstUVPlane,
       Ipp32u srcdstUVStep,
       IppIntraChromaPredMode_H264 intra_chroma_mode,
       Ipp32u cbp4x4,
       Ipp32u ChromaQP,
       Ipp8u edge_type_top,
       Ipp8u edge_type_bottom));

IPPFUN(IppStatus, ippiFilterDeblockingChroma_NV12_VerEdge_MBAFF_H264_8u_C1IR, (const IppiFilterDeblock_8u * pDeblockInfo));

#if 0
IPPFUN(IppStatus,ippiFilterDeblockingChroma_NV12_VerEdge_H264_8u_C2IR,(const IppiFilterDeblock_8u * pDeblockInfo));

IPPFUN(IppStatus,ippiFilterDeblockingChroma_NV12_HorEdge_H264_8u_C2IR,(const IppiFilterDeblock_8u * pDeblockInfo));


IPPFUN(IppStatus, ippiReconstructChromaIntra4x4MB_NV12_H264_16s8u_P2R, (Ipp16s **ppSrcDstCoeff,
                                                                   Ipp8u *pSrcDstUVPlane,
                                                                   Ipp32u _srcdstUVStep,
                                                                   IppIntraChromaPredMode_H264 intra_chroma_mode,
                                                                   Ipp32u cbp4x4,
                                                                   Ipp32u chromaQPU,
                                                                   Ipp32u chromaQPV,
                                                                   Ipp8u edge_type,
                                                                   const Ipp16s *pQuantTableU,
                                                                   const Ipp16s *pQuantTableV,
                                                                   Ipp8u bypass_flag));

IPPFUN(IppStatus, ippiReconstructChromaInter4x4MB_NV12_H264_16s8u_P2R, (Ipp16s **ppSrcDstCoeff,
                                                                   Ipp8u *pSrcDstUVPlane,
                                                                   Ipp32u _srcdstUVStep,
                                                                   Ipp32u cbp4x4,
                                                                   Ipp32u chromaQPU,
                                                                   Ipp32u chromaQPV,
                                                                   const Ipp16s *pQuantTableU,
                                                                   const Ipp16s *pQuantTableV,
                                                                   Ipp8u bypass_flag));

#endif

IPPFUN(IppStatus, ippiReconstructChromaIntraHalfs4x4MB_NV12_H264_16s8u_P2R, (Ipp16s **ppSrcDstCoeff,
                                                                        Ipp8u *pSrcDstUVPlane,
                                                                        Ipp32u _srcdstUVStep,
                                                                        IppIntraChromaPredMode_H264 intra_chroma_mode,
                                                                        Ipp32u cbp4x4,
                                                                        Ipp32u chromaQPU,
                                                                        Ipp32u chromaQPV,
                                                                        Ipp8u edge_type_top,
                                                                        Ipp8u edge_type_bottom,
                                                                        const Ipp16s *pQuantTableU,
                                                                        const Ipp16s *pQuantTableV,
                                                                        Ipp8u bypass_flag));



/* ///////////////////////////////////////////////////////////////////////////
//  Name:
//    ippiDequantChroma_H264_16s_C1
//
//  Purpose:
//    Performs dequantization of chroma residuals.
//
//  Parameters:
//    ppSrcCoeff - pointer to block of coefficients, if it's non zero(will be updated by function)
//    pDst       - pointer to the dequantized coefficients.
//    cbp4x4     - coded block pattern
//    ChromaQP   - chroma quantizer
//    iblFlag    - flag equal to a non-zero value when
//                 macroblock type is INTRA_BL
//
//  Returns:
//    ippStsNoErr         No error
//    ippStsNullPtrErr    One of the pointers is NULL
//    ippStsOutOfRangeErr ChromaQP is greater than 39
*/

IPPAPI(IppStatus, ippiDequantChroma_H264_16s_C1, (Ipp16s **ppSrcCoeff,
    Ipp16s *pDst,
    const Ipp32u cbp4x4,
    const Ipp32u ChromaQP,
    Ipp8u iblFlag))

/* ///////////////////////////////////////////////////////////////////////////
//  Name:
//    ippiDequantChromaHigh_H264_16s_C1
//
//  Purpose:
//    Performs dequantization of chroma residuals for high profile.
//
//  Parameters:
//    ppSrcCoeff - pointer to block of coefficients, if it's non zero(will be updated by function)
//    pDst       - pointer to the dequantized coefficients.
//    cbp4x4     - coded block pattern
//    chromaQPU     - chroma quantizer for U plane
//    chromaQPV     - chroma quantizer for V plane
//    pQuantTableU  - pointer to quantization table for U plane
//    pQuantTableV  - pointer to quantization table for V plane
//    iblFlag       - flag equal to a non-zero value when
//                    macroblock type is INTRA_BL
//
//  Returns:
//    ippStsNoErr         No error
//    ippStsNullPtrErr    One of the pointers is NULL
//    ippStsOutOfRangeErr ChromaQP is greater than 39
*/

IPPAPI(IppStatus, ippiDequantChromaHigh_H264_16s_C1, (Ipp16s **ppSrcCoeff,
    Ipp16s *pDst,
    const Ipp32u cbp4x4,
    const Ipp32s chromaQPU,
    const Ipp32s chromaQPV,
    const Ipp16s *pQuantTableU,
    const Ipp16s *pQuantTableV,
    Ipp8u iblFlag))

/* ///////////////////////////////////////////////////////////////////////////
//  Name:
//    ippiDequantResidual_H264_16s_C1I
//
//  Purpose:
//    Performs dequantization of residuals.
//
//  Parameters:
//    pCoeffs - pointer to the initial coefficients.
//    pDst    - pointer to the dequantized coefficients.
//    QP      - quantization parameter.
//
//  Returns:
//    ippStsNoErr          No error
//    ippStsNullPtrErr     One of the pointers is NULL
//    ippStsOutOfRangeErr  QP is less than 0 or greater than 51
*/

IPPAPI(IppStatus, ippiDequantResidual_H264_16s_C1, (const Ipp16s *pCoeffs,
    Ipp16s *pDst,
    Ipp32s QP))

/* ///////////////////////////////////////////////////////////////////////////
//  Name:
//    ippiDequantResidual4x4_H264_16s_C1
//
//  Purpose:
//    Performs dequantization of 4x4 residuals for high profile.
//
//  Parameters:
//    pCoeffs     - pointer to the initial coefficients.
//    pDst        - pointer to the dequantized coefficients.
//    QP          - quantization parameter.
//    pQuantTable - pointer to quantization table
//    bypass_flag - enable lossless coding when qpprime_y is zero
//
//  Returns:
//    ippStsNoErr          No error
//    ippStsNullPtrErr     One of the pointers is NULL
//    ippStsOutOfRangeErr  QP is less than 0 or greater than 51
*/

IPPAPI(IppStatus, ippiDequantResidual4x4_H264_16s_C1, (const Ipp16s *pCoeffs,
    Ipp16s *pDst,
    Ipp32s QP,
    const Ipp16s *pQuantTable,
    Ipp8u bypass_flag))

/* ///////////////////////////////////////////////////////////////////////////
//  Name:
//    ippiDequantResidual4x4_H264_16s_C1
//
//  Purpose:
//    Performs dequantization of 8x8 residuals for high profile.
//
//  Parameters:
//    pCoeffs     - pointer to the initial coefficients.
//    pDst        - pointer to the dequantized coefficients.
//    QP          - quantization parameter.
//    pQuantTable - pointer to quantization table
//    bypass_flag - enable lossless coding when qpprime_y is zero
//
//  Returns:
//    ippStsNoErr          No error
//    ippStsNullPtrErr     One of the pointers is NULL
//    ippStsOutOfRangeErr  QP is less than 0 or greater than 51
*/

IPPAPI(IppStatus, ippiDequantResidual8x8_H264_16s_C1, (const Ipp16s *pCoeffs,
    Ipp16s *pDst,
    Ipp32s QP,
    const Ipp16s *pQuantTable,
    Ipp8u bypass_flag))

/* ///////////////////////////////////////////////////////////////////////////
//  Name:
//    ippiTransformResidual_H264_16s16s_C1I
//
//  Purpose:
//    Performs integer inverse transformation and
//    shift by 6 bits for 4x4 block of residuals.
//
//  Parameters:
//    pCoeffs - pointer to the dequantized coefficients.
//    pDst    - pointer to the resultant residuals.
//    step    - step (in bytes) of the source and destination array.
//
//  Returns:
//    ippStsNoErr          No error
//    ippStsNullPtrErr     One of the pointers is NULL
*/

IPPAPI(IppStatus, ippiTransformResidual_H264_16s16s_C1I, (const Ipp16s *pCoeffs,
    Ipp16s *pDst,
    Ipp32s step))

/* ///////////////////////////////////////////////////////////////////////////
//  Name:
//    ippiTransformResidual8x8_H264_16s16s_C1I
//
//  Purpose:
//    Performs integer inverse transformation and
//    shift by 6 bits for 8x8 block of residuals.
//
//  Parameters:
//    pCoeffs - pointer to the dequantized coefficients.
//    pDst    - pointer to the resultant residuals.
//    step    - step (in bytes) of the source and destination array.
//
//  Returns:
//    ippStsNoErr          No error
//    ippStsNullPtrErr     One of the pointers is NULL
*/

IPPAPI(IppStatus, ippiTransformResidual8x8_H264_16s16s_C1I, (const Ipp16s *pCoeffs,
    Ipp16s *pDst,
    Ipp32s step))

/* ///////////////////////////////////////////////////////////////////////////
//  Name:
//    ippiTransformResidualAndAdd_H264_16s8u_C1I
//
//  Purpose:
//  Performs integer inverse transformation and
//  shift by 6 bits for 4x4 block of residuals
//  with subsequent intra prediction or motion
//  compensation.
//
//
//  Parameters:
//    pPred       -  pointer to the reference 4x4 block, which is used for intra
//                                       prediction or motion compensation.
//    pCoeffs     -  pointer to the dequantized coefficients - array of size 16.
//    pDst        -  pointer to the destination 4x4 block.
//    PredStep    -  reference frame step in bytes.
//    DstStep     -  destination frame step in bytes.
//
//  Returns:
//    ippStsNoErr          No error
//    ippStsNullPtrErr     One of the pointers is NULL
//
*/

IPPAPI(IppStatus, ippiTransformResidualAndAdd_H264_16s8u_C1I,(const Ipp8u *pPred,
    const Ipp16s *pCoeffs,
    Ipp8u *pDst,
    Ipp32s PredStep,
    Ipp32s DstStep))

/* ///////////////////////////////////////////////////////////////////////////
//  Name:
//    ippiTransformResidualAndAdd8x8_H264_16s8u_C1I
//
//  Purpose:
//  Performs integer inverse transformation and
//  shift by 6 bits for 8x8 block of residuals
//  with subsequent intra prediction or motion
//  compensation.
//
//
//  Parameters:
//    pPred       -  pointer to the reference 8x8 block, which is used for intra
//                                       prediction or motion compensation.
//    pCoeffs     -  pointer to the dequantized coefficients - array of size 64.
//    pDst        -  pointer to the destination 8x8 block.
//    PredStep    -  reference frame step in bytes.
//    DstStep     -  destination frame step in bytes.
//
//  Returns:
//    ippStsNoErr          No error
//    ippStsNullPtrErr     One of the pointers is NULL
//
*/

IPPAPI(IppStatus, ippiTransformResidualAndAdd8x8_H264_16s8u_C1I, (const Ipp8u *pPred,
    const Ipp16s *pCoeffs,
    Ipp8u *pDst,
    Ipp32s predStep,
    Ipp32s recStep))

/* ///////////////////////////////////////////////////////////////////////////
//  Name:
//    ippiTransformResidualAndAdd_H264_16s16s_C1I
//
//  Purpose:
//  Performs integer inverse transformation and
//  shift by 6 bits for 4x4 block of residuals
//  with subsequent residuals prediction.
//
//
//  Parameters:
//    pPred       -  pointer to the reference 4x4 block, which is used for
//                                       residuals prediction.
//    pCoeffs     -  pointer to the dequantized coefficients - array of size 16.
//    pDst        -  pointer to the destination 4x4 block.
//    PredStep    -  reference frame step in bytes.
//    DstStep     -  destination frame step in bytes.
//
//  Returns:
//    ippStsNoErr          No error
//    ippStsNullPtrErr     One of the pointers is NULL
//
*/

IPPAPI(IppStatus, ippiTransformResidualAndAdd_H264_16s16s_C1I, (const Ipp16s *pPred,
    const Ipp16s *pCoeffs,
    Ipp16s *pDst,
    Ipp32s PredStep,
    Ipp32s DstStep))

/* ///////////////////////////////////////////////////////////////////////////
//  Name:
//    ippiTransformResidualAndAdd8x8_H264_16s16s_C1I
//
//  Purpose:
//  Performs integer inverse transformation and
//  shift by 6 bits for 8x8 block of residuals
//  with subsequent residuals prediction.
//
//
//  Parameters:
//    pPred       -  pointer to the reference 8x8 block, which is used for
//                                        residuals prediction.
//    pCoeffs     -  pointer to the dequantized coefficients - array of size 64.
//    pDst        -  pointer to the destination 8x8 block.
//    PredStep    -  reference frame step in bytes.
//    DstStep     -  destination frame step in bytes.
//
//  Returns:
//    ippStsNoErr          No error
//    ippStsNullPtrErr     One of the pointers is NULL
//
*/

IPPAPI(IppStatus, ippiTransformResidualAndAdd8x8_H264_16s16s_C1I,(const Ipp16s *pPred,
    const Ipp16s *pCoeffs,
    Ipp16s *pRec,
    Ipp32s PredStep,
    Ipp32s DstStep))

/* ///////////////////////////////////////////////////////////////////////////
//  Name:
//      own_ippiDecodeCAVLCCoeffsIdxs_H264_1u16s
//
//  Purpose:
//      Decode CAVLC coded coefficients
//
//  Parameters:
//      ppBitStream     - double pointer to current dword in bitstream(will be updated by function)
//      pOffset         - pointer to offset in current dword(will be updated by function)
//      pNumCoeff       - output number of coefficients
//      ppPosCoefbuf    - pointer to 4x4 block of coefficients, if it's non zero(will be update by function)
//      uVLCSelect      - predictor on number of CoeffToken Table
//      uMaxNumCoeff    - maximum coefficients in block(16 for Intra16x16, 15 for the rest)
//      pTblCoeffToken  - CoeffToken Tables
//      ppTblTotalZeros - TotalZeros Tables
//      ppTblRunBefore  - RunBefore Tables
//      pScanMatrix     - inverse scan matrix for coefficients in block
//      scanIdxStart    - the first scanning position for the transform coefficient levels
//      scanIdxEnd      - the last scanning position for the transform coefficient levels
//
//  Returns:
//      ippStsNoErr         No error
//      ippStsNullPtrErr    if a pointer is NULL
//      ippStsOutOfRangeErr scanIdxStart or scanIdxEnd is out of the range [0, 15]
//      ippStsRangeErr      scanIdxStart is greater than scanIdxEnd
//
//  Notes:
//      H.264 standard: JVT-G050. ITU-T Recommendation and
//      Final Draft International Standard of Joint Video Specification
//      (ITU-T Rec. H.264 | ISO/IEC 14496-10 AVC) March, 2003.
*/

IPPAPI(IppStatus, own_ippiDecodeCAVLCCoeffsIdxs_H264_1u16s, (Ipp32u **ppBitStream,
    Ipp32s *pOffset,
    Ipp16s *pNumCoeff,
    Ipp16s **ppPosCoefbuf,
    Ipp32u uVLCSelect,
    Ipp16s uMaxNumCoeff,
    const Ipp32s **ppTblCoeffToken,
    const Ipp32s **ppTblTotalZeros,
    const Ipp32s **ppTblRunBefore,
    const Ipp32s *pScanMatrix,
    Ipp32s scanIdxStart,
    Ipp32s scanIdxEnd))


}; // UMC_H264_DECODER

using namespace UMC_H264_DECODER;

#endif // __UMC_H264_DEC_IPP_LEVEL_H
#endif // UMC_ENABLE_H264_VIDEO_DECODER
