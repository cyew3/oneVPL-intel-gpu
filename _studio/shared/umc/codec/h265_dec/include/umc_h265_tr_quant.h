// Copyright (c) 2012-2019 Intel Corporation
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
#ifdef MFX_ENABLE_H265_VIDEO_DECODE

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
    void InvTransformNxN(bool transQuantBypass, EnumTextType TxtType, uint32_t Mode, DstCoeffsType* pResidual, size_t Stride,
        CoeffsPtr pCoeff, uint32_t Size, bool transformSkip);

    // Recursively descend to basic transform blocks, inverse transform coefficients in them and/
    // add the result to prediction for complete reconstruct
    void InvRecurTransformNxN(H265CodingUnit* pCU, uint32_t AbsPartIdx, uint32_t Size, uint32_t TrMode);

    // Process coefficients with transform skip flag
    template <int bitDepth, typename DstCoeffsType>
    void InvTransformSkip(CoeffsPtr pCoeff, DstCoeffsType* pResidual, size_t Stride, uint32_t Size, bool inplace, uint32_t bit_depth);

    // Process coefficients with transquant bypass flag
    template <typename DstCoeffsType>
    void InvTransformByPass(CoeffsPtr pCoeff, DstCoeffsType* pResidual, size_t Stride, uint32_t Size, uint32_t bitDepth, bool inplace);

private:
    CoeffsPtr m_pointerToMemory;
    CoeffsPtr m_residualsBuffer;
    CoeffsPtr m_residualsBuffer1;

    // forward Transform
    void Transform(uint32_t Mode, uint8_t* pResidual, uint32_t Stride, int32_t* pCoeff, int32_t Width, int32_t Height);
};// END CLASS DEFINITION H265TrQuant

} // end namespace UMC_HEVC_DECODER

#endif //__H265_TR_QUANT_H
#endif // MFX_ENABLE_H265_VIDEO_DECODE
