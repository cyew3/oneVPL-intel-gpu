/******************************************************************************\
Copyright (c) 2018, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

This sample was distributed or derived from the Intel's Media Samples package.
The original version of this sample may be obtained from https://software.intel.com/en-us/intel-media-server-studio
or https://software.intel.com/en-us/media-client-solutions-support.
\**********************************************************************************/

#include "mse_estimator.h"
#include <algorithm>

static constexpr mfxF64 QSTEP[52] = {
    0.630,  0.707,  0.794,  0.891,  1.000,  1.122,   1.260,   1.414,   1.587,   1.782,   2.000,   2.245,   2.520,
    2.828,  3.175,  3.564,  4.000,  4.490,  5.040,   5.657,   6.350,   7.127,   8.000,   8.980,   10.079,  11.314,
    12.699, 14.254, 16.000, 17.959, 20.159, 22.627,  25.398,  28.509,  32.000,  35.919,  40.317,  45.255,  50.797,
    57.018, 64.000, 71.838, 80.635, 90.510, 101.594, 114.035, 128.000, 143.675, 161.270, 181.019, 203.187, 228.070
};

inline mfxU8 QPclamp(mfxU8 qp) { return Clip3(mfxU8(1), mfxU8(51), qp); }

// Least Squares approx for Betta coefficient
inline mfxF64 Betta(mfxU8 qp) { return std::max(0.1, 1.3 - 0.35*qp); }

// QP adjustment for Skip MSE estimation
inline mfxF64 deltaQP(mfxU8 qp) { return - 0.01*qp + 0.18; }

