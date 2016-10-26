//
// INTEL CORPORATION PROPRIETARY INFORMATION
//
// This software is supplied under the terms of a license agreement or
// nondisclosure agreement with Intel Corporation and may not be copied
// or disclosed except in accordance with the terms of that agreement.
//
// Copyright(C) 2012-2016 Intel Corporation. All Rights Reserved.
//

#include "umc_defs.h"
#ifdef UMC_ENABLE_H265_VIDEO_DECODER

#ifndef __H265_TR_QUANT_H
#define __H265_TR_QUANT_H

#include "umc_h265_dec_defs.h"
#include "mfx_h265_optimization.h"

namespace UMC_HEVC_DECODER
{
class DecodingContext;

// Transform and dequantization class
class H265TrQuant
{
    DISALLOW_COPY_AND_ASSIGN(H265TrQuant);

public:
    // Allocate inverse transform and dequantization buffers
    H265TrQuant();
    ~H265TrQuant();

    DecodingContext * m_context;

    // Do inverse transform of specified size
    template <typename DstCoeffsType>
    void InvTransformNxN(bool transQuantBypass, EnumTextType TxtType, Ipp32u Mode, DstCoeffsType* pResidual, size_t Stride,
        CoeffsPtr pCoeff, Ipp32u Size, bool transformSkip);

    // Recursively descend to basic transform blocks, inverse transform coefficients in them and/
    // add the result to prediction for complete reconstruct
    void InvRecurTransformNxN(H265CodingUnit* pCU, Ipp32u AbsPartIdx, Ipp32u Size, Ipp32u TrMode);

    // Process coefficients with transform skip flag
    template <int bitDepth, typename DstCoeffsType>
    void InvTransformSkip(CoeffsPtr pCoeff, DstCoeffsType* pResidual, size_t Stride, Ipp32u Size, bool inplace, Ipp32u bit_depth);

    // Process coefficients with transquant bypass flag
    template <typename DstCoeffsType>
    void InvTransformByPass(CoeffsPtr pCoeff, DstCoeffsType* pResidual, size_t Stride, Ipp32u Size, Ipp32u bitDepth, bool inplace);

private:
    CoeffsPtr m_pointerToMemory;
    CoeffsPtr m_residualsBuffer;
    CoeffsPtr m_residualsBuffer1;

    // forward Transform
    void Transform(Ipp32u Mode, Ipp8u* pResidual, Ipp32u Stride, Ipp32s* pCoeff, Ipp32s Width, Ipp32s Height);
};// END CLASS DEFINITION H265TrQuant

} // end namespace UMC_HEVC_DECODER

#endif //__H265_TR_QUANT_H
#endif // UMC_ENABLE_H265_VIDEO_DECODER