void MseEstimator::ProcessOneCU(const BS_HEVC2::CU* cu, const std::vector<mfxI32> refListActive[2])
{
    if (!cu)
        throw mfxError(MFX_ERR_UNDEFINED_BEHAVIOR, "ERROR: MseEstimator::ProcessOneCU : cu null pointer");

    if (!refListActive)
        throw mfxError(MFX_ERR_UNDEFINED_BEHAVIOR, "ERROR: MseEstimator::ProcessOneCU : refListActive null pointer");

    /* TMP CODE FOR TESTING */
    if (m_AlgorythmType != FAIR)
    {
        for (auto tu = cu->Tu; tu; tu = tu->Next)
        {
            if (!tu->tc_levels_luma)
                continue;

            switch (m_AlgorythmType)
            {
            case NNZ:
                m_CurrentFrameData.numNonZeroCoeff += std::count_if(tu->tc_levels_luma, tu->tc_levels_luma + tu->num_coeff, [](mfxI32 coeff) { return coeff != 0; });
                return;
            case SSC:
                m_CurrentFrameData.sumOfSquaredCoeff += std::accumulate(tu->tc_levels_luma, tu->tc_levels_luma + tu->num_coeff, mfxU64(0), [](mfxU64 sum_sq, mfxI32 coeff) { return sum_sq + coeff*coeff; });
                return;
            case FAIR:
                break;
            }
        }
    }

    mfxU32 cur_cu_size;

    if (cu->PredMode == BS_HEVC2::PRED_MODE::MODE_SKIP)
    {
        auto pu = cu->Pu;

        if (!pu)
            throw mfxError(MFX_ERR_UNDEFINED_BEHAVIOR, "ERROR: MseEstimator::ProcessOneCU : CU->Pu null pointer");

        switch (pu->inter_pred_idc)
        {
        case BS_HEVC2::INTER_PRED::PRED_L0:
            AddToCurrentPOCmap(refListActive[0].at(pu->ref_idx_l0), pu->w*pu->h);
            break;

        case BS_HEVC2::INTER_PRED::PRED_L1:
            AddToCurrentPOCmap(refListActive[1].at(pu->ref_idx_l1), pu->w*pu->h);
            break;

        case BS_HEVC2::INTER_PRED::PRED_BI:
            AddToCurrentPOCmap(refListActive[0].at(pu->ref_idx_l0), pu->w*pu->h >> 1);
            AddToCurrentPOCmap(refListActive[1].at(pu->ref_idx_l1), pu->w*pu->h >> 1);
            break;

        default:
            throw mfxError(MFX_ERR_UNDEFINED_BEHAVIOR, "ERROR: MseEstimator::ProcessOneCU : Invalid value of pu->inter_pred_idc");
        }
    }

    TransCoeffCuLevelStatData * current_level = nullptr;

    switch (cu->log2CbSize)
    {
    case 3:
        current_level = &m_CurrentFrameData.Transform8x8;
        cur_cu_size = 8 * 8;
        break;
    case 4:
        current_level = &m_CurrentFrameData.Transform16x16;
        cur_cu_size = 16 * 16;
        break;
    case 5:
        current_level = &m_CurrentFrameData.Transform32x32;
        cur_cu_size = 32 * 32;
        break;
    case 6:
        current_level = &m_CurrentFrameData.Transform64x64;
        cur_cu_size = 64 * 64;
        break;
    default:
        throw mfxError(MFX_ERR_UNDEFINED_BEHAVIOR, "ERROR: MseEstimator::ProcessOneCU : Invalid TU->log2TrafoSize");
    }
    m_CurrentFrameData.nPixels += cur_cu_size;

    switch (cu->PredMode)
    {
    case BS_HEVC2::PRED_MODE::MODE_INTRA:
        current_level->nIntra     += cur_cu_size;
        current_level->qpIntraSum += cu->QpY;
        ++current_level->numIntraCu;
        break;

    case BS_HEVC2::PRED_MODE::MODE_INTER:
        current_level->nInter     += cur_cu_size;
        current_level->qpInterSum += cu->QpY;
        ++current_level->numInterCu;
        break;

    case BS_HEVC2::PRED_MODE::MODE_SKIP:
        current_level->nSkip     += cur_cu_size;
        current_level->qpSkipSum += cu->QpY;
        ++current_level->numSkipCu;
        break;

    default:
        throw mfxError(MFX_ERR_UNDEFINED_BEHAVIOR, "ERROR: MseEstimator::ProcessOneCU : Invalid PredMode");
    }

    if (!cu->Tu && cu->PredMode == BS_HEVC2::PRED_MODE::MODE_INTER)
    {
        current_level->nZeroQuantCoeffs += cur_cu_size;
    }

    for (auto tu = cu->Tu; tu; tu = tu->Next)
    {
        if (tu->num_coeff == 0 && cu->PredMode == BS_HEVC2::PRED_MODE::MODE_INTER)
        {
            current_level->nZeroQuantCoeffs += 1 << (2*tu->log2TrafoSize);
            continue;
        }

        switch (cu->PredMode)
        {
        case BS_HEVC2::PRED_MODE::MODE_INTRA:
            //current_level->nZeroQuantCoeffs += std::count_if(tu->tc_levels_luma, tu->tc_levels_luma + tu->num_coeff, [](mfxI32 coeff) { return coeff == 0; });
            current_level->SumOfCoeffs        += std::accumulate(tu->tc_levels_luma, tu->tc_levels_luma + tu->num_coeff, 0);
            current_level->SumOfSquaredCoeffs += std::accumulate(tu->tc_levels_luma, tu->tc_levels_luma + tu->num_coeff, mfxU32(0), [](mfxU32 sum_sq, mfxI32 coeff) { return sum_sq + coeff*coeff; });
            break;

        case BS_HEVC2::PRED_MODE::MODE_INTER:
            current_level->nZeroQuantCoeffs   += std::count_if(tu->tc_levels_luma, tu->tc_levels_luma + tu->num_coeff, [](mfxI32 coeff) { return coeff == 0; });
            current_level->SumOfCoeffs        += std::accumulate(tu->tc_levels_luma, tu->tc_levels_luma + tu->num_coeff, 0);
            current_level->SumOfSquaredCoeffs += std::accumulate(tu->tc_levels_luma, tu->tc_levels_luma + tu->num_coeff, mfxU32(0), [](mfxU32 sum_sq, mfxI32 coeff) { return sum_sq + coeff*coeff; });
            break;
        }
    } // for (auto tu = cu->Tu; tu; tu = tu->Next)
}

void MseEstimator::SetFrameParameters(const BS_HEVC2::SPS* sps)
{
    if (!sps)
        throw mfxError(MFX_ERR_UNDEFINED_BEHAVIOR, "ERROR: MseEstimator::SetFrameParameters : SPS is a NULL ptr");

    m_CurrentFrameData.MinActiveCuLevel = 1 << (sps->log2_min_luma_coding_block_size_minus3 + 3);
    m_CurrentFrameData.MaxActiveCuLevel = 1 << (sps->log2_min_luma_coding_block_size_minus3 + 3
                                                + sps->log2_diff_max_min_luma_coding_block_size);
}

mfxF64 MseEstimator::EstimateMSE()
{
    /* TMP CODE FOR TESTING */
    switch (m_AlgorythmType)
    {
    case NNZ:
        return m_CurrentFrameData.EstimatedMSE = m_CurrentFrameData.numNonZeroCoeff;
    case SSC:
        return m_CurrentFrameData.EstimatedMSE = m_CurrentFrameData.sumOfSquaredCoeff;
    case FAIR:
        break;
    }

    mfxF64 MSE = 0.0, f = m_CurrentFrameData.qpOffset;

    mfxU32 nTotalIntra = 0, qpTotalIntraSum = 0 , nTotalIntraCu = 0, qpTotalSkipSum = 0, nTotalSkipCu = 0, nTotalInterZeroQuant = 0, nTotalSkip = 0;

    // Estimate distortion introduced by Inter coding
    for (mfxU8 level_i = m_CurrentFrameData.MinActiveCuLevel; level_i <= m_CurrentFrameData.MaxActiveCuLevel; level_i <<= 1)
    {
        TransCoeffCuLevelStatData * current_level = nullptr;

        switch (level_i)
        {
        case 8:
            current_level = &m_CurrentFrameData.Transform8x8;
            break;
        case 16:
            current_level = &m_CurrentFrameData.Transform16x16;
            break;
        case 32:
            current_level = &m_CurrentFrameData.Transform32x32;
            break;
        case 64:
            current_level = &m_CurrentFrameData.Transform64x64;
            break;
        default:
            throw mfxError(MFX_ERR_UNDEFINED_BEHAVIOR, "ERROR: MseEstimator::EstimateMSE : Invalid level_i");
        }
        // For further Intra estimation
        nTotalIntra += current_level->nIntra;
        // For further Skip estimation
        nTotalSkip += current_level->nSkip;

        mfxU32 nNonSkip             = current_level->nIntra + current_level->nInter;
        current_level->AllZeroQuant = current_level->nZeroQuantCoeffs && (current_level->nZeroQuantCoeffs == current_level->nInter);

        nTotalInterZeroQuant += current_level->nZeroQuantCoeffs;

        // For average Intra CU qp estimation
        qpTotalIntraSum += current_level->qpIntraSum;
        nTotalIntraCu   += current_level->numIntraCu;

        // For average Skip CU qp estimation
        qpTotalSkipSum += current_level->qpSkipSum;
        nTotalSkipCu   += current_level->numSkipCu;

        if (nNonSkip)
        {
            // Variance of current level
            current_level->Variance = mfxF64(current_level->SumOfSquaredCoeffs) / nNonSkip - std::pow(mfxF64(current_level->SumOfCoeffs) / nNonSkip, 2);
        }

        if (current_level->nInter)
        {
            mfxU8  qp = QPclamp(current_level->qpInterSum / current_level->numInterCu);
            mfxF64 q  = QSTEP[qp];

            // Estimator for Laplace distribution lambda parameter for current level (only inter)
            mfxF64 lambda_k;
            if (current_level->AllZeroQuant)
            {
                // Use exponential regression for all-zero quantized levels
                mfxF64 nu  = EstimateNu();
                mfxF64 gam = EstimateGam(nu);

                mfxF64 m = std::pow(10, gam);
                mfxF64 c = std::pow(10, nu);
                mfxF64 sig = m * std::pow(c, 7 - std::log2(level_i));

                lambda_k = sig ? std::sqrt(2.0 / sig) : 0.0;
            }
            else
            {
                lambda_k = current_level->nZeroQuantCoeffs ? -std::log(mfxF64(nNonSkip - current_level->nZeroQuantCoeffs) / nNonSkip) / ((1.0 - f) * q) : 0.0;
            }

            // Estimator for weight of current transform level (only inter)
            mfxF64 xu_k = mfxF64(current_level->nInter) / m_CurrentFrameData.nPixels;

            // Contribution of current transform level to result MSE (only inter)
            mfxF64 levelContribution = lambda_k ? xu_k * ((2.0 + lambda_k*q - 2.0 * f*lambda_k*q)*lambda_k*q*std::exp(f*lambda_k*q) + 2.0 - 2.0 * std::exp(lambda_k*q)) /
                (lambda_k*lambda_k * (1.0 - std::exp(lambda_k*q))) : xu_k * Betta(qp) * q*q*(3 * f*f - 3 * f + 1) / 3;

            MSE += levelContribution;
        }
    } // for (mfxU8 level_i = m_CurrentFrameData.MinActiveLevel; level_i <= m_CurrentFrameData.MaxActiveLevel; level_i <<= 1)

    // Check if current Inter frame consist of almost-all-zero-quant pixels
    mfxF64 ratio = mfxF64(nTotalInterZeroQuant + nTotalSkip) / m_CurrentFrameData.nPixels;
    if (ratio > 0.95)
    {
        m_CurrentFrameData.EstimatedMSE = m_FrameStatQueue.back().EstimatedMSE;
        return m_CurrentFrameData.EstimatedMSE;
    }

    // Estimate Intra blocks contribution
    if (nTotalIntraCu)
    {
        mfxU8 qp = QPclamp(qpTotalIntraSum / nTotalIntraCu);
        mfxF64 q = QSTEP[qp];
        mfxF64 IntraContribution = (mfxF64(nTotalIntra) / m_CurrentFrameData.nPixels) * Betta(qp) * q*q*(3 * f*f - 3 * f + 1) / 3;

        MSE += IntraContribution;
    }

    // Estimate Skip blocks contribution
    if (nTotalSkipCu)
    {
        mfxU8 qp = QPclamp(qpTotalSkipSum / nTotalSkipCu);

        mfxF64 SkipContribution = 0.0;
        for (auto poc_pixels : m_CurrentFrameData.POCtoNumSkipPixelsRef)
        {
            if (m_POCtoMSE.find(poc_pixels.first) == m_POCtoMSE.end())
                throw mfxError(MFX_ERR_UNDEFINED_BEHAVIOR, std::string("ERROR: MseEstimator::EstimateMSE : poc ") + std::to_string(poc_pixels.first) + std::string(" is not cached"));

            // Weighted average of references of current Skip block
            SkipContribution += m_POCtoMSE[poc_pixels.first] * (mfxF64(poc_pixels.second) / m_CurrentFrameData.nPixels + deltaQP(qp));
        }

        MSE += SkipContribution;
    }

    m_CurrentFrameData.EstimatedMSE = MSE;

    return m_CurrentFrameData.EstimatedMSE;
}

mfxF64 MseEstimator::GetMeanVar(mfxU8 CuSize)
{
    return std::accumulate(m_FrameStatQueue.begin(), m_FrameStatQueue.end(), 0.0,
        [CuSize](mfxF64 sum, EstimatorFrameStatData& frame_data)
    {
        switch (CuSize)
        {
        case 8:
            return sum + frame_data.Transform8x8.Variance;
        case 16:
            return sum + frame_data.Transform16x16.Variance;
        case 32:
            return sum + frame_data.Transform32x32.Variance;
        case 64:
            return sum + frame_data.Transform64x64.Variance;
        default:
            throw mfxError(MFX_ERR_UNDEFINED_BEHAVIOR, "ERROR: MseEstimator::GetMeanVar : Invalid CuSize");
        }
    }) / m_FrameStatQueue.size();
}

mfxF64 MseEstimator::EstimateNu()
{
    mfxF64 a = 0.0, b = 0.0, c = 0.0, d = 0.0, meanVar, logVar, norm = 1.0 / (m_CurrentFrameData.MaxActiveCuLevel - m_CurrentFrameData.MinActiveCuLevel);
    mfxU32 k;

    for (mfxU8 level_i = m_CurrentFrameData.MinActiveCuLevel; level_i <= m_CurrentFrameData.MaxActiveCuLevel; level_i <<= 1)
    {
        meanVar = GetMeanVar(level_i);

        if (meanVar == 0.0)
            continue;

        logVar  = std::log10(meanVar);
        k       = 7 - std::log2(level_i);

        a += k*logVar;
        b += logVar;
        c += k;
        d += k*k;
    }

    mfxF64 denom = d - norm * c * c;

    return denom ? (a - norm * b * c) / denom : 0.0;
}

mfxF64 MseEstimator::EstimateGam(mfxF64 nu)
{
    mfxF64 meanVar, logVar, norm = 1.0 / (m_CurrentFrameData.MaxActiveCuLevel - m_CurrentFrameData.MinActiveCuLevel), a = 0.0, b = 0.0;

    mfxU32 k;

    for (mfxU8 level_i = m_CurrentFrameData.MinActiveCuLevel; level_i <= m_CurrentFrameData.MaxActiveCuLevel; level_i <<= 1)
    {
        meanVar = GetMeanVar(level_i);

        if (meanVar == 0.0)
            continue;

        logVar = std::log10(meanVar);
        k      = 7 - std::log2(level_i);

        a += logVar;
        b += k;
    }

    return norm * (a - nu * b);
}
